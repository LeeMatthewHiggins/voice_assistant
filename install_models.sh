#!/bin/bash
# Script to install all Ollama models used by the voice assistant

# Check if Ollama is installed
if ! command -v ollama &> /dev/null; then
    echo "Ollama not found. Please install Ollama first."
    echo "Run ./install_ollama.sh to install Ollama."
    exit 1
fi

# Check if Ollama server is running
if ! timeout 1 bash -c 'cat < /dev/null > /dev/tcp/localhost/11434' 2>/dev/null; then
    echo "Ollama server is not running. Please start it first."
    echo "Run 'ollama serve' to start the Ollama server."
    exit 1
fi

# Install all models
echo "Installing all models..."

# List of models
MODELS=(
    "llama3"
    "gemma3:1b"
    "gemma3:4b"
    "gemma3:12b"
)

for model in "${MODELS[@]}"; do
    echo "Pulling model: $model"
    ollama pull "$model"
    
    if [ $? -ne 0 ]; then
        echo "Failed to pull model $model. Continuing with other models..."
    else
        echo "Successfully pulled model $model."
    fi
    
    echo "----------------------"
done

echo "Model installation complete!"
echo "You can now run the voice assistant with different models."
echo "Use './build/voice_assistant --setup' to select a model."