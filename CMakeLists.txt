cmake_minimum_required(VERSION 3.10)
project(voice_assistant VERSION 0.1.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable testing
enable_testing()

# Find dependencies
find_package(CURL REQUIRED)
find_package(ALSA REQUIRED)
find_path(NLOHMANN_JSON_INCLUDE_DIRS "nlohmann/json.hpp")

# Add include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include 
    ${CURL_INCLUDE_DIRS} 
    ${ALSA_INCLUDE_DIRS}
    ${NLOHMANN_JSON_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/whisper.cpp
    ${CMAKE_SOURCE_DIR}/whisper.cpp/include
    ${CMAKE_SOURCE_DIR}/whisper.cpp/ggml/include
)

# Define source files
set(VOICE_ASSISTANT_SOURCES
    src/main.cpp
    src/config.cpp
    src/streaming_audio_input.cpp
    src/streaming_whisper_stt.cpp
)

# Add executable
add_executable(voice_assistant ${VOICE_ASSISTANT_SOURCES})

# Find Whisper library
find_library(WHISPER_LIBRARY whisper PATHS 
    ${CMAKE_SOURCE_DIR}/whisper.cpp/build
    ${CMAKE_SOURCE_DIR}/whisper.cpp/build/src
    NO_DEFAULT_PATH)

if(NOT WHISPER_LIBRARY)
    message(WARNING "Whisper library not found in whisper.cpp/build. Using dynamic linking.")
    # Try to find it in system paths
    find_library(WHISPER_LIBRARY whisper)
    
    if(NOT WHISPER_LIBRARY)
        message(FATAL_ERROR "Could not find whisper library. Please build whisper.cpp first.")
    endif()
endif()

# Link libraries
target_link_libraries(voice_assistant 
    ${CURL_LIBRARIES}
    ${ALSA_LIBRARIES}
    ${WHISPER_LIBRARY}
    pthread
    dl
    m
)

# Always enable streaming audio mode
add_definitions(-DENABLE_STREAMING)

# Installation
install(TARGETS voice_assistant DESTINATION bin)

# Install config file if it doesn't exist
install(CODE "
    if(NOT EXISTS \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/etc/voice_assistant/config.json\")
        file(MAKE_DIRECTORY \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/etc/voice_assistant\")
        file(COPY \"${CMAKE_SOURCE_DIR}/config.json\" DESTINATION \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/etc/voice_assistant\")
    endif()
")

# Add tests directory
add_subdirectory(tests)