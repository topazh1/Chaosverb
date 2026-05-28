# Studio Acoustic Planner — core engine

Pure-logic Swift core for the iOS app described in [`PLAN.md`](PLAN.md). These
modules are **Foundation-only** (no RoomPlan/ARKit/SwiftUI), so they build and
unit-test on macOS *and* Linux. The camera-capture + UI layer lives in the Xcode
app project and depends on this package.

## Modules

| Module | Contents |
|---|---|
| `Acoustics` | `RoomModes` (standing-wave resonances + pile-ups), `ReverbTime` (Sabine/Eyring RT60 + absorption-to-target), `Reflections` (mirror-image first-reflection points), `SBIR` (speaker-boundary nulls), geometry types |
| `Measurement` | `REWTextParser` (REW "Export as text" → `FrequencyResponse`), `PeakDetector` (measured peaks/dips vs. smoothed trend), `Reconciliation` (cross-checks predicted modes against the measurement) |

## Run the tests

```bash
cd acoustic-planner
swift test            # needs a Swift toolchain (macOS, or Linux swift.org build)
```

## Physics validation

The expected values in the test suites are cross-checked by runnable Python
oracles (no Swift required):

```bash
cd acoustic-planner/validation
python3 oracle.py              # room modes, RT60, reflections, SBIR
python3 measurement_oracle.py  # peak detection + reconciliation
```

These oracles are the source of truth for the known-answer cases; keep them in
sync with the Swift implementation.

## REW sample

Drop a real REW **"Export measurement as text"** file (and optionally the
impulse-response WAV) into [`samples/`](samples/). The parser is written to
REW's documented format; once a real sample is present we confirm the exact
delimiter / column order / header lines against it.
