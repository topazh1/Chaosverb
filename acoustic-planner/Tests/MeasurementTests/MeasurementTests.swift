import XCTest
@testable import Measurement
import Acoustics

final class MeasurementTests: XCTestCase {

    // A minimal REW-style "Export measurement as text" fixture.
    let rewText = """
    * Measurement data measured by REW V5.20
    * Source: USB UMIK-1
    * Dated: 2026-01-15 10:30:00
    * Smoothing: 1/12 octave
    *
    * Freq(Hz) SPL(dB) Phase(degrees)
    20.000\t70.10\t-120.0
    30.000\t71.50\t-95.0
    40.000\t72.00\t-60.0
    50.000\t73.20\t-30.0
    57.000\t82.40\t-5.0
    65.000\t73.10\t20.0
    80.000\t72.80\t55.0
    100.000\t72.50\t90.0
    120.000\t72.20\t110.0
    150.000\t71.90\t130.0
    """

    func testParseValidREWText() throws {
        let result = try REWTextParser.parse(rewText)
        XCTAssertEqual(result.response.points.count, 10)
        XCTAssertEqual(result.response.points.first?.frequency, 20.0)
        XCTAssertEqual(result.response.points[4].frequency, 57.0)
        XCTAssertEqual(result.response.points[4].spl, 82.4, accuracy: 1e-6)
        XCTAssertEqual(result.response.points[4].phase, -5.0)
        XCTAssertEqual(result.meta.rewVersion, "Measurement data measured by REW V5.20")
        XCTAssertNotNil(result.meta.smoothing)
    }

    func testParseRejectsTooFewPoints() {
        let tiny = "* header\n20 70\n30 71\n"
        XCTAssertThrowsError(try REWTextParser.parse(tiny)) { error in
            guard case REWTextParser.ParseError.tooFewPoints(let n) = error else {
                return XCTFail("wrong error: \(error)")
            }
            XCTAssertEqual(n, 2)
        }
    }

    func testParseRejectsNoData() {
        XCTAssertThrowsError(try REWTextParser.parse("* only comments\n# nothing here\n"))
    }

    func testPeakDetectionFindsResonance() throws {
        let result = try REWTextParser.parse(rewText)
        let peaks = PeakDetector.detect(result.response, threshold: 3.0)
        let peak = peaks.first { $0.kind == .peak }
        XCTAssertNotNil(peak)
        XCTAssertEqual(peak!.frequency, 57.0, accuracy: 0.1)
        XCTAssertGreaterThan(peak!.levelDelta, 3.0)
    }

    func testReconciliationConfirmsMeasuredMode() throws {
        let room = RoomDimensions(length: 5.0, width: 4.0, height: 3.0)
        let modes = RoomModes.calculate(room)
        let result = try REWTextParser.parse(rewText)
        let peaks = PeakDetector.detect(result.response, threshold: 3.0)

        let report = Reconciliation.reconcile(modes: modes, peaks: peaks)
        // The 57 Hz (0,0,1) height mode should be CONFIRMED by the measured peak.
        let heightMode = report.modeFindings.first { ($0.mode.nx, $0.mode.ny, $0.mode.nz) == (0, 0, 1) }
        XCTAssertEqual(heightMode?.status, .confirmed)
        XCTAssertNotNil(heightMode?.measuredLevel)
        // The 54.9 Hz (1,1,0) tangential mode is also within ±5% of the 57 Hz peak.
        let tangential = report.modeFindings.first { ($0.mode.nx, $0.mode.ny, $0.mode.nz) == (1, 1, 0) }
        XCTAssertEqual(tangential?.status, .confirmed)
        // Exactly those two modes fall in the match window; the rest are damped.
        XCTAssertEqual(report.confirmedCount, 2)
        XCTAssertGreaterThan(report.modeFindings.filter { $0.status == .damped }.count, 0)
        XCTAssertTrue(report.unexplained.isEmpty)
    }
}
