#!/usr/bin/env python3
"""Validates the non-trivial DSP/geometry that the Swift code must reproduce:
  1. Equilateral listening-triangle placement (38% rule).
  2. Schroeder-integration RT60 recovery from a synthetic impulse response.
"""
import math
import random


# ----------------------------------------------------- placement (38% rule)
def placement(L, W, monitor_front_distance, ear_height=1.2):
    listener_x = 0.38 * L
    depth = listener_x - monitor_front_distance        # listener->monitor plane
    side = 2 * depth / math.sqrt(3)                    # equilateral triangle side
    spacing = side
    half = spacing / 2
    cy = W / 2
    return {
        "listener": (listener_x, cy, ear_height),
        "left": (monitor_front_distance, cy - half, ear_height),
        "right": (monitor_front_distance, cy + half, ear_height),
        "spacing": spacing,
        "depth": depth,
        "side": side,
    }


def check_placement():
    p = placement(5.0, 4.0, 0.5)
    L, R, lis = p["left"], p["right"], p["listener"]
    d_LR = math.dist(L, R)
    d_Llis = math.dist(L, lis)
    d_Rlis = math.dist(R, lis)
    print(f"placement: spacing={p['spacing']:.3f} depth={p['depth']:.3f}")
    print(f"  |L-R|={d_LR:.3f}  |L-lis|={d_Llis:.3f}  |R-lis|={d_Rlis:.3f}")
    assert abs(d_LR - d_Llis) < 1e-6 and abs(d_LR - d_Rlis) < 1e-6, "not equilateral"
    assert abs(lis[0] - 1.9) < 1e-9                       # 0.38*5
    # listener angle between the two monitors should be 60 deg
    v1 = tuple(L[i] - lis[i] for i in range(3))
    v2 = tuple(R[i] - lis[i] for i in range(3))
    dot = sum(v1[i] * v2[i] for i in range(3))
    ang = math.degrees(math.acos(dot / (d_Llis * d_Rlis)))
    print(f"  listener angle between monitors = {ang:.2f} deg (expect 60)")
    assert abs(ang - 60.0) < 0.5
    print("  PLACEMENT OK\n")


# --------------------------------------------------- Schroeder RT60 from IR
def make_ir(fs, rt60, n=None):
    """Exponentially-decaying white noise with the given RT60."""
    if n is None:
        n = int(fs * (rt60 * 1.5 + 0.2))
    # amplitude envelope: decays 60 dB over rt60 seconds -> tau
    tau = rt60 / (math.log(10 ** 6))   # since exp(-t/tau): 60dB=10^6 energy? handle below
    out = []
    random.seed(1)
    for i in range(n):
        t = i / fs
        # energy decays 60 dB in rt60 s => amplitude ~ 10^(-3 t / rt60)
        amp = 10 ** (-3 * t / rt60)
        out.append(amp * (random.random() * 2 - 1))
    return out


def schroeder_rt60(ir, fs, lower=-5.0, upper=-25.0):
    """T20-style: fit slope between lower/upper dB on the Schroeder EDC."""
    energy = [s * s for s in ir]
    # backward cumulative integration
    edc = [0.0] * len(energy)
    acc = 0.0
    for i in range(len(energy) - 1, -1, -1):
        acc += energy[i]
        edc[i] = acc
    ref = edc[0]
    edc_db = [10 * math.log10(e / ref) if e > 0 else -120.0 for e in edc]
    # find times crossing lower and upper
    t_lower = t_upper = None
    for i, db in enumerate(edc_db):
        if t_lower is None and db <= lower:
            t_lower = i / fs
        if t_upper is None and db <= upper:
            t_upper = i / fs
            break
    if t_lower is None or t_upper is None:
        return None
    slope = (upper - lower) / (t_upper - t_lower)   # dB per second (negative)
    return -60.0 / slope


def check_schroeder():
    fs = 48000
    for true_rt in (0.3, 0.6, 1.0):
        ir = make_ir(fs, true_rt)
        est = schroeder_rt60(ir, fs)
        err = abs(est - true_rt) / true_rt * 100
        print(f"  RT60 true={true_rt:.2f}s  est={est:.3f}s  err={err:.1f}%")
        assert err < 10, f"RT60 recovery off by {err:.1f}%"
    print("  SCHROEDER OK\n")


if __name__ == "__main__":
    print("=== placement ===")
    check_placement()
    print("=== schroeder RT60 ===")
    check_schroeder()
    print("ALL DSP/GEOMETRY CHECKS PASSED")
