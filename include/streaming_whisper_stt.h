#ifndef STREAMING_WHISPER_STT_H
#define STREAMING_WHISPER_STT_H

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <csignal>
#include "config.h"

// Forward declaration for whisper context to avoid including the full header
struct whisper_context;

class StreamingWhisperSTT {
private:
    WhisperConfig config;
    whisper_context* ctx = nullptr;
    bool is_initialized = false;
    std::atomic<bool> is_processing{false};
    bool debug_enabled = false;
    volatile sig_atomic_t* running_flag = nullptr;
    
    // Initialize whisper context
    bool initialize();
    
    // Free whisper context
    void cleanup();
    
public:
    StreamingWhisperSTT(const WhisperConfig& cfg, bool debug = false);
    ~StreamingWhisperSTT();
    
    // Set running flag pointer
    void set_running_flag(volatile sig_atomic_t* flag) { running_flag = flag; }
    
    // Process an audio buffer containing PCM float samples
    std::string process_audio(const std::vector<float>& audio_buffer, int sample_rate);
    
    // Get the last transcript from whisper (for partial results)
    std::string get_last_transcript() const;
    
    // Check if whisper is currently processing
    bool is_busy() const { return is_processing.load(); }
};

#endif // STREAMING_WHISPER_STT_H