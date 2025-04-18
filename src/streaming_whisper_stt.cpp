#include "streaming_whisper_stt.h"
#include <iostream>
#include <filesystem>
#include <cstring>
#include <cctype>  // For std::isspace
#include <whisper.h>

namespace fs = std::filesystem;

// Constructor
StreamingWhisperSTT::StreamingWhisperSTT(const WhisperConfig& cfg, bool debug)
    : config(cfg), debug_enabled(debug) {
    // Initialize whisper context
    if (!initialize()) {
        std::cerr << "Error: Failed to initialize Whisper model" << std::endl;
    }
}

// Destructor
StreamingWhisperSTT::~StreamingWhisperSTT() {
    cleanup();
}

// Initialize whisper context
bool StreamingWhisperSTT::initialize() {
    if (is_initialized) {
        if (debug_enabled) {
            std::cout << "Info: Whisper context already initialized" << std::endl;
        }
        return true;
    }
    
    // Check if whisper executable exists (for validation, not used directly)
    if (!fs::exists(config.executable)) {
        std::cerr << "Error: Whisper executable not found at " << config.executable << std::endl;
        std::cerr << "Please install whisper.cpp and update the config." << std::endl;
        return false;
    }
    
    // Check for whisper model
    std::string model_path = "./whisper.cpp/models/ggml-" + config.model + ".bin";
    if (!fs::exists(model_path)) {
        std::cerr << "Error: Whisper model not found: " << model_path << std::endl;
        std::cerr << "Please download it with: ./whisper.cpp/models/download-ggml-model.sh " 
                  << config.model << std::endl;
        return false;
    }
    
    if (debug_enabled) {
        std::cout << "Info: Loading Whisper model from " << model_path << std::endl;
    }
    
    // Initialize whisper context with the model
    ctx = whisper_init_from_file_with_params(model_path.c_str(), whisper_context_default_params());
    
    if (!ctx) {
        std::cerr << "Error: Failed to initialize whisper context" << std::endl;
        return false;
    }
    
    is_initialized = true;
    return true;
}

// Free whisper context
void StreamingWhisperSTT::cleanup() {
    if (ctx) {
        whisper_free(ctx);
        ctx = nullptr;
    }
    is_initialized = false;
}

// Simple linear resampling function
std::vector<float> resample_audio(const std::vector<float>& input, int input_rate, int output_rate) {
    if (input_rate == output_rate) {
        return input;
    }
    
    double ratio = static_cast<double>(output_rate) / input_rate;
    size_t output_size = static_cast<size_t>(input.size() * ratio);
    std::vector<float> output(output_size);
    
    for (size_t i = 0; i < output_size; i++) {
        double input_idx = i / ratio;
        size_t idx = static_cast<size_t>(input_idx);
        double frac = input_idx - idx;
        
        if (idx + 1 < input.size()) {
            // Linear interpolation
            output[i] = input[idx] * (1.0 - frac) + input[idx + 1] * frac;
        } else if (idx < input.size()) {
            output[i] = input[idx];
        } else {
            output[i] = 0.0f;
        }
    }
    
    return output;
}

// Process audio buffer
std::string StreamingWhisperSTT::process_audio(const std::vector<float>& audio_buffer, int sample_rate) {
    if (!is_initialized) {
        if (!initialize()) {
            std::cerr << "Error: Whisper context not initialized" << std::endl;
            return "";
        }
    }
    
    if (audio_buffer.empty()) {
        std::cerr << "Error: Empty audio buffer" << std::endl;
        return "";
    }
    
    // Prevent concurrent processing
    if (is_processing.exchange(true)) {
        std::cerr << "Error: Whisper is already processing audio" << std::endl;
        return "";
    }
    
    // Check and convert sample rate if needed
    // Whisper expects 16kHz mono audio
    std::vector<float> processed_audio;
    if (sample_rate != 16000) {
        if (debug_enabled) {
            std::cout << "Warning: Sample rate " << sample_rate << " Hz doesn't match Whisper's expected 16kHz" << std::endl;
            std::cout << "Info: Performing simple resampling" << std::endl;
        }
        
        // Resample to 16kHz (what Whisper expects)
        processed_audio = resample_audio(audio_buffer, sample_rate, 16000);
    } else {
        // No resampling needed
        processed_audio = audio_buffer;
    }
    
    // Add significant silence padding (3 seconds) at the end to help Whisper detect the end of sentences
    const int padding_samples = 16000 * 3; // 3 seconds of silence at 16kHz
    std::vector<float> padded_audio(processed_audio.size() + padding_samples);
    
    // Copy the original audio
    std::copy(processed_audio.begin(), processed_audio.end(), padded_audio.begin());
    
    // Set the padding to zero (silence)
    std::fill(padded_audio.begin() + processed_audio.size(), padded_audio.end(), 0.0f);
    
    // Boost the audio signal to improve detection
    // Find the max amplitude to normalize
    float max_amplitude = 0.0f;
    for (float sample : processed_audio) {
        float abs_sample = std::abs(sample);
        if (abs_sample > max_amplitude) max_amplitude = abs_sample;
    }
    
    // If audio is very quiet, apply gain
    if (max_amplitude > 0.0f && max_amplitude < 0.1f) {
        float gain = 0.8f / max_amplitude; // Boost to 80% of maximum
        if (debug_enabled) {
            std::cout << "Debug: Audio is quiet (max amplitude: " << max_amplitude 
                      << "), applying gain of " << gain << std::endl;
        }
        
        // Apply the gain
        for (size_t i = 0; i < processed_audio.size(); i++) {
            padded_audio[i] = processed_audio[i] * gain;
        }
    }
    
    // Use the padded audio for processing
    processed_audio = padded_audio;
    
    if (debug_enabled) {
        std::cout << "Info: Processing " << processed_audio.size() << " audio samples with Whisper" << std::endl;
        
        // Print some stats about the audio
        float max_amplitude = 0.0f;
        float avg_amplitude = 0.0f;
        for (float sample : processed_audio) {
            float abs_sample = std::abs(sample);
            if (abs_sample > max_amplitude) max_amplitude = abs_sample;
            avg_amplitude += abs_sample;
        }
        
        if (!processed_audio.empty()) {
            avg_amplitude /= processed_audio.size();
        }
        
        std::cout << "Debug: Audio stats - Max amplitude: " << max_amplitude 
                  << ", Avg amplitude: " << avg_amplitude << std::endl;
    }
    
    // Set up whisper parameters - switch to greedy sampling for more reliable basic transcription
    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.print_realtime = false;
    wparams.print_progress = debug_enabled;
    wparams.print_timestamps = false;
    wparams.translate = false;
    wparams.language = "en"; // Language code for English
    wparams.n_threads = 4;   // Use 4 threads for processing
    
    // For our use case, trying to get complete sentences:
    wparams.no_context = false;               // Use context for better continuity
    wparams.single_segment = false;           // Allow multiple segments for longer sentences
    wparams.max_len = 0;                      // No length limit on transcription
    wparams.temperature = 0.0f;               // Zero temperature for deterministic output
    wparams.prompt_tokens = nullptr;          // No prompt tokens
    wparams.prompt_n_tokens = 0;              // No prompt tokens count
    
    // These parameters work better for sentence detection
    wparams.token_timestamps = false;         // Don't need token timestamps
    wparams.thold_pt = 0.01f;                 // Lower threshold to catch more words
    wparams.n_max_text_ctx = 16384;           // Large context size
    wparams.duration_ms = 0;                  // Process the entire thing at once
    
    // Parse any additional parameters from config
    if (!config.params.empty()) {
        std::istringstream params_stream(config.params);
        std::string param;
        while (params_stream >> param) {
            if (param == "--translate") {
                wparams.translate = true;
            } else if (param == "-l" || param == "--language") {
                std::string lang;
                params_stream >> lang;
                wparams.language = lang.c_str();
            } else if (param == "-t" || param == "--threads") {
                int threads;
                params_stream >> threads;
                wparams.n_threads = threads;
            }
        }
    }
    
    // Run whisper processing
    if (whisper_full(ctx, wparams, processed_audio.data(), processed_audio.size()) != 0) {
        std::cerr << "Error: Failed to process audio with whisper" << std::endl;
        is_processing.store(false);
        return "";
    }
    
    // Check if we've been interrupted
    if (running_flag && !(*running_flag)) {
        std::cout << "Info: Processing interrupted by signal" << std::endl;
        is_processing.store(false);
        return "";
    }
    
    // Get the number of segments
    const int n_segments = whisper_full_n_segments(ctx);
    if (n_segments <= 0) {
        if (debug_enabled) {
            std::cout << "Info: No speech detected in audio" << std::endl;
        }
        is_processing.store(false);
        return "";
    }
    
    // Extract the transcription text from all segments
    std::string result;
    
    // Log how many segments we found
    if (debug_enabled) {
        std::cout << "Debug: Whisper found " << n_segments << " segment(s)" << std::endl;
    }
    
    // Join all segments into one complete transcript
    for (int i = 0; i < n_segments; i++) {
        const char* text = whisper_full_get_segment_text(ctx, i);
        
        // Log each segment separately for debugging
        if (debug_enabled) {
            std::cout << "Debug: Segment " << i << ": \"" << text << "\"" << std::endl;
        }
        
        // Check if this segment has non-whitespace content 
        std::string segment_text = text;
        bool has_content = false;
        for (char c : segment_text) {
            if (!std::isspace(c)) {
                has_content = true;
                break;
            }
        }
        
        // Only add non-empty segments
        if (has_content) {
            result += text;
            
            // Add space between segments if needed
            if (i < n_segments - 1 && !segment_text.empty() && 
                segment_text.back() != ' ' && segment_text.back() != '\n') {
                result += " ";
            }
        }
    }
    
    if (debug_enabled) {
        std::cout << "Info: Whisper transcription: \"" << result << "\"" << std::endl;
    }
    
    is_processing.store(false);
    return result;
}

// Get the last transcript
std::string StreamingWhisperSTT::get_last_transcript() const {
    if (!ctx || !is_initialized) {
        return "";
    }
    
    const int n_segments = whisper_full_n_segments(ctx);
    if (n_segments <= 0) {
        return "";
    }
    
    std::string result;
    for (int i = 0; i < n_segments; i++) {
        const char* text = whisper_full_get_segment_text(ctx, i);
        result += text;
        if (i < n_segments - 1) {
            result += " ";
        }
    }
    
    return result;
}