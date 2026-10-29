# Manual testing

This file records tests that should be run in VCV Rack after a feature builds successfully.

## Palette motif memory

Use a clock, a few active Palette note sliders, a VCO, an envelope, and audio output. Set Density to `100%` so every clock event is easy to hear.

### 0% Memory

- [ ] Set Memory to `0%`.
- [ ] Confirm that Palette creates fresh weighted events on every clock edge.
- [ ] Confirm that the eight buffer LEDs still fill and the current position moves, even though stored events are not reused.

### 100% Memory

- [ ] Press Reset, then set Memory to `100%`.
- [ ] Confirm that the first eight events fill the buffer.
- [ ] Confirm that events 9–16 repeat the same pitch and gate pattern as events 1–8.
- [ ] Confirm that the eight LEDs show one bright current slot and dim stored slots.

### Intermediate Memory

- [ ] Set Memory to an intermediate value, such as `50%`.
- [ ] Confirm that the phrase sometimes repeats a stored event and sometimes changes.
- [ ] Confirm that changing note weights affects newly generated events, not already stored events.

### Repeatability and polyphony

- [ ] With the same Seed, note weights, Density, Memory, clock pattern, and Reset point, confirm that Palette produces the same result again.
- [ ] Send a polyphonic clock to Palette and confirm that each voice maintains its own eight-event buffer.

## Notes

The Memory LEDs currently show whether a slot is empty or stored, plus the current buffer position. They do not yet show whether the current event was reused or replaced on that clock edge.
