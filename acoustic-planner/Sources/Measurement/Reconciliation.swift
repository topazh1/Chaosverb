import Foundation
import Acoustics

/// Merges the predicted acoustic model with an imported measurement so that
/// recommendations reflect what the room *actually* does, not just geometry.
public enum Reconciliation {

    /// Outcome of cross-checking one predicted mode against the measurement.
    public struct ModeFinding: Equatable {
        public enum Status: Equatable {
            case confirmed          // predicted AND measured → real problem, prioritize
            case damped             // predicted but NOT measured → already controlled, deprioritize
        }
        public let mode: RoomMode
        public let status: Status
        public let measuredLevel: Double?   // dB above trend, when confirmed
    }

    /// Measured problem with no matching predicted mode → not a room resonance
    /// (likely SBIR / comb-filter / leak) → fix via placement, not a trap.
    public struct UnexplainedPeak: Equatable {
        public let peak: Peak
    }

    public struct Report: Equatable {
        public let modeFindings: [ModeFinding]
        public let unexplained: [UnexplainedPeak]
        public var confirmedCount: Int {
            modeFindings.filter { $0.status == .confirmed }.count
        }
    }

    /// - Parameters:
    ///   - modes: predicted room modes.
    ///   - peaks: detected measured peaks/dips.
    ///   - tolerance: fractional frequency match window (default ±5%).
    public static func reconcile(
        modes: [RoomMode],
        peaks: [Peak],
        tolerance: Double = 0.05
    ) -> Report {
        let measuredPeaks = peaks.filter { $0.kind == .peak }
        var matchedPeakIdx = Set<Int>()
        var findings: [ModeFinding] = []

        for mode in modes {
            var matched: (idx: Int, level: Double)?
            for (idx, p) in measuredPeaks.enumerated() {
                if abs(p.frequency - mode.frequency) / mode.frequency <= tolerance {
                    // keep the strongest peak within the window
                    if matched == nil || p.levelDelta > matched!.level {
                        matched = (idx, p.levelDelta)
                    }
                }
            }
            if let m = matched {
                matchedPeakIdx.insert(m.idx)
                findings.append(ModeFinding(mode: mode, status: .confirmed, measuredLevel: m.level))
            } else {
                findings.append(ModeFinding(mode: mode, status: .damped, measuredLevel: nil))
            }
        }

        let unexplained = measuredPeaks.enumerated()
            .filter { !matchedPeakIdx.contains($0.offset) }
            .map { UnexplainedPeak(peak: $0.element) }

        return Report(modeFindings: findings, unexplained: unexplained)
    }
}
