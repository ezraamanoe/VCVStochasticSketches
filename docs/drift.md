# Drift

Drift is a polyphonic phrase capture and mutation module.

It does not generate audio itself. It takes pitch and gate CV from another module, records a short phrase, loops it, and gradually changes it in controlled ways.

Drift is designed to work especially well with Palette. Palette can generate a seeded random phrase, while Drift captures that phrase and creates variations that still relate to the original material.

## Inputs

| Input | Purpose |
| --- | --- |
| `CLOCK` | Controls phrase capture and playback timing. |
| `RESET` | Returns phrase playback to the first step. |
| `PITCH` | Receives 1 V/octave pitch CV to capture. |
| `GATE` | Receives gate signals to capture. |
| `PALETTE LINK` | Receives Palette’s twelve note-weight channels. |
| `MUTATION CHANGE CV` | Adds CV control to the mutation chance. |

## Outputs

| Output | Purpose |
| --- | --- |
| `PITCH` | The captured or mutated phrase as 1 V/octave CV. |
| `GATE` | Gate signal for the current phrase step. |
| `MUTATION CHANGE` | A short 10 V trigger whenever Drift successfully mutates a step. |

## Controls

| Control | Purpose |
| --- | --- |
| `CAPTURE` | Arms Drift to record a new phrase. |
| `RECALL` | Restores the original captured phrase and removes its mutations. |
| `MUTATION MODE` | Selects Octave, Palette, or Both mutation behaviour. |
| `CAPTURE LENGTH` | Sets the phrase length from 1 to 8 steps. |
| `DRIFT` | Sets the probability that a mutation happens at the end of each phrase loop. |
| `OCT. JUMP` | Sets the largest possible octave jump from 0 to 2 octaves. |
| `SEED` | Selects one of 999 repeatable mutation sequences. |

## Capture process

Pressing Capture does not immediately record a step. It arms Drift, which starts recording on the next rising Clock edge.

Drift watches the full high part of each clock pulse and stores the pitch and gate state when that pulse ends. This is important when capturing from Palette, because Palette may update its gate slightly after Drift sees the shared Clock edge.

Each captured step stores:

- pitch voltage
- whether a gate was active during that clock step

Drift records between 1 and 8 steps, depending on Capture Length.

While Drift is recording, it passes the incoming Pitch and Gate signals through unchanged. Once recording finishes, Drift loops the stored phrase instead.

## Captured phrase and working phrase

Drift stores two versions of every phrase:

- The **captured phrase** is the original recording.
- The **working phrase** is the version that Drift plays and mutates.

Mutations only change the working phrase. The original capture remains available as a reference.

Pressing Recall copies the captured phrase back into the working phrase, resets playback to the first step, and reseeds the random generator. This gives a reliable way to return to the original phrase.

## Playback

After capture, Drift plays one stored step per Clock edge.

The Pitch output holds the current step’s pitch voltage. The Gate output is `10 V` only while both of these conditions are true:

- the incoming Clock signal is high
- the captured or working step contains a gate

At the end of a phrase loop, Drift decides whether to mutate one note before the next loop starts.

Reset returns playback to the first step. It does not erase the phrase, recall the original capture, or change the current Seed.

## Mutation process

At the end of each phrase loop, Drift follows this process:

```text
1. Read the Drift knob and Mutation Change CV.
2. Decide whether a mutation should happen.
3. Choose one active, gated step in the working phrase.
4. Read the selected mutation mode.
5. Change the pitch class, octave, or both.
6. Send a short Mutation Change trigger.
```

Drift only mutates active steps. Rests remain rests.

The mutation chance is calculated from:

```text
Drift knob + Mutation Change CV / 10 V
```

The final value is limited between `0%` and `100%`.

## Mutation modes

### Octave

Octave mode keeps the original pitch class and moves it up or down by a random number of octaves.

The maximum distance is set by `OCT. JUMP`.

For example, with a captured E and an Octave Jump of 1, Drift may change the note to E one octave higher or lower.

### Palette

Palette mode changes the pitch class using Palette Link.

If Palette Link is connected, Drift uses Palette’s current note weights as probabilities. Higher Palette slider values make those notes more likely to be selected.

If Palette Link is disconnected, Drift falls back to choosing from pitch classes found in the original captured phrase.

### Both

Both mode changes the pitch class using Palette Link or captured-note fallback, then also makes an octave jump.

## Palette Link behaviour

Palette Link gives Drift a harmonic context without sending an exact melody.

```text
Palette LINK → Drift PALETTE LINK
```

Palette sends twelve note weights, one for each chromatic pitch class. Drift reads these values and uses weighted random selection when mutating in Palette or Both mode.

| Palette Link state | Drift behaviour |
| --- | --- |
| Connected with active note weights | Selects a new pitch class using Palette’s weights. |
| Connected with all weights set to zero | Does not make a Palette-based mutation. |
| Disconnected | Falls back to pitch classes in the captured phrase. |
| Octave mode | Does not use Palette Link. |

The full CV specification is documented in `palette-link-protocol.md`.

## Seeded randomness

Drift has its own seeded random generator for every polyphonic channel.

With the same:

- captured phrase
- Seed
- Clock pattern
- Mutation Chance
- Mutation Mode
- Octave Jump
- Palette Link state and note weights
- Reset and Recall points

Drift produces the same mutations again.

This makes Drift’s variation repeatable. A user can find an interesting mutated phrase, keep the Seed and settings, then return to that result later.

## Polyphony

Drift can capture and play a separate phrase for each polyphonic channel.

Each channel has its own:

- captured phrase
- working phrase
- playback position
- capture state
- random generator
- Mutation Change trigger

The interface shows capture state, phrase state, playback position, and mutation activity for the first channel only.

## Future work

Possible future improvements to Drift include:

- Context mode using note-to-note relationships learned from the captured phrase.
- A cellular-automata mutation mask to control which phrase steps are allowed to change.
- More detailed visual feedback for polyphonic channels.
- Saving captured phrases in Rack patch data.
- A clearer display of the working phrase’s current note values.