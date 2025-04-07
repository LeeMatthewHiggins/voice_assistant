#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <string>
#include <filesystem>
namespace fs = std::filesystem;

#include "tts_engine.h"

TEST_CASE("TTSEngine initialization", "[tts]") {
    TTSConfig config;
    config.engine = "espeak";
    config.voice = "en";
    config.speed = 150;
    
    TTSEngine tts(config);
    // Just test that it initializes without crashing
    REQUIRE(true);
}

TEST_CASE("TTSEngine can handle empty text", "[tts]") {
    TTSConfig config;
    config.engine = "espeak";
    config.voice = "en";
    config.speed = 150;
    
    TTSEngine tts(config);
    
    // Should not crash with empty text
    REQUIRE_NOTHROW(tts.speak(""));
}

TEST_CASE("TTSEngine can handle special characters", "[tts]") {
    TTSConfig config;
    config.engine = "espeak";
    config.voice = "en";
    config.speed = 150;
    
    TTSEngine tts(config);
    
    // Should not crash with special characters
    REQUIRE_NOTHROW(tts.speak("Special characters: !@#$%^&*()_+{}|:<>?"));
}