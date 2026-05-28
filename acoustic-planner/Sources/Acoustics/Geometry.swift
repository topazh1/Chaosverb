import Foundation

/// Speed of sound in air at ~20 °C, in metres/second.
public let speedOfSound: Double = 343.0

/// Room interior dimensions in metres. Axes: x = length, y = width, z = height.
public struct RoomDimensions: Equatable {
    public let length: Double
    public let width: Double
    public let height: Double

    public init(length: Double, width: Double, height: Double) {
        self.length = length
        self.width = width
        self.height = height
    }

    public var volume: Double { length * width * height }

    /// Total interior surface area (all six boundaries).
    public var surfaceArea: Double {
        2 * (length * width + length * height + width * height)
    }
}

/// A point in room coordinates (metres), origin at one floor corner.
public struct Point3: Equatable {
    public let x, y, z: Double
    public init(x: Double, y: Double, z: Double) {
        self.x = x; self.y = y; self.z = z
    }
}

public enum Axis { case x, y, z }
