# Stochastic Sketches

**Stochastic Sketches** is a VCV Rack plugin exploring controllable randomness for melodic composition.

## Palette

<img width="220" height="310" alt="image" src="https://github.com/user-attachments/assets/0ef4d0f2-6b74-4915-9104-9761e316d139" />

Palette is a polyphonic stochastic pitch and gate sequencer. It does not generate sound itself. Instead, it selects notes and sends pitch CV and gate signals to an oscillator, envelope, or other compatible Rack modules.

Palette takes inspiration from the [Stochastic Inspiration Generator](https://www.stochasticinstruments.com/product-page/stochastic-inspiration-generator) and [ProbKey](https://library.vcvrack.com/ImpromptuModular/Prob-Key). I liked how both modules let you guide random melodies instead of just generating completely uncontrolled notes. Palette is not meant to be a copy of either one. It is my own simpler version, built around note-weight sliders, density, polyphony, and Seed values so that an interesting random idea can be repeated later.

The visual design is inspired by the [Teenage Engineering EP–40 riddim](https://teenage.engineering/products/ep-40). 

On every rising clock edge, Palette:

1. Uses **density** to decide whether an event should occur.
2. Selects one note from the chromatic palette according to the twelve note-weight sliders.
3. Outputs the selected pitch as 1 V/octave CV and a 10 V gate.

The sliders are weights, not on/off switches: a higher slider makes that note more likely to be selected relative to the other notes.

### Controls and connections

| Control / connection | Purpose |
| --- | --- |
| **Density** | Probability of a note event at each clock edge. Fully clockwise means an event on every clock edge. |
| **Seed** | Selects one of 999 repeatable random sequences. The display shows the current seed. |
| **Note sliders** | Set the relative probability of C through B. |
| **CLOCK input** | Receives a clock or trigger signal. One clock channel drives one Palette voice. |
| **RESET input** | Restarts the current seeded sequence. A mono reset signal resets all active voices. |
| **PITCH output** | 1 V/octave pitch CV. Connect to an oscillator's V/OCT input. |
| **GATE output** | 10 V gate signal. Connect to an envelope's gate input. |

### Repeatable randomness

Palette uses its own seeded random generator rather than Rack's shared random source. With the same Seed, clock pattern, density, note weights, and Reset points, it produces the same sequence again. This makes chance-based ideas repeatable, comparable, and usable in a musical performance.

## Trial patch: seeded Cmaj9

<img width="1065" height="387" alt="image" src="https://github.com/user-attachments/assets/8ca26ae6-e8ee-49d5-99bc-4a7f060c466f" />

This patch demonstrates weighted selection, density, seed, and polyphony.

- Set **density** fully clockwise.
- Set the C, D, E, G, and B sliders high; set every other note slider to zero.
- Set the **seed** to `1`.

Palette should now create a repeatable stream of randomly ordered Cmaj9 notes.

Lower Density to introduce rests. Raise a different note slider to hear how its increased weight changes the melody over time.
