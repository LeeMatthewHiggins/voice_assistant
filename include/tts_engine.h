#ifndef TTS_ENGINE_H
#define TTS_ENGINE_H

#include <string>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include "config.h"

class TTSEngine {
private:
    TTSConfig config;
    std::string output_device;
    
    // List available audio output devices
    void list_devices() {
        std::cout << "Available audio output devices:" << std::endl;
        
        // Try ALSA devices
        std::system("aplay -l 2>/dev/null");
        
        // Try PulseAudio devices
        std::system("pactl list sinks 2>/dev/null | grep -E 'Name:|Description:'");
    }
    
public:
    TTSEngine(const TTSConfig& cfg) : config(cfg) {
        // Initialize output device
        output_device = config.output_device;
        
        // If the user requested "list" as the device, show available devices
        if (output_device == "list") {
            list_devices();
            output_device = "default"; // Reset to default after listing
        }
    }
    
    // Convert text to speech and play
    void speak(const std::string& text) {
        if (text.empty()) {
            return; // Nothing to speak
        }
        
        if (config.engine == "espeak") {
            speak_espeak(text);
        } else if (config.engine == "piper") {
            speak_piper(text);
        } else {
            std::cerr << "Unsupported TTS engine: " << config.engine << std::endl;
            std::cerr << "Falling back to espeak" << std::endl;
            speak_espeak(text);
        }
    }
    
private:
    // Use espeak for TTS
    void speak_espeak(const std::string& text) {
        // Create temporary file for text
        std::stringstream ss;
        ss << "/tmp/tts_text_" << std::time(nullptr) << ".txt";
        std::string text_file = ss.str();
        
        // Audio output file
        std::string audio_file = "/tmp/tts_output_" + std::to_string(std::time(nullptr)) + ".wav";
        
        // Write text to file
        std::ofstream file(text_file);
        if (!file.is_open()) {
            std::cerr << "Failed to create temporary text file" << std::endl;
            return;
        }
        
        file << text;
        file.close();
        
        std::cout << "Using voice: " << config.voice << std::endl;
        
        // Build command to generate audio file
        std::stringstream cmd;
        cmd << "espeak"
            << " -v " << config.voice
            << " -s " << config.speed
            << " -f " << text_file;
        
        // If we should generate a file instead of direct playback
        if (output_device != "default") {
            cmd << " -w " << audio_file;
            
            // Execute command to generate audio file
            int result = std::system(cmd.str().c_str());
            
            if (result != 0) {
                std::cerr << "Error running espeak" << std::endl;
                std::remove(text_file.c_str());
                return;
            }
            
            // Play the generated audio file on the specified device
            play_audio(audio_file);
            
            // Clean up audio file
            std::remove(audio_file.c_str());
        } else {
            // Direct playback using system default
            int result = std::system(cmd.str().c_str());
            
            if (result != 0) {
                std::cerr << "Error running espeak" << std::endl;
            }
        }
        
        // Clean up text file
        std::remove(text_file.c_str());
    }
    
    // Use piper TTS (if available)
    void speak_piper(const std::string& text) {
        // Create temporary file for text
        std::stringstream ss;
        ss << "/tmp/tts_text_" << std::time(nullptr) << ".txt";
        std::string text_file = ss.str();
        
        // Audio output file
        std::string audio_file = "/tmp/tts_output_" + std::to_string(std::time(nullptr)) + ".wav";
        
        // Write text to file
        std::ofstream file(text_file);
        if (!file.is_open()) {
            std::cerr << "Failed to create temporary text file" << std::endl;
            return;
        }
        
        file << text;
        file.close();
        
        // Build command
        std::stringstream cmd;
        cmd << "piper"
            << " --model piper-voices/" << config.voice << "/model.onnx"
            << " --output_file " << audio_file
            << " --text_file " << text_file;
            
        // Execute command
        int result = std::system(cmd.str().c_str());
        
        if (result != 0) {
            std::cerr << "Error running piper, falling back to espeak" << std::endl;
            speak_espeak(text);
            std::remove(text_file.c_str());
            return;
        }
        
        // Play the generated audio
        play_audio(audio_file);
        
        // Clean up
        std::remove(text_file.c_str());
        std::remove(audio_file.c_str());
    }
    
    // Play audio file on specified device
    void play_audio(const std::string& audio_file) {
        std::stringstream cmd;
        bool using_pulse = false;
        
        // Check if we should use PulseAudio for playback
        if (output_device != "default" && output_device.find("hw:") == std::string::npos) {
            // Attempt to use PulseAudio
            cmd << "paplay"
                << " --device=" << output_device
                << " " << audio_file;
            using_pulse = true;
        } else {
            // Try ALSA
            cmd << "aplay";
            
            if (output_device != "default") {
                cmd << " -D " << output_device;
            }
            
            cmd << " " << audio_file;
        }
        
        // Execute command
        int result = std::system(cmd.str().c_str());
        
        if (result != 0) {
            // If first attempt fails, try alternative approach
            cmd.str("");
            
            if (using_pulse) {
                // Try ALSA as fallback
                cmd << "aplay -D default " << audio_file;
            } else {
                // Try PulseAudio as fallback
                cmd << "paplay --device=@DEFAULT_SINK@ " << audio_file;
            }
            
            result = std::system(cmd.str().c_str());
            
            if (result != 0) {
                // Last resort: try SoX
                cmd.str("");
                cmd << "play -q " << audio_file;
                
                result = std::system(cmd.str().c_str());
                
                if (result != 0) {
                    std::cerr << "No suitable audio player found" << std::endl;
                }
            }
        }
    }
};

#endif // TTS_ENGINE_H