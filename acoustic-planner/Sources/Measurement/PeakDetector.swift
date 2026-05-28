import Foundation

/// Finds peaks (resonances) and dips (cancellations) in a measured response,
/// relative to a smoothed trend, within a frequency band.
public enum PeakDetector {

    /// - Parameters:
    ///   - response: the measured magnitude response.
    ///   - band: frequency range to search (default bass region, where room
    ///     problems dominate).
    ///   - smoothingWindow: number of neighbouring points for the moving-average
    ///     trend (odd; centred).
    ///   - threshold: minimum |deviation| from the trend, in dB, to report.
    public static func detect(
        _ response: FrequencyResponse,
        band: ClosedRange<Double> = 20...300,
        smoothingWindow: Int = 9,
        threshold: Double = 3.0
    ) -> [Peak] {
        let pts = response.points.filter { band.contains($0.frequency) }
        guard pts.count >= 3 else { return [] }

        let trend = movingAverage(pts.map(\.spl), window: max(3, smoothingWindow | 1))
        var peaks: [Peak] = []

        for i in 1..<(pts.count - 1) {
            let delta = pts[i].spl - trend[i]
            let isLocalMax = pts[i].spl >= pts[i - 1].spl && pts[i].spl >= pts[i + 1].spl
            let isLocalMin = pts[i].spl <= pts[i - 1].spl && pts[i].spl <= pts[i + 1].spl

            if delta >= threshold && isLocalMax {
                peaks.append(Peak(frequency: pts[i].frequency, levelDelta: delta, kind: .peak))
            } else if -delta >= threshold && isLocalMin {
                peaks.append(Peak(frequency: pts[i].frequency, levelDelta: delta, kind: .dip))
            }
        }
        return peaks
    }

    private static func movingAverage(_ xs: [Double], window: Int) -> [Double] {
        let half = window / 2
        return xs.indices.map { i in
            let lo = max(0, i - half), hi = min(xs.count - 1, i + half)
            var sum = 0.0
            for j in lo...hi { sum += xs[j] }
            return sum / Double(hi - lo + 1)
        }
    }
}
