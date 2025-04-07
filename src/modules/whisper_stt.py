"""
Module for speech-to-text using whisper.cpp
"""

import os
import subprocess
import tempfile
from pathlib import Path

def transcribe_audio(audio_file, config):
    """
    Transcribe audio using whisper.cpp
    
    Args:
        audio_file: Path to the audio file
        config: Whisper configuration
        
    Returns:
        Transcribed text
    """
    whisper_executable = Path(config["executable"])
    
    if not whisper_executable.exists():
        raise FileNotFoundError(
            f"Whisper executable not found at {whisper_executable}. "
            "Please install whisper.cpp and update the config."
        )
    
    cmd = [
        str(whisper_executable),
        "-f", str(audio_file),
        "-m", f"./whisper.cpp/models/ggml-{config['model']}.bin",
        "-otxt"
    ]
    
    # Add any additional parameters from config
    if "params" in config:
        for param in config["params"].split():
            cmd.append(param)
    
    try:
        result = subprocess.run(
            cmd, 
            capture_output=True, 
            text=True, 
            check=True
        )
        
        # Whisper.cpp outputs to a file with .txt extension
        output_file = Path(str(audio_file).replace(Path(audio_file).suffix, ".txt"))
        
        if output_file.exists():
            with open(output_file, "r") as f:
                transcript = f.read().strip()
            os.remove(output_file)  # Clean up
            return transcript
        else:
            # If no output file, try to get from stdout
            return result.stdout.strip()
            
    except subprocess.CalledProcessError as e:
        print(f"Error running whisper.cpp: {e}")
        print(f"stderr: {e.stderr}")
        return ""