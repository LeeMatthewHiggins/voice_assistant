#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <string>

#include "ollama_client.h"

TEST_CASE("OllamaClient initialization", "[ollama]") {
    OllamaConfig config;
    config.model = "llama3";
    config.system_prompt = "You are a helpful assistant.";
    config.host = "http://localhost:11434";
    
    OllamaClient ollama(config);
    // Just test that it initializes without crashing
    REQUIRE(true);
}

// Note: Testing the actual API calls would require a running ollama server
// So we'll just test some basic behavior
TEST_CASE("OllamaClient handles error conditions", "[ollama]") {
    OllamaConfig config;
    config.model = "llama3";
    config.system_prompt = "You are a helpful assistant.";
    // Use a non-existent host to force failure
    config.host = "http://nonexistent.host:11434";
    
    OllamaClient ollama(config);
    
    // Should return an error message, not crash
    std::string result = ollama.process("Test query");
    REQUIRE(!result.empty());
    REQUIRE(result.find("Sorry") != std::string::npos);
}