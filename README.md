# Stochastic Sketches

**Stochastic Sketches** is a VCV Rack plugin exploring controllable randomness for melodic composition.

## Palette

<img width="220" height="310" alt="image" src="https://github.com/user-attachments/assets/0ef4d0f2-6b74-4915-9104-9761e316d139" />

Palette is a polyphonic stochastic pitch and gate sequencer. It does not generate sound itself. Instead, it selects notes and sends pitch CV and gate signals to an oscillator, envelope, or other compatible Rack modules.

Palette takes inspiration from the [Stochastic Inspiration Generator](https://www.stochasticinstruments.com/product-page/stochastic-inspiration-generator) and [ProbKey](https://library.vcvrack.com/ImpromptuModular/Prob-Key). I liked how both modules let you guide random melodies instead of just generating completely uncontrolled notes.

The visual design is inspired by the [Teenage Engineering EP–40 riddim](https://teenage.engineering/products/ep-40). 

On every rising clock edge, Palette:

1. Uses **Density** to decide whether an event should occur.
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

### Demo patch: seeded Cmaj9

https://github.com/user-attachments/assets/26d0c601-b079-45b7-bcfa-b4fc4b3a98ee

This patch demonstrates weighted selection, density, seed, and polyphony.

- Set **Density** fully clockwise.
- Set the C, D, E, G, and B sliders high; set every other note slider to zero.
- Set the **Seed** to `1`.

Palette should now create a repeatable stream of randomly ordered Cmaj9 notes.

Lower Density to introduce rests. Raise a different note slider to hear how its increased weight changes the melody over time.

## Drift

<img width="220" height="310" alt="image" src="https://github.com/user-attachments/assets/7872c666-bbec-4d65-9202-33375eb2dfe6" />


Drift is a polyphonic phrase capture and mutation sequencer. It records incoming pitch CV and gate signals, then loops the captured phrase while making controlled random changes over time.

Drift is designed to work after Palette, but it can capture pitch and gate signals from any compatible sequencer. Palette creates the initial random melodic material, while Drift makes a recognisable phrase evolve gradually instead of creating an entirely new sequence on every clock pulse.

When Capture is armed, Drift records a phrase of one to eight clock steps. It stores both the original phrase and a separate working copy. Mutations only affect the working copy, so Recall can always restore the original capture.

On each completed phrase loop, Drift:

1. Uses **Drift** to decide whether a mutation should occur.
2. Selects one active step in the captured phrase.
3. Changes the step’s octave, note degree, or both, depending on the selected Mutation Mode.
4. Outputs a short 10 V trigger from **Mutation Change** when a mutation occurs.

### Controls and connections

| Control / connection | Purpose |
| --- | --- |
| **Capture** | Arms Drift to record a new phrase on the next clock edge. |
| **Recall** | Restores the original captured phrase and restarts its repeatable mutation sequence. |
| **Capture Length** | Sets the number of steps to record, from 1 to 8. |
| **Drift** | Probability of a mutation at the end of each phrase loop. Fully clockwise means Drift attempts a mutation on every loop. |
| **Mutation Mode** | Chooses whether mutation changes an octave, a note degree, or both. |
| **Octaves** | Sets the largest possible octave jump. |
| **Seed** | Selects one of 999 repeatable mutation sequences. |
| **CLOCK input** | Receives the clock used for capture and phrase playback. |
| **RESET input** | Returns playback to the first step of the stored phrase. |
| **PITCH input** | Receives incoming polyphonic 1 V/octave pitch CV to capture. |
| **GATE input** | Receives incoming polyphonic gates to capture. |
| **DRIFT CV input** | Adds CV control to the Drift mutation probability. `0–10 V` corresponds to 0–100% additional mutation chance. |
| **PALETTE LINK input** | Reserved for a future twelve-channel connection from Palette containing C–B note weights. |
| **PITCH output** | Outputs the stored and mutated polyphonic 1 V/octave pitch CV. |
| **GATE output** | Outputs the stored polyphonic gate pattern. |
| **MUTATION CHANGE output** | Outputs a short 10 V trigger whenever Drift changes a phrase step. |

### Mutation modes

| Mode | Behaviour |
| --- | --- |
| **Octave** | Keeps the captured note degree and moves it up or down by the selected octave range. |
| **Palette** | Changes the note degree while keeping its captured octave. Until Palette Link is implemented, Drift chooses from the note degrees already present in the captured phrase. |
| **Both** | Changes both the note degree and octave. |

### Repeatable mutation

Drift uses a seeded random generator for each polyphonic voice. With the same original phrase, Seed, settings, and clock pattern, it produces the same series of mutations again.

This is important because it means a useful evolving phrase is not lost. You can experiment with Drift, return to the original phrase with Recall, and reproduce the same changes from the same Seed.

### Demo patch: Palette into Drift

https://github.com/user-attachments/assets/dd0ee77d-c6d5-49f4-bfd9-dd5138ab242a

- Set Drift Capture Length to 4 or 8.
- Press Capture and let Drift record one full phrase.
- Set Drift low at first, such as 20–30%.
- Choose Octave, Palette, or Both mutation mode.
- Increase Drift gradually to hear the phrase become less predictable while remaining connected to the original idea.


