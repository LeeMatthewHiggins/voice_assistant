#ifndef AUDIO_INPUT_H
#define AUDIO_INPUT_H

#include <string>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <memory>
#include <thread>
#include <atomic>
#include <csignal>
#include "config.h"

// Reference to the global running flag from main.cpp
extern volatile sig_atomic_t g_running;

class AudioInput {
private:
    AudioConfig config;
    bool continuous_mode;
    bool debug_enabled = false;
    
    // List available audio devices
    void list_devices() {
        std::cout << "Available audio input devices:" << std::endl;
        
        // Try ALSA devices
        std::system("arecord -l 2>/dev/null");
        
        // Try PulseAudio devices
        std::system("pactl list sources 2>/dev/null | grep -E 'Name:|Description:' | grep -v monitor");
    }
    
public:
    AudioInput(const AudioConfig& cfg, bool continuous = false, bool debug = false) 
        : config(cfg), continuous_mode(continuous), debug_enabled(debug) {
        // If the user requested "list" as the device, show available devices
        if (config.device == "list") {
            list_devices();
            config.device = "default"; // Reset to default after listing
        }
    }
    
    // Set continuous mode
    void set_continuous_mode(bool enabled) {
        continuous_mode = enabled;
    }
    
    // Check if continuous mode is enabled
    bool is_continuous_mode() const {
        return continuous_mode;
    }
    
    // Record audio from microphone, returns empty string if interrupted by Ctrl+C
    std::string record() {
        // Create a temporary file for the recording
        std::stringstream ss;
        ss << "/tmp/recording_" << std::time(nullptr) << ".wav";
        std::string output_file = ss.str();
        
        // Check if we've been interrupted
        if (!g_running) {
            std::cout << "Recording canceled by user." << std::endl;
            return "";
        }
        
        // Build command
        std::stringstream cmd;
        bool using_pulse = false;
        
        if (debug_enabled) {
            std::cout << "Info: Recording with device: " << config.device << std::endl;
            std::cout << "Info: Sample rate: " << config.sample_rate << " Hz" << std::endl;
            std::cout << "Info: Duration: " << config.duration << " seconds" << std::endl;
        }
        
        // Use ALSA for recording by default (most reliable)
        if (debug_enabled) {
            std::cout << "Info: Using ALSA for recording..." << std::endl;
        }
        cmd.str("");
        cmd << "arecord"
            << " -D " << config.device
            << " -f S16_LE"
            << " -c 1"
            << " -r " << config.sample_rate
            << " -d " << config.duration
            << (debug_enabled ? " -v" : " -q") // Verbose only in debug mode
            << " " << output_file;
        
        // Print the command for debugging
        if (debug_enabled) {
            std::cout << "Info: Executing: " << cmd.str() << std::endl;
        }
        
        // Execute command
        int result = std::system(cmd.str().c_str());
        
        // Check if interrupted by Ctrl+C
        if (!g_running) {
            if (debug_enabled) {
                std::cout << "Info: Recording was interrupted by Ctrl+C." << std::endl;
            }
            // Clean up any partial recording
            std::remove(output_file.c_str());
            return "";
        }
        
        if (result != 0) {
            std::cerr << "Error: Recording failed with exit code: " << result << std::endl;
            
            // If first attempt fails, try alternative approach
            cmd.str("");
            
            if (using_pulse) {
                // Try ALSA as fallback
                std::cout << "Trying ALSA as fallback..." << std::endl;
                cmd << "arecord"
                    << " -D default" // Use default ALSA device as fallback
                    << " -f S16_LE"
                    << " -c 1"
                    << " -r " << config.sample_rate
                    << " -d " << config.duration
                    << " -v" // Verbose mode for debugging
                    << " " << output_file;
            } else {
                // Try PulseAudio as fallback
                std::cout << "Trying PulseAudio as fallback..." << std::endl;
                cmd << "parecord"
                    << " --device=@DEFAULT_SOURCE@" // Use system default source
                    << " --record"
                    << " --file-format=wav"
                    << " -r " << config.sample_rate
                    << " --channels=1"
                    << " " << output_file
                    << " --max-file-time=" << config.duration;
            }
            
            // Print the command for debugging
            std::cout << "Executing: " << cmd.str() << std::endl;
            
            result = std::system(cmd.str().c_str());
            
            if (result != 0) {
                std::cerr << "Second recording attempt failed with exit code: " << result << std::endl;
                
                // Last resort: try SoX
                std::cout << "Trying SoX as final fallback..." << std::endl;
                cmd.str("");
                cmd << "rec"
                    << " -V" // Verbose mode for debugging
                    << " " << output_file
                    << " rate " << config.sample_rate
                    << " channels 1"
                    << " trim 0 " << config.duration;
                    
                // Print the command for debugging
                std::cout << "Executing: " << cmd.str() << std::endl;
                
                result = std::system(cmd.str().c_str());
                
                if (result != 0) {
                    std::cerr << "Failed to record audio with any available method." << std::endl;
                    return "";  // Failed to record
                }
            }
        }
        
        // Check if the file was created and has content
        std::ifstream file(output_file, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open recorded audio file" << std::endl;
            return "";
        }
        
        std::streamsize size = file.tellg();
        if (size < 100) { // Arbitrary small size to check for empty files
            std::cerr << "Error: Recorded file is too small (" << size << " bytes)" << std::endl;
            std::remove(output_file.c_str());
            return "";
        }
        
        std::cout << "Successfully recorded audio to: " << output_file << " (" << size << " bytes)" << std::endl;
        return output_file;
    }
};

#endif // AUDIO_INPUT_H