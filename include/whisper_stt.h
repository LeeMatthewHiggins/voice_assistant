#ifndef WHISPER_STT_H
#define WHISPER_STT_H

#include <string>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include "config.h"

namespace fs = std::filesystem;

class WhisperSTT {
private:
    WhisperConfig config;
    
public:
    WhisperSTT(const WhisperConfig& cfg) : config(cfg) {}
    
    // Get the executable path
    std::string get_executable() const {
        return config.executable;
    }
    
    // Transcribe audio using whisper.cpp
    std::string transcribe(const std::string& audio_file) {
        // Check if whisper executable exists
        if (!fs::exists(config.executable)) {
            std::cerr << "Whisper executable not found at " << config.executable << std::endl;
            std::cerr << "Please install whisper.cpp and update the config." << std::endl;
            return "";
        }
        
        // Check if the audio file exists
        if (!fs::exists(audio_file)) {
            std::cerr << "Audio file not found: " << audio_file << std::endl;
            return "";
        }
        
        // Check the file size
        std::ifstream file_check(audio_file, std::ios::binary | std::ios::ate);
        if (!file_check.is_open()) {
            std::cerr << "Error: Could not open audio file for size check" << std::endl;
            return "";
        }
        
        std::streamsize size = file_check.tellg();
        file_check.close();
        
        std::cout << "Audio file size: " << size << " bytes" << std::endl;
        if (size < 100) {
            std::cerr << "Error: Audio file is too small to contain speech" << std::endl;
            return "";
        }
        
        // Check if the whisper model exists
        std::string model_path = "./whisper.cpp/models/ggml-" + config.model + ".bin";
        if (!fs::exists(model_path)) {
            std::cerr << "Whisper model not found: " << model_path << std::endl;
            std::cerr << "Please download the model with: ./whisper.cpp/models/download-ggml-model.sh " 
                      << config.model << std::endl;
            return "";
        }
        
        // Build command with increased verbosity for debugging
        std::stringstream cmd;
        cmd << config.executable
            << " -f " << audio_file
            << " -m " << model_path
            << " -of txt" // Explicitly set the output format to text
            << " -pp" // Enable print progress
            << " -ps" // Print special tokens
            << " -pc"; // Print colors
            
        // Add any additional parameters
        if (!config.params.empty()) {
            cmd << " " << config.params;
        }
        
        std::cout << "Transcribing with command: " << cmd.str() << std::endl;
        
        // Execute command and capture output
        std::string whisper_output;
        FILE* pipe = popen(cmd.str().c_str(), "r");
        if (!pipe) {
            std::cerr << "Error: Could not open pipe to whisper.cpp" << std::endl;
            return "";
        }
        
        char temp_buffer[1024];
        while (fgets(temp_buffer, sizeof(temp_buffer), pipe) != NULL) {
            whisper_output += temp_buffer;
            std::cout << temp_buffer; // Print real-time output
        }
        
        int result = pclose(pipe);
        
        if (result != 0) {
            std::cerr << "Error running whisper.cpp (exit code: " << result << ")" << std::endl;
            std::cerr << "Whisper output: " << whisper_output << std::endl;
            return "";
        }
        
        std::cout << "Parsing transcription from output..." << std::endl;
        
        // Parse the transcription directly from the console output
        std::string transcript;
        bool in_transcription = false;
        std::stringstream output_stream(whisper_output);
        std::string line;
        
        // This pattern extracts the raw transcribed text which typically appears
        // right before the <|endoftext|> tag in the output
        std::string raw_text;
        size_t pos = whisper_output.find("<|endoftext|>");
        if (pos != std::string::npos) {
            // Work backwards from <|endoftext|> to find the start of the transcription
            size_t start_pos = whisper_output.rfind("\n", pos);
            if (start_pos == std::string::npos) {
                start_pos = 0;
            } else {
                start_pos += 1; // Skip the newline
            }
            
            // Extract the transcription
            raw_text = whisper_output.substr(start_pos, pos - start_pos);
            // Trim any leading/trailing whitespace
            while (!raw_text.empty() && std::isspace(raw_text.front())) raw_text.erase(0, 1);
            while (!raw_text.empty() && std::isspace(raw_text.back())) raw_text.pop_back();
            
            std::cout << "Extracted transcription: \"" << raw_text << "\"" << std::endl;
            return raw_text;
        }
        
        // Fall back to line-by-line processing if we couldn't find the <|endoftext|> pattern
        while (std::getline(output_stream, line)) {
            // Skip lines that are likely to be timing information or other debug output
            if (line.find("whisper_print_timings") != std::string::npos ||
                line.find("output_") != std::string::npos) {
                continue;
            }
            
            // Skip empty lines
            if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos) {
                continue;
            }
            
            // Check if this line is part of the actual transcription
            // This is a basic heuristic - you may need to adjust based on actual output
            if (!line.empty() && line[0] != '[' && line[0] != '*' && 
                line.find("whisper") == std::string::npos && 
                line.find("error") == std::string::npos) {
                transcript += line + " ";
            }
        }
        
        // Trim any leading/trailing whitespace
        while (!transcript.empty() && std::isspace(transcript.front())) transcript.erase(0, 1);
        while (!transcript.empty() && std::isspace(transcript.back())) transcript.pop_back();
        
        std::cout << "Extracted transcription from output: \"" << transcript << "\"" << std::endl;
        
        if (transcript.empty()) {
            std::cerr << "Could not find transcription in whisper output" << std::endl;
            return "";
        }
        
        return transcript;
    }
};

#endif // WHISPER_STT_H