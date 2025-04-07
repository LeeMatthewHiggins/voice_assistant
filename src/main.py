#!/usr/bin/env python3
"""
Voice Assistant using whisper.cpp for STT, ollama for processing, and TTS for speech synthesis.
"""

import argparse
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

from modules.whisper_stt import transcribe_audio
from modules.ollama_process import process_with_ollama
from modules.tts import text_to_speech
from modules.audio_input import record_audio

def main():
    """Main entry point for the voice assistant."""
    parser = argparse.ArgumentParser(description="Voice Assistant")
    parser.add_argument("--config", type=str, default="config.json", help="Path to config file")
    parser.add_argument("--model", type=str, help="Ollama model to use")
    parser.add_argument("--whisper-model", type=str, help="Whisper model to use")
    parser.add_argument("--tts-engine", type=str, help="TTS engine to use")
    parser.add_argument("--continuous", action="store_true", help="Run in continuous mode")
    args = parser.parse_args()

    # Load configuration
    config_path = Path(args.config)
    if not config_path.exists():
        print(f"Config file not found: {args.config}")
        create_default_config(args.config)
        print(f"Created default config at {args.config}")
    
    with open(config_path, "r") as f:
        config = json.load(f)
    
    # Override config with command line arguments
    if args.model:
        config["ollama"]["model"] = args.model
    if args.whisper_model:
        config["whisper"]["model"] = args.whisper_model
    if args.tts_engine:
        config["tts"]["engine"] = args.tts_engine

    print(f"Using whisper model: {config['whisper']['model']}")
    print(f"Using ollama model: {config['ollama']['model']}")
    print(f"Using TTS engine: {config['tts']['engine']}")
    
    # Run voice assistant loop
    if args.continuous:
        print("Running in continuous mode. Press Ctrl+C to exit.")
        try:
            while True:
                run_assistant(config)
        except KeyboardInterrupt:
            print("\nExiting voice assistant.")
    else:
        run_assistant(config)

def run_assistant(config):
    """Run a single interaction with the voice assistant."""
    print("\nListening... (press Ctrl+C to stop)")
    audio_file = record_audio(config["audio"])
    
    print("Transcribing...")
    transcript = transcribe_audio(audio_file, config["whisper"])
    print(f"You said: {transcript}")
    
    if not transcript.strip():
        print("No speech detected. Please try again.")
        return
    
    print("Processing with Ollama...")
    response = process_with_ollama(transcript, config["ollama"])
    print(f"Assistant: {response}")
    
    print("Converting to speech...")
    text_to_speech(response, config["tts"])

def create_default_config(config_path):
    """Create a default configuration file."""
    default_config = {
        "whisper": {
            "model": "base.en",
            "executable": "./whisper.cpp/main",
            "params": "--model models/ggml-base.en.bin -l en"
        },
        "ollama": {
            "model": "llama3",
            "system_prompt": "You are a helpful voice assistant. Provide concise responses."
        },
        "tts": {
            "engine": "piper",
            "voice": "en_US-amy-medium"
        },
        "audio": {
            "device": "default",
            "sample_rate": 16000,
            "duration": 5
        }
    }
    
    with open(config_path, "w") as f:
        json.dump(default_config, f, indent=2)

if __name__ == "__main__":
    main()