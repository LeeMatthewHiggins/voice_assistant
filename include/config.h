#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <fstream>
#include <nlohmann/json.hpp>

// Audio recording configuration
struct AudioConfig {
    std::string device = "default";
    int sample_rate = 16000;
    int duration = 5;
};

// Whisper configuration
struct WhisperConfig {
    std::string model = "base.en";
    std::string executable = "./whisper.cpp/main";
    std::string params = "-l en";
};

// Ollama configuration
struct OllamaConfig {
    std::string model = "llama3";
    std::string system_prompt = "You are a helpful voice assistant. Provide concise responses.";
    std::string host = "http://localhost:11434";
};

// TTS configuration
struct TTSConfig {
    std::string engine = "espeak";
    std::string voice = "en";
    int speed = 150;
    std::string output_device = "default";
};

// Personality configuration
struct PersonalityConfig {
    std::string name = "default";
    std::string system_prompt = "You are a helpful voice assistant. Provide concise responses.";
    std::string description = "Default helpful assistant";
};

// Available models
struct AvailableModels {
    std::vector<std::string> models = {
        "llama3", "gemma3:1b", "gemma3:4b", "gemma3:12b"
    };
    
    std::vector<std::string> get_names() const {
        return models;
    }
    
    std::string get_default() const {
        return models[0];
    }
};

// Available personalities
struct AvailablePersonalities {
    std::vector<PersonalityConfig> personalities = {
        {
            "tech_coworker",
            "You are a helpful tech co-worker who specializes in software development, "
            "systems administration, and technical problem-solving. Provide practical, "
            "accurate and concise advice on technical matters. Use industry-standard "
            "terminology but explain complex concepts clearly. Be collaborative and "
            "solution-oriented. Keep your responses short, conversational, and suitable for "
            "speech. Avoid using markdown, code blocks, bullets, or other formatting. Use "
            "complete sentences with natural pauses. Speak as you would in a real conversation.",
            "Tech Co-Worker: Helpful technical colleague who provides expert advice on software and tech issues"
        },
        {
            "personal_friend",
            "You are a close personal friend who is supportive, understanding, and "
            "conversational. Your tone is casual and friendly. You ask thoughtful "
            "follow-up questions and share personal-sounding anecdotes when appropriate. "
            "You're encouraging, empathetic and a good listener. You care about the "
            "person's wellbeing. Keep your responses short, conversational, and suitable for "
            "speech. Avoid using markdown, code blocks, bullets, or other formatting. Use "
            "complete sentences with natural pauses. Speak as you would in a real conversation.",
            "Personal Friend: Supportive, understanding friend who speaks casually and shows empathy"
        },
        {
            "tutor",
            "You are a patient and knowledgeable tutor who specializes in explaining "
            "complex topics clearly. You break down difficult concepts into simple terms "
            "and provide helpful examples. You're encouraging and positive, but also "
            "focused on accuracy and true understanding. You ask questions to check comprehension. "
            "Keep your responses short, conversational, and suitable for speech. Avoid using "
            "markdown, code blocks, bullets, or other formatting. Use complete sentences with "
            "natural pauses. Speak as you would in a real tutoring session.",
            "Tutor: Patient teacher who explains complex topics clearly and checks understanding"
        },
        {
            "life_coach",
            "You are a motivational life coach focused on personal development and "
            "achieving goals. You ask insightful questions to promote self-reflection "
            "and provide actionable advice. You're encouraging but also challenging, "
            "helping to identify limiting beliefs and overcome obstacles. You focus on "
            "practical steps toward personal growth. Keep your responses short, conversational, "
            "and suitable for speech. Avoid using markdown, code blocks, bullets, or other "
            "formatting. Use complete sentences with natural pauses. Speak as you would in a "
            "real coaching session.",
            "Life Coach: Motivational guide who helps with personal development and achieving goals"
        }
    };
    
    std::vector<std::string> get_names() const {
        std::vector<std::string> names;
        for (const auto& p : personalities) {
            names.push_back(p.name);
        }
        return names;
    }
    
    std::vector<std::string> get_descriptions() const {
        std::vector<std::string> descriptions;
        for (const auto& p : personalities) {
            descriptions.push_back(p.description);
        }
        return descriptions;
    }
    
    std::string get_prompt(const std::string& name) const {
        for (const auto& p : personalities) {
            if (p.name == name) {
                return p.system_prompt;
            }
        }
        return personalities[0].system_prompt;
    }
    
    std::string get_default() const {
        return personalities[0].name;
    }
};

// Available voices
struct AvailableVoices {
    std::vector<std::pair<std::string, std::string>> voices = {
        {"en-us-male", "English (US) - Male"},
        {"en-us-female", "English (US) - Female"},
        {"en-gb-male", "English (UK) - Male"},
        {"en-gb-female", "English (UK) - Female"}
    };
    
    std::vector<std::string> get_codes() const {
        std::vector<std::string> codes;
        for (const auto& v : voices) {
            codes.push_back(v.first);
        }
        return codes;
    }
    
    std::vector<std::string> get_descriptions() const {
        std::vector<std::string> descriptions;
        for (const auto& v : voices) {
            descriptions.push_back(v.second);
        }
        return descriptions;
    }
    
    std::string get_default() const {
        return voices[0].first;
    }
};

// System information
struct SystemInfo {
    std::string whisper_version = "whisper.cpp latest";
    std::string ollama_version = "ollama latest";
    std::string build_date;
    std::string current_time;
    std::string os_info;
    std::string cpu_info;
    std::string gpu_info;
    std::string memory_info;
    std::string disk_info;
    std::string network_info;
    
    // Generate formatted info text
    std::string get_formatted_info() const {
        std::stringstream ss;
        ss << "System Information:\n"
           << "- Current date and time: " << current_time << "\n"
           << "- Speech-to-text: " << whisper_version << "\n"
           << "- Language model: " << ollama_version << "\n"
           << "- Build date: " << build_date << "\n"
           << "- OS: " << os_info << "\n"
           << "- CPU: " << cpu_info;
        
        if (!gpu_info.empty()) ss << "\n- GPU: " << gpu_info;
        if (!memory_info.empty()) ss << "\n- Memory: " << memory_info;
        if (!disk_info.empty()) ss << "\n- Disk: " << disk_info;
        if (!network_info.empty()) ss << "\n- Network: " << network_info;
        
        return ss.str();
    }
};

// Main configuration
class Config {
public:
    AudioConfig audio;
    WhisperConfig whisper;
    OllamaConfig ollama;
    TTSConfig tts;
    SystemInfo system_info;
    
    // Static instances of available options
    static AvailableModels available_models;
    static AvailablePersonalities available_personalities;
    static AvailableVoices available_voices;
    
    // Load configuration from file
    void load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open config file: " + filename);
        }
        
        nlohmann::json j;
        file >> j;
        
        // Parse audio config
        if (j.contains("audio")) {
            if (j["audio"].contains("device")) audio.device = j["audio"]["device"];
            if (j["audio"].contains("sample_rate")) audio.sample_rate = j["audio"]["sample_rate"];
            if (j["audio"].contains("duration")) audio.duration = j["audio"]["duration"];
        }
        
        // Parse whisper config
        if (j.contains("whisper")) {
            if (j["whisper"].contains("model")) whisper.model = j["whisper"]["model"];
            if (j["whisper"].contains("executable")) whisper.executable = j["whisper"]["executable"];
            if (j["whisper"].contains("params")) whisper.params = j["whisper"]["params"];
        }
        
        // Parse ollama config
        if (j.contains("ollama")) {
            if (j["ollama"].contains("model")) ollama.model = j["ollama"]["model"];
            if (j["ollama"].contains("system_prompt")) ollama.system_prompt = j["ollama"]["system_prompt"];
            if (j["ollama"].contains("host")) ollama.host = j["ollama"]["host"];
        }
        
        // Parse TTS config
        if (j.contains("tts")) {
            if (j["tts"].contains("engine")) tts.engine = j["tts"]["engine"];
            if (j["tts"].contains("voice")) tts.voice = j["tts"]["voice"];
            if (j["tts"].contains("speed")) tts.speed = j["tts"]["speed"];
            if (j["tts"].contains("output_device")) tts.output_device = j["tts"]["output_device"];
        }
    }
    
    // Create default configuration
    void create_default() {
        // Default values are already set in the struct definitions
    }
    
    // Save configuration to file
    void save(const std::string& filename) {
        nlohmann::json j;
        
        // Build JSON
        j["audio"]["device"] = audio.device;
        j["audio"]["sample_rate"] = audio.sample_rate;
        j["audio"]["duration"] = audio.duration;
        
        j["whisper"]["model"] = whisper.model;
        j["whisper"]["executable"] = whisper.executable;
        j["whisper"]["params"] = whisper.params;
        
        j["ollama"]["model"] = ollama.model;
        j["ollama"]["system_prompt"] = ollama.system_prompt;
        j["ollama"]["host"] = ollama.host;
        
        j["tts"]["engine"] = tts.engine;
        j["tts"]["voice"] = tts.voice;
        j["tts"]["speed"] = tts.speed;
        j["tts"]["output_device"] = tts.output_device;
        
        // Write to file
        std::ofstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Could not write config file: " + filename);
        }
        
        file << j.dump(2);
    }
    
    // Set personality
    void set_personality(const std::string& personality_name) {
        ollama.system_prompt = available_personalities.get_prompt(personality_name);
    }
    
    // Map simple voice names to espeak voice codes
    std::string map_voice_to_espeak(const std::string& voice_code) {
        if (voice_code == "en-us-male") return "en-us";
        if (voice_code == "en-us-female") return "en-us+f3";
        if (voice_code == "en-gb-male") return "en-gb";
        if (voice_code == "en-gb-female") return "en-gb+f3";
        return "en"; // default
    }
};

// Initialize static members
AvailableModels Config::available_models;
AvailablePersonalities Config::available_personalities;
AvailableVoices Config::available_voices;

#endif // CONFIG_H