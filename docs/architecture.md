# Stochastic Sketches architecture

Stochastic Sketches is a VCV Rack plugin written in C++. At the moment, it contains two modules: Palette and Drift.

The aim of the plugin is to explore controllable randomness in melodic composition. Palette creates a weighted random pool of notes. Drift captures a short phrase and changes it in controlled ways. They can also communicate through Palette Link.

## Overall structure

Each VCV Rack module has two main parts:

- A `Module` class, which contains the musical behaviour, signal processing, parameters, inputs, outputs, and saved state.
- A `ModuleWidget` class, which contains the visible panel, knobs, sliders, ports, lights, and other interface elements.

```text
VCV Rack
    │
    └── Stochastic Sketches plugin
            │
            ├── Palette
            │     ├── stochastic pitch and gate generation
            │     ├── note weights, Density, and Seed
            │     └── Palette Link output
            │
            └── Drift
                  ├── phrase capture and playback
                  ├── controlled phrase mutation
                  └── Palette Link input
```

## Main files

```text
src/
  plugin.cpp        Registers Palette and Drift with VCV Rack.
  plugin.hpp        Contains shared plugin declarations.
  Palette.cpp       Contains Palette’s engine code and interface code.
  Drift.cpp         Contains Drift’s engine code and interface code.

res/
  Palette.svg       Palette panel layout.
  Drift.svg         Drift panel layout.
  *.png             Panel artwork and visual assets.

docs/
  Technical documentation for the project.
```

## Plugin registration

`plugin.cpp` is where VCV Rack is told which modules belong to the plugin.

Each module has a model, such as `modelPalette` or `modelDrift`. When Rack starts, the plugin registers these models so they appear in the module browser.

## Palette

Palette is a polyphonic stochastic pitch and gate sequencer. It does not generate audio itself.

It receives a clock signal and, on each rising clock edge, decides whether a note should happen. Density controls the chance of an event. If an event happens, Palette chooses one of the twelve chromatic notes using the slider weights.

```text
Clock input
    ↓
Density decides whether an event happens
    ↓
Weighted random note selection
    ↓
Pitch CV + Gate output
```

Palette has a separate seeded random generator for each voice. This means that the same Seed, clock pattern, note weights, Density value, and Reset point will produce the same sequence again.

Palette’s main outputs are:

- `PITCH`: 1 V/octave pitch CV.
- `GATE`: 10 V gate signal.
- `LINK`: twelve-channel polyphonic CV signal containing the current note-slider weights.

Palette currently supports up to sixteen voices. Each clock channel can drive a separate voice.

## Drift

Drift is a phrase capture and mutation module. It receives pitch and gate signals from another module, such as Palette, and records a short phrase.

Drift stores two versions of the phrase:

- The **captured phrase** is the original recorded version.
- The **working phrase** is the version that can be changed during playback.

Recall copies the captured phrase back into the working phrase. This means mutations can be undone without recording the phrase again.

```text
Pitch + Gate input
    ↓
Capture a short phrase
    ↓
Store original phrase and working phrase
    ↓
Loop working phrase
    ↓
Mutate selected steps at phrase boundaries
```

Drift has three mutation modes:

- **Octave**: keeps the original pitch class but changes octave.
- **Palette**: chooses a new pitch class from Palette Link, or from captured notes when LINK is disconnected.
- **Both**: changes both pitch class and octave.

Drift outputs Pitch, Gate, and a short Mutation Change trigger.

## Palette Link

Palette Link is the main connection between the two modules.

Palette sends its twelve current note weights through one polyphonic CV output. Drift reads the same twelve channels and uses them as probabilities when choosing a new pitch class in Palette or Both mode.

```text
Palette LINK ──────→ Drift PALETTE LINK
```

This means Palette defines the harmonic space, while Drift can create variation inside that same space.

The full channel and voltage specification is documented in `palette-link-protocol.md`.

## Polyphony

Both modules are designed to work with polyphonic cables.

Palette keeps separate random-generator, trigger, selected-note, and active-note state for each voice. Drift keeps separate captured phrases, working phrases, playback positions, and random-generator state for each channel.

The interface mostly shows information for the first voice or channel. The audio and CV behaviour can still be polyphonic even when the visual feedback is not.

## Interface and visuals

The panel artwork is designed in Inkscape and saved as SVG files. VCV Rack uses the SVG for the panel layout, while the C++ `ModuleWidget` code places interactive components at matching positions.

The visual style is inspired by Teenage Engineering’s EP–40 riddim. The aim is to make the modules feel playful and approachable while still showing useful musical information.

## Design approach

The project is intentionally focused on a small number of modules rather than a large collection of unrelated tools.

Palette provides controlled stochastic generation. Drift provides memory and controlled variation. Palette Link makes the two modules behave as part of one system.

Future features should fit this idea: randomness should be shaped by musical context, user control, Seeds, captured phrases, or connections between modules.