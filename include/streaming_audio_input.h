#ifndef STREAMING_AUDIO_INPUT_H
#define STREAMING_AUDIO_INPUT_H

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <csignal>
#include "config.h"

// Reference to the global running flag from main.cpp
extern volatile sig_atomic_t g_running;

// VAD (Voice Activity Detection) parameters
struct VADParams {
    float threshold = 0.6f;       // Voice activation threshold (0.0 to 1.0)
    float freq_threshold = 100.0f; // Frequency threshold for speech detection
    int min_speech_ms = 300;      // Minimum speech duration in ms to be considered valid
    int max_silence_ms = 1000;    // Maximum silence duration in ms before stopping capture
    int padding_ms = 500;         // Padding at the beginning and end of speech segments
};

class StreamingAudioInput {
private:
    AudioConfig config;
    bool debug_enabled = false;
    VADParams vad_params;

    // Audio buffers
    std::vector<float> audio_buffer;     // Main audio buffer for processing
    std::vector<float> capture_buffer;   // Buffer for ongoing capture
    
    // Threading
    std::thread capture_thread;
    std::mutex buffer_mutex;
    std::condition_variable cv;
    std::atomic<bool> is_capturing{false};
    std::atomic<bool> speech_detected{false};
    
    // Audio capture method using ALSA/PulseAudio
    bool start_audio_capture();
    void stop_audio_capture();
    void capture_thread_func();
    
    // VAD functions
    bool detect_speech(const std::vector<float>& audio, int sample_rate);
    
    // List available audio devices
    void list_devices();

public:
    StreamingAudioInput(const AudioConfig& cfg, bool debug = false);
    ~StreamingAudioInput();
    
    // Start/stop the audio capture thread
    bool start();
    void stop();
    
    // Wait for speech and return a buffer of audio containing the speech
    std::vector<float> wait_for_speech(int timeout_ms = 10000);
    
    // Check if speech is currently being detected
    bool is_speech_active() const;
    
    // Set VAD parameters
    void set_vad_params(const VADParams& params);
    
    // Get sample rate
    int get_sample_rate() const { return config.sample_rate; }
};

#endif // STREAMING_AUDIO_INPUT_H