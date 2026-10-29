# Palette

Palette is a polyphonic stochastic pitch and gate sequencer.

It does not create audio itself. Instead, it creates pitch CV and gate signals which can control a VCO, envelope, voice module, or other compatible VCV Rack modules.

Palette is designed to make random melodic material more controllable. The user chooses which chromatic notes are available and how likely each note is to appear. Palette then uses a clock, Density, and a repeatable Seed to generate note events.

## Inputs

| Input | Purpose |
| --- | --- |
| `CLOCK` | Receives clock or trigger signals. A rising edge creates a possible note event. |
| `RESET` | Restarts the seeded random sequence. A mono Reset resets every active voice. A polyphonic Reset can reset voices individually. |

## Outputs

| Output | Purpose |
| --- | --- |
| `PITCH` | 1 V/octave pitch CV output. Connect to a VCO or oscillator’s V/OCT input. |
| `GATE` | 10 V gate output. Connect to an ADSR envelope or voice module’s Gate input. |
| `LINK` | Twelve-channel polyphonic CV output containing Palette’s current note-slider weights. Intended for Drift’s PALETTE LINK input. |

Palette Link is documented separately in `palette-link-protocol.md`.

## Controls

| Control | Purpose |
| --- | ---|
| `DENSITY` | The chance that a clock edge produces a note event. |
| `MEMORY` | The chance that Palette reuses the event stored at the current motif position instead of creating a new one. |
| `SEED` | Selects one of 999 repeatable random sequences. |
| `C` to `B` note sliders | Set the relative chance of each chromatic pitch class being selected. |

The note sliders are weights, not simple on/off switches.

For example, if C has a weight of `1.0` and E has a weight of `0.5`, C is twice as likely to be selected as E. A note with a weight of `0` is not intended to be selected.

## How note generation works

On every rising Clock edge, Palette follows this process:

```text
1. Move to the current position in an eight-event memory buffer.
2. If that position already contains an event, use Memory to decide whether to reuse it.
3. If the event is not reused, use Density to decide whether a new note event should happen.
4. If a new event happens, choose one note using the twelve slider weights.
5. Store the resulting pitch and gate state in the current memory position.
6. Send pitch as 1 V/octave CV and send a 10 V gate while the incoming Clock is high.
```

At `100%` Density, Palette creates an event on every clock edge. At `0%` Density, it does not intentionally create events.

The gate is based on the incoming clock pulse. Palette does not create its own fixed gate length.

## Motif memory

Palette has an eight-event memory buffer for each polyphonic voice. Each slot stores:

- the selected chromatic pitch class
- whether that clock event contained a gate or a rest

Memory controls whether Palette reuses the current stored slot or replaces it with a newly generated event.

| Memory value | Behaviour |
| --- | --- |
| `0%` | Every clock event is freshly generated. Palette still writes the result into the buffer, but never reads it back. |
| `1–99%` | Stored events are sometimes reused and sometimes replaced. This creates motifs that gradually change. |
| `100%` | Palette fills the eight slots once, then repeats that eight-event pitch and gate motif. |

The first eight events after Reset, or after changing Seed, are always new because the buffer is empty. At `100%` Memory, the loop begins from the ninth clock event.

The eight LEDs show the Memory buffer for voice one:

- off: the slot is empty
- dim: the slot contains a stored event
- bright: the current slot

The LEDs currently show buffer position and stored state. They do not yet visually distinguish a stored event being reused from a stored event being replaced.

## Pitch output

Palette uses one octave of chromatic pitch classes:

| Note | Pitch output |
| --- | --- |
| C | `0 V` |
| C# | `1 / 12 V` |
| D | `2 / 12 V` |
| … | … |
| B | `11 / 12 V` |

This is standard 1 V/octave pitch CV. Connected oscillators can use these values as notes within their current octave range.

## Seeded randomness

Palette has its own random generator for each voice. It does not use Rack’s shared random source.

The Seed is an integer from `1` to `999`. Changing it reseeds every Palette voice and restarts their random state.

With the same:

- Seed
- clock pattern
- Density value
- Memory value
- note-slider weights
- Reset points

Palette produces the same note sequence again.

This makes Palette’s randomness repeatable. A user can find an interesting generated melody, keep its Seed, and return to the same result later.

## Polyphony

Palette supports up to sixteen voices.

Each channel in the Clock input drives one Palette voice. Palette outputs matching polyphonic Pitch and Gate channels.

Each voice has its own:

- clock trigger state
- reset trigger state
- selected note
- active-note state
- seeded random generator
- eight pitch-and-gate memory slots
- memory position

This means polyphonic voices generate related but independent stochastic note sequences.

The panel’s note LEDs currently show the note played by the first voice only.

## Computer keyboard chord input

Palette can use the computer keyboard as a quick way to set the active note sliders. This makes it easier to change chords while using Palette as a stochastic arpeggiator.

The mouse must be hovering over Palette for keyboard input to work.

| Computer key | Note |
| --- | --- |
| A | C |
| W | C# |
| S | D |
| E | D# |
| D | E |
| F | F |
| T | F# |
| G | G |
| Y | G# |
| H | A |
| U | A# |
| J | B |

The first key pressed after all mapped keys have been released starts a new chord. Palette clears the previous note selection and sets the new note slider to `1.0`.

Holding additional mapped keys adds them to the same chord. For example, holding `A + D + G` selects C, E, and G.

Releasing the keys does not clear the sliders. The completed chord stays active, so Palette can continue generating notes from it.

Starting the next chord replaces the previous Palette. This means Palette does not briefly become empty while changing from one chord to another.

Keyboard-selected notes currently receive equal weights of `1.0`. Velocity-sensitive MIDI input and more detailed weighting are planned as future work.

## Palette Link

Palette Link sends the current state of the twelve note sliders continuously.

It does not send:

- audio
- Clock
- Gate
- the exact note Palette most recently generated
- a melody sequence

Instead, it sends the harmonic probability map. Drift can use this map to constrain its own phrase mutations to the notes and note probabilities currently selected in Palette.

## No active note weights

If every note slider is set to zero, Palette has no valid pitch to choose. In this case, a freshly generated event becomes a rest rather than reusing an old pitch. A stored event can still replay until it is replaced or the module is Reset.

## Future work

Possible future improvements to Palette include:

- MIDI chord input from an external MIDI keyboard.
- Velocity-sensitive note weights.
- Euclidean rhythm mode as an alternative to independent Density probability.
- Better visual feedback for polyphonic voices.
- Visual feedback which distinguishes a reused memory event from a newly written event.
