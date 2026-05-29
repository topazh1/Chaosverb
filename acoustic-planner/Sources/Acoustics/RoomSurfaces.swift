import Foundation

/// The six boundaries of a rectangular room.
public enum Boundary: String, CaseIterable {
    case frontWall, backWall, leftWall, rightWall, floor, ceiling
}

/// Maps each room boundary to a tagged material (from `Catalog`).
public struct MaterialTags: Equatable {
    public var byBoundary: [Boundary: String]    // boundary → material id
    public init(byBoundary: [Boundary: String]) { self.byBoundary = byBoundary }

    /// Convenience: tag every boundary with the same material.
    public init(uniform materialID: String) {
        byBoundary = Dictionary(uniqueKeysWithValues: Boundary.allCases.map { ($0, materialID) })
    }
}

public extension RoomDimensions {
    /// Area of each boundary, in m².
    func area(of boundary: Boundary) -> Double {
        switch boundary {
        case .frontWall, .backWall: return width * height
        case .leftWall, .rightWall: return length * height
        case .floor, .ceiling:      return length * width
        }
    }

    /// Build the absorption surface list for one octave band, given material tags.
    /// Untagged boundaries are treated as fully reflective (α = 0).
    func surfaces(tags: MaterialTags, at band: OctaveBand) -> [Surface] {
        Boundary.allCases.map { boundary in
            let alpha = tags.byBoundary[boundary]
                .flatMap { Catalog.material(id: $0) }
                .map { $0.coefficient(at: band) } ?? 0
            return Surface(area: area(of: boundary), absorption: alpha)
        }
    }
}
