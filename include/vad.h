#ifndef VAD_H
#define VAD_H

#include <vector>
#include <cmath>
#include <algorithm>

// Simple voice activity detection function based on signal energy and frequency analysis
// Returns true if speech is detected in the audio buffer
inline bool detect_voice_activity(const std::vector<float>& audio, int sample_rate, float threshold, float freq_threshold) {
    if (audio.empty()) {
        return false;
    }
    
    // Calculate energy of the signal
    float energy = 0;
    for (float sample : audio) {
        energy += sample * sample;
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
    
    // Debug output
    // printf("Energy: %.6f, Frequency: %.1f Hz\n", energy, freq);
    
    // Return true if energy is above threshold and frequency is reasonable for speech
    return energy > threshold && freq > freq_threshold;
}

#endif // VAD_H