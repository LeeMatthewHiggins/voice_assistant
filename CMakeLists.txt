cmake_minimum_required(VERSION 3.10)
project(voice_assistant VERSION 0.1.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable testing
enable_testing()

# Find dependencies
find_package(CURL REQUIRED)
find_path(NLOHMANN_JSON_INCLUDE_DIRS "nlohmann/json.hpp")

# Add include directories
include_directories(${CMAKE_SOURCE_DIR}/include ${CURL_INCLUDE_DIRS} ${NLOHMANN_JSON_INCLUDE_DIRS})

# Add executable
add_executable(voice_assistant src/main.cpp)

# Link libraries
target_link_libraries(voice_assistant ${CURL_LIBRARIES})

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