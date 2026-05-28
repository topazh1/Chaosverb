import Foundation

/// A bounded surface with a frequency-band absorption coefficient (0...1).
public struct Surface: Equatable {
    public let area: Double          // m²
    public let absorption: Double    // α for the band of interest
    public init(area: Double, absorption: Double) {
        self.area = area; self.absorption = absorption
    }
}

public enum ReverbTime {

    /// Sabine reverberation time: RT60 = 0.161·V / Σ(Sᵢ·αᵢ).
    /// Returns the time in seconds and the total absorption (sabins).
    public static func sabine(volume: Double, surfaces: [Surface]) -> (rt60: Double, totalAbsorption: Double) {
        let a = surfaces.reduce(0) { $0 + $1.area * $1.absorption }
        guard a > 0 else { return (.infinity, 0) }
        return (0.161 * volume / a, a)
    }

    /// Eyring RT60 — more accurate than Sabine for deader rooms (mean α high).
    /// RT60 = 0.161·V / (−S·ln(1 − ᾱ)), where ᾱ is the area-weighted mean.
    public static func eyring(volume: Double, surfaces: [Surface]) -> Double {
        let totalArea = surfaces.reduce(0) { $0 + $1.area }
        guard totalArea > 0 else { return .infinity }
        let meanAlpha = surfaces.reduce(0) { $0 + $1.area * $1.absorption } / totalArea
        guard meanAlpha > 0, meanAlpha < 1 else { return meanAlpha >= 1 ? 0 : .infinity }
        return 0.161 * volume / (-totalArea * log(1 - meanAlpha))
    }

    /// Extra absorption (sabins) needed to reach `targetRT60` from `currentAbsorption`.
    /// Positive = add treatment; negative = room is already deader than target.
    public static func absorptionToTarget(volume: Double, currentAbsorption: Double, targetRT60: Double) -> Double {
        let needed = 0.161 * volume / targetRT60
        return needed - currentAbsorption
    }
}
