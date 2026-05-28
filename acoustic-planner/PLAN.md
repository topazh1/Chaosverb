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
| Accuracy model (v1) | **Prediction only** | Geometry + established acoustic formulas. No mic measurement in v1. |
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

3. **The honest caveat** — Geometry-based prediction is an excellent *first pass* but is
   not the same as *measuring* the room. The gold standard is measure → treat →
   re-measure (Phase 2 adds a mic-sweep measurement mode). v1 is explicitly a
   "smart advisor," and the UI must say so.

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
│   ├─ ReverbTime          Sabine/Eyring
│   └─ Recommendation      aggregates → RecommendationReport
├─ Visualize/              SceneKit/RealityKit 3D + AR overlay of placements
├─ Resources/              monitors.json, materials.json, panels.json
└─ Tests/                  AcousticsTests (known-answer cases)
```

**Tech stack:** Swift + SwiftUI · RoomPlan · ARKit/RealityKit · SceneKit for 3D ·
Swift Package for the `Acoustics` module (so it stays portable & testable).

---

## 5. User flow (v1)

1. **Onboarding:** "This is an advisor, not a mic measurement" + permissions (camera).
2. **Scan:** Walk around the room (RoomPlan), or tap corners (fallback). Get dimensions.
3. **Tag materials:** Tap each wall/floor/ceiling → pick material chip.
4. **Add gear:** Pick monitor model from list; enter stand height + desk size.
5. **Compute:** Engine runs → **RecommendationReport**.
6. **Results:**
   - 3D room with desk/monitors in recommended spots (drag to compare vs current).
   - Panel & bass-trap markers with sizes.
   - Plain-language report: "Move your desk back 40 cm. You have a strong 58 Hz
     resonance. Put a 60×120 panel here ⌖. Add corner traps in the front corners."
7. **Save / share** the plan (PDF + saved project).

---

## 6. Phased roadmap

- **Phase 1 (MVP):** RoomPlan capture + manual inputs + full prediction engine + 3D
  result view + text report + PDF export. **Sellable on its own.**
- **Phase 1.5:** AR overlay — point camera, see panel markers pinned on real walls.
- **Phase 2:** Mic measurement mode — sine sweep → recorded frequency response →
  before/after verification (uncalibrated mic, relative comparison + calibration profiles).
- **Phase 3:** Auto-detect from video (Vision/Core ML) to pre-fill materials & gear;
  product database with prices/buy links; Android/cross-platform port.

---

## 7. Risks & mitigations

| Risk | Mitigation |
|---|---|
| Non-LiDAR devices can't auto-scan | ARKit corner-tap fallback + manual dimension entry. |
| Users overtrust predictions | UI framing as "advisor"; Phase 2 measurement for verification. |
| Material tagging is tedious | Sensible defaults per surface; auto-detect (Phase 3) pre-fills. |
| Monitor DB coverage | Ship top ~100 models; "generic + enter dimensions" fallback. |

---

## 8. Immediate next steps (when we start building)

1. Scaffold the Xcode/SwiftUI project + `Acoustics` Swift Package.
2. Implement `RoomModes`, `ReverbTime`, `Reflections` with unit tests (known-answer cases
   from textbook examples) — engine first, no UI needed to validate the physics.
3. Wire RoomPlan capture → `RoomModel`.
4. Build the results/report UI on top of the validated engine.
