# Ethereal Harmonizer 🎸✨

A high-fidelity, zero-latency, multi-scale guitar pitch tracker and interval harmonizer built for the Electro-Smith Daisy Seed.

Ethereal is designed to bring robust, artifact-free intelligent harmony to the Daisy Seed platform. By employing advanced signal processing techniques like Cortex-M7 floating-point denormal flushing and custom Schmitt-Triggering, Ethereal tracks your guitar's exact fundamental pitch and instantly synthesizes diatonic or pentatonic harmonies without the dreaded tracking glitches, zipper-noise, or subnormal CPU crashes common to embedded guitar DSP.

## Features

* **Intelligent Pitch Interval Tracking**: Dynamically synthesizes intervals (3rds or 5ths) based on the current musical scale.
* **4-Octave Quantization**: Intelligently locks synthesized harmonies to the closest in-key interval across a massive 4-octave range.
* **True Analog-Style Dry Bypass**: The pure instrument signal passes completely unaffected by the DSP gating when mixed.
* **Smart Noise Gate Hysteresis**: Decoupled VCA attack/release and Schmitt Triggering allows the harmony to tail off organically like an acoustic compressor.
* **High-Frequency Transient Rejection**: Ignores rapid pick-attacks to prevent the pitch detector from octave-jumping or choking.

## Documentation

Comprehensive documentation on building, wiring, and understanding the DSP algorithms powering the pedal:

* [**Hardware Setup & Wiring Table**](docs/hardware_setup.md): Complete pinout guide, breadboard wiring, and AC-coupling instructions for hooking up the Daisy Seed.
* [**Noise Mitigation & DSP Algorithms**](docs/noise_mitigation.md): An in-depth look into the custom DSP fixes, including the DC blockers, SVF envelope trackers, and FPU denormal logic.

## Source Code

The primary DSP engine and audio routing logic for the Ethereal Harmonizer is located entirely within a single source file:
* [**`src/harmonizer.cpp`**](src/harmonizer.cpp)

## Harmony Generation Logic

When the pedal detects a solid pitch and opens the noise gate, it simultaneously synthesizes a secondary harmony note to accompany the fundamental guitar tone. This harmony is not fixed; it intelligently adapts based on the user-selected musical **Scale**.

The generated interval is derived directly from the fundamental note's detected pitch before being mathematically forced (quantized) into the nearest legal scale degree.

* **Major & Minor Scales (Diatonic)**: The pedal defaults to adding a **Major 3rd (+4 semitones)** on top of the original note. Because this new raw pitch is immediately forced through the quantization matrix, the 3rd will automatically flatten into a Minor 3rd when musically appropriate for the key.
* **Pentatonic & Blues Scales**: In these sparser scales, a 3rd is less desirable as it often clashes or resolves poorly. When the scale knob is set to Pentatonic or Blues (Scale type $\ge$ 3), the pedal switches to adding a **Perfect 5th (+7 semitones)** to create powerful, open sounding harmonies heavily used in rock.

## Building & Flashing

Ethereal is built using the standard `libDaisy` toolchain. 

```bash
# Compile the firmware
make

# Flash via USB DFU to the Daisy Seed
make program-dfu
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Author

* **Robert Jones** - [rljonesiii](https://github.com/rljonesiii)

## Citation

If you use this library in your research, please cite:

```bibtex
@misc{jones2026ethereal,
  author = {Jones, Robert},
  title = {Ethereal: A high-fidelity, zero-latency, multi-scale guitar pitch tracker and interval harmonizer},
  year = {2026},
  publisher = {GitHub},
  journal = {GitHub repository},
  howpublished = {\url{https://github.com/rljonesiii/ethereal}}
}
```