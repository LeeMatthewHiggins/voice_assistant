"""
Module for text-to-speech synthesis
"""

import os
import subprocess
import tempfile
from pathlib import Path

def text_to_speech(text, config):
    """
    Convert text to speech using the configured TTS engine
    
    Args:
        text: Text to convert to speech
        config: TTS configuration
    """
    engine = config.get("engine", "piper")
    
    if engine == "piper":
        _tts_piper(text, config)
    elif engine == "espeak":
        _tts_espeak(text, config)
    else:
        print(f"Unsupported TTS engine: {engine}")
        print("Supported engines: piper, espeak")

def _tts_piper(text, config):
    """
    Use piper for text-to-speech
    
    Args:
        text: Text to convert to speech
        config: TTS configuration
    """
    voice = config.get("voice", "en_US-amy-medium")
    
    with tempfile.NamedTemporaryFile(suffix='.txt', delete=False) as text_file:
        text_file.write(text.encode('utf-8'))
        text_file_path = text_file.name
    
    output_file = os.path.join(tempfile.gettempdir(), "tts_output.wav")
    
    try:
        cmd = [
            "piper",
            "--model", f"piper-voices/{voice}/model.onnx",
            "--output_file", output_file,
            "--text_file", text_file_path
        ]
        
        subprocess.run(cmd, check=True)
        
        # Play the audio
        play_audio(output_file)
        
    except FileNotFoundError:
        print("Piper not found. Make sure it's installed.")
        print("Install with: pip install piper-tts")
    except subprocess.CalledProcessError as e:
        print(f"Error running piper: {e}")
    finally:
        # Clean up
        os.unlink(text_file_path)
        if os.path.exists(output_file):
            os.unlink(output_file)

def _tts_espeak(text, config):
    """
    Use espeak for text-to-speech
    
    Args:
        text: Text to convert to speech
        config: TTS configuration
    """
    voice = config.get("voice", "en")
    speed = config.get("speed", 150)
    
    try:
        cmd = ["espeak", "-v", voice, "-s", str(speed), text]
        subprocess.run(cmd, check=True)
    except FileNotFoundError:
        print("espeak not found. Make sure it's installed.")
        print("Install with: sudo apt-get install espeak")
    except subprocess.CalledProcessError as e:
        print(f"Error running espeak: {e}")

def play_audio(audio_file):
    """
    Play an audio file using an available audio player
    
    Args:
        audio_file: Path to the audio file to play
    """
    players = [
        ["aplay", []],
        ["paplay", []],
        ["play", ["-q"]],
    ]
    
    for player, args in players:
        try:
            cmd = [player] + args + [audio_file]
            subprocess.run(cmd, check=True)
            return
        except (FileNotFoundError, subprocess.CalledProcessError):
            continue
    
    print("No suitable audio player found. Install aplay, paplay, or sox.")