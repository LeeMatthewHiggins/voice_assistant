"""
Module for recording audio input
"""

import os
import subprocess
import tempfile
import time

def record_audio(config):
    """
    Record audio from microphone
    
    Args:
        config: Audio recording configuration
        
    Returns:
        Path to the recorded audio file
    """
    duration = config.get("duration", 5)
    sample_rate = config.get("sample_rate", 16000)
    device = config.get("device", "default")
    
    # Create a temporary file for the recording
    output_file = os.path.join(tempfile.gettempdir(), f"recording_{int(time.time())}.wav")
    
    try:
        # Try to use arecord (ALSA) for recording
        cmd = [
            "arecord",
            "-D", device,
            "-f", "S16_LE",
            "-c", "1",
            "-r", str(sample_rate),
            "-d", str(duration),
            output_file
        ]
        
        subprocess.run(cmd, check=True)
        return output_file
        
    except (FileNotFoundError, subprocess.CalledProcessError):
        # Fallback to sox if arecord fails
        try:
            cmd = [
                "rec",
                "-q",
                output_file,
                "rate", str(sample_rate),
                "channels", "1",
                "trim", "0", str(duration)
            ]
            
            subprocess.run(cmd, check=True)
            return output_file
            
        except (FileNotFoundError, subprocess.CalledProcessError) as e:
            print(f"Error recording audio: {e}")
            print("Make sure you have either ALSA tools (arecord) or SoX (rec) installed.")
            print("Install with: sudo apt-get install alsa-utils or sudo apt-get install sox")
            raise