#!/usr/bin/env python3
"""
Generate audio feedback sounds for Clarity Layer.

Creates simple synthesized WAV files for UI feedback sounds.
Run this script to populate the assets/sounds/ directory.
"""

import wave
import struct
import math
import os
from pathlib import Path

SAMPLE_RATE = 44100
AMPLITUDE = 0.5  # 0.0 - 1.0


def generate_sine_wave(frequency, duration, amplitude=AMPLITUDE):
    """Generate a sine wave."""
    num_samples = int(SAMPLE_RATE * duration)
    samples = []
    for i in range(num_samples):
        t = i / SAMPLE_RATE
        sample = amplitude * math.sin(2 * math.pi * frequency * t)
        samples.append(sample)
    return samples


def generate_frequency_sweep(start_freq, end_freq, duration, amplitude=AMPLITUDE):
    """Generate a frequency sweep (rising or falling tone)."""
    num_samples = int(SAMPLE_RATE * duration)
    samples = []
    for i in range(num_samples):
        t = i / num_samples
        # Linear interpolation between frequencies
        frequency = start_freq + (end_freq - start_freq) * t
        sample_t = i / SAMPLE_RATE
        sample = amplitude * math.sin(2 * math.pi * frequency * sample_t)
        samples.append(sample)
    return samples


def generate_chord(frequencies, duration, amplitude=AMPLITUDE):
    """Generate a chord (multiple frequencies mixed)."""
    num_samples = int(SAMPLE_RATE * duration)
    samples = [0.0] * num_samples
    amp_per_freq = amplitude / len(frequencies)

    for freq in frequencies:
        for i in range(num_samples):
            t = i / SAMPLE_RATE
            samples[i] += amp_per_freq * math.sin(2 * math.pi * freq * t)

    return samples


def apply_envelope(samples, attack=0.01, decay=0.05, sustain=0.8, release=0.1):
    """Apply ADSR envelope to samples."""
    num_samples = len(samples)
    attack_samples = int(attack * SAMPLE_RATE)
    decay_samples = int(decay * SAMPLE_RATE)
    release_samples = int(release * SAMPLE_RATE)
    sustain_samples = num_samples - attack_samples - decay_samples - release_samples

    if sustain_samples < 0:
        # Short sound, just apply linear fade
        for i in range(num_samples):
            t = i / num_samples
            if t < 0.1:
                samples[i] *= t / 0.1
            elif t > 0.9:
                samples[i] *= (1 - t) / 0.1
        return samples

    for i in range(num_samples):
        if i < attack_samples:
            # Attack: ramp up
            envelope = i / attack_samples
        elif i < attack_samples + decay_samples:
            # Decay: ramp down to sustain level
            decay_progress = (i - attack_samples) / decay_samples
            envelope = 1.0 - (1.0 - sustain) * decay_progress
        elif i < attack_samples + decay_samples + sustain_samples:
            # Sustain
            envelope = sustain
        else:
            # Release: ramp down to 0
            release_progress = (i - attack_samples - decay_samples - sustain_samples) / release_samples
            envelope = sustain * (1 - release_progress)

        samples[i] *= envelope

    return samples


def save_wav(filename, samples):
    """Save samples as 16-bit mono WAV file."""
    with wave.open(filename, 'w') as wav_file:
        wav_file.setnchannels(1)  # Mono
        wav_file.setsampwidth(2)  # 16-bit
        wav_file.setframerate(SAMPLE_RATE)

        # Convert float samples to 16-bit integers
        for sample in samples:
            # Clamp to [-1, 1]
            sample = max(-1.0, min(1.0, sample))
            # Convert to 16-bit integer
            int_sample = int(sample * 32767)
            wav_file.writeframes(struct.pack('<h', int_sample))

    print(f"Created: {filename}")


def main():
    # Get output directory
    script_dir = Path(__file__).parent
    output_dir = script_dir.parent / "assets" / "sounds"
    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"Generating sounds in: {output_dir}")
    print()

    # Enable sound - rising tone (440Hz to 880Hz, 200ms)
    samples = generate_frequency_sweep(440, 880, 0.2)
    samples = apply_envelope(samples, attack=0.01, decay=0.02, sustain=0.7, release=0.05)
    save_wav(str(output_dir / "enable.wav"), samples)

    # Disable sound - falling tone (880Hz to 440Hz, 200ms)
    samples = generate_frequency_sweep(880, 440, 0.2)
    samples = apply_envelope(samples, attack=0.01, decay=0.02, sustain=0.7, release=0.05)
    save_wav(str(output_dir / "disable.wav"), samples)

    # Zoom in - quick rising (600Hz to 900Hz, 150ms)
    samples = generate_frequency_sweep(600, 900, 0.15)
    samples = apply_envelope(samples, attack=0.005, decay=0.01, sustain=0.8, release=0.03)
    save_wav(str(output_dir / "zoom_in.wav"), samples)

    # Zoom out - quick falling (900Hz to 600Hz, 150ms)
    samples = generate_frequency_sweep(900, 600, 0.15)
    samples = apply_envelope(samples, attack=0.005, decay=0.01, sustain=0.8, release=0.03)
    save_wav(str(output_dir / "zoom_out.wav"), samples)

    # Profile switch - pleasant chord (C-E-G major chord, 300ms)
    samples = generate_chord([523.25, 659.25, 783.99], 0.3)  # C5, E5, G5
    samples = apply_envelope(samples, attack=0.02, decay=0.05, sustain=0.6, release=0.1)
    save_wav(str(output_dir / "profile.wav"), samples)

    # Speak start - soft blip (500Hz, 100ms)
    samples = generate_sine_wave(500, 0.1, 0.4)
    samples = apply_envelope(samples, attack=0.005, decay=0.01, sustain=0.8, release=0.02)
    save_wav(str(output_dir / "speak_start.wav"), samples)

    # Speak stop - lower blip (400Hz, 100ms)
    samples = generate_sine_wave(400, 0.1, 0.4)
    samples = apply_envelope(samples, attack=0.005, decay=0.01, sustain=0.8, release=0.02)
    save_wav(str(output_dir / "speak_stop.wav"), samples)

    # Panic/emergency - descending three-note (800-600-400Hz, 500ms total)
    samples1 = generate_sine_wave(800, 0.15, 0.6)
    samples1 = apply_envelope(samples1, attack=0.01, decay=0.01, sustain=0.9, release=0.01)
    samples2 = generate_sine_wave(600, 0.15, 0.6)
    samples2 = apply_envelope(samples2, attack=0.01, decay=0.01, sustain=0.9, release=0.01)
    samples3 = generate_sine_wave(400, 0.2, 0.6)
    samples3 = apply_envelope(samples3, attack=0.01, decay=0.01, sustain=0.7, release=0.05)
    samples = samples1 + samples2 + samples3
    save_wav(str(output_dir / "panic.wav"), samples)

    # Error - low buzz (200Hz, 200ms)
    samples = generate_sine_wave(200, 0.2, 0.5)
    samples = apply_envelope(samples, attack=0.01, decay=0.02, sustain=0.8, release=0.03)
    save_wav(str(output_dir / "error.wav"), samples)

    # Click - short high blip (1000Hz, 50ms)
    samples = generate_sine_wave(1000, 0.05, 0.3)
    samples = apply_envelope(samples, attack=0.002, decay=0.005, sustain=0.8, release=0.01)
    save_wav(str(output_dir / "click.wav"), samples)

    # Focus - very soft tone (700Hz, 80ms)
    samples = generate_sine_wave(700, 0.08, 0.25)
    samples = apply_envelope(samples, attack=0.005, decay=0.01, sustain=0.7, release=0.02)
    save_wav(str(output_dir / "focus.wav"), samples)

    print()
    print("All sounds generated successfully!")
    print(f"Files are in: {output_dir}")


if __name__ == "__main__":
    main()
