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
    
    if (debug_enabled) {
        std::cout << "Info: Processing " << audio_buffer.size() << " audio samples with Whisper" << std::endl;
    }
    
    // Set up whisper parameters
    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH);
    wparams.print_realtime = false;
    wparams.print_progress = debug_enabled;
    wparams.print_timestamps = false;
    wparams.translate = false;
    wparams.language = "en"; // Language code for English
    wparams.n_threads = 4;   // Use 4 threads for processing
    
    // Enhance parameters for better sentence detection
    wparams.beam_search.beam_size = 5;        // Increase beam size for better search
    wparams.no_context = false;               // Use context for better continuity
    wparams.single_segment = true;            // Treat as a single segment
    wparams.max_len = 0;                      // No length limit on transcription
    wparams.temperature = 0.0f;               // Use greedy decoding for more accuracy
    
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
    if (whisper_full(ctx, wparams, audio_buffer.data(), audio_buffer.size()) != 0) {
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