#include "streaming_audio_input.h"
#include "vad.h"
#include <iostream>
#include <chrono>
#include <memory>
#include <fstream>
#include <cstdio>
#include <array>

// ALSA headers
#include <alsa/asoundlib.h>

// Constructor
StreamingAudioInput::StreamingAudioInput(const AudioConfig& cfg, bool debug)
    : config(cfg), debug_enabled(debug) {
    // If the user requested "list" as the device, show available devices
    if (config.device == "list") {
        list_devices();
        config.device = "default"; // Reset to default after listing
    }
}

// Destructor - ensure capture is stopped
StreamingAudioInput::~StreamingAudioInput() {
    stop();
}

// List available audio devices
void StreamingAudioInput::list_devices() {
    std::cout << "Available audio input devices:" << std::endl;
    
    // Try ALSA devices
    std::system("arecord -l 2>/dev/null");
    
    // Try PulseAudio devices
    std::system("pactl list sources 2>/dev/null | grep -E 'Name:|Description:' | grep -v monitor");
}

// Start audio capture thread
bool StreamingAudioInput::start() {
    if (is_capturing.load()) {
        if (debug_enabled) {
            std::cout << "Info: Audio capture is already running" << std::endl;
        }
        return true;
    }
    
    // Clear any existing audio data
    {
        std::lock_guard<std::mutex> lock(buffer_mutex);
        audio_buffer.clear();
        capture_buffer.clear();
    }
    
    is_capturing.store(true);
    speech_detected.store(false);
    
    try {
        capture_thread = std::thread(&StreamingAudioInput::capture_thread_func, this);
        
        if (debug_enabled) {
            std::cout << "Info: Audio capture thread started" << std::endl;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error: Failed to start audio capture thread: " << e.what() << std::endl;
        is_capturing.store(false);
        return false;
    }
}

// Stop audio capture thread
void StreamingAudioInput::stop() {
    is_capturing.store(false);
    
    if (capture_thread.joinable()) {
        cv.notify_all(); // Wake up any waiting threads
        capture_thread.join();
        
        if (debug_enabled) {
            std::cout << "Info: Audio capture thread stopped" << std::endl;
        }
    }
}

// Set VAD parameters
void StreamingAudioInput::set_vad_params(const VADParams& params) {
    vad_params = params;
}

// Check if speech is currently active
bool StreamingAudioInput::is_speech_active() const {
    return speech_detected.load();
}

// Wait for speech and return audio buffer
std::vector<float> StreamingAudioInput::wait_for_speech(int timeout_ms) {
    // Start audio capture if not already running
    if (!is_capturing.load()) {
        if (!start()) {
            std::cerr << "Error: Failed to start audio capture" << std::endl;
            return {};
        }
    }
    
    std::unique_lock<std::mutex> lock(buffer_mutex);
    
    // Wait for speech detection or timeout
    auto timeout = std::chrono::milliseconds(timeout_ms);
    bool speech_found = cv.wait_for(lock, timeout, [this] {
        return !is_capturing.load() || !audio_buffer.empty();
    });
    
    if (!speech_found || audio_buffer.empty()) {
        if (debug_enabled) {
            std::cout << "Info: No speech detected within timeout" << std::endl;
        }
        return {};
    }
    
    // Copy the audio data and clear the buffer
    std::vector<float> result = audio_buffer;
    audio_buffer.clear();
    
    return result;
}

// Main capture thread function
void StreamingAudioInput::capture_thread_func() {
    if (debug_enabled) {
        std::cout << "Info: Audio capture thread starting on device: " << config.device << std::endl;
    }
    
    snd_pcm_t* pcm_handle = nullptr;
    int err;
    
    // Open ALSA device for capture
    if ((err = snd_pcm_open(&pcm_handle, config.device.c_str(), SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        std::cerr << "Error: Cannot open audio device " << config.device << ": " << snd_strerror(err) << std::endl;
        is_capturing.store(false);
        return;
    }
    
    // Set hardware parameters
    snd_pcm_hw_params_t* hw_params;
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(pcm_handle, hw_params);
    
    // Set access type
    err = snd_pcm_hw_params_set_access(pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        std::cerr << "Error: Cannot set access type: " << snd_strerror(err) << std::endl;
        snd_pcm_close(pcm_handle);
        is_capturing.store(false);
        return;
    }
    
    // Set sample format (16-bit signed little endian)
    err = snd_pcm_hw_params_set_format(pcm_handle, hw_params, SND_PCM_FORMAT_S16_LE);
    if (err < 0) {
        std::cerr << "Error: Cannot set sample format: " << snd_strerror(err) << std::endl;
        snd_pcm_close(pcm_handle);
        is_capturing.store(false);
        return;
    }
    
    // Set sample rate
    unsigned int rate = static_cast<unsigned int>(config.sample_rate);
    err = snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &rate, 0);
    if (err < 0) {
        std::cerr << "Error: Cannot set sample rate: " << snd_strerror(err) << std::endl;
        snd_pcm_close(pcm_handle);
        is_capturing.store(false);
        return;
    }
    
    // Set channels (mono)
    err = snd_pcm_hw_params_set_channels(pcm_handle, hw_params, 1);
    if (err < 0) {
        std::cerr << "Error: Cannot set channels: " << snd_strerror(err) << std::endl;
        snd_pcm_close(pcm_handle);
        is_capturing.store(false);
        return;
    }
    
    // Set buffer size (100ms worth of samples)
    snd_pcm_uframes_t buffer_size = rate / 10;
    err = snd_pcm_hw_params_set_buffer_size_near(pcm_handle, hw_params, &buffer_size);
    if (err < 0) {
        std::cerr << "Error: Cannot set buffer size: " << snd_strerror(err) << std::endl;
        snd_pcm_close(pcm_handle);
        is_capturing.store(false);
        return;
    }
    
    // Apply hardware parameters
    err = snd_pcm_hw_params(pcm_handle, hw_params);
    if (err < 0) {
        std::cerr << "Error: Cannot set hardware parameters: " << snd_strerror(err) << std::endl;
        snd_pcm_close(pcm_handle);
        is_capturing.store(false);
        return;
    }
    
    // Prepare device for use
    err = snd_pcm_prepare(pcm_handle);
    if (err < 0) {
        std::cerr << "Error: Cannot prepare audio interface: " << snd_strerror(err) << std::endl;
        snd_pcm_close(pcm_handle);
        is_capturing.store(false);
        return;
    }
    
    // Buffer to store 100ms of 16-bit PCM samples
    const int frames_per_chunk = static_cast<int>(rate / 10);
    std::vector<int16_t> pcm_buffer(frames_per_chunk);
    
    // Buffers for VAD processing
    std::vector<float> vad_buffer;
    vad_buffer.reserve(static_cast<size_t>(rate) * 2); // Reserve 2 seconds worth of samples
    
    // Detection state variables
    bool was_speaking = false;           // Was speaking in previous chunk
    int silence_frames = 0;              // Consecutive silence frames
    int speech_frames = 0;               // Consecutive speech frames
    int padding_frames = vad_params.padding_ms * rate / 1000; // Frames to add as padding
    int min_speech_frames = vad_params.min_speech_ms * rate / 1000; // Minimum speech duration
    int max_silence_frames = vad_params.max_silence_ms * rate / 1000; // Maximum silence duration
    
    // Main capture loop
    while (is_capturing.load() && g_running) {
        // Read audio data from device
        err = snd_pcm_readi(pcm_handle, pcm_buffer.data(), frames_per_chunk);
        
        if (err == -EPIPE) {
            // Underrun occurred, recover
            snd_pcm_prepare(pcm_handle);
            if (debug_enabled) {
                std::cerr << "Warning: Buffer underrun occurred" << std::endl;
            }
            continue;
        } else if (err < 0) {
            // Other error
            std::cerr << "Error: Cannot read from audio interface: " << snd_strerror(err) << std::endl;
            break;
        } else if (err != frames_per_chunk) {
            // Partial read
            if (debug_enabled) {
                std::cerr << "Warning: Partial read, only got " << err << " frames" << std::endl;
            }
        }
        
        // Convert int16 PCM to float32 normalized to [-1, 1]
        std::vector<float> float_buffer(frames_per_chunk);
        for (int i = 0; i < err; i++) {
            float_buffer[i] = static_cast<float>(pcm_buffer[i]) / 32768.0f;
        }
        
        // Add to capture buffer for continuous processing
        {
            std::lock_guard<std::mutex> lock(buffer_mutex);
            capture_buffer.insert(capture_buffer.end(), float_buffer.begin(), float_buffer.end());
            
            // Keep a reasonable size for the capture buffer (max 30 seconds)
            const size_t max_buffer_size = rate * 30;
            if (capture_buffer.size() > max_buffer_size) {
                capture_buffer.erase(capture_buffer.begin(), capture_buffer.begin() + (capture_buffer.size() - max_buffer_size));
            }
        }
        
        // Add to VAD buffer for speech detection
        vad_buffer.insert(vad_buffer.end(), float_buffer.begin(), float_buffer.end());
        
        // Process VAD on a 200ms window
        const size_t vad_window_size = rate / 5; // 200ms
        if (vad_buffer.size() >= vad_window_size) {
            // Process a sliding window for VAD
            std::vector<float> vad_window(vad_buffer.end() - vad_window_size, vad_buffer.end());
            
            // Detect speech in the current window
            bool is_speech = detect_voice_activity(vad_window, rate, vad_params.threshold, vad_params.freq_threshold);
            
            if (is_speech) {
                speech_frames += err;
                silence_frames = 0;
                
                if (!was_speaking && speech_frames >= min_speech_frames) {
                    // Speech start detected
                    if (debug_enabled) {
                        std::cout << "Info: Speech detected" << std::endl;
                    }
                    
                    speech_detected.store(true);
                    was_speaking = true;
                    
                    // When speech starts, grab audio from the beginning with padding
                    std::lock_guard<std::mutex> lock(buffer_mutex);
                    size_t padding_start = 0;
                    if (capture_buffer.size() > padding_frames) {
                        padding_start = capture_buffer.size() - vad_window_size - padding_frames;
                    }
                    
                    // Start a new audio buffer from the padded point
                    audio_buffer.assign(capture_buffer.begin() + padding_start, capture_buffer.end());
                }
            } else {
                // Not speech
                silence_frames += err;
                
                if (was_speaking) {
                    if (silence_frames >= max_silence_frames) {
                        // Speech end detected
                        if (debug_enabled) {
                            std::cout << "Info: Speech ended after " << speech_frames * 1000 / rate << " ms" << std::endl;
                        }
                        
                        // When speech ends, include the silence as padding
                        {
                            std::lock_guard<std::mutex> lock(buffer_mutex);
                            // Add the silence as padding
                            audio_buffer.insert(audio_buffer.end(), 
                                               capture_buffer.end() - silence_frames,
                                               capture_buffer.end());
                        }
                        
                        // Reset state
                        was_speaking = false;
                        speech_frames = 0;
                        speech_detected.store(false);
                        
                        // Notify waiting threads that we have audio data
                        cv.notify_all();
                    }
                } else {
                    // Reset detection if we've been silent too long
                    speech_frames = 0;
                }
            }
            
            // Slide the VAD buffer, keeping just enough for next window
            if (vad_buffer.size() > vad_window_size) {
                vad_buffer.erase(vad_buffer.begin(), vad_buffer.end() - vad_window_size/2);
            }
        }
        
        // Small sleep to prevent high CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Close the audio device
    snd_pcm_close(pcm_handle);
    
    // Signal end of capture
    is_capturing.store(false);
    cv.notify_all();
    
    if (debug_enabled) {
        std::cout << "Info: Audio capture thread exiting" << std::endl;
    }
}

// Implement detect_speech using our VAD function
bool StreamingAudioInput::detect_speech(const std::vector<float>& audio, int sample_rate) {
    return detect_voice_activity(audio, sample_rate, vad_params.threshold, vad_params.freq_threshold);
}