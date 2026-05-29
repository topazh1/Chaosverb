import XCTest
@testable import Acoustics

// Expected values cross-checked against validation/oracle.py.
final class AcousticsTests: XCTestCase {

    let room = RoomDimensions(length: 5.0, width: 4.0, height: 3.0)

    func testRoomGeometry() {
        XCTAssertEqual(room.volume, 60, accuracy: 1e-9)
        XCTAssertEqual(room.surfaceArea, 94, accuracy: 1e-9)   // 2(20+15+12)
    }

    func testAxialModeFundamentals() {
        let modes = RoomModes.calculate(room)
        func freq(_ n: (Int, Int, Int)) -> Double? {
            modes.first { ($0.nx, $0.ny, $0.nz) == n }?.frequency
        }
        XCTAssertEqual(freq((1, 0, 0))!, 34.30, accuracy: 0.05)   // 343/(2·5)
        XCTAssertEqual(freq((0, 1, 0))!, 42.88, accuracy: 0.05)   // 343/(2·4)
        XCTAssertEqual(freq((0, 0, 1))!, 57.17, accuracy: 0.05)   // 343/(2·3)
    }

    func testModeCountsAndOrdering() {
        let modes = RoomModes.calculate(room)
        XCTAssertEqual(modes.count, 122)
        XCTAssertEqual(modes.filter { $0.type == .axial }.count, 12)
        XCTAssertTrue(modes.allSatisfy { (20...300).contains($0.frequency) })
        XCTAssertEqual(modes, modes.sorted { $0.frequency < $1.frequency })
    }

    func testPileupDetection() {
        let modes = RoomModes.calculate(room)
        let pileups = RoomModes.pileups(modes)
        XCTAssertFalse(pileups.isEmpty)
        // 54.91 Hz and 57.17 Hz are within 5%.
        XCTAssertTrue(pileups.contains { abs($0.0.frequency - 54.91) < 0.1 && abs($0.1.frequency - 57.17) < 0.1 })
    }

    func testSabineRT60() {
        let surfaces = [Surface(area: room.surfaceArea, absorption: 0.15)]
        let (rt, a) = ReverbTime.sabine(volume: room.volume, surfaces: surfaces)
        XCTAssertEqual(a, 14.10, accuracy: 0.01)
        XCTAssertEqual(rt, 0.685, accuracy: 0.01)
        let extra = ReverbTime.absorptionToTarget(volume: room.volume, currentAbsorption: a, targetRT60: 0.30)
        XCTAssertEqual(extra, 18.10, accuracy: 0.1)
    }

    func testReflectionPointLandsOnWall() {
        let src = Point3(x: 1.0, y: 1.2, z: 1.2)
        let lis = Point3(x: 2.9, y: 2.0, z: 1.2)
        let p = Reflections.reflectionPoint(source: src, listener: lis, axis: .x, planeCoord: 0.0)
        XCTAssertEqual(p.x, 0.0, accuracy: 1e-9)               // on the wall
        XCTAssertEqual(p.y, 1.41, accuracy: 0.01)              // matches oracle
        XCTAssertTrue((1.2...2.0).contains(p.y))
    }

    func testSBIRNulls() {
        XCTAssertEqual(SBIR.firstNull(distance: 0.6), 142.9, accuracy: 0.5)   // 343/2.4
        XCTAssertTrue(SBIR.nullIsProblematic(distance: 1.0))                  // 85.8 Hz in band
        XCTAssertFalse(SBIR.nullIsProblematic(distance: 0.3))                 // 285.8 Hz out of band
    }

    func testPlacementIsEquilateralAt38Percent() {
        let s = Placement.recommend(room: room, monitorFrontDistance: 0.5)
        XCTAssertEqual(s.listener.x, 1.9, accuracy: 1e-9)            // 0.38 * 5
        func dist(_ a: Point3, _ b: Point3) -> Double {
            (pow(a.x-b.x,2) + pow(a.y-b.y,2) + pow(a.z-b.z,2)).squareRoot()
        }
        let lr = dist(s.leftMonitor, s.rightMonitor)
        XCTAssertEqual(lr, dist(s.leftMonitor, s.listener), accuracy: 1e-6)
        XCTAssertEqual(lr, dist(s.rightMonitor, s.listener), accuracy: 1e-6)
        XCTAssertEqual(lr, s.monitorSpacing, accuracy: 1e-6)
        XCTAssertEqual(s.toeInDegrees, 30)
        XCTAssertTrue(s.fits(in: room))
    }

    func testCatalogLoadsBundledData() {
        XCTAssertFalse(Catalog.monitors.isEmpty)
        XCTAssertFalse(Catalog.materials.isEmpty)
        XCTAssertFalse(Catalog.panels.isEmpty)
        XCTAssertEqual(Catalog.monitor(id: "yamaha_hs8")?.brand, "Yamaha")
        // Absorption coefficients must be in [0,1].
        for m in Catalog.materials {
            for band in OctaveBand.allCases {
                XCTAssertTrue((0...1).contains(m.coefficient(at: band)), "\(m.id) \(band)")
            }
        }
    }

    func testRecommendationReportIsCoherent() {
        let tags = MaterialTags(byBoundary: [
            .floor: "carpet_thin", .ceiling: "gypsum_ceiling",
            .frontWall: "drywall", .backWall: "drywall",
            .leftWall: "drywall", .rightWall: "glass",
        ])
        let report = RecommendationEngine.generate(room: room, tags: tags)
        XCTAssertFalse(report.isMeasured)
        XCTAssertGreaterThan(report.rt60, 0)
        // Has at least the core categories.
        let cats = Set(report.items.map(\.category))
        XCTAssertTrue(cats.contains(.placement))
        XCTAssertTrue(cats.contains(.reflection))
        XCTAssertTrue(cats.contains(.bassTrap))
        // Sorted high → low priority.
        XCTAssertEqual(report.items, report.items.sorted { $0.priority > $1.priority })
        // Reflection items carry AR locations.
        XCTAssertTrue(report.items.filter { $0.category == .reflection }.allSatisfy { $0.location != nil })
    }
}
