# Studio Acoustic Planner — Project Plan (v1)

**Goal:** An iOS app that scans a room with the camera, takes a few simple inputs
(monitor model, stand/desk dimensions, surface materials), and tells a home producer
**where to put their desk, monitors, absorption panels, and bass traps** to get a
balanced sound — without needing a measurement mic or acoustics knowledge.

**Status:** Planning / feasibility approved. This document is the design spec; no app
code exists yet.

---

## 1. Scope decisions (locked)

| Decision | Choice | Why |
|---|---|---|
| Platform | **iOS first** | LiDAR + Apple RoomPlan make room capture accurate and cheap to build. Android/RN port is a later phase. |
| Accuracy model (v1) | **Prediction, refined by optional REW import** | Geometry + formulas give the baseline; importing a real REW measurement reconciles prediction against reality for far more accurate advice. The import is optional — the app is fully usable without it. |
| Measurement source | **Import from REW** (Room EQ Wizard) | Reuses the user's existing calibrated mic + measurements. Avoids the uncalibrated-phone-mic problem entirely. Built-in phone-mic capture is a later, lower-accuracy convenience. |
| Inputs | Monitor from list · materials tap-to-tag · numeric stand/desk entry | Reliable, deterministic. |
| Auto-detect from video | **Phase 3 enhancement** | Used to *pre-fill* manual inputs, never a v1 dependency (least reliable piece). |

---

## 2. Feasibility summary

The app does three things; here is the honest difficulty of each:

1. **Capture room geometry** — *Solved.* Apple **RoomPlan** (LiDAR iPhone/iPad Pro)
   returns a parametric model: wall dimensions, ceiling height, doors, windows,
   furniture. ARKit corner-tapping is the fallback for non-LiDAR devices.

2. **Compute acoustic recommendations** — *Deterministic physics.* Everything in v1
   comes from closed-form formulas (room modes, mirror-image reflections, SBIR, Sabine
   RT60). No ML or guesswork required. This is the core value.

3. **Closing the prediction gap with REW** — Geometry-based prediction is an excellent
   *first pass* but is not the same as *measuring* the room. The gold standard is
   measure → treat → re-measure. Rather than building our own (uncalibrated) phone-mic
   capture first, v1 lets the user **import a real measurement from REW** — they already
   have a calibrated mic, and a text/WAV export is trivial and stable to parse. When a
   measurement is present, the engine reconciles its predictions against the measured
   reality (see §9). Without a measurement the app still works as a "smart advisor," and
   the UI must say which mode it's in.

---

## 3. Acoustic engine — the math (v1, all deterministic)

All formulas use speed of sound `c = 343 m/s` (20°C). Engine is a pure-Swift module
with **zero UI dependencies** so it is unit-testable and later portable to Android.

### 3.1 Room modes (resonances → "where the room honks")
```
f(nx,ny,nz) = (c/2) · sqrt((nx/L)² + (ny/W)² + (nz/H)²)
```
- Enumerate modes for nx,ny,nz ∈ {0..4}, keep f in 20–300 Hz.
- Axial modes (one index nonzero) are strongest → flag these.
- Detect **modal pile-ups** (modes within ~5% of each other = boomy notes) and
  **gaps** (sparse regions = uneven bass). Output: a list of problem frequencies +
  a "modal quality" score. Also surfaces the *room-ratio* quality vs Bolt/Louden ideals.

### 3.2 Monitor + listening position
- **38% rule:** listener at 38% of room length from the front wall (modal sweet spot).
- **Equilateral triangle:** the two monitors and the listener form an equilateral
  triangle; monitors toed in ~30° (60° apart) aimed at the ears.
- **Tweeter height** = seated ear height (~1.2 m) using the entered stand/desk height.
- **Left–right symmetry:** monitors equidistant from side walls (asymmetry = imaging error).
- Output: target desk position, monitor spacing, toe-in angle, and height delta from
  current stand entry.

### 3.3 First-reflection points (→ where absorption panels go)
**Mirror-image method.** For each monitor, reflect its position across each surface
(left/right walls, ceiling, floor, front wall, **desktop surface**). The point where
the line from the mirrored source to the listener pierces that surface is the
reflection point → that's where a panel belongs.
- Output: list of {surface, 2D position on that surface, panel size suggestion}.
- These map directly to AR markers in Phase 1.5 / Phase 3.

### 3.4 Speaker-Boundary Interference (SBIR → how far monitors sit from walls)
Quarter-wavelength cancellation from a boundary at distance `d`:
```
f_null ≈ c / (4·d)
```
- Compute the null from front-wall and side-wall distances; warn if a null lands in the
  20–200 Hz region. Recommend either flush/soffit mounting or a distance that pushes the
  null out of the critical band.

### 3.5 Reverb time (RT60 → how live/dead the room is)
**Sabine** (Eyring for deader rooms):
```
RT60 = 0.161 · V / Σ(Sᵢ · aᵢ)
```
- `V` = room volume; `Sᵢ` = each surface area; `aᵢ` = absorption coefficient from the
  tagged material, **per octave band** (125 Hz … 4 kHz).
- Compare to target (~0.3 s for a small mix room). Compute the **m² of absorption to
  add** to hit target, and split it into bass traps vs broadband panels.

### 3.6 Bass traps
- Tri-corners (wall-wall-ceiling/floor) and vertical wall-wall corners are pressure
  maxima for *all* modes → always recommend corner traps there, prioritized by which
  corners load the flagged problem modes most.

**Data tables shipped with the engine:**
- `monitors.json` — model → {dimensions, weight, port type, recommended wall distance}.
- `materials.json` — material → per-octave-band absorption coefficients (standard
  published tables: drywall, glass, carpet, concrete, curtains, etc.).
- `panels.json` — common treatment products → {thickness, absorption curve, size, price}.

---

## 4. Architecture (iOS)

```
StudioAcousticPlanner/
├─ App/                    SwiftUI app, navigation, onboarding
├─ Capture/
│   ├─ RoomPlanScanner     RoomPlan (LiDAR) capture → RoomModel
│   └─ ManualScanner       ARKit corner-tap fallback → RoomModel
├─ Model/
│   ├─ RoomModel           geometry: surfaces, dims, openings, furniture
│   ├─ MonitorSpec         from monitors.json
│   └─ MaterialTag         surface → material (tap-to-tag)
├─ Acoustics/              ★ PURE SWIFT, no UIKit — fully unit-tested
│   ├─ RoomModes
│   ├─ Placement           38% rule, triangle, symmetry
│   ├─ Reflections         mirror-image
│   ├─ SBIR
│   ├─ ReverbTime          Sabine/Eyring (estimate) — overridden by measured RT60
│   ├─ Reconciliation      ★ merges predicted modes/RT60 with imported measurement (§9)
│   └─ Recommendation      aggregates → RecommendationReport
├─ Measurement/            ★ REW import
│   ├─ REWTextParser       "Export measurement as text" → FrequencyResponse
│   ├─ ImpulseResponse     REW IR WAV → RT60 per band (Schroeder integration)
│   ├─ PeakDetector        finds measured peaks/dips (freq, level, Q)
│   └─ MeasurementImport   share-sheet / Files / iCloud import handler
├─ Visualize/              SceneKit/RealityKit 3D + AR overlay + measured-vs-predicted chart
├─ Resources/              monitors.json, materials.json, panels.json
└─ Tests/                  AcousticsTests + MeasurementTests (known-answer cases)
```

**Tech stack:** Swift + SwiftUI · RoomPlan · ARKit/RealityKit · SceneKit for 3D ·
Swift Package for the `Acoustics` module (so it stays portable & testable).

---

## 5. User flow (v1)

1. **Onboarding:** "This is an advisor, not a mic measurement" + permissions (camera).
2. **Scan:** Walk around the room (RoomPlan), or tap corners (fallback). Get dimensions.
3. **Tag materials:** Tap each wall/floor/ceiling → pick material chip.
4. **Add gear:** Pick monitor model from list; enter stand height + desk size.
5. **(Optional) Import REW measurement:** Share/open a REW text export (and optionally
   the impulse-response WAV) into the app. Tag the mic position (listening spot / sub /
   L / R). The app shows measured-vs-predicted and switches to "Measured" mode.
6. **Compute:** Engine runs → **RecommendationReport** (reconciled if a measurement exists).
7. **Results:**
   - 3D room with desk/monitors in recommended spots (drag to compare vs current).
   - Panel & bass-trap markers with sizes.
   - Measured-vs-predicted frequency chart (when a measurement is imported), with
     confirmed problem frequencies highlighted.
   - Plain-language report: "Move your desk back 40 cm. **Measured** 58 Hz peak +9 dB →
     add corner traps front + a tuned trap here ⌖. Put a 60×120 panel here ⌖."
8. **Treat → re-measure → re-import:** verify the fix and iterate.
9. **Save / share** the plan (PDF + saved project).

---

## 6. Phased roadmap

- **Phase 1 (MVP):** RoomPlan capture + manual inputs + full prediction engine + 3D
  result view + text report + PDF export. **Sellable on its own.**
- **Phase 1.5:** **REW import** — parse REW text export (and optional IR WAV) →
  reconciliation layer → measured-vs-predicted view + measurement-driven recommendations.
  *Low engineering cost, large accuracy gain; uses the user's existing calibrated mic.*
- **Phase 2:** AR overlay — point camera, see panel markers pinned on real walls.
- **Phase 2.5:** Built-in phone-mic measurement — sine sweep → frequency response →
  before/after (uncalibrated; relative comparison + calibration profiles). Convenience
  tier for users who don't run REW.
- **Phase 3:** Auto-detect from video (Vision/Core ML) to pre-fill materials & gear;
  product database with prices/buy links; Android/cross-platform port.

---

## 7. Risks & mitigations

| Risk | Mitigation |
|---|---|
| Non-LiDAR devices can't auto-scan | ARKit corner-tap fallback + manual dimension entry. |
| Users overtrust predictions | UI framing as "advisor"; REW import (§9) for verification. |
| Material tagging is tedious | Sensible defaults per surface; auto-detect (Phase 3) pre-fills. |
| Monitor DB coverage | Ship top ~100 models; "generic + enter dimensions" fallback. |
| REW export format drift across versions | Parse defensively (skip `*`/`#` comments, detect column order from header, tolerate tab/space/comma); validate with sample files from several REW versions; never touch the binary `.mdat`. |
| Measured curve depends on mic position | Require the user to tag mic position; only reconcile modes/RT60 when a listening-position measurement is present. |

---

## 8. Immediate next steps (when we start building)

1. Scaffold the Xcode/SwiftUI project + `Acoustics` Swift Package.
2. Implement `RoomModes`, `ReverbTime`, `Reflections` with unit tests (known-answer cases
   from textbook examples) — engine first, no UI needed to validate the physics.
3. Implement the **`REWTextParser`** + `PeakDetector` + `Reconciliation` against real REW
   sample exports — also pure logic, fully unit-testable without an iOS device.
4. Wire RoomPlan capture → `RoomModel`.
5. Build the results/report UI (incl. measured-vs-predicted chart) on the validated engine.

---

## 9. Measurement import (REW)

### 9.1 What we import
- **Primary — "Export measurement as text":** ASCII file. Header lines begin with `*`
  (and sometimes `#`) carrying metadata (REW version, date, smoothing, mic cal); then
  whitespace/comma-delimited numeric rows. Columns are typically
  `Freq(Hz)  SPL(dB)  Phase(degrees)`. Frequency points may be log-spaced (e.g. 48/96
  points-per-octave) — the parser must not assume linear spacing.
- **Optional — Impulse Response (WAV):** lets us compute decay metrics ourselves
  (RT60, EDT) via Schroeder backward integration, independent of REW's own RT60 graph.
- **Optional — Filters as text:** REW's generated EQ filters, used only as a
  cross-check / "EQ as last resort" suggestion.
- **Explicitly NOT supported:** the proprietary binary `.mdat` project format.

### 9.2 Parsing strategy (defensive)
1. Read lines; drop blanks and comment lines (`*` / `#`). Harvest version/date/smoothing
   from the header for display + provenance.
2. From the first data line (or a header label line) detect delimiter (tab/space/comma)
   and column order; default to `Freq, SPL, Phase`.
3. Parse into `FrequencyResponse { points: [(freq, splDb, phaseDeg?)] }`; reject files
   with too few points or non-monotonic frequency.
4. `PeakDetector` finds local maxima/minima in 20–300 Hz relative to a smoothed trend,
   returning `{freq, levelDb (± vs trend), approxQ}` for each peak/dip.
5. `ImpulseResponse` (if WAV provided) → octave-band-filtered Schroeder decay → RT60/band.

### 9.3 Reconciliation — how measurement sharpens advice
The `Reconciliation` module merges the predicted model with the measurement:

- **Room modes:** for each *predicted* mode, look for a *measured* peak within a
  tolerance (~5%).
  - Predicted **and** measured (severe) → **confirmed problem**: raise priority; size the
    trap to the measured level/Q (broadband corner trap for wide peaks, tuned
    membrane/Helmholtz for narrow high-Q peaks).
  - Predicted but **not** measured → likely already damped → **deprioritize**.
  - Measured peak with **no** predicted mode → flag as SBIR/comb-filter or
    furniture/leak → route to a placement fix rather than a trap.
- **RT60:** measured RT60 per band **overrides** the Sabine estimate, so the
  "add N m² of absorption" figure becomes exact rather than an estimate, and is split
  per band (more low-end trapping if low-band RT60 is long).
- **SBIR nulls:** confirm predicted boundary cancellations against measured dips →
  refine the recommended monitor-to-wall distance.
- **Confidence:** every recommendation is tagged `predicted` vs `measured`, and the
  report surfaces a before/after target the user can verify by re-measuring.

### 9.4 Data model
```
Measurement {
    source: .rewText | .rewImpulse
    micPosition: .listening | .sub | .left | .right | .other(String)
    response: FrequencyResponse?      // from text export
    rt60ByBand: [Band: Seconds]?      // from IR, or REW RT60 export
    peaks: [Peak]                     // detected
    meta: { rewVersion, date, smoothing, calFileName }
}
```

### 9.5 Future (not v1)
- REW's local **HTTP API** (`localhost:4735`, v5.20+) for a live desktop↔app bridge so
  measurements flow in without manual file export.
