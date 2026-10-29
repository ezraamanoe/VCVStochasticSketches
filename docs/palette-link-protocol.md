# Palette Link protocol

Palette Link is the connection between Palette and Drift. It lets Palette share its current note preferences with Drift, so Drift can mutate a captured phrase using the same harmonic palette.

Palette Link does not send audio, clock, gates, or Palette’s exact generated melody. It sends the current values of Palette’s twelve note-weight sliders.

## Connection

Palette has one polyphonic `LINK` output. Drift has one `PALETTE LINK` input.

```text
Palette LINK → Drift PALETTE LINK
```

The single cable contains twelve CV channels: one channel for each chromatic pitch class.

## Channel mapping

| Code channel | Note |
| --- | --- |
| 0 | C |
| 1 | C# |
| 2 | D |
| 3 | D# |
| 4 | E |
| 5 | F |
| 6 | F# |
| 7 | G |
| 8 | G# |
| 9 | A |
| 10 | A# |
| 11 | B |

## Voltage mapping

Palette converts each note slider from a weight between `0` and `1` into a voltage between `0 V` and `10 V`.

```text
0.0 slider weight = 0 V
0.5 slider weight = 5 V
1.0 slider weight = 10 V
```

For example, this Palette:

```text
C = 1.0
E = 0.5
G = 0.25
all other notes = 0.0
```

sends:

```text
C = 10 V
E = 5 V
G = 2.5 V
all other channels = 0 V
```

## How Drift uses it

In **Palette** and **Both** mutation modes, Drift reads the twelve incoming voltages and converts them back into note weights.

Drift then makes a weighted random choice:

- C has a weight of `1.0`
- E has a weight of `0.5`
- G has a weight of `0.25`

This means C is most likely to be chosen, E is less likely, and G is least likely. Notes with a weight of zero are never selected.

The random choice is made by Drift’s own seeded random generator. Palette Link gives Drift the note probabilities, but it does not force Drift to copy Palette’s current note sequence.

## Connection behaviour

| Palette Link state | Drift behaviour |
| --- | --- |
| Connected with one or more active note weights | Drift selects a new pitch class using the received weighted probabilities. |
| Connected with every note weight set to zero | Drift does not make a Palette-based mutation. |
| Disconnected | Drift falls back to pitch classes found in its captured phrase. |
| Drift in Octave mode | Palette Link is not used because Octave mode only changes octave. |

## Reason for this feature

Palette Link makes Palette and Drift work as a connected system. Palette defines a harmonic space through its note sliders, and Drift creates variation inside that same space. This keeps Drift’s mutations related to the notes currently being used in Palette, while still allowing controlled randomness and repeatable results through Seeds.