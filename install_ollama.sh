#!/bin/bash
# Script to install and start Ollama for the voice assistant

# Check if Ollama is already installed
if command -v ollama &> /dev/null; then
    echo "Ollama is already installed."
else
    echo "Installing Ollama..."
    curl -fsSL https://ollama.com/install.sh | sh
    
    if [ $? -ne 0 ]; then
        echo "Failed to install Ollama. Please install it manually from https://ollama.com/"
        exit 1
    fi
    
    echo "Ollama installed successfully."
fi

# Check if Ollama is already running
if timeout 1 bash -c 'cat < /dev/null > /dev/tcp/localhost/11434' 2>/dev/null; then
    echo "Ollama server is already running."
else
    echo "Starting Ollama server..."
    ollama serve &
    
    # Give it a moment to start
    sleep 3
    
    # Check if it's running now
    if timeout 1 bash -c 'cat < /dev/null > /dev/tcp/localhost/11434' 2>/dev/null; then
        echo "Ollama server started successfully."
    else
        echo "Failed to start Ollama server. Please start it manually with 'ollama serve'."
        exit 1
    fi
fi

# Pull the model specified in the config
CONFIG_FILE="config.json"
if [ -f "$CONFIG_FILE" ]; then
    MODEL=$(grep -o '"model":[^,}]*' "$CONFIG_FILE" | cut -d'"' -f4)
    
    if [ -n "$MODEL" ]; then
        echo "Pulling Ollama model: $MODEL"
        ollama pull "$MODEL"
        
        if [ $? -ne 0 ]; then
            echo "Failed to pull model $MODEL. Please pull it manually with 'ollama pull $MODEL'."
            exit 1
        fi
        
        echo "Model $MODEL pulled successfully."
    else
        echo "Could not determine model from config.json."
        echo "Please pull the model manually with 'ollama pull <model>'."
    fi
else
    echo "Config file not found: $CONFIG_FILE"
    echo "Using default model: llama3"
    
    echo "Pulling Ollama model: llama3"
    ollama pull llama3
    
    if [ $? -ne 0 ]; then
        echo "Failed to pull model llama3. Please pull it manually with 'ollama pull llama3'."
        exit 1
    fi
    
    echo "Model llama3 pulled successfully."
fi

echo "Ollama setup complete. The voice assistant should now be able to connect to it."
echo "You can now run the voice assistant with: ./build/voice_assistant"