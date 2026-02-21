# Daisy Seed Harmonizer: Noise Mitigation Strategies

This document details the specific hardware and software strategies implemented to eliminate background noise, ground loop hum, and DSP artifacts when building guitar effects on the Daisy Seed.

## 1. Hardware Enhancements

### 1.1 Impedance Matching & Input Buffer (TL072/OPA2134)
**The Problem**: A raw electric guitar outputs a very weak, high-impedance (~500kΩ) signal. Connecting this directly to the Daisy Seed's low-impedance (~30kΩ) ADC causes severe "tone suck" (loss of high frequencies) and a drastically low Signal-to-Noise Ratio (SNR), resulting in deafening digital background noise.
**The Fix**: Implemented an active, non-inverting unity-gain buffer using an audiophile Op-Amp (TL072 or OPA2134).
*   **1MΩ Pull-down Resistor**: Placed at the input to set a massive input impedance, preserving guitar dynamics and treble.
*   **Virtual Ground (Vb)**: A voltage divider (two 100kΩ resistors) sets the op-amp bias at exactly 4.5V (half of a 9V supply) to allow the AC audio signal to swing cleanly.
*   **AC Coupling Caps**: 1µF Film capacitors are placed at the input (before the op-amp) and the output (before the Daisy Seed) to block all DC bias voltage, ensuring only pure AC audio passes.

### 1.2 "Star Grounding" to Eliminate Ground Loops
**The Problem**: The Daisy Seed contains internal resistance separating its Analog Ground (AGND - Pin 20) and Digital Ground (DGND - Pin 40). "Daisy-chaining" ground wires across the breadboard allows noisy digital return currents from the processor to flow through the sensitive audio ground paths, inducing aggressive 60Hz hum and switching noise.
**The Fix**: All ground connections are wired in a "Star Topology" back to a single central point.
*   The main 9V Power Supply ground goes to this central point.
*   AGND (Pin 20) and DGND (Pin 40) run on *independent* wires directly to this central point.
*   Input/Output Audio Jacks and Op-Amp ground run directly to this central point.

### 1.3 Power Supply Filtering
**The Problem**: Standard 9V wall adapters ripple and inject noise into the circuit.
**The Fix**: Placed a large **100µF Electrolytic Capacitor** across the incoming +9V and Ground rails to act as a bulk reservoir, smoothing out low-frequency 50/60Hz hum. Placed a tiny **0.1µF (100nF) Film Capacitor** in parallel as close to the op-amp as possible to short-circuit high-frequency digital switching noise to ground.

---

## 2. Software (DSP) Enhancements

### 2.1 2nd-Order RMS Envelope Detection
**The Problem**: Generating a control voltage (CV) for the noise gate using a simple `fabsf(input)` full-wave rectifier causes "Ripple". Because the envelope follower tracks the audio wave up and down at an audio rate, applying this jittery volume to the output causes massive **Amplitude Modulation (AM) Distortion**, often perceived as harsh, metallic "zipper noise".
**The Fix**: Replaced the instantaneous envelope follower with a **2nd-order (2-pole) RMS Detector** using a State Variable Filter (SVF) at 50Hz. This smooths out audio-rate ripples entirely while responding fast enough for musical dynamics.

### 2.2 Soft VCA (Voltage Controlled Amplifier)
**The Problem**: Implementing a "hard boolean gate" (`if (gate_close) out = 0`) causes a massive transient spike/pop if the gate snaps shut exactly when the audio wave is at its peak.
**The Fix**: The noise gate now calculates a target volume scalar (`target_vca`) and uses a low-pass slew limiter (`current_vca += 0.001f * (target_vca - current_vca)`) to smoothly fade the audio out over a few milliseconds, completely eliminating pops and clicks.

### 2.3 Universal Input Gating (Global Noise Gate)
**The Problem**: Initially, the noise gate only muted the *wet, synthesized harmonizer signal*. The raw, dry guitar input was still routed to the Mix knob, bleeding the breadboard trace noise floor continuously.
**The Fix**: The computed `current_vca` scalar (the noise gate envelope) is multiplied against BOTH the wet harmonizer audio and the raw `input` dry audio. When the guitar stops playing, the entire pedal output plunges gracefully into complete silence.

### 2.4 Control Parameter Smoothing
**The Problem**: Raw ADC readings from potentiometers constantly fluctuate due to electrical noise, causing zipper noise when directly modulating filter cutoffs.
**The Fix**: Every potentiometer reading is smoothed locally at the block rate (`smooth_filter += 0.05f * (filter_knob - smooth_filter)`). The actual DSP modules only reference these sloped, smoothed variables, allowing for perfectly clean, analog-style parameter sweeps.

### 2.5 Explicit ADC Pin Constants
**The Problem**: Initializing the ADCs using integer index mappings like `hw.GetPin(22)` referred to the wrong internal GPIO pins, leaving the ADCs floating (reading 0.0V). This bypassed the mix, bypassed effects, and squashed the filter cutoff to 100Hz.
**The Fix**: Replaced integer initialization with explicit hardware Pin constants (`seed::A0`, `seed::A1`, etc.) ensuring perfectly mapped hardware potentiometers.
