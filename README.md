Here is the audio wiring for the Daisy Seed on a breadboard.

### Audio Pinout
The audio pins are located on the **Left Header** (when USB is facing UP), at the bottom.

| Pin Number | Label | Function | [basic.cpp](cci:7://file:///Users/rjones/Desktop/Work/_Bloodhoney/Projects/ethereal/src/harmonizer/basic.cpp:0:0-0:0) Variable |
|---|---|---|---|
| **16** | **IN 1** | Audio Input Left | `in[0]` |
| **17** | **IN 2** | Audio Input Right | `in[1]` (Not used in basic.cpp) |
| **18** | **OUT 1** | Audio Output Left | `out[0]` |
| **19** | **OUT 2** | Audio Output Right | `out[1]` |
| **20** | **AGND** | Analog Ground | Audio Jack Sleeves |

### Wiring Diagram (Breadboard)
Connect your 3.5mm or 1/4" jacks as follows:

**Audio Input (Line In)**
*   **Tip (Signal)** -> Connect to **Pin 16**
*   **Sleeve (Ground)** -> Connect to **Pin 20 (AGND)**

**Audio Output (Line Out)**
*   **Tip (Signal)** -> Connect to **Pin 18** (and Pin 19 if stereo)
*   **Sleeve (Ground)** -> Connect to **Pin 20 (AGND)**

> **Note**: Since your code sets `out[0]` and `out[1]` to the same signal, you can listen to either Pin 18 or 19.

### Important Notes
1.  **AC Coupling**: The Daisy Seed inputs/outputs are AC coupled on the board, so you can connect line-level signals directly.
2.  **Grounding**: Always connect the "Sleeve" or "Ground" of your audio jacks to **AGND** (Pin 20) for the cleanest audio. Do not use the Digital Ground (DGND) pins if you can avoid it.
3.  **Levels**: The inputs expect continuous line-level signals (approx 1V-1.5V RMS). If you are connecting a raw guitar pickup, it will be very quiet (you may need a preamp circuit). If connecting Eurorack/Modular (10Vpp), you need a voltage divider to attenuate it or you will clip/damage the input.

---

### Harmony Generation Interval

When the pedal detects a solid pitch and opens the noise gate, it simultaneously synthesizes a secondary harmony note to accompany the fundamental guitar tone. This harmony is not fixed; it intelligently adapts based on the user-selected musical **Scale**. 

The generated interval is derived directly from the fundamental note's detected pitch before being mathematically forced (quantized) into the nearest legal scale degree.

*   **Major & Minor Scales (Diatonic)**: The pedal defaults to adding a **Major 3rd (+4 semitones)** on top of the original note. Because this new raw pitch is immediately forced through the quantization matrix, the 3rd will automatically flatten into a Minor 3rd when musically appropriate for the key. 
*   **Pentatonic & Blues Scales**: In these sparser scales, a 3rd is less desirable as it often clashes or resolves poorly. When the scale knob is set to Pentatonic or Blues (Scale type $\ge$ 3), the pedal switches to adding a **Perfect 5th (+7 semitones)** to create powerful, open sounding harmonies heavily used in rock.