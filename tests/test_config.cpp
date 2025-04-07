#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <fstream>
#include <string>
#include <filesystem>
namespace fs = std::filesystem;

#include "config.h"

TEST_CASE("Config can be created with default values", "[config]") {
    Config config;
    
    // Check default values
    REQUIRE(config.audio.device == "default");
    REQUIRE(config.audio.sample_rate == 16000);
    REQUIRE(config.audio.duration == 5);
    
    REQUIRE(config.whisper.model == "base.en");
    REQUIRE(config.whisper.executable == "./whisper.cpp/main");
    REQUIRE(config.whisper.params == "-l en");
    
    REQUIRE(config.ollama.model == "llama3");
    REQUIRE(config.ollama.system_prompt == "You are a helpful voice assistant. Provide concise responses.");
    REQUIRE(config.ollama.host == "http://localhost:11434");
    
    REQUIRE(config.tts.engine == "espeak");
    REQUIRE(config.tts.voice == "en");
    REQUIRE(config.tts.speed == 150);
    REQUIRE(config.tts.output_device == "default");
}

TEST_CASE("Config can be saved and loaded", "[config]") {
    // Create a temp file for testing
    std::string temp_file = "/tmp/test_config.json";
    
    // Create a config with custom values
    Config config1;
    config1.audio.device = "test_device";
    config1.audio.sample_rate = 22050;
    config1.audio.duration = 10;
    
    config1.whisper.model = "tiny";
    config1.whisper.executable = "/custom/path/whisper";
    config1.whisper.params = "-custom params";
    
    config1.ollama.model = "mistral";
    config1.ollama.system_prompt = "Custom prompt";
    config1.ollama.host = "http://custom:11434";
    
    config1.tts.engine = "custom_tts";
    config1.tts.voice = "fr";
    config1.tts.speed = 200;
    config1.tts.output_device = "custom_device";
    
    // Save to file
    config1.save(temp_file);
    
    // Load into a new config
    Config config2;
    config2.load(temp_file);
    
    // Check that values were preserved
    REQUIRE(config2.audio.device == "test_device");
    REQUIRE(config2.audio.sample_rate == 22050);
    REQUIRE(config2.audio.duration == 10);
    
    REQUIRE(config2.whisper.model == "tiny");
    REQUIRE(config2.whisper.executable == "/custom/path/whisper");
    REQUIRE(config2.whisper.params == "-custom params");
    
    REQUIRE(config2.ollama.model == "mistral");
    REQUIRE(config2.ollama.system_prompt == "Custom prompt");
    REQUIRE(config2.ollama.host == "http://custom:11434");
    
    REQUIRE(config2.tts.engine == "custom_tts");
    REQUIRE(config2.tts.voice == "fr");
    REQUIRE(config2.tts.speed == 200);
    REQUIRE(config2.tts.output_device == "custom_device");
    
    // Clean up
    std::remove(temp_file.c_str());
}

TEST_CASE("Config handles missing file", "[config]") {
    Config config;
    std::string nonexistent_file = "/tmp/nonexistent_config_file.json";
    
    // Make sure the file doesn't exist
    std::remove(nonexistent_file.c_str());
    
    // Attempt to load should throw
    REQUIRE_THROWS(config.load(nonexistent_file));
}