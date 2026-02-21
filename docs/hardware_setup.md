# Daisy Seed Ethereal: Hardware Setup & Wiring

This document provides the standard breadboard wiring and pinout diagram to connect physical audio jacks to the Electro-Smith Daisy Seed for the Ethereal Harmonizer.

## Audio Pinout
The audio pins are located on the **Left Header** (when USB is facing UP), at the bottom.

| Pin Number | Label | Function | Variable Reference |
|---|---|---|---|
| **16** | **IN 1** | Audio Input Left | `in[0]` |
| **17** | **IN 2** | Audio Input Right | `in[1]` |
| **18** | **OUT 1** | Audio Output Left | `out[0]` |
| **19** | **OUT 2** | Audio Output Right | `out[1]` |
| **20** | **AGND** | Analog Ground | Audio Jack Sleeves |

## Wiring Diagram (Breadboard)
Connect your 3.5mm or 1/4" instrument jacks as follows:

**Audio Input (Line In)**
* **Tip (Signal)** -> Connect to **Pin 16**
* **Sleeve (Ground)** -> Connect to **Pin 20 (AGND)**

**Audio Output (Line Out)**
* **Tip (Signal)** -> Connect to **Pin 18** (and Pin 19 if stereo)
* **Sleeve (Ground)** -> Connect to **Pin 20 (AGND)**

> **Note**: Ethereal currently processes audio in Mono and outputs the dual-mixed mono signal to both the left and right outs. You can listen to either Pin 18 or 19.

## Important Hardware Rules
1. **AC Coupling**: The Daisy Seed inputs/outputs are internally AC coupled on the board, so you can connect line-level audio signals directly.
2. **Grounding**: Always connect the "Sleeve" or "Ground" of your audio jacks to **AGND** (Pin 20) for the cleanest audio. Do not use the Digital Ground (DGND) pins for audio lines, as they carry digital switching noise.
3. **Levels**: The inputs expect continuous line-level signals (approx 1V-1.5V RMS). 
    * If you are connecting a raw, passive electric guitar pickup, the signal will be extremely quiet. You will need a preamp or buffer circuit.
    * If connecting Eurorack/Modular levels (10Vpp), you **must** use a voltage divider to attenuate the signal, or you will clip the ADC and potentially damage the input.
