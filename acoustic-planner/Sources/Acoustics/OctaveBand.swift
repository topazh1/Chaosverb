import Foundation

/// Standard octave-band centre frequencies used for absorption coefficients
/// and measured RT60 reporting.
public enum OctaveBand: Int, CaseIterable, Codable {
    case hz125 = 125
    case hz250 = 250
    case hz500 = 500
    case hz1000 = 1000
    case hz2000 = 2000
    case hz4000 = 4000

    public var frequency: Double { Double(rawValue) }
}
