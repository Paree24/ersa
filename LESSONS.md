# LESSONS — every issue hit building ERSA, and what fixed it

Companion to `CHECKS.md` (the pre-flight list). This file records *what
happened, why, and the fix*, so future plugins don't re-learn it the hard way.
Ordered roughly chronologically.

## 1. Build breaks against JUCE 8 API

**Issue:** Fresh JUCE 8 fetch broke the first build in five places at once:
`LadderFilter::processSample` is protected, `drawGroupComponent` was renamed
to `drawGroupComponentOutline`, `IIR::Filter::state` is private (use the
`coefficients` member), `dsp::Reverb` has no `setSampleRate` (needs
`prepare(spec)`), `Font::getStringWidthFloat` is deprecated.
**Fix:** Replaced the ladder filter with a hand-rolled cascaded TPT-SVF
(also gave us the gradual resonance curve we wanted), used the correct
override signatures, assigned coefficients objects.
**Avoid:** After any JUCE version bump, build immediately and fix override
signatures first — mismatched `override`s either fail loudly (good) or
silently change behavior (bad).

## 2. Instant DAW crash on plugin load (segfault in `setBounds`)

**Issue:** Every DAW crashed scanning the plugin.
**Root cause:** The editor constructor called `setSize()` first, which
synchronously fires `resized()`, which touched the MIDI keyboard member that
wasn't constructed until later in the constructor. Null dereference.
**Fix:** Moved `setSize()` to the end of the constructor, null-guarded
`resized()`. Reproduced headlessly and confirmed with a gdb backtrace
(`Component::setBounds ← sendMovedResizedMessages ← Ersa8Editor`).
**Avoid:** Never trigger layout before all members exist; null-guard
everything `resized()` touches.

## 3. Non-finite (NaN) audio from `getSampleRate() == 0`

**Issue:** First render produced NaN.
**Root cause:** EQ coefficient code called `getSampleRate()`, which returns 0
until the plugin wrapper calls `setRateAndBufferSizeDetails` — the harness
(and some hosts) only called `prepareToPlay`.
**Fix:** Cache the rate from `prepareToPlay` into `cachedSr`; never trust
`getSampleRate()` in DSP code. Harness replicates the wrapper call.
**Avoid:** Cache the sample rate yourself; treat `getSampleRate()` as
unreliable outside the wrapper path.

## 4. Presets leaked into each other

**Issue:** Loading preset 0 after preset 27 kept 27's +4 dB EQ.
**Root cause:** Factory presets were sparse override maps with no reset, and
JUCE 8 `replaceState()` swaps the tree without pushing values to parameters.
**Fix:** Every program load resets all params to creation defaults first,
then applies overrides. Covered by a determinism test.
**Avoid:** Programs must always be reset-then-apply, never overlay-only.

## 5. Mono/legato staccato went silent (the "Acid" bug)

**Issue:** Detached notes on mono patches smeared into silence.
**Root cause:** Any new note arriving while the voice was releasing was
treated as legato, so envelopes never retriggered.
**Fix:** Split *glide* from *re-articulation* into independent flags:
detached notes always restart filter+amp (pitch may still glide with
portamento), true overlapping legato slurs.
**Avoid:** Never conflate "voice is busy" with "player is legatoing" —
track physical held-keys separately.

## 6. ComboBox preset names rendered twice (the "overlap")

**Issue:** Every dropdown name looked smeared/overlapped, permanently.
**Root cause:** JUCE's ComboBox paints its own internal text label, and our
custom look-and-feel *also* drew the text underneath it.
**Fix:** Draw only background + arrow in `drawComboBox`; style the text via
`textColourId` + `getComboBoxFont`.
**Avoid:** Know which parts JUCE paints itself before custom-painting a widget.

## 7. Unison shifted pitch up N octaves

**Issue:** Turning up unison raised the pitch (2 taps = octave up).
**Root cause:** All unison taps shared one phase accumulator, so each tap
advanced it again per sample — N taps ran the oscillator N× fast.
**Fix:** Independent phase accumulator per tap. Regression test proves 4×
unison still reads concert pitch.
**Avoid:** Parallel voices need independent state. Shared mutable phase is
always a pitch bug.

## 8. The 12× detune bug (`pow(2, det)` vs `pow(2, det/12)`)

**Issue:** After the unison rewrite "nothing was in tune, unusable."
**Root cause:** Slop moved from "added to MIDI note" (semitones, correct) to
a frequency multiplier `pow(2, det)` — but that function takes *octaves*, so
every ±4¢ tolerance hit 12× too hard (±48¢+ random per note).
**Fix:** One-line `/12`. Proven with an FFT tuning probe (VCO2 read 452 Hz
instead of 440 pre-fix; 0–8¢ everywhere post-fix).
**Avoid:** Unit-test tuning with an FFT probe after touching any pitch path.
Keep semitone/octave conversions in one place with the units in the name.

## 9. Footage-to-octave migration cascade

**Issue:** A script migrating `16'/8'/4'` → `-2..+2` indices corrupted every
preset.
**Root cause:** Sequential find-replace of overlapping values (0→1 rewrites
what 1→2 then rewrites again).
**Fix:** Regenerated from an index-based plan instead of string replacement.
**Avoid:** Never sequential-replace overlapping values; always map old→new
in one pass from a table.

## 10. Sub-20 Hz rumble on the analyzer

**Issue:** Huge sub-20 Hz energy (mostly <10 Hz) on bass patches.
**Root cause:** The sub oscillator itself went infrasonic on low notes (a
16 Hz sine on low C) — inaudible, but eats headroom and pumps the bus.
**Fix:** Sub fades out fully below 20 Hz + a steep 20 Hz cleanup filter on
the master. Verified −55 dB in the 8–20 Hz band with zero DC.
**Avoid:** Every synth needs a master highpass and a spectrum test; subs must
be gated to the audible range.

## 11. Steal clicks under busy playing

**Issue:** Clicks during fast legato/chord playing despite clean sustains.
**Root cause:** Voice stealing swapped waveforms instantly (verified with a
"steal torture" test hitting 0.42 steps).
**Fix:** Steal the quietest voice + 3 ms crossfade from its frozen output.
Also: fresh-attack fade-in, quietest-voice stealing, LFO edge slew.
**Avoid:** Sustained-note tests are not enough; always torture-test stealing
and live parameter drags.

## 12. JUCE 8 stores denormalized values in APVTS trees

**Issue:** User-preset load set absurd values (eqhigh read 12.0 instead of 4.0).
**Root cause:** Assumed tree `value`s were normalized 0–1; JUCE 8 stores real
units — must `convertTo0to1()` on load.
**Fix:** Convert on load; regression test asserts round-trip values.
**Avoid:** Never assume normalization direction; assert a round-trip value in
tests, not just "applied > 0".

## 13. Pitch-bend center bug (8192, not 0)

**Issue:** Any wheel touch sharpened everything; jitter randomly detuned notes.
**Root cause:** Code divided the raw 0–16383 value by 8192, so *center*
computed as full-up bend.
**Fix:** Subtract-then-divide, smooth bend for held notes, and a ±48 LSB
center deadband so worn wheels pin exact zero.
**Avoid:** MIDI centers are 8192 for bend, 64 for CCs — never treat raw wheel
bytes as bipolar directly.

## 14. Pitch-wheel snaps caused laser zaps

**Issue:** Occasional laser "pew" while playing; stopped when touching the wheel.
**Root cause:** The wheel handler snapped every held voice's target pitch
instantly, so each jitter event stepped the pitch discontinuously.
**Fix:** Removed the immediate snap — voices follow the smoothed bend path
(~8 ms glide). Verified with a zero-crossing pitch tracker (46¢ max jump).
**Avoid:** MIDI input must never step DSP state directly; everything from
the outside world goes through a smoother.

## 15. THE BIG ONE: smoothing self-cannibalisation (the zap root cause)

**Issue:** Random laser zaps; a "silent" voice sang a steady 220+440 Hz tone.
**Root cause:** `smX += (p.x - smX) * k; p.x = smX;` inside the sample loop.
After sample 0 writes back into `p.x`, every remaining sample computes
`(sm-sm) = 0` — each smoother advanced one sample-step per *block*, i.e.
512× too slow. Every knob/preset change glided over ~4 s (slow filter sweeps
sound exactly like lasers) and voices sat at mid-slew levels for seconds.
**Fix:** Snapshot targets into locals once per block, smooth toward those.
Silent voices now read bit-exact 0.00000. (Members like `bendSemis` were
immune — only read-then-overwrite of `p.*` self-cannibalises.)
**Avoid:** This is now a CHECKS.md item with its own regression tests
(silent-voice, filter-closed, pitch-slew). Any per-sample smoother that both
reads and writes the same field is suspect on sight.

## 16. Chorus Wide == Manual (copy-paste constant bug)

**Issue:** Wide mode sounded identical to Manual.
**Root cause:** The mode branch assigned the same constants (leftover copy-paste).
**Fix:** Wide got its own design (longer base + quadrature LFO) plus
wet-path-only darkening for all modes.
**Avoid:** When adding modes, test each mode's output differs (or at least
listens different) — dead branches hide behind shared code paths.

## 17. UI geometry by eyeball (the 20 px gap, Mix/bar overlap)

**Issue:** Bottom-row gaps uneven; Mix knob overlapped the section bar.
**Root cause:** Hand-computed coordinates, twice.
**Fix:** Measured bar edges/gaps from rendered pixels; 6 px everywhere,
verified by script, not by looking.
**Avoid:** Assert layout numerically (pixel-measure in tests/renders);
eyes adapt, rulers don't.

## 18. Stale-binary confusion (two incidents)

**Issue:** (a) Screenshots showed old UI after a rebuild; (b) the shipped
`.so` lacked new code while the build log was quiet.
**Root cause:** (a) harness wrote the PNG without deleting the old one;
(b) a relink race left a stale shared object.
**Fix:** Harness deletes screenshots before writing; verify shipped binaries
with a `strings` check for new symbols, not just the build log.
**Avoid:** Never trust "build succeeded" — verify the artifact contains the
change. Delete-then-write for all generated test outputs.

## 19. Test flakiness from timing assumptions

**Issue:** Idle-gate and tuning tests flaked intermittently.
**Root cause:** Fixed block counts that didn't cover release time + gate
time, and FFT peak-picking biased by sub-oscillator dominance.
**Fix:** Windows sized from actual time constants (release + 6 s gate);
FFT searches ±3% around the *expected* partial; paranoid tolerances with
documented rationale (slop/drift are random by design).
**Avoid:** Derive every test window from the DSP's own time constants, and
remember analog-character features make exact-value asserts invalid —
assert bands, not points.

## 20. Preset system grown out of code
**Issue:** Needed a second 128-preset bank, editable/overwritable presets,
and bank tags — without recompiling.
**Fix:** Presets are now data files (`NNN Name [Bank].xml`), scanned at
runtime; user overwrites shadow (never modify) shipped files; DEL restores.
`getNumPrograms` follows scanned content.
**Avoid:** Ship content as data from day one. Bonus lesson: when reordering
an indexed list (arp divisions slow→fast), migrate stored indices and
preserve original file line-endings (CRLF→LF flip polluted a whole commit).

## 21. Exact float equality in tests breaks on ARM (FMA contraction)

**Issue:** Self-test passed on Linux/Windows x86-64 but failed on macOS ARM64
at `CHECK(raw eqhigh == 4.0f)`.
**Root cause:** JUCE 8 `getRawParameterValue` returns *denormalised* values,
so the check round-trips 4.0 → normalized → back. ARM64 clang fuses
multiply-add by default, landing on `4.0000005f` instead of exactly `4.0f`.
The test (not the DSP) was wrong.
**Fix:** All such checks now use epsilon comparison (`< 1e-3f`).
**Avoid:** Never `==`-compare floats in tests, especially across a
normalize/denormalize round-trip, and especially when CI spans x86 + ARM.
