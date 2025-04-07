#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <ctime>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <nlohmann/json.hpp>

#include "audio_input.h"
#include "whisper_stt.h"
#include "ollama_client.h"
#include "tts_engine.h"
#include "config.h"

// Global flag for handling Ctrl+C - this is referenced in other files via extern
volatile sig_atomic_t g_running = 1;

// Signal handler for Ctrl+C
void signal_handler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nShutting down immediately..." << std::endl;
        g_running = 0;
        
        // Kill any active recording processes immediately
        std::system("pkill -f arecord 2>/dev/null; pkill -f parecord 2>/dev/null; pkill -f play 2>/dev/null; pkill -f espeak 2>/dev/null");
    }
}

// Forward declarations
bool run_assistant_cycle(AudioInput* audio, WhisperSTT* whisper, OllamaClient* ollama, TTSEngine* tts, bool debug);
void run_diagnostics(AudioConfig& audio_config, WhisperConfig& whisper_config);
void gather_system_info(SystemInfo& info);

// Interactive setup function
void run_interactive_setup(Config& config) {
    std::cout << "\n===== Voice Assistant Setup =====\n" << std::endl;
    
    // 1. Choose model
    std::cout << "Available models:" << std::endl;
    auto models = Config::available_models.get_names();
    for (size_t i = 0; i < models.size(); i++) {
        std::cout << "  " << (i+1) << ". " << models[i] << std::endl;
    }
    
    int model_choice = 0;
    while (model_choice < 1 || model_choice > static_cast<int>(models.size())) {
        std::cout << "Choose a model (1-" << models.size() << ") [default: 1]: ";
        std::string input;
        std::getline(std::cin, input);
        
        if (input.empty()) {
            model_choice = 1; // Default
            break;
        }
        
        try {
            model_choice = std::stoi(input);
        } catch (const std::exception& e) {
            std::cout << "Invalid input. Please enter a number." << std::endl;
        }
    }
    
    config.ollama.model = models[model_choice - 1];
    std::cout << "Selected model: " << config.ollama.model << std::endl;
    
    // 2. Choose personality
    std::cout << "\nAvailable personalities:" << std::endl;
    auto personalities = Config::available_personalities.get_names();
    auto descriptions = Config::available_personalities.get_descriptions();
    for (size_t i = 0; i < personalities.size(); i++) {
        std::cout << "  " << (i+1) << ". " << descriptions[i] << std::endl;
    }
    
    int personality_choice = 0;
    while (personality_choice < 1 || personality_choice > static_cast<int>(personalities.size())) {
        std::cout << "Choose a personality (1-" << personalities.size() << ") [default: 1]: ";
        std::string input;
        std::getline(std::cin, input);
        
        if (input.empty()) {
            personality_choice = 1; // Default
            break;
        }
        
        try {
            personality_choice = std::stoi(input);
        } catch (const std::exception& e) {
            std::cout << "Invalid input. Please enter a number." << std::endl;
        }
    }
    
    config.set_personality(personalities[personality_choice - 1]);
    std::cout << "Selected personality: " << descriptions[personality_choice - 1] << std::endl;
    
    // 3. Choose voice
    std::cout << "\nAvailable voices:" << std::endl;
    auto voice_codes = Config::available_voices.get_codes();
    auto voice_descriptions = Config::available_voices.get_descriptions();
    for (size_t i = 0; i < voice_codes.size(); i++) {
        std::cout << "  " << (i+1) << ". " << voice_descriptions[i] << std::endl;
    }
    
    int voice_choice = 0;
    while (voice_choice < 1 || voice_choice > static_cast<int>(voice_codes.size())) {
        std::cout << "Choose a voice (1-" << voice_codes.size() << ") [default: 1]: ";
        std::string input;
        std::getline(std::cin, input);
        
        if (input.empty()) {
            voice_choice = 1; // Default
            break;
        }
        
        try {
            voice_choice = std::stoi(input);
        } catch (const std::exception& e) {
            std::cout << "Invalid input. Please enter a number." << std::endl;
        }
    }
    
    std::string voice_code = voice_codes[voice_choice - 1];
    config.tts.voice = config.map_voice_to_espeak(voice_code);
    std::cout << "Selected voice: " << voice_descriptions[voice_choice - 1] << std::endl;
    
    // Save configuration
    try {
        config.save("config.json");
        std::cout << "\nConfiguration saved to config.json" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error saving configuration: " << e.what() << std::endl;
    }
    
    std::cout << "\nSetup complete! Press Enter to continue...";
    std::cin.get();
}

// Main function
int main(int argc, char** argv) {
    // Register signal handler with more aggressive handling
    std::signal(SIGINT, signal_handler);
    
    // Enable immediate Ctrl+C handling (disable buffering of Ctrl+C)
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = SA_RESTART; // Don't restart system calls
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        std::cerr << "Failed to set up signal handler." << std::endl;
        return 1;
    }
    
    std::cout << "Voice Assistant Starting..." << std::endl;
    
    // Load configuration
    std::string config_path = "config.json";
    bool continuous_mode = false;
    
    // Command line options
    std::string input_device = "";
    std::string output_device = "";
    bool list_devices = false;
    bool debug_mode = false;
    bool setup_mode = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--continuous") {
            continuous_mode = true;
        } else if (arg == "--input-device" && i + 1 < argc) {
            input_device = argv[++i];
        } else if (arg == "--output-device" && i + 1 < argc) {
            output_device = argv[++i];
        } else if (arg == "--list-devices") {
            list_devices = true;
        } else if (arg == "--debug") {
            debug_mode = true;
        } else if (arg == "--setup") {
            setup_mode = true;
        } else if (arg == "--help") {
            std::cout << "Usage: voice_assistant [options]\n"
                      << "Options:\n"
                      << "  --config FILE         Path to configuration file\n"
                      << "  --continuous          Run in continuous mode\n"
                      << "  --input-device DEV    Specify audio input device\n"
                      << "  --output-device DEV   Specify audio output device\n"
                      << "  --list-devices        List available audio devices\n"
                      << "  --debug               Run in debug mode with extra diagnostics\n"
                      << "  --setup               Run interactive setup to configure the assistant\n"
                      << "  --help                Show this help message\n\n"
                      << "Voice commands:\n"
                      << "  \"over\"               Signal the end of your turn in a conversation\n"
                      << "  \"exit\"               Exit the application\n"
                      << "  \"quit\"               Exit the application\n"
                      << "  \"goodbye\"            Exit the application\n"
                      << "  \"bye bye\"            Exit the application\n"
                      << "  \"end conversation\"    Exit the application\n";
            return 0;
        }
    }
    
    // Load configuration
    Config config;
    try {
        config.load(config_path);
    } catch (const std::exception& e) {
        std::cerr << "Error loading configuration: " << e.what() << std::endl;
        std::cerr << "Creating default configuration..." << std::endl;
        
        config.create_default();
        config.save(config_path);
        
        std::cout << "Default configuration created at " << config_path << std::endl;
        
        // Auto-run setup if config was just created
        setup_mode = true;
    }
    
    // Run interactive setup if requested
    if (setup_mode) {
        run_interactive_setup(config);
    }
    
    // Override config with command line options
    if (!input_device.empty()) {
        config.audio.device = input_device;
    }
    
    if (!output_device.empty()) {
        config.tts.output_device = output_device;
    }
    
    // If list_devices is true, set devices to "list" to trigger listing
    if (list_devices) {
        config.audio.device = "list";
        config.tts.output_device = "list";
    }
    
    // Gather system information
    gather_system_info(config.system_info);
    
    // Update current time for each run
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::stringstream time_ss;
    time_ss << std::ctime(&now_time);
    std::string current_time = time_ss.str();
    if (!current_time.empty() && current_time.back() == '\n') {
        current_time.pop_back();  // Remove trailing newline
    }
    config.system_info.current_time = current_time;
    
    // Format system info with current configuration details
    std::stringstream detailed_info;
    detailed_info << config.system_info.get_formatted_info() << "\n"
                  << "- Current configuration:\n"
                  << "  * Speech-to-text model: " << config.whisper.model << " (Whisper)\n"
                  << "  * Language model: " << config.ollama.model << " (Ollama)\n"
                  << "  * Voice: " << config.tts.voice << " (ESpeak)\n";
                  
    std::string system_info_str = detailed_info.str();
    std::cout << "\nSystem Information:\n" << system_info_str << std::endl;
    
    // Initialize components
    std::unique_ptr<AudioInput> audio = std::make_unique<AudioInput>(config.audio, continuous_mode, debug_mode);
    std::unique_ptr<WhisperSTT> whisper = std::make_unique<WhisperSTT>(config.whisper);
    std::unique_ptr<OllamaClient> ollama = std::make_unique<OllamaClient>(config.ollama, system_info_str);
    std::unique_ptr<TTSEngine> tts = std::make_unique<TTSEngine>(config.tts);
    
    if (debug_mode) {
        std::cout << "Info: Vibe Voice Assistant Configuration:" << std::endl;
        std::cout << "Info: - Speech recognition: Whisper (" << config.whisper.model << " model)" << std::endl;
        std::cout << "Info: - Language processing: Ollama (" << config.ollama.model << " model)" << std::endl;
        std::cout << "Info: - Speech synthesis: " << config.tts.engine << " (voice: " << config.tts.voice << ")" << std::endl;
        std::cout << "Info: - Audio input device: " << config.audio.device << std::endl;
        std::cout << "Info: - Audio output device: " << config.tts.output_device << std::endl;
        std::cout << "Info: - Current time: " << config.system_info.current_time << std::endl;
        
        // Display hardware info
        if (!config.system_info.cpu_info.empty()) 
            std::cout << "Info: - CPU: " << config.system_info.cpu_info << std::endl;
        if (!config.system_info.gpu_info.empty()) 
            std::cout << "Info: - GPU: " << config.system_info.gpu_info << std::endl;
        if (!config.system_info.memory_info.empty()) 
            std::cout << "Info: - Memory: " << config.system_info.memory_info << std::endl;
        if (!config.system_info.disk_info.empty()) 
            std::cout << "Info: - Disk: " << config.system_info.disk_info << std::endl;
    }
    
    // If we just wanted to list devices, exit now
    if (list_devices) {
        return 0;
    }
    
    // Run diagnostics in debug mode
    if (debug_mode) {
        run_diagnostics(config.audio, config.whisper);
    }
    
    // Increase recording duration in debug mode to give more time to speak
    if (debug_mode && config.audio.duration < 8) {
        std::cout << "Debug: Increasing recording duration from " << config.audio.duration 
                  << " to 8 seconds for better results" << std::endl;
        config.audio.duration = 8;
    }
    
    // Main loop
    if (continuous_mode) {
        std::cout << "Info: Running in continuous mode. Press Ctrl+C to exit or say 'exit', 'quit', 'goodbye', or 'end conversation'." << std::endl;
        std::cout << "\n--- Starting Conversation ---\n" << std::endl;
        
        bool should_exit = false;
        while (g_running && !should_exit) {
            should_exit = run_assistant_cycle(audio.get(), whisper.get(), ollama.get(), tts.get(), debug_mode);
        }
    } else {
        std::cout << "Info: Press Ctrl+C to exit or say 'exit', 'quit', 'goodbye', or 'end conversation'." << std::endl;
        std::cout << "\n--- Starting Conversation ---\n" << std::endl;
        run_assistant_cycle(audio.get(), whisper.get(), ollama.get(), tts.get(), debug_mode);
    }
    
    std::cout << "Voice Assistant Exiting" << std::endl;
    return 0;
}

// Gather system information
void gather_system_info(SystemInfo& info) {
    // Get current date and time
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::stringstream date_ss;
    date_ss << std::ctime(&now_time);
    info.build_date = date_ss.str();
    if (!info.build_date.empty() && info.build_date.back() == '\n') {
        info.build_date.pop_back();  // Remove trailing newline
    }
    
    // Get current timestamp for the assistant
    auto current_time = std::chrono::system_clock::now();
    std::time_t current_time_t = std::chrono::system_clock::to_time_t(current_time);
    std::stringstream current_time_ss;
    current_time_ss << std::ctime(&current_time_t);
    info.current_time = current_time_ss.str();
    if (!info.current_time.empty() && info.current_time.back() == '\n') {
        info.current_time.pop_back();  // Remove trailing newline
    }
    
    // Get OS information in simplified format
    FILE* pipe = popen("cat /etc/os-release | grep -E '^NAME=' | cut -d'=' -f2", "r");
    if (pipe) {
        char buffer[256];
        std::stringstream os_ss;
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            os_ss << buffer;
        }
        std::string os_name = os_ss.str();
        if (!os_name.empty() && os_name.back() == '\n') {
            os_name.pop_back();  // Remove trailing newline
        }
        
        // Remove quotes if present
        if (os_name.front() == '"' && os_name.back() == '"') {
            os_name = os_name.substr(1, os_name.length() - 2);
        }
        
        // Get OS version
        pclose(pipe);
        pipe = popen("cat /etc/os-release | grep -E '^VERSION=' | cut -d'=' -f2", "r");
        if (pipe) {
            std::stringstream version_ss;
            while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
                version_ss << buffer;
            }
            std::string version = version_ss.str();
            if (!version.empty() && version.back() == '\n') {
                version.pop_back();
            }
            
            // Remove quotes if present
            if (version.front() == '"' && version.back() == '"') {
                version = version.substr(1, version.length() - 2);
            }
            
            if (!version.empty()) {
                info.os_info = os_name + " " + version;
            } else {
                info.os_info = os_name;
            }
            
            pclose(pipe);
        } else {
            info.os_info = os_name;
        }
        
    } else {
        // Fallback to uname if /etc/os-release doesn't exist
        pipe = popen("uname -s", "r");
        if (pipe) {
            std::stringstream kernel_ss;
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
                kernel_ss << buffer;
            }
            std::string kernel = kernel_ss.str();
            if (!kernel.empty() && kernel.back() == '\n') {
                kernel.pop_back();
            }
            
            info.os_info = kernel + " Operating System";
            pclose(pipe);
        } else {
            info.os_info = "Unknown Operating System";
        }
    }
    
    // Get CPU information in a more human-friendly format
    pipe = popen("cat /proc/cpuinfo | grep 'model name' | head -n 1 | cut -d: -f2", "r");
    if (pipe) {
        char buffer[256];
        std::stringstream cpu_ss;
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            cpu_ss << buffer;
        }
        std::string full_cpu_info = cpu_ss.str();
        if (!full_cpu_info.empty() && full_cpu_info.back() == '\n') {
            full_cpu_info.pop_back();  // Remove trailing newline
        }
        
        // Simplify CPU info - extract brand and basic info
        std::string simplified_cpu;
        if (full_cpu_info.find("Intel") != std::string::npos) {
            simplified_cpu = "Intel";
        } else if (full_cpu_info.find("AMD") != std::string::npos) {
            simplified_cpu = "AMD";
        } else {
            simplified_cpu = "Generic";
        }
        
        // Try to extract processor type
        if (full_cpu_info.find("Core") != std::string::npos) {
            if (full_cpu_info.find("i7") != std::string::npos) 
                simplified_cpu += " Core i7";
            else if (full_cpu_info.find("i5") != std::string::npos) 
                simplified_cpu += " Core i5";
            else if (full_cpu_info.find("i3") != std::string::npos) 
                simplified_cpu += " Core i3";
            else if (full_cpu_info.find("i9") != std::string::npos) 
                simplified_cpu += " Core i9";
            else 
                simplified_cpu += " Core processor";
        } else if (full_cpu_info.find("Ryzen") != std::string::npos) {
            simplified_cpu += " Ryzen processor";
        } else {
            simplified_cpu += " processor";
        }
        
        // Get CPU clock speed
        pipe = popen("cat /proc/cpuinfo | grep 'cpu MHz' | head -n 1 | cut -d: -f2", "r");
        if (pipe) {
            char buffer[50];
            std::stringstream mhz_ss;
            while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
                mhz_ss << buffer;
            }
            std::string mhz = mhz_ss.str();
            if (!mhz.empty() && mhz.back() == '\n') {
                mhz.pop_back();
            }
            
            // Convert to GHz for better readability
            try {
                float mhz_float = std::stof(mhz);
                float ghz = mhz_float / 1000.0f;
                std::stringstream ghz_ss;
                ghz_ss << std::fixed << std::setprecision(1) << ghz;
                simplified_cpu += " running at " + ghz_ss.str() + " GHz";
            } catch (const std::exception&) {
                // If conversion fails, just use the MHz value
                simplified_cpu += " running at " + mhz + " MHz";
            }
            
            pclose(pipe);
        }
        
        // Get CPU core count
        pipe = popen("nproc", "r");
        if (pipe) {
            char buffer[50];
            std::stringstream cores_ss;
            while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
                cores_ss << buffer;
            }
            std::string cores = cores_ss.str();
            if (!cores.empty() && cores.back() == '\n') {
                cores.pop_back();
            }
            simplified_cpu += " with " + cores + " cores";
            pclose(pipe);
        }
        
        info.cpu_info = simplified_cpu;
        
    } else {
        info.cpu_info = "Unknown CPU";
    }
    
    // Try to get GPU information in a simplified format
    pipe = popen("lspci | grep -i 'vga\\|3d\\|2d' | cut -d: -f3", "r");
    if (pipe) {
        char buffer[256];
        std::stringstream gpu_ss;
        bool found_gpu = false;
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            gpu_ss << buffer;
            found_gpu = true;
        }
        if (found_gpu) {
            std::string full_gpu_info = gpu_ss.str();
            if (!full_gpu_info.empty() && full_gpu_info.back() == '\n') {
                full_gpu_info.pop_back();  // Remove trailing newline
            }
            
            // Simplify GPU info
            std::string simplified_gpu;
            if (full_gpu_info.find("NVIDIA") != std::string::npos) {
                simplified_gpu = "NVIDIA";
                
                // Try to get more specific for NVIDIA
                if (full_gpu_info.find("GeForce") != std::string::npos) {
                    simplified_gpu += " GeForce";
                    
                    // Try to get the series (RTX, GTX, etc)
                    if (full_gpu_info.find("RTX") != std::string::npos) {
                        simplified_gpu += " RTX";
                    } else if (full_gpu_info.find("GTX") != std::string::npos) {
                        simplified_gpu += " GTX";
                    }
                } else if (full_gpu_info.find("Quadro") != std::string::npos) {
                    simplified_gpu += " Quadro";
                }
            } else if (full_gpu_info.find("AMD") != std::string::npos || 
                      full_gpu_info.find("ATI") != std::string::npos || 
                      full_gpu_info.find("Radeon") != std::string::npos) {
                if (full_gpu_info.find("Radeon") != std::string::npos) {
                    simplified_gpu = "AMD Radeon";
                } else {
                    simplified_gpu = "AMD";
                }
            } else if (full_gpu_info.find("Intel") != std::string::npos) {
                simplified_gpu = "Intel";
                if (full_gpu_info.find("Iris") != std::string::npos) {
                    simplified_gpu += " Iris";
                } else if (full_gpu_info.find("HD Graphics") != std::string::npos) {
                    simplified_gpu += " HD Graphics";
                } else if (full_gpu_info.find("UHD Graphics") != std::string::npos) {
                    simplified_gpu += " UHD Graphics";
                }
            } else {
                // Just use the first few words to keep it simple
                std::istringstream iss(full_gpu_info);
                std::string word;
                int count = 0;
                while (iss >> word && count < 3) {
                    if (count > 0) simplified_gpu += " ";
                    simplified_gpu += word;
                    count++;
                }
                if (simplified_gpu.empty()) {
                    simplified_gpu = "Graphics card";
                }
            }
            
            info.gpu_info = simplified_gpu;
        }
        pclose(pipe);
    }
    
    // Get memory information in simplified format
    pipe = popen("free -h | grep Mem: | awk '{print $2}'", "r");
    if (pipe) {
        char buffer[256];
        std::stringstream mem_ss;
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            mem_ss << buffer;
        }
        std::string total_mem = mem_ss.str();
        if (!total_mem.empty() && total_mem.back() == '\n') {
            total_mem.pop_back();  // Remove trailing newline
        }
        
        // Clean up the memory value
        total_mem.erase(std::remove_if(total_mem.begin(), total_mem.end(), ::isspace), total_mem.end());
        
        info.memory_info = total_mem + " of RAM";
        pclose(pipe);
    }
    
    // Get disk information in simplified format
    pipe = popen("df -h / | tail -1 | awk '{print $2\" total, \"$4\" free (\"$5\" used)\"}'", "r");
    if (pipe) {
        char buffer[256];
        std::stringstream disk_ss;
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            disk_ss << buffer;
        }
        std::string disk_info = disk_ss.str();
        if (!disk_info.empty() && disk_info.back() == '\n') {
            disk_info.pop_back();  // Remove trailing newline
        }
        
        info.disk_info = disk_info + " disk space";
        pclose(pipe);
    }
    
    // Get basic network information
    // For privacy reasons, we'll just indicate network connectivity without showing the actual IP
    pipe = popen("ping -c 1 -W 1 8.8.8.8 > /dev/null 2>&1 && echo 'Connected to the internet' || echo 'Not connected to the internet'", "r");
    if (pipe) {
        char buffer[256];
        std::stringstream net_ss;
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            net_ss << buffer;
        }
        info.network_info = net_ss.str();
        if (!info.network_info.empty() && info.network_info.back() == '\n') {
            info.network_info.pop_back();  // Remove trailing newline
        }
        pclose(pipe);
    }
    
    // Get whisper.cpp version
    pipe = popen("./whisper.cpp/build/bin/whisper-cli --version 2>&1 | grep -o 'whisper.cpp.*'", "r");
    if (pipe) {
        char buffer[256];
        std::stringstream whisper_ss;
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            whisper_ss << buffer;
        }
        std::string version = whisper_ss.str();
        if (!version.empty()) {
            if (version.back() == '\n') version.pop_back();
            info.whisper_version = version;
        }
        pclose(pipe);
    }
    
    // Get ollama version
    pipe = popen("ollama --version 2>&1", "r");
    if (pipe) {
        char buffer[256];
        std::stringstream ollama_ss;
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            ollama_ss << buffer;
        }
        std::string version = ollama_ss.str();
        if (!version.empty()) {
            if (version.back() == '\n') version.pop_back();
            info.ollama_version = version;
        }
        pclose(pipe);
    }
}

// Run diagnostics to help identify audio and whisper.cpp issues
void run_diagnostics(AudioConfig& audio_config, WhisperConfig& whisper_config) {
    std::cout << "\n========== RUNNING DIAGNOSTICS ==========\n" << std::endl;
    
    // Check system audio setup
    std::cout << "Checking audio setup..." << std::endl;
    std::system("which arecord parecord rec");
    std::system("arecord --version | head -n 1");
    
    // List PulseAudio sources
    std::cout << "\nPulseAudio sources:" << std::endl;
    std::system("pactl list sources short 2>/dev/null || echo 'PulseAudio not installed or not running'");
    
    // Check ALSA devices
    std::cout << "\nALSA recording devices:" << std::endl;
    std::system("arecord -l 2>/dev/null || echo 'ALSA tools not installed or no devices found'");
    
    // Check for microphone permissions
    std::cout << "\nChecking microphone permissions..." << std::endl;
    std::system("ls -l /dev/snd/* 2>/dev/null");
    
    // Test recording with multiple methods
    std::cout << "\nTesting recording with ALSA..." << std::endl;
    std::string test_file = "/tmp/test_recording.wav";
    std::stringstream cmd;
    
    // Try ALSA
    cmd << "arecord -d 3 -f S16_LE -r 16000 -c 1 " << test_file << " && echo 'ALSA recording successful: " 
        << test_file << "' || echo 'ALSA recording failed'";
    std::system(cmd.str().c_str());
    
    // Check file size
    if (fs::exists(test_file)) {
        std::ifstream file(test_file, std::ios::binary | std::ios::ate);
        if (file.is_open()) {
            std::streamsize size = file.tellg();
            std::cout << "  Recorded file size: " << size << " bytes" << std::endl;
            file.close();
        }
        fs::remove(test_file);
    }
    
    // Check whisper.cpp setup
    std::cout << "\nChecking whisper.cpp setup..." << std::endl;
    if (fs::exists(whisper_config.executable)) {
        std::cout << "  Whisper executable found: " << whisper_config.executable << std::endl;
    } else {
        std::cout << "  ERROR: Whisper executable not found at: " << whisper_config.executable << std::endl;
        std::cout << "  Please make sure whisper.cpp is properly installed." << std::endl;
    }
    
    // Check for whisper models
    std::string model_path = "./whisper.cpp/models/ggml-" + whisper_config.model + ".bin";
    if (fs::exists(model_path)) {
        std::cout << "  Whisper model found: " << model_path << std::endl;
        // Get model size
        std::ifstream model_file(model_path, std::ios::binary | std::ios::ate);
        if (model_file.is_open()) {
            std::streamsize size = model_file.tellg();
            std::cout << "  Model size: " << (size / (1024.0 * 1024.0)) << " MB" << std::endl;
            model_file.close();
        }
    } else {
        std::cout << "  ERROR: Whisper model not found: " << model_path << std::endl;
        std::cout << "  Please download it with: ./whisper.cpp/models/download-ggml-model.sh " 
                  << whisper_config.model << std::endl;
    }
    
    // Suggest fixes
    std::cout << "\nPossible solutions to 'No speech detected' issue:" << std::endl;
    std::cout << "  1. Make sure your microphone is properly connected and unmuted" << std::endl;
    std::cout << "  2. Try specifying a different input device with --input-device" << std::endl;
    std::cout << "  3. Increase recording duration with 'duration' in config.json" << std::endl;
    std::cout << "  4. Ensure you have permission to access audio devices" << std::endl;
    std::cout << "  5. Try a different whisper model (tiny.en or small.en)" << std::endl;
    
    std::cout << "\n========== END DIAGNOSTICS ==========\n" << std::endl;
}

// Check if transcript contains the "over" keyword at the end
bool has_over_keyword(const std::string& text) {
    // Convert to lowercase for case-insensitive matching
    std::string lower_text = text;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
    
    // Check for "over" at the end of the text with possible punctuation
    return (lower_text.find(" over.") != std::string::npos) ||
           (lower_text.find(" over!") != std::string::npos) ||
           (lower_text.find(" over?") != std::string::npos) ||
           (lower_text.find(" over") != std::string::npos && 
            lower_text.find(" over") == lower_text.length() - 5);
}

// Check if transcript contains the exit keyword
bool has_exit_keyword(const std::string& text) {
    // Convert to lowercase for case-insensitive matching
    std::string lower_text = text;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
    
    // Check for exit keywords - using simpler phrases that are easier to recognize
    return (lower_text.find("exit") != std::string::npos) ||
           (lower_text.find("quit") != std::string::npos) ||
           (lower_text.find("goodbye") != std::string::npos) ||
           (lower_text.find("bye bye") != std::string::npos) ||
           (lower_text.find("end conversation") != std::string::npos);
}

// Check if transcript contains a silence marker or non-speech sounds
bool is_silence_marker(const std::string& text) {
    // This pattern suggests whisper detected silence or very little speech
    std::string lower_text = text;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
    
    // Trim whitespace from the string
    lower_text.erase(0, lower_text.find_first_not_of(" \t\n\r\f\v"));
    lower_text.erase(lower_text.find_last_not_of(" \t\n\r\f\v") + 1);
    
    // Special patterns that should be detected as silence
    static const std::vector<std::string> silence_markers = {
        "[silence]", 
        "[noise]", 
        "[inaudible]", 
        "[blank_audio]",
        "[applause]",
        "[music]",
        "[laughter]",
        "background noise",
        "silence"
    };
    
    // Check for empty transcript or simple markers
    if (lower_text.empty() || lower_text == "." || lower_text == "...") {
        return true;
    }
    
    // Check for specific silence markers
    for (const auto& marker : silence_markers) {
        if (lower_text.find(marker) != std::string::npos) {
            return true;
        }
    }
    
    // Check for parenthetical descriptions - but only if they make up the whole text
    // This avoids filtering out valid speech that might contain parentheses
    if (lower_text.front() == '(' && lower_text.back() == ')') {
        return true;
    }
    
    // Only filter bracketed content if it makes up the whole text or most of it
    if (lower_text.front() == '[' && lower_text.back() == ']') {
        return true;
    }
    
    // If it has brackets or parentheses but also has substantial text outside them
    // (more than 5 characters), then it's probably valid speech
    if ((lower_text.find('[') != std::string::npos && lower_text.find(']') != std::string::npos) ||
        (lower_text.find('(') != std::string::npos && lower_text.find(')') != std::string::npos)) {
        // Extract text outside of brackets/parentheses
        std::string text_outside = lower_text;
        // Remove text between [] brackets
        while (true) {
            size_t start = text_outside.find('[');
            size_t end = text_outside.find(']');
            if (start == std::string::npos || end == std::string::npos || start > end) {
                break;
            }
            text_outside.erase(start, end - start + 1);
        }
        // Remove text between () parentheses
        while (true) {
            size_t start = text_outside.find('(');
            size_t end = text_outside.find(')');
            if (start == std::string::npos || end == std::string::npos || start > end) {
                break;
            }
            text_outside.erase(start, end - start + 1);
        }
        
        // Trim again after removing brackets
        text_outside.erase(0, text_outside.find_first_not_of(" \t\n\r\f\v"));
        text_outside.erase(text_outside.find_last_not_of(" \t\n\r\f\v") + 1);
        
        // If there's substantial text outside brackets/parentheses, it's not silence
        return text_outside.length() < 5;
    }
    
    return false;
}

// Process a single transcript and return true if conversation should continue
bool process_transcript(const std::string& transcript, OllamaClient* ollama, TTSEngine* tts, bool debug) {
    // Safety check: We should never process silence markers or empty transcripts
    if (transcript.empty() || is_silence_marker(transcript)) {
        if (debug) {
            std::cout << "Info: Empty or silence transcript passed to process_transcript. Skipping processing." << std::endl;
        }
        return true; // Continue listening without responding
    }

    // Remove "over" from the end of the transcript for processing
    std::string clean_transcript = transcript;
    size_t over_pos = clean_transcript.find(" over");
    if (over_pos != std::string::npos && over_pos > 0 && 
        (over_pos + 5 >= clean_transcript.length() || 
         clean_transcript[over_pos + 5] == '.' || 
         clean_transcript[over_pos + 5] == '!' ||
         clean_transcript[over_pos + 5] == '?')) {
        clean_transcript = clean_transcript.substr(0, over_pos);
    }

    // Process with Ollama
    if (debug) {
        std::cout << "Info: Processing with Ollama..." << std::endl;
    }
    std::string response = ollama->process(clean_transcript);
    
    // Display output in chat format
    std::cout << "Vibe: " << response << std::endl;
    
    // Convert to speech
    if (debug) {
        std::cout << "Info: Converting to speech..." << std::endl;
    }
    tts->speak(response);
    
    // Check if the user said "over" to indicate conversation should continue
    return has_over_keyword(transcript);
}

// Run voice assistant in conversational mode - returns true if application should exit
bool run_assistant_cycle(AudioInput* audio, WhisperSTT* whisper, OllamaClient* ollama, TTSEngine* tts, bool debug) {
    bool continue_conversation = true;
    bool should_exit = false;
    int silence_counter = 0;
    const int max_silence_turns = 3; // Exit after this many consecutive silent turns
    
    while (continue_conversation && g_running && !should_exit) {
        std::cout << "\nListening... (press Ctrl+C to stop)" << std::endl;
        
        // Record audio
        std::string audio_file = audio->record();
        if (audio_file.empty()) {
            std::cerr << "Failed to record audio." << std::endl;
            
            // Count consecutive silent turns
            silence_counter++;
            if (silence_counter >= max_silence_turns && !audio->is_continuous_mode()) {
                std::cout << "Multiple silent recordings detected. Ending conversation." << std::endl;
                break;
            }
            
            continue; // Try again
        }
        
        if (debug) {
            // In debug mode, check the audio file
            std::cout << "Debug: Checking audio file " << audio_file << "..." << std::endl;
            
            // Print info about the audio file
            std::stringstream cmd;
            cmd << "file " << audio_file;
            std::system(cmd.str().c_str());
            
            // Try to play it back
            std::cout << "Debug: Playing back recorded audio for verification..." << std::endl;
            cmd.str("");
            cmd << "aplay " << audio_file << " 2>/dev/null || "
                << "paplay " << audio_file << " 2>/dev/null || "
                << "play " << audio_file << " 2>/dev/null";
            std::system(cmd.str().c_str());
        }
        
        // Transcribe audio
        std::cout << "Transcribing..." << std::endl;
        std::string transcript = whisper->transcribe(audio_file, debug);
        bool has_speech = false;
        
        // Check if transcript is empty or a silence marker
        if (transcript.empty() || is_silence_marker(transcript)) {
            // For debugging purposes only, attempt fallback transcription
            if (debug && transcript.empty()) {
                std::cout << "Debug: Empty transcript detected. Attempting fallback transcription..." << std::endl;
                
                // Try a more basic transcription as a fallback
                std::stringstream cmd;
                cmd << whisper->get_executable()
                    << " -f " << audio_file
                    << " -m ./whisper.cpp/models/ggml-base.en.bin"
                    << " -l en"
                    << " --greedy"; // Simplified decoding
                    
                std::cout << "Debug: Running " << cmd.str() << std::endl;
                
                // Execute and capture output
                std::string debug_output;
                FILE* debug_pipe = popen(cmd.str().c_str(), "r");
                if (debug_pipe) {
                    char buffer[1024];
                    while (fgets(buffer, sizeof(buffer), debug_pipe) != NULL) {
                        debug_output += buffer;
                        std::cout << buffer; // Print real-time output
                    }
                    
                    pclose(debug_pipe);
                    
                    // Try to extract any transcription
                    size_t pos = debug_output.find("<|endoftext|>");
                    if (pos != std::string::npos) {
                        size_t start_pos = debug_output.rfind("\n", pos);
                        if (start_pos != std::string::npos) {
                            std::string fallback_text = debug_output.substr(start_pos + 1, pos - start_pos - 1);
                            std::cout << "Debug: Found fallback transcription: \"" << fallback_text << "\"" << std::endl;
                            
                            // Only use fallback text if it's not silence and has actual content
                            if (!fallback_text.empty() && !is_silence_marker(fallback_text)) {
                                transcript = fallback_text;
                                has_speech = true;
                                silence_counter = 0;
                                
                                // Check for exit keywords in fallback text
                                if (has_exit_keyword(fallback_text)) {
                                    std::cout << "Exit keyword detected. Ending conversation and exiting." << std::endl;
                                    // Say goodbye
                                    std::string goodbye = "Goodbye. Exiting voice assistant.";
                                    std::cout << "Assistant: " << goodbye << std::endl;
                                    tts->speak(goodbye);
                                    should_exit = true;
                                    break;
                                }
                            }
                        }
                    }
                }
                
                // Save audio file for debugging
                std::string debug_copy = "/tmp/debug_last_failed_recording.wav";
                std::stringstream cp_cmd;
                cp_cmd << "cp " << audio_file << " " << debug_copy;
                std::system(cp_cmd.str().c_str());
                std::cout << "Debug: Saved copy of audio file to " << debug_copy << " for inspection" << std::endl;
            }
            
            // If still no speech detected after fallback attempt
            if (!has_speech) {
                // Clearly log what was detected
                if (transcript.empty()) {
                    std::cout << "Empty transcript. Continuing to listen..." << std::endl;
                } else {
                    // Check if it's a specific kind of non-speech sound
                    std::string lower_text = transcript;
                    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
                    
                    if (lower_text.find("[blank_audio]") != std::string::npos) {
                        std::cout << "Detected blank audio. Continuing to listen..." << std::endl;
                    } else if (lower_text.find("[") != std::string::npos) {
                        std::cout << "Detected markup: '" << transcript << "'. Continuing to listen..." << std::endl;
                    } else if (lower_text.find("(") != std::string::npos) {
                        std::cout << "Detected sound: '" << transcript << "'. Continuing to listen..." << std::endl;
                    } else {
                        std::cout << "Detected only: '" << transcript << "'. Continuing to listen..." << std::endl;
                    }
                }
                
                // Count consecutive silent turns
                silence_counter++;
                if (silence_counter >= max_silence_turns && !audio->is_continuous_mode()) {
                    std::cout << "Multiple silent recordings detected. Ending conversation." << std::endl;
                    break;
                }
                
                // Delete the audio file to save space
                if (!debug) {
                    if (remove(audio_file.c_str()) == 0) {
                        std::cout << "Removed empty audio file." << std::endl;
                    }
                }
                
                // Continue the loop without processing
                continue;
            }
        } 
        else {
            // Real speech detected
            has_speech = true;
            silence_counter = 0;
        }
        
        // If we have actual speech content to process
        if (has_speech) {
            std::cout << "You said: " << transcript << std::endl;
            
            // Check for exit keywords before processing
            if (has_exit_keyword(transcript)) {
                std::cout << "Exit keyword detected. Ending conversation and exiting." << std::endl;
                // Say goodbye
                std::string goodbye = "Goodbye. Exiting voice assistant.";
                std::cout << "Assistant: " << goodbye << std::endl;
                tts->speak(goodbye);
                should_exit = true;
                break;
            }
            
            // Process the transcript and check if we should continue
            continue_conversation = process_transcript(transcript, ollama, tts, debug);
            
            // If not in continuous mode and no "over" was detected, stop the conversation
            if (!audio->is_continuous_mode() && !continue_conversation) {
                std::cout << "No 'over' detected, ending conversation." << std::endl;
                break;
            }
        }
        
        // Small delay before next cycle
        // Check for interrupt during the delay
        for (int i = 0; i < 5 && g_running; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    return should_exit;
}