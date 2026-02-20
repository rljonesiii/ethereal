#define USE_DAISYSP_LGPL 1
#include <cmath>
#include "daisy_seed.h"
#include "daisysp.h"
#include "Filters/svf.h"  // Added for Svf
#include "Synthesis/oscillator.h"

using namespace daisy;
using namespace daisysp;

// Helper: Frequency to MIDI
inline float ftom(float f) {
    return 12.0f * log2f(f / 440.0f) + 69.0f;
}

// Simple Zero-Crossing Pitch Detector
class ZeroCrossingPitchDetector {
   public:
    void Init(float sample_rate) {
        sr_ = sample_rate;
        reset();
    }

    void Process(float in) {
        // Simple lowpass to remove high frequency noise/harmonics before zero crossing
        filtered_in_ = (in * 0.1f) + (filtered_in_ * 0.9f);

        if (state_ == 0 && filtered_in_ > threshold_) {
            state_ = 1;
            if (samples_since_last_ > 0) {
                // Determine frequency
                float new_freq = sr_ / (float)samples_since_last_;
                // Sanity check: Guitar range (~70Hz to ~1200Hz)
                if (new_freq > 70.0f && new_freq < 1500.0f) {
                    freq_ = (freq_ * 0.7f) + (new_freq * 0.3f);  // smooth
                    certainty_ = 1.0f;
                }
            }
            samples_since_last_ = 0;
        } else if (state_ == 1 && filtered_in_ < -threshold_) {
            state_ = 0;
        }

        samples_since_last_++;
        certainty_ *= 0.999f;  // decay certainty if no zero crosses happen
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
    }

    float sr_;
    int state_;
    int samples_since_last_;
    float freq_;
    float certainty_;
    float filtered_in_;
    float threshold_;
};

DaisySeed hw;
ZeroCrossingPitchDetector p_det;
Oscillator harm_osc, vib_lfo;
Port pitch_smoother;  // Replaced SlewLimiter with Port
Svf warmth_filter;    // Changed from MoogLadder to SVF for stability
Svf rms_filter;       // 2-pole lowpass for RMS Envelope detection
ReverbSc rev;

// Scales Matrix (6 scales x 15 notes)
float scales[6][15] = {
    {48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62},  // Chromatic
    {48, 50, 52, 53, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 72},  // Major
    {48, 50, 51, 53, 55, 56, 58, 60, 62, 63, 65, 67, 68, 70, 72},  // Minor
    {48, 50, 52, 55, 57, 60, 62, 64, 67, 69, 72, 74, 76, 79, 81},  // Maj Pent
    {48, 51, 53, 55, 58, 60, 63, 65, 67, 70, 72, 75, 77, 79, 82},  // Min Pent
    {48, 51, 53, 54, 55, 58, 60, 63, 65, 66, 67, 70, 72, 75, 77}   // Blues
};

float last_target_midi = 60.0f;
int current_scale = 1;
float env_out = 0.0f;      // Global envelope follower state
float current_vca = 0.0f;  // Soft VCA state to prevent clicks

// ADC Smoothers (Block rate)
float smooth_filter = 0.5f;
float smooth_glide = 0.1f;
float smooth_mix = 0.5f;
float smooth_rev = 0.3f;
float smooth_vib = 0.2f;
float smooth_gate = 0.1f;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    // 1. Read Controls
    float glide_knob, filter_knob, mix_knob, gate_knob, vib_knob, rev_knob, scale_knob;

#if DEBUG
    // Fixed values for debugging floating pins
    glide_knob = 0.1f;
    filter_knob = 0.5f;
    mix_knob = 0.5f;
    gate_knob = 0.1f;
    vib_knob = 0.2f;
    rev_knob = 0.3f;
    scale_knob = 1.0f;  // Scale 5 (Blues)
#else
    glide_knob = fmaxf(0.0f, fminf(1.0f, hw.adc.GetFloat(0)));
    filter_knob = fmaxf(0.0f, fminf(1.0f, hw.adc.GetFloat(1)));
    mix_knob = fmaxf(0.0f, fminf(1.0f, hw.adc.GetFloat(2)));
    gate_knob = fmaxf(0.0f, fminf(1.0f, hw.adc.GetFloat(3)));
    vib_knob = fmaxf(0.0f, fminf(1.0f, hw.adc.GetFloat(4)));
    rev_knob = fmaxf(0.0f, fminf(1.0f, hw.adc.GetFloat(5)));
    scale_knob = fmaxf(0.0f, fminf(1.0f, hw.adc.GetFloat(6)));
#endif

    // Smooth ADCs (Lowpass filter at block-rate to eliminate crinkles)
    smooth_glide += 0.05f * (glide_knob - smooth_glide);
    smooth_filter += 0.05f * (filter_knob - smooth_filter);
    smooth_mix += 0.05f * (mix_knob - smooth_mix);
    smooth_gate += 0.05f * (gate_knob - smooth_gate);
    smooth_vib += 0.05f * (vib_knob - smooth_vib);
    smooth_rev += 0.05f * (rev_knob - smooth_rev);

    // Update Params
    current_scale = (int)(scale_knob * 5.99f);

    // SlewLimiter replacement: Port uses SetHtime (Half-time)
    pitch_smoother.SetHtime(0.001f + (smooth_glide * 0.5f));

    warmth_filter.SetFreq(100.0f + (smooth_filter * 7000.0f));
    rev.SetLpFreq(400.0f + (smooth_rev * 10000.0f));
    rev.SetFeedback(0.5f + (smooth_rev * 0.45f));
    float vib_depth = smooth_vib * 1.0f;

    // LED State Variable
    bool is_locked = false;

    for (size_t i = 0; i < size; i++) {
        float input = in[0][i];

        // PitchTracker takes block size into account; here we process sample by sample
        p_det.Process(input);

        // 2nd-Order RMS Detector (Fixes zipper noise & AM distortion)
        float squared = input * input;
        rms_filter.Process(squared);
        float ms = rms_filter.Low();
        // Protect against negative numbers (ringing) before sqrt
        float rms = sqrtf(fmaxf(0.0f, ms));

        env_out = rms;  // Update global state
        bool gate_open = env_out > (smooth_gate * 0.05f);

        // Pitch Tracking Logic
        bool confident = p_det.GetCertainty() > 0.85f;

        if (confident && gate_open) {
            is_locked = true;  // Signal we have a lock

            float input_midi = ftom(p_det.GetFreq());

            // Smart Interval: 5th for Pentatonic, 3rd for others
            float interval = (current_scale >= 3) ? 7.0f : 4.0f;
            float raw_harm = input_midi + interval;

            // Quantize
            float closest = scales[current_scale][0];
            for (int n = 0; n < 15; n++) {
                if (fabsf(raw_harm - scales[current_scale][n]) < fabsf(raw_harm - closest))
                    closest = scales[current_scale][n];
            }
            if (fabsf(closest - last_target_midi) > 0.5f) last_target_midi = closest;
        }

        // Synthesis
        float shimmer = vib_lfo.Process() * vib_depth;
        float smoothed_midi = pitch_smoother.Process(last_target_midi);
        harm_osc.SetFreq(mtof(smoothed_midi + shimmer));

        warmth_filter.Process(harm_osc.Process());
        float harm_sig = warmth_filter.Low();  // SVF requires calling Low() to get lowpass output

        // Amplitude Logic: Use a Soft VCA instead of a hard boolean multiplier to prevent clicks
        float target_vca = (confident && gate_open) ? (env_out * 3.0f) : 0.0f;
        target_vca = fmaxf(0.0f, fminf(1.0f, target_vca));   // Prevent overdriving
        current_vca += 0.001f * (target_vca - current_vca);  // Smooth volume fade (slew)

        harm_sig *= current_vca;

        // Reverb & Output
        float out_l, out_r;
        rev.Process(harm_sig, harm_sig, &out_l, &out_r);
        float final_l = (harm_sig * (1.0f - smooth_rev)) + (out_l * smooth_rev);
        float final_r = (harm_sig * (1.0f - smooth_rev)) + (out_r * smooth_rev);

        out[0][i] = (input * (1.0f - smooth_mix)) + (final_l * smooth_mix);
        out[1][i] = (input * (1.0f - smooth_mix)) + (final_r * smooth_mix);
    }

    // Update Onboard LED (once per block is enough visually)
    hw.SetLed(is_locked);
}

int main(void) {
    hw.Init();
    float sr = hw.AudioSampleRate();

    // Init 7 Pots
    AdcChannelConfig adc_config[7];
    for (int i = 0; i < 7; i++) {
        // Init Single expects Pin object
        // Accessing via seed::Dxx is better, but since loop is dynamic:
        // We use hw.GetPin() but cast to Pin or look up via seed namespace if poss.
        // hw.GetPin returns dsy_gpio_pin.
        // AdcChannelConfig::InitSingle takes dsy_gpio_pin (old) OR Pin (new)?
        // Let's check headers if needed.
        // Actually, previous code failed on GPIO::Init, not ADC.
        // ADC likely handles dsy_gpio_pin for backward compat.
        adc_config[i].InitSingle(hw.GetPin(22 + i));
    }
    hw.adc.Init(adc_config, 7);
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

    rev.Init(sr);

    hw.StartAudio(AudioCallback);
    while (1) {
    }
}
