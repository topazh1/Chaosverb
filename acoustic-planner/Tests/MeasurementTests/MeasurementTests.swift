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

    // MARK: - Impulse response (WAV + Schroeder)

    /// Deterministic exponentially-decaying noise with a known RT60, encoded as
    /// a 16-bit mono PCM WAV.
    private func makeDecayWAV(rt60: Double, fs: Double = 48000) -> Data {
        let n = Int(fs * (rt60 * 1.5 + 0.2))
        var seed: UInt64 = 1
        func rnd() -> Double {                       // simple LCG in [-1,1]
            seed = seed &* 6364136223846793005 &+ 1442695040888963407
            return Double(Int64(bitPattern: seed)) / Double(Int64.max)
        }
        var pcm = Data()
        for i in 0..<n {
            let amp = pow(10.0, -3 * Double(i) / fs / rt60)
            let v = Int16(max(-1, min(1, amp * rnd())) * 32767)
            pcm.append(UInt8(truncatingIfNeeded: v)); pcm.append(UInt8(truncatingIfNeeded: v >> 8))
        }
        func le32(_ v: UInt32) -> Data { withUnsafeBytes(of: v.littleEndian) { Data($0) } }
        func le16(_ v: UInt16) -> Data { withUnsafeBytes(of: v.littleEndian) { Data($0) } }
        var wav = Data("RIFF".utf8)
        wav += le32(UInt32(36 + pcm.count)); wav += Data("WAVE".utf8)
        wav += Data("fmt ".utf8); wav += le32(16); wav += le16(1); wav += le16(1)
        wav += le32(UInt32(fs)); wav += le32(UInt32(fs) * 2); wav += le16(2); wav += le16(16)
        wav += Data("data".utf8); wav += le32(UInt32(pcm.count)); wav += pcm
        return wav
    }

    func testWAVDecodeAndRT60() throws {
        let ir = try WAVDecoder.decode(makeDecayWAV(rt60: 0.5))
        XCTAssertEqual(ir.sampleRate, 48000)
        XCTAssertGreaterThan(ir.samples.count, 24000)
        let rt = try XCTUnwrap(ReverbDecay.rt60(ir))
        XCTAssertEqual(rt, 0.5, accuracy: 0.075)        // within 15%
    }

    func testWAVDecodeRejectsGarbage() {
        XCTAssertThrowsError(try WAVDecoder.decode(Data([0, 1, 2, 3, 4, 5])))
    }

    // MARK: - Measured refinement

    func testMeasuredRefinementEscalatesAndOverridesRT60() throws {
        let room = RoomDimensions(length: 5.0, width: 4.0, height: 3.0)
        let predicted = RecommendationEngine.generate(room: room, tags: MaterialTags(uniform: "drywall"))
        XCTAssertFalse(predicted.isMeasured)

        let modes = RoomModes.calculate(room)
        let peaks = PeakDetector.detect(try REWTextParser.parse(rewText).response)
        let recon = Reconciliation.reconcile(modes: modes, peaks: peaks)

        let refined = MeasuredRefinement.refine(
            predicted: predicted, reconciliation: recon,
            measuredRT60: [.hz500: 0.42])

        XCTAssertTrue(refined.isMeasured)
        XCTAssertEqual(refined.rt60, 0.42, accuracy: 1e-9)     // measured overrides Sabine
        // The confirmed-resonance item is high priority and surfaces first.
        XCTAssertEqual(refined.items.first?.category, .bassTrap)
        XCTAssertTrue(refined.items.first?.title.contains("Measured") ?? false)
    }
}
