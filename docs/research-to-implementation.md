# Research to implementation

This document keeps track of how the reading relates to Stochastic Sketches.

I'm not treating every paper as a feature to copy. Some sources suggest specific algorithms, others help me decide what makes the project musically useful, controllable, and worth evaluating.

## Working research question

> How can seeded stochastic generation, short-term motif memory, and controlled phrase mutation be combined in a modular synthesizer to create melodic material that is unpredictable but still recognisable?

Palette and Drift are the current practical response to this question.

## Current implementation

| Project feature | Research connection | How it is used |
| --- | --- | --- |
| Seeded randomness in Palette | Monro | Palette uses a fixed Seed so a random sequence can be repeated from the same starting conditions. |
| Seeded randomness in Drift | Monro | Drift can repeat the same mutation decisions when it has the same captured phrase, Seed, settings, and Reset or Recall points. |
| Palette note-weight sliders | Stochastic Inspiration Generator and ProbKey | The user shapes random note selection instead of accepting completely uncontrolled randomness. |
| Palette motif memory | Music Thing Turing Machine and Monro | Palette stores pitch-and-gate events in an eight-step buffer. Memory controls whether a slot is reused or regenerated, moving from fresh randomness towards a locked motif. |
| Drift’s captured and working phrases | Cope | Drift keeps an original phrase and creates variations from it, rather than replacing it with unrelated random notes. |
| Palette Link | Cope | Palette shares a current harmonic probability map with Drift, so Drift mutations can remain related to Palette’s note preferences. |
| Clear controls and visible state | Esler | The user should be able to understand and shape the algorithm while performing, rather than treating it as a black box. |

## Cope: musical identity and recombination

I found Cope’s discussion of musical signatures and recombination useful because it suggests that variation is stronger when it keeps a connection to existing material.

This is already reflected in Drift:

- Drift captures a short phrase instead of generating a completely unrelated sequence.
- It keeps an unchanged captured phrase in memory.
- It creates mutations in a separate working phrase.
- Recall restores the original material.

The main future feature from this idea is **Context mode**.

When Drift captures a phrase, it could count which pitch classes tend to follow each other. During mutation, it would choose a replacement note from the notes that normally follow the previous note in the captured phrase.

For example, if a captured phrase often moves from C to E or G, a mutation after C would prefer E or G. The result would still change, but it would feel more related to the captured phrase.

This would be a small first-order Markov-style model. It wouldn't be machine learning, composer imitation, or analysis of external music. It would only use the phrase currently stored in Drift.

## Monro: emergence and seeded processes

Monro’s discussion of emergence is useful because the project is built from simple rules that can create more interesting behaviour when combined.

At the lowest level, Palette and Drift use simple processes:

```text
Clock + Seed + note weights + random number
```

At a higher level, these processes can create phrases, motifs, repeated patterns, and interactions between modules.

This does not mean that any random output is automatically musical or creative. The project needs to evaluate whether the interaction of these rules produces results that a musician can control, repeat, and recognise.

Monro’s discussion of seeded pseudorandomness also supports the use of Seeds in Palette and Drift. A Seed makes chance-based output repeatable enough to test, compare, perform, and return to later.

## Toussaint: Euclidean rhythm

Toussaint’s work on Euclidean rhythms provides a possible structured alternative to Palette’s current Density control.

Palette’s current Density behaviour is independent probability:

```text
Every clock edge has a separate chance of producing a note.
```

This can create useful variation, but it can also create clusters of notes or several rests in a row.

A future Euclidean mode could distribute a chosen number of note events evenly across a fixed number of clock steps.

For example:

```text
3 events across 8 steps
X..X..X.
```

This would give Palette a more predictable rhythmic shape while keeping weighted random pitch selection.

This is planned as a later optional gate mode, not a replacement for Density.

## Turing Machine: musical memory

The Turing Machine is useful as an inspiration for the relationship between randomness and repetition.

Its musical behaviour can move between:

- new random material
- material that changes slowly
- a locked repeating loop

Palette now explores similar behaviour using stored pitch and gate events.

This would not be a direct Turing Machine clone. The original module uses a binary shift-register design, while Palette works with musical pitch classes, gates, polyphony, note weights, and Seeds.

The implemented Memory control decides how likely a stored event is replayed instead of regenerated:

```text
Low memory     = mostly new notes
Medium memory  = a phrase that slowly changes
High memory    = a stable repeating phrase
```

Palette uses a fixed eight-event buffer for each polyphonic voice. At `100%` Memory, the buffer fills during the first eight clock events and repeats from the ninth. At `0%`, Palette writes new events into the same buffer but never reuses them. This is useful for comparison because the module can move between fresh randomness, gradual change, and a stable loop without changing its harmonic weights or Seed.

The implementation builds successfully. Focused Rack testing is still needed for the 0%, 100%, intermediate Memory, Reset, and polyphonic cases.

## Shapiro and Huber: Markov-style note context

Shapiro and Huber’s Markov-chain discussion supports the planned Context mode for Drift.

A first-order Markov model looks only at the current or previous state when selecting the next state. In Drift, this means:

```text
previous pitch class → likely following pitch classes
```

Drift would build this small relationship map from the captured phrase. It would then use it only when Context mode is selected.

This feature would improve recognisability without needing a large dataset or external training material.

## Burraston and Edmonds: cellular automata

Cellular automata are another possible way to add structured variation.

Rather than using cellular automata to generate all pitch values, I would use them as a **mutation mask** for Drift.

An eight-step phrase could have an eight-cell pattern. The cells decide which phrase steps are currently allowed to mutate.

```text
Active cell   = this step may mutate
Inactive cell = this step is protected
```

The cellular automaton could evolve at each phrase boundary. This would add an evolving structure to Drift’s mutations while keeping its captured phrase and Palette Link behaviour intact.

This is a later feature because the current capture, mutation, Link, and testing behaviour need to be reliable first.

## Pexton: levels of description

Pexton’s discussion of emergence is useful for explaining the project.

Stochastic Sketches can be described at several levels:

```text
Low level:
clock signals, CV voltages, Seeds, random numbers, arrays

Module level:
weighted note selection, phrase capture, playback, mutation

Patch level:
Palette and Drift connected through Palette Link

Musical level:
melody, repetition, variation, tension, recognition, performer control
```

This gives a way to explain how simple technical rules can become a more meaningful musical system when connected together.

## Milicevic: complexity is not automatically musical value

Milicevic’s discussion of chaos, fractals, and complexity is an important warning for the project.

Using a more complicated algorithm does not automatically make the module more creative or more useful. New features should be added only when they improve musical control, recognisability, or the range of useful output.

This affects the evaluation plan. I should not only ask whether an algorithm works technically. I should also ask:

- Can a user understand what the control does?
- Can they repeat a useful result?
- Does the output remain related to the original phrase or selected palette?
- Does the feature create a meaningful musical difference?
- Is the extra complexity worth it?

## Esler: performer interaction

Esler’s work is useful because it treats algorithmic music as something that can be performed and shaped by a person.

This supports several interface decisions in Stochastic Sketches:

- simple visible controls instead of hidden behaviour
- LEDs and sequence displays showing current activity
- note sliders that make harmonic choices readable
- Capture and Recall controls that let the user control Drift’s memory
- Seeds that let a performer return to a useful random state

The goal is not to automate composition completely. The user should remain part of the system by choosing notes, clocks, chance, memory, captured material, and module connections.

## Research priorities

| Priority | Feature or task | Research connection |
| --- | --- | --- |
| Now | Test Palette and Drift baseline behaviour | Monro and Milicevic |
| Now | Document Palette Link and test its edge cases | Cope and Esler |
| Next | MIDI chord input for Palette | Esler: performer control |
| Next | Drift Context mode | Cope and Shapiro and Huber |
| Now | Test Palette Memory at 0%, 100%, and intermediate values | Turing Machine inspiration and Monro |
| Later | Euclidean gate mode | Toussaint |
| Later | Cellular-automata mutation mask | Burraston and Edmonds |
| Ongoing | Listen to and evaluate patches | Monro, Pexton, and Milicevic |

## Design references

The Stochastic Inspiration Generator and ProbKey are practical module inspirations for Palette. Teenage Engineering’s EP-40 riddim is a visual design inspiration for Palette and Drift.

These references influence the interface and overall direction, but the academic sources above provide the main basis for the discussion of algorithms, emergence, musical identity, and evaluation.
