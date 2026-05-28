import Foundation

public enum ModeType: String { case axial, tangential, oblique }

/// A standing-wave resonance of the room.
public struct RoomMode: Equatable {
    public let nx, ny, nz: Int
    public let frequency: Double   // Hz
    public let type: ModeType

    public init(nx: Int, ny: Int, nz: Int, frequency: Double, type: ModeType) {
        self.nx = nx; self.ny = ny; self.nz = nz
        self.frequency = frequency; self.type = type
    }
}

public enum RoomModes {

    /// Enumerate standing-wave modes within `range`, up to the given order per axis.
    ///
    ///     f(nx,ny,nz) = (c/2)·sqrt((nx/L)² + (ny/W)² + (nz/H)²)
    ///
    /// Axial modes (one nonzero index) are the strongest and most audible.
    public static func calculate(
        _ dims: RoomDimensions,
        maxOrder: Int = 4,
        range: ClosedRange<Double> = 20...300
    ) -> [RoomMode] {
        var modes: [RoomMode] = []
        for nx in 0...maxOrder {
            for ny in 0...maxOrder {
                for nz in 0...maxOrder where !(nx == 0 && ny == 0 && nz == 0) {
                    let f = (speedOfSound / 2) * (
                        pow(Double(nx) / dims.length, 2) +
                        pow(Double(ny) / dims.width, 2) +
                        pow(Double(nz) / dims.height, 2)
                    ).squareRoot()
                    guard range.contains(f) else { continue }
                    let nonzero = [nx, ny, nz].filter { $0 > 0 }.count
                    let type: ModeType = nonzero == 1 ? .axial : (nonzero == 2 ? .tangential : .oblique)
                    modes.append(RoomMode(nx: nx, ny: ny, nz: nz, frequency: f, type: type))
                }
            }
        }
        return modes.sorted { $0.frequency < $1.frequency }
    }

    /// Adjacent modes closer than `tolerance` (fractional) pile up into an
    /// audible boom. Input is assumed sorted ascending by frequency.
    public static func pileups(
        _ modes: [RoomMode],
        tolerance: Double = 0.05
    ) -> [(RoomMode, RoomMode)] {
        var result: [(RoomMode, RoomMode)] = []
        for i in modes.indices.dropLast() {
            let a = modes[i], b = modes[i + 1]
            if a.frequency > 0, (b.frequency - a.frequency) / a.frequency <= tolerance {
                result.append((a, b))
            }
        }
        return result
    }
}
