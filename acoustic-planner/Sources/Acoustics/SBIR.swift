import Foundation

/// Speaker-Boundary Interference Response.
public enum SBIR {

    /// First cancellation null from a boundary at `distance` metres
    /// (quarter-wavelength path-difference model): f = c / (4·d).
    public static func firstNull(distance: Double) -> Double {
        guard distance > 0 else { return .infinity }
        return speedOfSound / (4 * distance)
    }

    /// True when the first null lands inside the critical bass band (default
    /// 20–200 Hz), i.e. the monitor-to-wall distance will audibly suck out bass.
    public static func nullIsProblematic(distance: Double, band: ClosedRange<Double> = 20...200) -> Bool {
        band.contains(firstNull(distance: distance))
    }
}
