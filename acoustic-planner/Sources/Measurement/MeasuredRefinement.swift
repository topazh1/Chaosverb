import Foundation
import Acoustics

/// Refines a prediction-only `RecommendationReport` using imported measurement
/// data: confirmed modes get escalated, unexplained peaks become placement
/// fixes, and the representative RT60 is replaced by the measured value.
public enum MeasuredRefinement {

    public static func refine(
        predicted: RecommendationReport,
        reconciliation: Reconciliation.Report,
        measuredRT60: [OctaveBand: Double]? = nil
    ) -> RecommendationReport {
        var items = predicted.items

        // Confirmed modes → a high-priority, measurement-backed action.
        let confirmed = reconciliation.modeFindings
            .filter { $0.status == .confirmed }
            .sorted { ($0.measuredLevel ?? 0) > ($1.measuredLevel ?? 0) }
        if let worst = confirmed.first, let level = worst.measuredLevel {
            let freqs = confirmed.map { String(format: "%.0f Hz", $0.mode.frequency) }.joined(separator: ", ")
            let tuned = worst.mode.type == .axial && level > 6
            items.insert(RecommendationItem(
                category: .bassTrap, priority: .high,
                title: "Measured bass resonance confirmed",
                detail: String(format: "Your measurement shows real peaks at %@ (worst ≈ +%.1f dB at %.0f Hz). %@",
                               freqs, level, worst.mode.frequency,
                               tuned ? "This narrow peak warrants a tuned membrane/Helmholtz trap in addition to corner trapping."
                                     : "Prioritise corner bass trapping to tame it.")),
                at: 0)
        }

        // Unexplained measured peaks → not a room mode → placement / SBIR fix.
        for u in reconciliation.unexplained {
            items.append(RecommendationItem(
                category: .sbir, priority: .medium,
                title: String(format: "Unexplained peak at %.0f Hz", u.peak.frequency),
                detail: "No room mode matches this — likely speaker-boundary interference or a desk/furniture reflection. Adjust monitor-to-wall distance or treat the nearby surface rather than adding a bass trap."))
        }

        // Measured RT60 overrides the Sabine estimate (use 500 Hz, else mid-band mean).
        var rt60 = predicted.rt60
        if let m = measuredRT60, !m.isEmpty {
            rt60 = m[.hz500] ?? (m.values.reduce(0, +) / Double(m.count))
        }

        return RecommendationReport(
            setup: predicted.setup,
            problemModes: predicted.problemModes,
            rt60: rt60,
            absorptionToAddM2: predicted.absorptionToAddM2,
            items: items.sorted { $0.priority > $1.priority },
            isMeasured: true)
    }
}
