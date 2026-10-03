# CHECKS — hard-won checklist for future synth projects

Every item below is a real bug found while building ERSA. Check each one when
starting or modifying a plugin, especially after refactors.

## Crashes / load failures

- [ ] Editor constructor: never call `setSize()` (triggers `resized()`) before
      all members exist; null-guard everything `resized()` touches.
- [ ] JUCE major versions rename virtuals (`drawGroupComponent` →
      `drawGroupComponentOutline`); mismatched `override`s fail the build,
      silent signature changes double-draw.
- [ ] A UI edit that "succeeds" may still be missing — verify wiring exists in
      the file (a null-called overlay crashes the DAW on click).
- [ ] Verify the SHIPPED binary contains new code (`strings` check). Relink
      races and stale objects are real; never trust a quiet build log alone.

## DSP correctness

- [ ] Unison/parallel oscillators need INDEPENDENT phase accumulators. One
      shared accumulator advanced N times per sample shifts pitch up N times.
- [ ] Detune applied exactly once, in consistent units (semitones vs octaves —
      a missing `/12` detunes every note randomly and reads as "broken").
- [ ] Per-osc unison detune must be centered per oscillator, not on the union
      span (else a solo osc inherits an edge detune).
- [ ] Cross-mod direction matches the intended architecture (VCO2 → VCO1 here).
- [ ] Filter must stay bounded under fast modulation: clamp coefficients,
      compensate resonance peaks, keep a limiter + tanh backstop at the end.
- [ ] Resonance curve: linear Q mapping makes everything scream alike;
      prefer gradual (quadratic) mapping + loudness compensation.
- [ ] Saturation placement matters: pre-filter saturates bass into mud,
      post-filter keeps low end tight.
- [ ] Sub oscillator: fade out below ~20 Hz (infrasonic subs eat headroom),
      keep level modest, verify with spectrum measurement.
- [ ] Oscillator mix/normalization must be level-stable when tap counts morph
      (normalize by live tap gains, not target counts).
- [ ] New unison taps must fade in/out (~5 ms), never switch discretely.
- [ ] Envelopes: exponential RC curves sound analog; linear ramps sound flat.
      Verify release threshold is reachable in finite time at all settings.
- [ ] Oscillator phase: free-running + random start (never hard-reset on
      retrigger); keep filter state running across retriggers (no dips).

## Clicks / pops

- [ ] Fresh attacks: short fade-in (~2 ms raised cosine).
- [ ] Retriggers on live voices: NO fade (would dip); keep oscs/filter running.
- [ ] Voice stealing: steal the QUIETEST voice + 3 ms crossfade from frozen output.
- [ ] Smooth continuous params per-sample (cutoff, reso, levels, PW, volume,
      mix, tune, xmod, drive) + short slew on LFO output.
- [ ] Delay-line params (chorus mode/base/depth, reverb size): glide over
      tens of ms or mode switches zip.
- [ ] Split pitch-glide from envelope-retrigger (mono/legato/arp each need
      independent control or staccato breaks).
- [ ] Master: stereo-linked peak limiter (fast attack, slow release) + idle
      gate to bit-exact silence + PANIC control.
- [ ] Sub-20 Hz + DC: highpass the master (~20 Hz, steep), assert in tests.

## MIDI / voices

- [ ] Pitch bend center is 8192, not 0; smooth bend for held notes.
- [ ] Velocity-0 noteOn counts as noteOff (JUCE defaults handle it — verify).
- [ ] Voice-mode switches must release hanging voices (else stuck notes).
- [ ] Arp hold must unlatch on hold-off AND on arp-off (transition detection).
- [ ] Sustain pedal explicitly supported or explicitly ignored (document it).

## Presets (data, never code)

- [ ] Factory presets live as FILES (XML), scanned at runtime; never compiled maps.
- [ ] Programs must reset-to-defaults before applying overrides (else state leaks).
- [ ] JUCE 8 `replaceState` does NOT push values to parameters — reset params directly.
- [ ] JUCE 8 APVTS trees store DENORMALIZED values — convert on load.
- [ ] User overwrites shadow (never modify) shipped files; delete restores.
- [ ] Filenames carry index+name (`NNN Name.xml`); parse defensively.
- [ ] getNumPrograms follows scanned content; setCurrentProgram bounds-checks it.
- [ ] Preset migration scripts: never sequential find-replace overlapping
      values (0→1→2→3 cascade); regenerate from an index-based plan instead.
- [ ] Brace/comma integrity after script edits: re-verify counts programmatically.

## UI

- [ ] Group bars/knobs: measure alignment from pixels (bar edges, gaps, window
      overflow), don't eyeball — off-by-20px errors hide in plain sight.
- [ ] Verify every group ends inside the window (right/bottom margins).
- [ ] No control may overlap the section bar (knob tops need clearance).
- [ ] Keep all text inside its control bounds (toggle labels, combo text).
- [ ] ComboBox draws its own internal label — never draw the same text in
      `drawComboBox` too (double-draw smear).
- [ ] Embed fonts (system fonts vary); keep OFL attribution. No system-font
      `setFont` calls bypassing the look-and-feel.
- [ ] Popup menus, scrollbars, buttons: style everything or stock JUCE parts
      will clash with a dark theme.
- [ ] No instructional text on the panel; tooltips for non-obvious controls.
- [ ] Resizable editor = fixed-layout content + scale transform + letterbox.
- [ ] Harness screenshots: delete the PNG before writing (stale-file confusion).

## Tests (headless harness)

- [ ] Replicate the plugin wrapper: `setRateAndBufferSizeDetails` before
      `prepareToPlay`, or `getSampleRate()` reads 0 and rate-derived DSP breaks.
- [ ] FFT probes: JUCE wants consecutive samples in the first half (not
      interleaved); search around the EXPECTED partial (subs dominate peaks).
- [ ] Test windows must exceed release + gate times or timing flakes result.
- [ ] Never clear a MIDI buffer containing the event you just added.
- [ ] Float comparisons need epsilon; slop/drift make exact values random.
- [ ] Cover: all presets render, tuning (isolated + in-context), unison octave,
      steal torture, FX torture, state round-trip, user preset IO, UI creation.
