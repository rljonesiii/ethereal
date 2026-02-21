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

### 2.3 True Dry Signal Pass-Through
**The Problem**: Initially, the noise gate muted both the wet synthesizer and the dry guitar signal to kill breadboard noise, which defeated the purpose of a "dry mix" knob since dead air caused total silence.
**The Fix**: The wet synthesized track has its own distinct VCA driven by the pitch tracker's lock certainty and the noise gate threshold. The raw `input` signal bypasses the gate entirely (`gated_input = input`), allowing the user to seamlessly balance pure, unaffected guitar tone using the Mix knob, even when not playing loud enough to trigger the harmonizer.

### 2.4 Control Parameter Smoothing & Exponential Scaling
**The Problem**: Raw ADC readings from potentiometers constantly fluctuate, causing zipper noise. Furthermore, mapping a linear pot directly to a guitar's RMS volume (which lives mostly between 0.001 and 0.05) makes the gate threshold knob hopelessly over-sensitive.
**The Fix**: Every potentiometer reading is smoothed locally at the block rate (`smooth_filter += 0.05f * (filter_knob - smooth_filter)`). The `Gate Threshold` knob specifically applies an exponential scale (`a^2`) so the first 70% of the knob's rotation is dedicated to micro-voltage adjustments precisely where the guitar's natural decay lives.

### 2.5 Explicit ADC Pin Constants
**The Problem**: Initializing the ADCs using integer index mappings like `hw.GetPin(22)` referred to the wrong internal GPIO pins, leaving the ADCs floating.
**The Fix**: Replaced integer initialization with explicit hardware Pin constants (`seed::A0`, etc.) ensuring accurately mapped hardware potentiometers.

### 2.6 Cortex-M7 Denormal Protection
**The Problem**: When the guitar signal degrades to absolute silence, State Variable Filters (SVFs) and infinite-impulse recursive multipliers fall into the `1e-38` subnormal floating-point range. The Daisy's Cortex-M7 CPU lacks hardware FPU flushing flags by default, causing the DSP loop to choke on the subnormal math and completely hang the board.
**The Fix**: Tiny, mathematically inaudible DC offsets (`1e-9f`) were directly injected into the inputs of both SVF filters (`rms_filter` and `warmth_filter`). Additionally, manual algebraic bounds (`if (val < 1e-6f) val = 0.0f`) were added to the pitch tracker certainty scalars, preventing any variable from crossing into the subnormal death-zone.

### 2.7 Robust Zero-Crossing Pitch Tracking
The `ZeroCrossingPitchDetector` was entirely broken due to three compounding math failures:

* **Half-Cycle Period Bug**: The detector was triggering the period calculation on any edge change (both rising and falling edges). This caused the algorithm to measure the distance of a half-cycle of the wave, resulting in the detector always outputting a pitch exactly one octave higher than the actual note. The state machine was fixed to only measure full periods (rising-edge to rising-edge).
* **High-Frequency Transient Misfires**: The detector had no awareness of the physical limits of a guitar string. A pick attack generates intense, high-frequency "hash" that caused the threshold to be crossed dozens of times per millisecond. This drove the `samples_since_last_` counter down to < 5 samples, resulting in perceived frequencies over 10,000Hz. Since 10kHz was clamped out by the sanity checks, the detector essentially "turned off" during pick attacks. A 30-sample hold-off timer was implemented to reject any crossing that occurs faster than ~1500Hz.
* **DC Offset Blocking**: To prevent the ADC's resting voltage from forcing the signal above the zero-crossing threshold permanently (causing a total freeze), a 1-pole Highpass Filter (DC Blocker) was added immediately preceding the edge detection.

### 2.8 Noise Gate Schmitt Trigger (Hysteresis)
**The Problem**: As a guitar string decays, its volume naturally flutters across the exact value of the noise gate threshold, causing the VCA to stutter open and closed repeatedly (chatter).
**The Fix**: The algorithm utilizes Schmitt Trigger logic where the turn-on threshold is strictly double the turn-off threshold. Once a string opens the gate, the harmony stays locked in until the volume plummets to half that value. Furthermore, the VCA uses decoupled slews (fast attack, ultra-slow release) so that when the gate eventually does close, the harmony acts like an acoustic compressor fading out rather than abruptly snapping to silence.
