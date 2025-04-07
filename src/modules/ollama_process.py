"""
Module for processing text with ollama
"""

import json
import subprocess
import requests

def process_with_ollama(text, config):
    """
    Process text with ollama
    
    Args:
        text: Input text to process
        config: Ollama configuration
    
    Returns:
        Response from ollama
    """
    model = config["model"]
    system_prompt = config.get("system_prompt", "You are a helpful assistant.")
    
    # Check if ollama is running
    try:
        response = requests.get("http://localhost:11434/api/tags")
        if response.status_code != 200:
            print("Ollama server not responding. Make sure it's running.")
            return "Sorry, I'm having trouble connecting to my thinking module."
    except requests.exceptions.ConnectionError:
        print("Ollama server not running. Please start ollama service.")
        return "Sorry, I'm having trouble connecting to my thinking module."
    
    # Make the API call to ollama
    try:
        payload = {
            "model": model,
            "prompt": text,
            "system": system_prompt,
            "stream": False
        }
        
        response = requests.post(
            "http://localhost:11434/api/generate",
            json=payload
        )
        
        if response.status_code == 200:
            result = response.json()
            return result.get("response", "")
        else:
            print(f"Error from ollama API: {response.status_code}")
            print(response.text)
            return "Sorry, I encountered an error while processing your request."
    
    except Exception as e:
        print(f"Error calling ollama: {e}")
        return "Sorry, I encountered an error while processing your request."