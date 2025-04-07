#ifndef OLLAMA_CLIENT_H
#define OLLAMA_CLIENT_H

#include <string>
#include <sstream>
#include <cstdlib>
#include <regex>
#include <curl/curl.h>
#include "config.h"

// Callback for CURL to write response data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

class OllamaClient {
private:
    OllamaConfig config;
    std::string system_info;
    std::vector<std::pair<std::string, std::string>> conversation_history; // Pairs of (user, assistant) messages
    
    // Process text to make it more TTS-friendly
    std::string process_text_for_tts(const std::string& text) {
        std::string result = text;
        
        // Replace markdown formatting
        auto replace_all = [](std::string& str, const std::string& from, const std::string& to) {
            size_t start_pos = 0;
            while((start_pos = str.find(from, start_pos)) != std::string::npos) {
                str.replace(start_pos, from.length(), to);
                start_pos += to.length();
            }
        };
        
        // Remove code blocks
        std::regex code_block_regex("```[\\s\\S]*?```");
        result = std::regex_replace(result, code_block_regex, "I've prepared some code for you, but I won't read it aloud.");
        
        // Remove inline code
        std::regex inline_code_regex("`([^`]+)`");
        result = std::regex_replace(result, inline_code_regex, "$1");
        
        // Convert bullet points to complete sentences
        std::regex bulletPointLine("^\\s*[\\*\\-•]\\s*(.+?)$");
        std::string bulletReplacement = "Point: $1. ";
        result = std::regex_replace(result, bulletPointLine, bulletReplacement);
        
        // Replace remaining bullet characters
        replace_all(result, "* ", "Point: ");
        replace_all(result, "- ", "Point: ");
        replace_all(result, "• ", "Point: ");
        
        // Replace URLs with more speech-friendly text
        std::regex url_regex("https?://\\S+");
        result = std::regex_replace(result, url_regex, "website link");
        
        // Remove markdown links but keep text
        std::regex link_regex("\\[([^\\]]+)\\]\\([^\\)]+\\)");
        result = std::regex_replace(result, link_regex, "$1");
        
        // Replace heading formatting with spoken phrases
        std::regex h1_regex("# ([^\n]+)");
        result = std::regex_replace(result, h1_regex, "Main topic: $1. ");
        
        std::regex h2_regex("## ([^\n]+)");
        result = std::regex_replace(result, h2_regex, "Subtopic: $1. ");
        
        std::regex h3_regex("### ([^\n]+)");
        result = std::regex_replace(result, h3_regex, "Section: $1. ");
        
        // Remove bold and italic formatting
        replace_all(result, "**", "");
        replace_all(result, "__", "");
        replace_all(result, "*", "");
        replace_all(result, "_", "");
        
        // Replace colons in text with spoken language
        std::regex colonPattern("([^:]):([^:]|$)");
        result = std::regex_replace(result, colonPattern, "$1 is $2");
        
        // Replace newlines with spaces for better flow in speech
        replace_all(result, "\n\n", ". ");
        replace_all(result, "\n", " ");
        
        // Remove multiple spaces
        std::regex multi_spaces("\\s+");
        result = std::regex_replace(result, multi_spaces, " ");
        
        // Remove multiple periods
        replace_all(result, ".. ", ". ");
        replace_all(result, "... ", ". ");
        
        // Add pauses after sentences for better speech rhythm
        std::regex sentence_end("\\. ");
        result = std::regex_replace(result, sentence_end, ". ");
        
        // Expand common abbreviations
        replace_all(result, " e.g. ", " for example ");
        replace_all(result, " i.e. ", " that is ");
        replace_all(result, " etc. ", " etcetera ");
        replace_all(result, " vs. ", " versus ");
        replace_all(result, " approx. ", " approximately ");
        
        // Make numbers more speech-friendly
        replace_all(result, "1st", "first");
        replace_all(result, "2nd", "second");
        replace_all(result, "3rd", "third");
        replace_all(result, "4th", "fourth");
        replace_all(result, "5th", "fifth");
        
        // Read special characters in a more natural way
        replace_all(result, "&", " and ");
        replace_all(result, "%", " percent ");
        replace_all(result, "$", " dollars ");
        replace_all(result, "=", " equals ");
        replace_all(result, "+", " plus ");
        replace_all(result, "-", " minus ");
        replace_all(result, "/", " divided by ");
        replace_all(result, ">", " greater than ");
        replace_all(result, "<", " less than ");
        
        // Replace common emojis with spoken descriptions
        replace_all(result, "😊", " smiling face ");
        replace_all(result, "👍", " thumbs up ");
        replace_all(result, "👎", " thumbs down ");
        replace_all(result, "❤️", " heart ");
        replace_all(result, "👋", " waving hand ");
        replace_all(result, "🙂", " slightly smiling face ");
        replace_all(result, "😀", " grinning face ");
        replace_all(result, "🤖", " robot face ");
        replace_all(result, "✅", " check mark ");
        replace_all(result, "⚠️", " warning ");
        replace_all(result, "⭐", " star ");
        replace_all(result, "🚀", " rocket ");
        
        return result;
    }
    
    // Format conversation history for the prompt
    std::string format_conversation_history() const {
        if (conversation_history.empty()) {
            return "";
        }
        
        std::stringstream history;
        history << "\n\nConversation history:\n";
        
        // Get the last 5 turns at most (or fewer if there aren't that many)
        size_t start_idx = conversation_history.size() > 5 ? conversation_history.size() - 5 : 0;
        
        for (size_t i = start_idx; i < conversation_history.size(); ++i) {
            const auto& turn = conversation_history[i];
            history << "User: " << turn.first << "\n";
            history << "Assistant: " << turn.second << "\n\n";
        }
        
        return history.str();
    }
    
public:
    OllamaClient(const OllamaConfig& cfg, const std::string& sysinfo = "") 
        : config(cfg), system_info(sysinfo), conversation_history() {
        // Initialize CURL globally (should be done once)
        curl_global_init(CURL_GLOBAL_ALL);
    }
    
    // Set system information
    void set_system_info(const std::string& sysinfo) {
        system_info = sysinfo;
    }
    
    // Clear conversation history
    void clear_history() {
        conversation_history.clear();
    }
    
    // Get the number of conversation turns
    size_t history_size() const {
        return conversation_history.size();
    }
    
    ~OllamaClient() {
        // Cleanup CURL globally
        curl_global_cleanup();
    }
    
    // Process text with ollama
    std::string process(const std::string& text) {
        // Safety check - do not process empty text
        if (text.empty()) {
            std::cerr << "Error: Attempted to process empty text" << std::endl;
            return ""; // Return empty response for empty input
        }
        
        CURL* curl;
        CURLcode res;
        std::string readBuffer;
        
        // Create CURL handle
        curl = curl_easy_init();
        if (!curl) {
            std::cerr << "Failed to initialize CURL" << std::endl;
            return "Sorry, I'm having trouble connecting to my thinking module.";
        }
        
        // Set URL
        std::string url = config.host + "/api/generate";
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        
        // Set timeout (5 seconds)
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
        
        // Set headers
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        // Create a proper JSON object with the nlohmann/json library
        nlohmann::json request_json;
        request_json["model"] = config.model;
        request_json["prompt"] = text;
        
        // Add system information and conversation history to the system prompt
        std::string enhanced_system_prompt = config.system_prompt;
        
        // Add system information if available
        if (!system_info.empty()) {
            enhanced_system_prompt += "\n\nYou are a voice assistant called Vibe. You consist of multiple components working together:\n"
                                    "1. Whisper speech-to-text engine to convert user's voice to text\n"
                                    "2. Ollama for language model processing (you are the language model part)\n" 
                                    "3. ESpeak text-to-speech for converting your responses to speech\n\n"
                                    "Since your responses will be read aloud by a text-to-speech system, follow these guidelines:\n"
                                    "1. Use complete sentences with natural phrasing\n"
                                    "2. Never use bullet points with symbols like *, -, or •. Instead, start with phrases like 'First point,' 'Second point,' etc.\n"
                                    "3. Avoid using colons in your responses - use complete sentences instead\n"
                                    "4. Never use emojis or special characters that can't be read aloud naturally\n"
                                    "5. Keep responses concise and directly address the user's question\n"
                                    "6. Avoid technical jargon or complex terminology\n\n"
                                    "When the user asks about you or your hardware, explain in simple, conversational terms without long model numbers or technical jargon. "
                                    "Always use first person when referring to yourself ('I am...').\n\n"
                                    "Here is your system information (keep descriptions brief and user-friendly when speaking about this): \n"
                                    + system_info + "\n\n"
                                    "Important: When asked about the current time or date, use the information provided above, not your training data. "
                                    "When describing your hardware capabilities, be conversational and avoid overly technical information.";
        }
        
        // Add conversation history if available
        std::string history = format_conversation_history();
        if (!history.empty()) {
            enhanced_system_prompt += history + 
                                    "\nPlease respond to the user's latest message, taking into account the conversation history above.";
        }
        
        request_json["system"] = enhanced_system_prompt;
        request_json["stream"] = false;
        
        // Convert to string
        std::string json_data = request_json.dump();
        
        // Set data to send
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
        
        // Set callback function for received data
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        
        // Perform the request
        res = curl_easy_perform(curl);
        
        // Check for errors
        if (res != CURLE_OK) {
            std::cerr << "CURL error: " << curl_easy_strerror(res) << std::endl;
            
            // Provide more specific error messages based on error code
            if (res == CURLE_COULDNT_CONNECT) {
                std::cerr << "Could not connect to Ollama server. Is it running?" << std::endl;
                std::cerr << "Start it with: ollama serve" << std::endl;
                curl_easy_cleanup(curl);
                curl_slist_free_all(headers);
                return "I can't reach my thinking module. Please make sure Ollama is running with 'ollama serve'.";
            } else if (res == CURLE_OPERATION_TIMEDOUT) {
                std::cerr << "Connection to Ollama server timed out" << std::endl;
                curl_easy_cleanup(curl);
                curl_slist_free_all(headers);
                return "It's taking too long to get a response. Is the model loaded? Try 'ollama pull " + config.model + "'.";
            }
            
            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);
            return "Sorry, I encountered an error while processing your request.";
        }
        
        // Get HTTP response code
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        // Clean up
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        
        // Process response
        if (http_code == 200 && !readBuffer.empty()) {
            // Parse JSON response
            try {
                nlohmann::json response = nlohmann::json::parse(readBuffer);
                if (response.contains("response")) {
                    std::string resp_text = response["response"];
                    
                    // Process the response for TTS friendliness
                    std::string processed_text = process_text_for_tts(resp_text);
                    
                    // Only add to conversation history if there was actual speech and a valid response
                    if (!text.empty() && !resp_text.empty()) {
                        conversation_history.push_back(std::make_pair(text, resp_text));
                    }
                    
                    return processed_text;
                } else {
                    std::cerr << "Response missing 'response' field: " << readBuffer << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error parsing JSON response: " << e.what() << std::endl;
                std::cerr << "Raw response: " << readBuffer << std::endl;
            }
        } else if (http_code == 404) {
            std::cerr << "Model not found: " << config.model << std::endl;
            return "I can't find the model '" + config.model + "'. Please run 'ollama pull " + config.model + "' first.";
        } else if (http_code == 500) {
            std::cerr << "Ollama server error: " << readBuffer << std::endl;
            return "The Ollama server encountered an error processing your request.";
        } else {
            std::cerr << "Unexpected HTTP code: " << http_code << std::endl;
            std::cerr << "Response: " << readBuffer << std::endl;
        }
        
        return "Sorry, I couldn't process your request properly.";
    }
};

#endif // OLLAMA_CLIENT_H