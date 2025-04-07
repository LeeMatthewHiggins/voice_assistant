cmake_minimum_required(VERSION 3.10)
project(voice_assistant_tests)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch Catch2 for testing
include(FetchContent)
FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v2.13.10
)
FetchContent_MakeAvailable(catch2)

# Find dependencies
find_package(CURL REQUIRED)
find_path(NLOHMANN_JSON_INCLUDE_DIRS "nlohmann/json.hpp")

# Add include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CURL_INCLUDE_DIRS}
    ${NLOHMANN_JSON_INCLUDE_DIRS}
)

# Test executables
add_executable(test_config test_config.cpp)
target_link_libraries(test_config Catch2::Catch2)

add_executable(test_whisper test_whisper.cpp)
target_link_libraries(test_whisper Catch2::Catch2 ${CURL_LIBRARIES})

add_executable(test_ollama test_ollama.cpp)
target_link_libraries(test_ollama Catch2::Catch2 ${CURL_LIBRARIES})

add_executable(test_tts test_tts.cpp)
target_link_libraries(test_tts Catch2::Catch2)

# For convenience, create a custom target that runs all tests
add_custom_target(run_tests
    COMMAND test_config
    COMMAND test_whisper
    COMMAND test_ollama
    COMMAND test_tts
    DEPENDS test_config test_whisper test_ollama test_tts
)