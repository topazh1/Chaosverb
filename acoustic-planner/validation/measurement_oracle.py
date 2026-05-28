#!/usr/bin/env python3
"""Verifies the Measurement test expectations (peak detect + reconciliation)
match the actual algorithm, using the same fixture as MeasurementTests.swift."""
import math
from oracle import room_modes

REW = [
    (20.0, 70.10), (30.0, 71.50), (40.0, 72.00), (50.0, 73.20), (57.0, 82.40),
    (65.0, 73.10), (80.0, 72.80), (100.0, 72.50), (120.0, 72.20), (150.0, 71.90),
]


def moving_average(xs, window):
    half = window // 2
    out = []
    for i in range(len(xs)):
        lo, hi = max(0, i - half), min(len(xs) - 1, i + half)
        out.append(sum(xs[lo:hi + 1]) / (hi - lo + 1))
    return out


def detect(points, window=9, threshold=3.0):
    if window % 2 == 0:
        window += 1
    spls = [p[1] for p in points]
    trend = moving_average(spls, window)
    peaks = []
    for i in range(1, len(points) - 1):
        delta = spls[i] - trend[i]
        is_max = spls[i] >= spls[i - 1] and spls[i] >= spls[i + 1]
        is_min = spls[i] <= spls[i - 1] and spls[i] <= spls[i + 1]
        if delta >= threshold and is_max:
            peaks.append((points[i][0], round(delta, 2), "peak"))
        elif -delta >= threshold and is_min:
            peaks.append((points[i][0], round(delta, 2), "dip"))
    return peaks


def reconcile(modes, peaks, tol=0.05):
    measured = [p for p in peaks if p[2] == "peak"]
    matched = set()
    confirmed, damped = [], []
    for f, kind, idxs in [(m[0], m[1], m[2]) for m in modes]:
        best = None
        for j, p in enumerate(measured):
            if abs(p[0] - f) / f <= tol:
                if best is None or p[1] > measured[best][1]:
                    best = j
        if best is not None:
            matched.add(best)
            confirmed.append((idxs, f))
        else:
            damped.append((idxs, f))
    unexplained = [p for j, p in enumerate(measured) if j not in matched]
    return confirmed, damped, unexplained


def main():
    peaks = detect(REW)
    print("peaks:", peaks)
    assert len(peaks) == 1 and peaks[0][2] == "peak"
    assert abs(peaks[0][0] - 57.0) < 0.1 and peaks[0][1] > 3.0

    modes = room_modes(5.0, 4.0, 3.0)
    confirmed, damped, unexplained = reconcile(modes, peaks)
    print("confirmed modes:", confirmed)
    print("unexplained:", unexplained)
    print(f"confirmedCount = {len(confirmed)}, damped = {len(damped)}")

    confirmed_idx = {c[0] for c in confirmed}
    assert (0, 0, 1) in confirmed_idx, "height mode should be confirmed"
    print("\nVERIFIED:",
          f"confirmedCount == {len(confirmed)}",
          "(both 54.9 Hz tangential + 57.2 Hz axial fall within 5% of the 57 Hz peak)")


if __name__ == "__main__":
    main()
