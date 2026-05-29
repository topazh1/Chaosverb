import Foundation

/// A building material with per-octave-band absorption coefficients.
public struct Material: Codable, Equatable, Identifiable {
    public let id: String
    public let name: String
    /// Keyed by octave-band centre frequency as a string ("125"…"4000").
    public let absorption: [String: Double]

    /// Absorption coefficient for a band (falls back to the nearest available).
    public func coefficient(at band: OctaveBand) -> Double {
        absorption[String(band.rawValue)] ?? absorption["500"] ?? 0
    }
}

public struct Monitor: Codable, Equatable, Identifiable {
    public let id: String
    public let brand: String
    public let model: String
    public let heightMm: Double
    public let widthMm: Double
    public let depthMm: Double
    public let port: String          // "front" | "rear"
    public let recommendedWallDistanceM: Double
}

public struct Panel: Codable, Equatable, Identifiable {
    public let id: String
    public let name: String
    public let type: String          // "broadband" | "bass_trap"
    public let thicknessMm: Double
    public let widthMm: Double
    public let heightMm: Double
    public let coverageM2: Double
}

/// Loads the bundled JSON data tables.
public enum Catalog {
    public static let materials: [Material] = load("materials")
    public static let monitors: [Monitor] = load("monitors")
    public static let panels: [Panel] = load("panels")

    public static func material(id: String) -> Material? { materials.first { $0.id == id } }
    public static func monitor(id: String) -> Monitor? { monitors.first { $0.id == id } }

    private static func load<T: Decodable>(_ name: String) -> [T] {
        guard let url = Bundle.module.url(forResource: name, withExtension: "json"),
              let data = try? Data(contentsOf: url),
              let decoded = try? JSONDecoder().decode([T].self, from: data)
        else { return [] }
        return decoded
    }
}
