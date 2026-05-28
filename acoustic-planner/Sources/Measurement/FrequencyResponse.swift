import Foundation

/// One measured point: SPL (and optional phase) at a frequency.
public struct ResponsePoint: Equatable {
    public let frequency: Double   // Hz
    public let spl: Double         // dB
    public let phase: Double?      // degrees, if present
    public init(frequency: Double, spl: Double, phase: Double? = nil) {
        self.frequency = frequency; self.spl = spl; self.phase = phase
    }
}

/// A measured magnitude response (e.g. from a REW text export).
public struct FrequencyResponse: Equatable {
    public let points: [ResponsePoint]
    public init(points: [ResponsePoint]) { self.points = points }
}

/// A detected resonance (peak) or cancellation (dip) in a measurement.
public struct Peak: Equatable {
    public enum Kind { case peak, dip }
    public let frequency: Double      // Hz
    public let levelDelta: Double     // dB relative to the smoothed trend (+peak / −dip)
    public let kind: Kind
    public init(frequency: Double, levelDelta: Double, kind: Kind) {
        self.frequency = frequency; self.levelDelta = levelDelta; self.kind = kind
    }
}

/// Where the measurement mic was placed — only listening-position measurements
/// are used to reconcile modes/RT60.
public enum MicPosition: Equatable {
    case listening, sub, left, right
    case other(String)
}

/// Provenance harvested from a REW export header.
public struct MeasurementMeta: Equatable {
    public var rewVersion: String?
    public var date: String?
    public var smoothing: String?
    public var note: String?
    public init() {}
}
