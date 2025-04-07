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
        std::cerr << "Hint: You may need to adjust the audio.device in config.json or use --input-device" << std::endl;
        is_capturing.store(false);
        return;
    }
    
    std::cout << "Debug: Successfully opened audio device: " << config.device << std::endl;
    
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
    std::cout << "Debug: Set access type to interleaved" << std::endl;
    
    // Set sample format (16-bit signed little endian)
    err = snd_pcm_hw_params_set_format(pcm_handle, hw_params, SND_PCM_FORMAT_S16_LE);
    if (err < 0) {
        std::cerr << "Error: Cannot set sample format: " << snd_strerror(err) << std::endl;
        snd_pcm_close(pcm_handle);
        is_capturing.store(false);
        return;
    }
    std::cout << "Debug: Set sample format to 16-bit signed little endian" << std::endl;
    
    // Set sample rate
    unsigned int rate = static_cast<unsigned int>(config.sample_rate);
    unsigned int original_rate = rate;
    err = snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &rate, 0);
    if (err < 0) {
        std::cerr << "Error: Cannot set sample rate: " << snd_strerror(err) << std::endl;
        snd_pcm_close(pcm_handle);
        is_capturing.store(false);
        return;
    }
    
    if (rate != original_rate) {
        std::cout << "Debug: Requested sample rate " << original_rate << " Hz, but got " << rate << " Hz" << std::endl;
    } else {
        std::cout << "Debug: Set sample rate to " << rate << " Hz" << std::endl;
    }
    
    // Set channels (mono)
    err = snd_pcm_hw_params_set_channels(pcm_handle, hw_params, 1);
    if (err < 0) {
        std::cerr << "Error: Cannot set channels: " << snd_strerror(err) << std::endl;
        snd_pcm_close(pcm_handle);
        is_capturing.store(false);
        return;
    }
    std::cout << "Debug: Set channels to mono (1 channel)" << std::endl;
    
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
    std::cout << "Debug: Starting audio capture loop" << std::endl;
    int buffer_count = 0;
    while (is_capturing.load() && g_running) {
        // Read audio data from device
        err = snd_pcm_readi(pcm_handle, pcm_buffer.data(), frames_per_chunk);
        
        // Every 100 buffers (10 seconds at 100ms buffers), print an info message
        if (++buffer_count % 100 == 0) {
            std::cout << "Debug: Still capturing audio, processed " << buffer_count << " buffers" << std::endl;
        }
        
        if (err == -EPIPE) {
            // Underrun occurred, recover
            snd_pcm_prepare(pcm_handle);
            std::cerr << "Warning: Buffer underrun occurred" << std::endl;
            continue;
        } else if (err < 0) {
            // Other error
            std::cerr << "Error: Cannot read from audio interface: " << snd_strerror(err) << std::endl;
            break;
        } else if (err != frames_per_chunk) {
            // Partial read
            std::cerr << "Warning: Partial read, only got " << err << " frames" << std::endl;
        } else {
            if (debug_enabled && buffer_count % 50 == 0) {
                // Calculate peak amplitude of this buffer
                float peak = 0.0f;
                for (int i = 0; i < err; i++) {
                    float sample = std::abs(pcm_buffer[i]) / 32768.0f;
                    if (sample > peak) peak = sample;
                }
                std::cout << "Debug: Successfully read " << err << " frames, peak amplitude: " << peak << std::endl;
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
            
            // Keep a reasonable size for the capture buffer based on buffer_history_ms
            const size_t buffer_history_frames = (vad_params.buffer_history_ms * rate) / 1000;
            if (capture_buffer.size() > buffer_history_frames) {
                // Instead of removing entire excess, keep at least double the history
                // This gives us plenty of context before speech starts
                capture_buffer.erase(capture_buffer.begin(), 
                                    capture_buffer.begin() + (capture_buffer.size() - buffer_history_frames));
            }
            
            // Print capture buffer size periodically (every 200 buffers)
            if (debug_enabled && buffer_count % 200 == 0) {
                const float buffer_seconds = static_cast<float>(capture_buffer.size()) / rate;
                std::cout << "Debug: Capture buffer size: " << capture_buffer.size() 
                          << " samples (" << buffer_seconds << " seconds)" << std::endl;
            }
        }
        
        // Add to VAD buffer for speech detection
        vad_buffer.insert(vad_buffer.end(), float_buffer.begin(), float_buffer.end());
        
        // Process VAD on a 500ms window for better speech detection
        const size_t vad_window_size = rate / 2; // 500ms for better speech detection
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
                    
                    // When speech starts, grab audio from the beginning with ample padding
                    std::lock_guard<std::mutex> lock(buffer_mutex);
                    
                    // Use more padding to ensure we capture the beginning of speech
                    // Instead of just using padding_frames, use at least 50% of the available buffer
                    size_t padding_start = 0;
                    if (capture_buffer.size() > padding_frames) {
                        // Use the larger of: specified padding OR half the available buffer
                        size_t half_buffer = capture_buffer.size() / 2;
                        size_t padding_frames_size = static_cast<size_t>(padding_frames);
                        size_t extended_padding = (padding_frames_size > half_buffer) ? padding_frames_size : half_buffer;
                        
                        // But don't go beyond the buffer start
                        if (extended_padding < capture_buffer.size()) {
                            padding_start = capture_buffer.size() - extended_padding;
                        }
                    }
                    
                    // Log how much context we're including
                    if (debug_enabled) {
                        float context_seconds = static_cast<float>(padding_start) / rate;
                        std::cout << "Debug: Including " << context_seconds << " seconds of audio context" << std::endl;
                    }
                    
                    // Start a new audio buffer from the padded point
                    audio_buffer.assign(capture_buffer.begin() + padding_start, capture_buffer.end());
                }
            } else {
                // Not speech
                silence_frames += err;
                
                if (was_speaking) {
                    // Check if silence has been detected for max_silence_ms milliseconds
                    // Or if silence detected after at least 1 second of speech
                    bool long_enough_speech = speech_frames > rate; // At least 1 second of speech
                    bool silence_detected = silence_frames >= max_silence_frames;
                    bool speech_followed_by_short_silence = long_enough_speech && silence_frames >= (max_silence_frames / 3);
                    
                    if (silence_detected || speech_followed_by_short_silence) {
                        // Speech end detected
                        if (debug_enabled) {
                            std::cout << "Info: Speech ended after " << speech_frames * 1000 / rate << " ms "
                                      << "(silence: " << silence_frames * 1000 / rate << " ms)" << std::endl;
                            
                            if (speech_followed_by_short_silence && !silence_detected) {
                                std::cout << "Info: Detected end of speech due to short silence after long speech" << std::endl;
                            }
                        }
                        
                        // When speech ends, include the silence as padding
                        {
                            std::lock_guard<std::mutex> lock(buffer_mutex);
                            
                            // Add the silence as padding, but add even more for safety
                            // We want to ensure we don't cut off any speech at the end
                            size_t silence_padding = static_cast<size_t>(silence_frames + padding_frames);
                            size_t available_padding = 0;
                            if (capture_buffer.size() > audio_buffer.size()) {
                                available_padding = capture_buffer.size() - audio_buffer.size();
                            }
                            size_t padding_to_add = (silence_padding < available_padding) ? silence_padding : available_padding;
                            
                            if (debug_enabled) {
                                float padding_seconds = static_cast<float>(padding_to_add) / rate;
                                std::cout << "Debug: Adding " << padding_seconds << " seconds of end padding" << std::endl;
                            }
                            
                            if (padding_to_add > 0) {
                                audio_buffer.insert(audio_buffer.end(), 
                                                  capture_buffer.end() - padding_to_add,
                                                  capture_buffer.end());
                            }
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