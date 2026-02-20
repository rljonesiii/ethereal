#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

// Helper: Frequency to MIDI
inline float ftom(float f) { return 12.0f * log2f(f / 440.0f) + 69.0f; }

// Mock PitchDetector (User must implement or include library)
class PitchDetector {
public:
    void Init(float sample_rate) {}
    void Process(float in) {}
    float GetFreq() { return 440.0f; } // Return A440 for now
};

DaisySeed hw;
PitchDetector p_det;
Oscillator    harm_osc;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    for (size_t i = 0; i < size; i++) {
        float input = in[0][i];

        // 1. Detect input pitch
        p_det.Process(input);
        float detected_freq = p_det.GetFreq();

        // 2. Convert to MIDI
        float input_midi = ftom(detected_freq);

        // 3. Add harmony (e.g., +7 semitones for a Perfect Fifth / "Sol")
        float harm_midi = input_midi + 7.0f;

        // 4. Set oscillator to harmony frequency
        harm_osc.SetFreq(mtof(harm_midi));

        // 5. Mix: 50% Dry Guitar, 50% Harmony synth
        float harmony_sig = harm_osc.Process();
        out[0][i] = out[1][i] = (input * 0.5f) + (harmony_sig * 0.5f);
    }
}

int main(void) {
    hw.Init();
    float sample_rate = hw.AudioSampleRate();

    // Init Pitch Detector
    p_det.Init(sample_rate);

    // Init Oscillator (using a Sine for a cleaner "organ" like harmony)
    harm_osc.Init(sample_rate);
    harm_osc.SetWaveform(Oscillator::WAVE_SIN);
    harm_osc.SetAmp(0.5f);

    hw.StartAudio(AudioCallback);
    while(1) {}
}
