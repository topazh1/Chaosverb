import Foundation

/// Recommended desk / monitor / listening geometry.
///
/// Axes: x = length (front wall at x=0), y = width, z = height.
public struct ListeningSetup: Equatable {
    public let listener: Point3
    public let leftMonitor: Point3
    public let rightMonitor: Point3
    public let monitorSpacing: Double             // m, centre-to-centre
    public let listeningDistance: Double          // m, monitor→listener (triangle side)
    public let toeInDegrees: Double               // each monitor angled toward listener
    public let monitorDistanceFromFrontWall: Double
    public let earHeight: Double                  // tweeter height = seated ears

    /// True if the monitors + a side clearance actually fit the room width.
    public func fits(in room: RoomDimensions, sideClearance: Double = 0.3) -> Bool {
        monitorSpacing + 2 * sideClearance <= room.width
    }
}

public enum Placement {

    /// Recommend a symmetric, equilateral listening triangle using the 38% rule.
    ///
    /// - The listener sits at 38% of the room length from the front wall (a modal
    ///   sweet spot).
    /// - The two monitors and the listener form an equilateral triangle, so the
    ///   listener subtends a 60° angle between the monitors and each monitor is
    ///   angled ~30° inward (toe-in).
    /// - Tweeters at seated ear height; monitors symmetric about the room centre.
    public static func recommend(
        room: RoomDimensions,
        monitorFrontDistance: Double = 0.5,
        earHeight: Double = 1.2
    ) -> ListeningSetup {
        let listenerX = 0.38 * room.length
        let depth = max(listenerX - monitorFrontDistance, 0.01)   // along-axis distance
        let side = 2 * depth / 3.0.squareRoot()                   // equilateral side
        let half = side / 2
        let cy = room.width / 2

        return ListeningSetup(
            listener: Point3(x: listenerX, y: cy, z: earHeight),
            leftMonitor: Point3(x: monitorFrontDistance, y: cy - half, z: earHeight),
            rightMonitor: Point3(x: monitorFrontDistance, y: cy + half, z: earHeight),
            monitorSpacing: side,
            listeningDistance: side,
            toeInDegrees: 30,
            monitorDistanceFromFrontWall: monitorFrontDistance,
            earHeight: earHeight
        )
    }
}
