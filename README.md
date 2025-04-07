# C++ Voice Assistant

A voice assistant using whisper.cpp for speech recognition, ollama for text processing, and espeak for text-to-speech responses.

## Components

- **Speech Recognition**: Uses [whisper.cpp](https://github.com/ggerganov/whisper.cpp) for fast, local speech-to-text
- **Processing**: Uses [ollama](https://ollama.ai/) to run local LLMs with speech-optimized output
- **Text-to-Speech**: Uses espeak for speech synthesis with natural-sounding responses

## Dependencies

- CMake (build system)
- C++17 compatible compiler (g++ or clang++)
- libcurl (for HTTP requests to ollama)
- nlohmann/json (for JSON parsing)
- ALSA utilities (for audio recording)
- espeak (for text-to-speech)
- whisper.cpp (included as a submodule)

## Setup

1. Make the setup script executable:
   ```
   chmod +x setup.sh
   ```

2. Run the setup script:
   ```
   ./setup.sh
   ```

3. The setup script will:
   - Install required system dependencies
   - Clone and build whisper.cpp
   - Download whisper model
   - Create a default configuration file
   - Build the voice assistant
   
4. Install and start Ollama:
   ```
   chmod +x install_ollama.sh
   ./install_ollama.sh
   ```
   
   This script will:
   - Install Ollama if it's not already installed
   - Start the Ollama server if it's not already running
   - Pull the model specified in your config.json

## Usage

1. Make sure ollama is running:
   ```
   ollama serve
   ```

2. Pull the model you want to use (if not already available):
   ```
   ollama pull llama3
   ```

3. Run the voice assistant:
   ```
   ./build/voice_assistant
   ```

4. Speak when prompted and the assistant will respond

## Configuration

The assistant can be configured by editing the `config.json` file, which includes settings for:

- Whisper model and parameters
- Ollama model and system prompt
- Text-to-speech engine and voice
- Audio recording settings

Example configuration:
```json
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
```

## Command Line Options

You can pass command-line arguments:

```
./voice_assistant --config custom_config.json --continuous
```

Options:
- `--config`: Specify a custom config file path
- `--continuous`: Run in continuous mode (keep listening for commands)
- `--input-device`: Specify audio input device (e.g., webcam microphone)
- `--output-device`: Specify audio output device (e.g., speakers)
- `--list-devices`: List all available audio input and output devices
- `--debug`: Run in debug mode with extra diagnostics
- `--setup`: Run interactive setup to configure model, personality, and voice
- `--help`: Show help message

## Setup Options

Run the interactive setup to configure your assistant:

```
./build/voice_assistant --setup
```

This will allow you to choose:

1. **Model** - Select from various Ollama models:
   - llama3
   - gemma3:1b
   - gemma3:4b
   - gemma3:12b

2. **Personality** - Choose your assistant's role:
   - Tech Co-Worker: Technical expert for software and IT help
   - Personal Friend: Casual, supportive conversational companion
   - Tutor: Patient explainer of complex concepts
   - Life Coach: Motivational guide focused on personal growth

3. **Voice** - Select voice characteristics:
   - English (US) - Male
   - English (US) - Female
   - English (UK) - Male
   - English (UK) - Female
   
To install all available models at once, run:
```
./install_models.sh
```

## Voice-Optimized Responses

All responses are automatically processed to be more voice-friendly:

- Removes markdown formatting, code blocks, and other text styling
- Converts URLs and technical notation to speech-friendly formats
- Expands common abbreviations (e.g., "e.g." becomes "for example")
- Improves sentence flow by adding natural pauses
- Makes numbers and special characters more speech-friendly
- Converts bullet points to a more natural spoken format

Each personality is also instructed to provide responses that are:
- Conversational and natural-sounding
- Free of complex formatting or visual elements
- Structured with complete sentences and natural pauses
- Similar to how a person would speak in a real-life conversation

## System Self-Awareness

The assistant has knowledge of its own components and configuration:

- You can ask questions like:
  * "What whisper model are you using?"
  * "What LLM model are you running on?"
  * "What voice are you using to speak?"
  * "What are your system specifications?"

- The assistant is automatically provided with:
  * Whisper.cpp version information
  * Ollama version information
  * Current model selections
  * System specifications
  * Build information

This information is injected into the system prompt, allowing the assistant to answer questions about its own configuration accurately.

### Using a Webcam Microphone

To use a webcam microphone:

1. List available devices:
   ```
   ./build/voice_assistant --list-devices
   ```

2. Look for your webcam in the list of input devices. You'll see something like:
   ```
   Available audio input devices:
   Name: alsa_input.usb-046d_HD_Pro_Webcam_C920_XXXXXXXX-00.analog-stereo
   Description: HD Pro Webcam C920 Analog Stereo
   ```

3. Run the assistant with the selected input device:
   ```
   ./build/voice_assistant --input-device alsa_input.usb-046d_HD_Pro_Webcam_C920_XXXXXXXX-00.analog-stereo
   ```

4. Alternatively, set it permanently in the config.json file:
   ```json
   "audio": {
     "device": "alsa_input.usb-046d_HD_Pro_Webcam_C920_XXXXXXXX-00.analog-stereo",
     "sample_rate": 16000,
     "duration": 5
   }
   ```

## Troubleshooting

### Debug Mode

If you encounter issues with the voice assistant not detecting your speech, run in debug mode:

```
./build/voice_assistant --debug
```

Debug mode will:
1. Run diagnostics to check your audio setup and whisper.cpp installation
2. Increase recording duration to give you more time to speak
3. Provide verbose output during recording and transcription
4. Play back your recorded audio for verification
5. Save a copy of any failed recordings for later inspection

### Common Issues

- **"No speech detected" error**: This typically means there was a problem with audio recording or the speech recognition failed. Try the following:
  
  1. Run with `--debug` to run diagnostics
  2. Check if your microphone is working properly:
     ```
     arecord -d 5 test.wav && aplay test.wav
     ```
  3. Try using a different input device:
     ```
     ./build/voice_assistant --list-devices
     ./build/voice_assistant --input-device DEVICE_NAME
     ```
  4. Increase audio recording duration in config.json:
     ```json
     "audio": {
       "duration": 10,
       ...
     }
     ```
  5. Try a different whisper model:
     ```json
     "whisper": {
       "model": "tiny.en",
       ...
     }
     ```

- **Audio recording issues**: Make sure your microphone is properly connected and selected. You can specify a different device in the configuration.
- **Whisper.cpp errors**: Make sure the models are downloaded correctly by running `./whisper.cpp/models/download-ggml-model.sh base.en`.
- **Ollama errors**: Ensure ollama is running with `ollama serve` and that you've pulled the model you want to use.
- **Build errors**: If you encounter build errors, make sure all dependencies are installed correctly.

## Building Manually

If you prefer to build manually:

```
mkdir -p build
cd build
cmake ..
make
```

## Running Tests

To run the tests:

```
mkdir -p build_tests
cd build_tests
cmake .. -DBUILD_TESTING=ON
make
make run_tests
```

The tests check each component of the voice assistant:
- Config loading and saving
- Whisper STT functionality
- Ollama client API interaction
- TTS engine operation