import Foundation

public enum Reflections {

    /// First-reflection point via the mirror-image method.
    ///
    /// Reflect `source` (a monitor) across the plane defined by `axis = planeCoord`,
    /// then intersect the line from the image source to the `listener` with that
    /// plane. The returned point on the boundary is where an absorption panel goes.
    public static func reflectionPoint(
        source: Point3,
        listener: Point3,
        axis: Axis,
        planeCoord: Double
    ) -> Point3 {
        let (sCoord, lCoord): (Double, Double)
        switch axis {
        case .x: (sCoord, lCoord) = (source.x, listener.x)
        case .y: (sCoord, lCoord) = (source.y, listener.y)
        case .z: (sCoord, lCoord) = (source.z, listener.z)
        }
        let imgCoord = 2 * planeCoord - sCoord          // mirrored source coordinate
        let denom = lCoord - imgCoord
        // Parallel to the plane (no crossing) → no valid reflection on it.
        guard abs(denom) > 1e-9 else { return source }
        let t = (planeCoord - imgCoord) / denom

        func image(_ s: Double, _ axisCoord: Double, _ isAxis: Bool) -> Double {
            isAxis ? (2 * planeCoord - s) : s
        }
        let ix = image(source.x, planeCoord, axis == .x)
        let iy = image(source.y, planeCoord, axis == .y)
        let iz = image(source.z, planeCoord, axis == .z)
        return Point3(
            x: ix + t * (listener.x - ix),
            y: iy + t * (listener.y - iy),
            z: iz + t * (listener.z - iz)
        )
    }
}
