#!/bin/bash
# Setup script for C++ voice assistant

set -e

echo "Setting up voice assistant..."

# Check for required tools
command -v cmake >/dev/null 2>&1 || { echo "Error: cmake is required but not installed. Please install cmake."; exit 1; }
command -v g++ >/dev/null 2>&1 || { echo "Error: g++ is required but not installed. Please install g++."; exit 1; }
command -v make >/dev/null 2>&1 || { echo "Error: make is required but not installed. Please install make."; exit 1; }

# Install system dependencies (for Debian/Ubuntu)
if command -v apt-get >/dev/null 2>&1; then
    echo "Installing system dependencies..."
    sudo apt-get update
    sudo apt-get install -y libcurl4-openssl-dev nlohmann-json3-dev alsa-utils espeak libespeak-dev git build-essential
elif command -v yum >/dev/null 2>&1; then
    echo "Installing system dependencies (CentOS/RHEL/Fedora)..."
    sudo yum install -y libcurl-devel alsa-utils espeak git gcc-c++ make
    # nlohmann-json might need manual installation
    if [ ! -f "/usr/include/nlohmann/json.hpp" ]; then
        echo "Installing nlohmann/json..."
        mkdir -p /tmp/json
        curl -L https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp -o /tmp/json/json.hpp
        sudo mkdir -p /usr/include/nlohmann
        sudo cp /tmp/json/json.hpp /usr/include/nlohmann/
        rm -rf /tmp/json
    fi
else
    echo "Warning: Package manager not recognized. Please install libcurl, nlohmann-json, ALSA utils, and espeak manually."
fi

# Clone and build whisper.cpp if not already present
if [ ! -d "whisper.cpp" ]; then
    echo "Cloning whisper.cpp..."
    git clone https://github.com/ggerganov/whisper.cpp.git
    
    echo "Building whisper.cpp..."
    cd whisper.cpp
    make
    
    echo "Downloading whisper model..."
    ./models/download-ggml-model.sh base.en
    cd ..
fi

# Create default config if not exists
if [ ! -f "config.json" ]; then
    echo "Creating default configuration..."
    cat > config.json << EOL
{
  "whisper": {
    "model": "base.en",
    "executable": "./whisper.cpp/build/bin/whisper-cli",
    "params": "-l en --no-timestamps"
  },
  "ollama": {
    "model": "llama3",
    "system_prompt": "You are a helpful voice assistant. Provide concise responses.",
    "host": "http://localhost:11434"
  },
  "tts": {
    "engine": "espeak",
    "voice": "en",
    "speed": 150,
    "output_device": "default"
  },
  "audio": {
    "device": "default",
    "sample_rate": 16000,
    "duration": 5
  }
}
EOL
fi

# Create build directory and build project
echo "Building voice assistant..."
mkdir -p build
cd build
cmake ..
make

echo "Running tests..."
cd ..
mkdir -p build_tests
cd build_tests
cmake .. -DBUILD_TESTING=ON
make
make run_tests

cd ..

echo "Setup complete! To run the voice assistant:"
echo "1. Make sure ollama is running with: ollama serve"
echo "2. Pull the model: ollama pull llama3"
echo "3. Run the assistant: ./build/voice_assistant"