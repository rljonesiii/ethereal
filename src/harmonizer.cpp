#define USE_DAISYSP_LGPL 1
#include <cmath>
#include "daisy_seed.h"
#include "daisysp.h"
#include "Filters/svf.h"  // Added for Svf
#include "Synthesis/oscillator.h"

using namespace daisy;
using namespace daisysp;

/**
 * @brief Converts a frequency in Hertz to its corresponding fractional MIDI note number.
 *
 * This uses the standard logarithmic scale mapping where A4 = 440.0Hz = MIDI note 69.
 * The formula calculates how many semitones (the 12.0f multiplier) the input frequency `f`
 * is away from 440Hz, and offsets it by 69.
 *
 * @param f The fundamental frequency in Hertz (e.g., from the pitch detector).
 * @return float The continuous, unquantized MIDI note value (e.g., 60.5 for a note exactly between C4 and C#4).
 */
inline float ftom(float f) {
    return 12.0f * log2f(f / 440.0f) + 69.0f;
}

/**
 * @class ZeroCrossingPitchDetector
 * @brief A lightweight Pitch Detector optimized for Monophonic Electric Guitar.
 *
 * This detector uses a low-pass filter to strip away harsh harmonics and string
 * noise, then measures the time (in samples) between zero-crossings to estimate
 * the fundamental frequency. It includes a sanity check to ignore frequencies outside
 * the standard guitar range (~70Hz - 1500Hz).
 */
class ZeroCrossingPitchDetector {
   public:
    void Init(float sample_rate) {
        sr_ = sample_rate;
        reset();
    }

    void Process(float in) {
        // DC Blocker (Highpass) to prevent getting stuck on ADC offsets
        // Standard 1-pole HPF equation is: y[i] = x[i] - x[i-1] + R * y[i-1]
        dc_block_ = in - prev_in_ + (0.995f * dc_block_);
        prev_in_ = in;
        if (fabsf(dc_block_) < 1e-6f) dc_block_ = 0.0f;

        // Simple lowpass to remove high frequency noise/harmonics before zero crossing
        filtered_in_ = (dc_block_ * 0.1f) + (filtered_in_ * 0.9f);
        if (fabsf(filtered_in_) < 1e-6f) filtered_in_ = 0.0f;  // Denormal protection

        // Look for actual zero crossings, with a distinct hysteresis gap to reject noise.
        // We use +/- 0.002f to ensure we only trigger on actual signal, not floor hiss.
        if (state_ == 0 && filtered_in_ > 0.002f) {
            state_ = 1;
            // Hold-off Timer: The highest normal note on a guitar is ~1200Hz.
            // At 48kHz, a full period is at least 40 samples. This filters out
            // any false positive crossings from high-frequency pick transients.
            if (samples_since_last_ > 30) {
                // Determine frequency based on one full period (rising edge to rising edge)
                float new_freq = sr_ / (float)samples_since_last_;
                // Sanity check: Guitar range (~60Hz to ~1500Hz)
                if (new_freq > 60.0f && new_freq < 1500.0f) {
                    freq_ = (freq_ * 0.7f) + (new_freq * 0.3f);  // smooth
                    certainty_ = 1.0f;
                }
                // Reset the period counter *only* if the wavelength was long enough to be a string
                samples_since_last_ = 0;
            }
        } else if (state_ == 1 && filtered_in_ < -0.002f) {
            state_ = 0;
        }

        samples_since_last_++;
        certainty_ *= 0.99995f;                     // decay certainty smoothly (softened for transients)
        if (certainty_ < 1e-6f) certainty_ = 0.0f;  // Denormal protection
    }

    float GetFreq() {
        return freq_;
    }
    float GetCertainty() {
        return certainty_;
    }

   private:
    void reset() {
        state_ = 0;
        samples_since_last_ = 0;
        freq_ = 440.0f;
        certainty_ = 0.0f;
        filtered_in_ = 0.0f;
        threshold_ = 0.01f;
        dc_block_ = 0.0f;
        prev_in_ = 0.0f;
    }

    float sr_;
    int state_;
    int samples_since_last_;
    float freq_;
    float certainty_;
    float filtered_in_;
    float threshold_;
    float dc_block_;
    float prev_in_;
};

// Global Variables
DaisySeed hw;
ZeroCrossingPitchDetector p_det;
Oscillator harm_osc, vib_lfo;
Port pitch_smoother;  // Replaced SlewLimiter with Port
Svf warmth_filter;    // Changed from MoogLadder to SVF for stability
Svf rms_filter;       // 2-pole lowpass for RMS Envelope detection

/**
 * @brief Pitch Quantization Matrices (6 Scales x 15 Notes)
 *
 * This 2D array defines the exact MIDI pitches allowed for the synthesized harmony.
 * - Dimension 1 (6): The user-selectable scale type (Chromatic, Major, Minor, Pentatonic, etc.).
 * - Dimension 2 (15): The available notes in that scale.
 *
 * Why start at C2 (MIDI 36)?
 * Standard guitar tuning bottoms out at E2 (MIDI Note 40). By starting our quantization
 * matrix at C2 (MIDI 36), we guarantee that any fundamental played on the lowest guitar strings
 * will have a valid destination note in the array to snap to.
 *
 * Why 15 notes?
 * A standard diatonic scale spans 1 octave in 7 notes. Our zero-crossing pitch detector allows
 * fundamentals up to ~1500Hz. The DSP logic adds an interval (like a 3rd or 5th) on top of the
 * detected fundamental *before* quantization. By defining 15 notes, we span over two full
 * octaves. This guarantees that even if the user plays the highest allowed pitch and adds a 5th,
 * the quantization loop (`for (int n = 0; n < 15; n++)`) will always find a valid target note
 * without throwing out-of-bounds errors or wrapping around to a low bass note.
 */
float scales[6][15] = {
    {36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50},  // Chromatic
    {36, 38, 40, 41, 43, 45, 47, 48, 50, 52, 53, 55, 57, 59, 60},  // Major
    {36, 38, 39, 41, 43, 44, 46, 48, 50, 51, 53, 55, 56, 58, 60},  // Minor
    {36, 38, 40, 43, 45, 48, 50, 52, 55, 57, 60, 62, 64, 67, 69},  // Maj Pent
    {36, 39, 41, 43, 46, 48, 51, 53, 55, 58, 60, 63, 65, 67, 70},  // Min Pent
    {36, 39, 41, 42, 43, 46, 48, 51, 53, 54, 55, 58, 60, 63, 65}   // Blues
};

float last_target_midi = 60.0f;
int current_scale = 1;
// Global Envelope and VCA States
float env_out = 0.0f;       // Smoothed RMS output representing the current guitar volume
float current_vca = 0.0f;   // Slew-limited scalar applied to the output to prevent popping
float current_gate = 0.0f;  // Slew-limited noise gate for the dry signal (0.0 to 1.0)

/**
 * ADC Smoothers (Block rate)
 * Raw ADC pots are extremely noisy. If their values jump around while mapped to
 * a filter cutoff, it creates audible "crackle" or static. We apply a 1-pole
 * lowpass filter to these values once per audio block to ensure the parameters
 * "glide" smoothly to their new positions.
 */
float smooth_filter = 0.5f;
float smooth_glide = 0.1f;
float smooth_mix = 0.5f;
float smooth_vib = 0.2f;
float smooth_gate = 0.1f;

static void AudioCallback(daisy::AudioHandle::InputBuffer in, daisy::AudioHandle::OutputBuffer out, size_t size) {
    // 1. Hardware Interface & Control Routing
    // The ADC pins stream floating-point values from 0.0 (GND) to 1.0 (3.3V).
    // These are mapped directly to DSP parameters as follows:
    // Pot 0 = Glide: Portamento time between notes (0ms to ~500ms slew rate)
    // Pot 1 = Filter: Cutoff frequency of the SVF filter on the harmonized synth (100Hz - 7.1kHz)
    // Pot 2 = Mix: Dry/Wet split mixing the gated clean guitar (0.0) with the new harmony (1.0)
    // Pot 3 = Gate Threshold: The minimum guitar volume (RMS envelope) needed to break the VCA open
    // Pot 4 = Vibrato: Depth of the 6Hz sine LFO applied to the harmony pitch
    // Pot 5 = Scale Selection: Snaps the 0-1 float to one of 6 matrix indices (Chromatic -> Blues)
    float glide_knob, filter_knob, mix_knob, gate_knob, vib_knob, scale_knob;

#if DEBUG
    // Fixed values for debugging floating pins
    glide_knob = 0.1f;
    filter_knob = 0.5f;
    mix_knob = 0.5f;
    gate_knob = 0.1f;
    vib_knob = 0.2f;
    scale_knob = 1.0f;  // Blues scale
#else
    glide_knob = fmaxf(0.0f, fminf(1.0f, hw.adc.GetFloat(0)));
    filter_knob = fmaxf(0.0f, fminf(1.0f, hw.adc.GetFloat(1)));
    mix_knob = fmaxf(0.0f, fminf(1.0f, hw.adc.GetFloat(2)));
    gate_knob = fmaxf(0.0f, fminf(1.0f, hw.adc.GetFloat(3)));
    vib_knob = fmaxf(0.0f, fminf(1.0f, hw.adc.GetFloat(4)));
    scale_knob = fmaxf(0.0f, fminf(1.0f, hw.adc.GetFloat(5)));
#endif

    // Smooth ADCs (Lowpass filter at block-rate to eliminate crinkles)
    smooth_glide += 0.05f * (glide_knob - smooth_glide);
    smooth_filter += 0.05f * (filter_knob - smooth_filter);
    smooth_mix += 0.05f * (mix_knob - smooth_mix);
    smooth_gate += 0.05f * (gate_knob - smooth_gate);
    smooth_vib += 0.05f * (vib_knob - smooth_vib);

    // Update Params
    current_scale = (int)(scale_knob * 5.99f);

    // SlewLimiter replacement: Port uses SetHtime (Half-time)
    pitch_smoother.SetHtime(0.001f + (smooth_glide * 0.5f));

    warmth_filter.SetFreq(100.0f + (smooth_filter * 7000.0f));
    float vib_depth = smooth_vib * 1.0f;

    // LED State Variable
    bool is_locked = false;

    for (size_t i = 0; i < size; i++) {
        float input = in[0][i];

        // 1. Pitch Tracking
        // Process the incoming signal sample-by-sample to constantly update pitch estimates.
        p_det.Process(input);

        // 2. Amplitude Envelope Detection (2nd-Order RMS)
        // A simple `fabsf(input)` envelope follower causes Amplitude Modulation (AM) Distortion
        // (audible as harsh "zipper noise") because it tracks the audio wave up and down too fast.
        // Squaring the signal and passing it through a 2-pole lowpass filter (SVF) at ~50Hz
        // yields a perfectly smooth, ripple-free envelope.
        // Add 1e-9f DC offset to prevent the SVF filter from crashing the Cortex-M7 with denormals
        float squared = (input * input) + 1e-9f;
        rms_filter.Process(squared);
        float ms = rms_filter.Low();

        // Protect against the SVF filter ringing below zero before taking the square root (NaN prevention)
        float rms = sqrtf(fmaxf(0.0f, ms - 1e-9f));

        env_out = rms;  // Store in global state for the VCA

        // Gate is open if the guitar envelope exceeds the user-defined threshold pot
        // We use an exponential curve (squaring) on the knob because RMS values from a guitar
        // live mostly in the 0.001 - 0.05 range. A linear pot wastes 90% of its turn.
        // We add Hysteresis (Schmitt Trigger logic) to prevent the gate from stuttering rapidly
        // when the decaying guitar string hovers right around the threshold line.
        float gate_thresh_on = (smooth_gate * smooth_gate) * 0.05f;
        float gate_thresh_off = gate_thresh_on * 0.5f;  // Must fall to half volume to close

        static bool gate_open = false;
        if (!gate_open && env_out > gate_thresh_on) {
            gate_open = true;
        } else if (gate_open && env_out < gate_thresh_off) {
            gate_open = false;
        }

        // Pitch Tracking Logic
        bool confident = p_det.GetCertainty() > 0.85f;

        if (confident && gate_open) {
            is_locked = true;  // Signal we have a lock

            float input_midi = ftom(p_det.GetFreq());

            // Smart Interval: 5th for Pentatonic, 3rd for others
            float interval = (current_scale >= 3) ? 7.0f : 4.0f;
            float raw_harm = input_midi + interval;

            // Quantize (Search across 4 overlapping octaves to cover the whole fretboard)
            float closest = scales[current_scale][0];
            float min_diff = 1000.0f;
            for (int oct = 0; oct < 5; oct++) {
                for (int n = 0; n < 15; n++) {
                    float candidate = scales[current_scale][n] + (oct * 12.0f);
                    float diff = fabsf(raw_harm - candidate);
                    if (diff < min_diff) {
                        min_diff = diff;
                        closest = candidate;
                    }
                }
            }
            if (fabsf(closest - last_target_midi) > 0.5f) last_target_midi = closest;
        }

        // Synthesis
        float shimmer = vib_lfo.Process() * vib_depth;
        float smoothed_midi = pitch_smoother.Process(last_target_midi);
        harm_osc.SetFreq(mtof(smoothed_midi + shimmer));

        // 4. Output Processing & Voltage Controlled Amplifier (VCA)
        float osc_out = harm_osc.Process();
        warmth_filter.Process(osc_out + 1e-9f);        // Add 1e-9f DC offset to prevent SVF denormals
        float harm_sig = warmth_filter.Low() - 1e-9f;  // SVF requires calling Low() to get lowpass output

        // Soft VCA Logic
        // The harmony volume scales dynamically with the guitar's RMS envelope, boosted by a soft curve
        float dyn_vol = env_out * 4.0f;
        float target_vca = (confident && gate_open) ? dyn_vol : 0.0f;
        target_vca = fmaxf(0.0f, fminf(1.0f, target_vca));  // Hard clamp to prevent digital clipping

        // Independent Attack/Release Slew Limiter
        // If the noise gate opens, we attack fast. If the noise gate closes, we decay very slowly
        // to prevent the harmony from abruptly chopping off mid-sustain.
        float slew_rate = (target_vca > current_vca) ? 0.01f : 0.0001f;
        current_vca += slew_rate * (target_vca - current_vca);
        if (fabsf(current_vca - target_vca) < 1e-6f) current_vca = target_vca;  // Denormal protection

        // Apply the VCAs
        harm_sig *= current_vca;
        float gated_input = input;  // True pure dry signal pass-through

        // 5. Final Mix (Dual Mono)
        out[0][i] = (gated_input * (1.0f - smooth_mix)) + (harm_sig * smooth_mix);
        out[1][i] = out[0][i];
    }

    // Update Onboard LED (once per block is enough visually)
    hw.SetLed(is_locked);
}

int main(void) {
    // Hardware Initialization
    hw.Init();
    float sr = hw.AudioSampleRate();

    // ADC Initialization
    // We explicitly use the libDaisy pin constants (seed::A0 to seed::A5).
    // Using simple integer indices like `hw.GetPin(22)` maps to internal system GPIO pins,
    // leaving the ADCs floating (reading 0.0) which causes the DSP to output pure dry signal
    // and muffle the filter cutout completely.
    Pin adc_pins[6] = {seed::A0, seed::A1, seed::A2, seed::A3, seed::A4, seed::A5};
    AdcChannelConfig adc_config[6];
    for (int i = 0; i < 6; i++) {
        adc_config[i].InitSingle(adc_pins[i]);
    }
    hw.adc.Init(adc_config, 6);
    hw.adc.Start();

    // DSP Init
    p_det.Init(sr);
    harm_osc.Init(sr);
    harm_osc.SetWaveform(Oscillator::WAVE_SQUARE);  // Try TRI or SAW for different flavors
    vib_lfo.Init(sr);
    vib_lfo.SetFreq(6.0f);
    vib_lfo.SetWaveform(Oscillator::WAVE_SIN);

    // Slew -> Port
    pitch_smoother.Init(sr, 0.01f);

    warmth_filter.Init(sr);
    warmth_filter.SetRes(0.3f);

    // RMS Filter (2-pole LP at 50Hz for 20ms window)
    rms_filter.Init(sr);
    rms_filter.SetFreq(50.0f);
    rms_filter.SetRes(0.0f);  // Critically damped

    hw.StartAudio(AudioCallback);
    while (1) {
    }
}
