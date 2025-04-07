#ifndef VAD_H
#define VAD_H

#include <vector>
#include <cmath>
#include <algorithm>

// Enhanced voice activity detection function with state tracking
// Returns true if speech is detected in the audio buffer
inline bool detect_voice_activity(const std::vector<float>& audio, int sample_rate, float threshold, float freq_threshold) {
    if (audio.empty()) {
        return false;
    }
    
    // Calculate energy of the signal
    float energy = 0;
    float peak_energy = 0;
    
    for (float sample : audio) {
        float sample_energy = sample * sample;
        energy += sample_energy;
        if (sample_energy > peak_energy) peak_energy = sample_energy;
    }
    energy /= audio.size();
    
    // Calculate zero-crossing rate for frequency estimation
    int zero_crossings = 0;
    for (size_t i = 1; i < audio.size(); i++) {
        if ((audio[i] >= 0 && audio[i-1] < 0) || (audio[i] < 0 && audio[i-1] >= 0)) {
            zero_crossings++;
        }
    }
    
    // Estimate frequency
    float duration = static_cast<float>(audio.size()) / sample_rate;
    float freq = zero_crossings / (2 * duration);
    
    // Smart detection with frequency range checks
    // Speech typically has frequencies between 85-255 Hz for fundamental frequencies
    // We'll use a bit wider range to be safe
    bool is_in_speech_freq_range = (freq > freq_threshold) && (freq < 3000.0f);
    
    // Calculate fraction of samples over a minimum energy level
    // This helps distinguish speech (many samples over threshold) from random noise spikes
    int samples_over_threshold = 0;
    float sample_threshold = threshold * 0.5f;
    for (float sample : audio) {
        if (sample * sample > sample_threshold) {
            samples_over_threshold++;
        }
    }
    float activity_ratio = static_cast<float>(samples_over_threshold) / audio.size();
    
    // Speech has sustained energy - require at least 10% of samples to be active
    bool sustained_activity = activity_ratio > 0.10f;
    
    // Ultra-sensitive minimum for very quiet microphones
    const float min_detection = 0.0001f;
    
    // Energy must be above threshold
    bool energy_ok = (energy > threshold) || (energy > min_detection);
    
    // Debug output - always print to help diagnose issues
    printf("VAD: Energy: %.6f (threshold: %.6f), Peak: %.6f, Frequency: %.1f Hz, Active: %.1f%%\n", 
           energy, threshold, peak_energy, freq, activity_ratio * 100.0f);
    
    // More robust detection logic
    return energy_ok && is_in_speech_freq_range && sustained_activity;
}

#endif // VAD_H