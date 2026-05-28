#!/usr/bin/env python3
"""
Known-answer oracle for the Acoustics engine.

This is NOT app code. It exists to validate the physics formulas (and produce
expected values) that the Swift `Acoustics` module must reproduce in its unit
tests. Run: python3 oracle.py
"""
import math

C = 343.0  # speed of sound, m/s @ 20C


# ---------------------------------------------------------------- room modes
def room_modes(L, W, H, max_order=4, fmin=20.0, fmax=300.0):
    modes = []
    for nx in range(max_order + 1):
        for ny in range(max_order + 1):
            for nz in range(max_order + 1):
                if nx == ny == nz == 0:
                    continue
                f = (C / 2) * math.sqrt((nx / L) ** 2 + (ny / W) ** 2 + (nz / H) ** 2)
                if not (fmin <= f <= fmax):
                    continue
                nonzero = sum(1 for n in (nx, ny, nz) if n > 0)
                kind = {1: "axial", 2: "tangential", 3: "oblique"}[nonzero]
                modes.append((round(f, 2), kind, (nx, ny, nz)))
    modes.sort()
    return modes


def modal_pileups(modes, tol=0.05):
    """Pairs of modes within `tol` fractional spacing = audible boom."""
    pile = []
    fs = [m[0] for m in modes]
    for i in range(len(fs) - 1):
        if fs[i] > 0 and (fs[i + 1] - fs[i]) / fs[i] <= tol:
            pile.append((fs[i], fs[i + 1]))
    return pile


# ------------------------------------------------------------------- RT60
def rt60_sabine(volume, areas_and_coeffs):
    """areas_and_coeffs: list of (surface_area_m2, absorption_coeff)."""
    A = sum(s * a for s, a in areas_and_coeffs)
    return 0.161 * volume / A, A


def absorption_to_target(volume, current_A, target_rt60):
    """Extra sabins needed to reach target RT60."""
    needed_A = 0.161 * volume / target_rt60
    return needed_A - current_A


# ---------------------------------------------------- first reflection point
def reflection_point_on_plane(source, listener, axis, plane_coord):
    """
    Mirror-image method. Reflect `source` across the plane (axis = 0:x,1:y,2:z
    at coordinate plane_coord), then intersect image->listener with the plane.
    Returns the (x,y,z) reflection point where a panel goes.
    """
    img = list(source)
    img[axis] = 2 * plane_coord - source[axis]
    # parametric line img + t*(listener-img); solve for axis == plane_coord
    denom = listener[axis] - img[axis]
    t = (plane_coord - img[axis]) / denom
    return tuple(img[i] + t * (listener[i] - img[i]) for i in range(3))


# --------------------------------------------------------------------- SBIR
def sbir_first_null(distance_to_boundary):
    """Quarter-wavelength cancellation null from a boundary at distance d."""
    return C / (4 * distance_to_boundary)


# =================================================================== checks
def approx(a, b, tol=0.05):
    return abs(a - b) <= tol


def main():
    print("=== Room modes: 5.0 x 4.0 x 3.0 m ===")
    modes = room_modes(5.0, 4.0, 3.0)
    axial = [m for m in modes if m[1] == "axial"]
    print(f"total modes 20-300Hz: {len(modes)}; axial: {len(axial)}")
    # hand-check axial fundamentals:
    #   length 5.0 -> 343/10 = 34.30 Hz
    #   width  4.0 -> 343/8  = 42.875 Hz
    #   height 3.0 -> 343/6  = 57.167 Hz
    assert approx(axial[0][0], 34.30), axial[0]
    assert approx([m[0] for m in axial if m[2] == (0, 1, 0)][0], 42.88)
    assert approx([m[0] for m in axial if m[2] == (0, 0, 1)][0], 57.17)
    print("  first axial modes:", [m for m in axial][:3])
    print("  pileups:", modal_pileups(modes)[:5])

    print("\n=== RT60 Sabine: 5x4x3, avg a=0.15 ===")
    V = 5 * 4 * 3                       # 60 m3
    surf = 2 * (5 * 4) + 2 * (5 * 3) + 2 * (4 * 3)   # 94 m2
    rt, A = rt60_sabine(V, [(surf, 0.15)])
    print(f"  V={V} S={surf} A={A:.2f} RT60={rt:.3f}s")
    assert approx(rt, 0.685, 0.01), rt      # 0.161*60/14.1 = 0.685
    extra = absorption_to_target(V, A, 0.30)
    print(f"  sabins to hit 0.30s target: +{extra:.2f}")
    assert extra > 0

    print("\n=== First reflection point ===")
    # room 5(L,x) x 4(W,y) x 3(H,z); left wall plane x=0
    # monitor (source) and listener both at y=2 (centered), z=1.2 (ear height)
    src = (1.0, 1.2, 1.2)        # left monitor near front, off-center
    lis = (2.9, 2.0, 1.2)        # listening position
    p = reflection_point_on_plane(src, lis, axis=0, plane_coord=0.0)
    print(f"  left-wall reflection point: ({p[0]:.2f}, {p[1]:.2f}, {p[2]:.2f})")
    assert approx(p[0], 0.0, 1e-6)              # lies on the wall
    assert 1.2 <= p[1] <= 2.0                   # between source.y and listener.y

    print("\n=== SBIR null ===")
    for d in (0.3, 0.6, 1.0):
        print(f"  d={d}m -> first null {sbir_first_null(d):.1f} Hz")
    assert approx(sbir_first_null(0.6), 142.9, 0.5)   # 343/2.4

    print("\nALL KNOWN-ANSWER CHECKS PASSED")


if __name__ == "__main__":
    main()
