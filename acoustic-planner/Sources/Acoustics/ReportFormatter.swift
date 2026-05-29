import Foundation

/// Renders a `RecommendationReport` as human-readable text — the basis for the
/// in-app summary and the PDF/share export.
public enum ReportFormatter {

    public static func plainText(_ report: RecommendationReport) -> String {
        var out = "STUDIO ACOUSTIC PLAN\n"
        out += report.isMeasured ? "(refined with your REW measurement)\n" : "(prediction — import a REW measurement to refine)\n"
        out += String(repeating: "=", count: 48) + "\n\n"

        let s = report.setup
        out += "LISTENING SETUP\n"
        out += String(format: "  • Sit %.2f m from the front wall (38%% rule)\n", s.listener.x)
        out += String(format: "  • Monitors %.2f m apart, %.2f m ear height, 30° toe-in\n", s.monitorSpacing, s.earHeight)
        out += String(format: "  • Monitors %.2f m off the front wall\n\n", s.monitorDistanceFromFrontWall)

        out += String(format: "ROOM RESPONSE\n  • RT60 ≈ %.2f s%@\n", report.rt60,
                      report.isMeasured ? " (measured)" : " (estimated)")
        if report.absorptionToAddM2 > 0 {
            out += String(format: "  • Add ≈ %.1f m² of broadband absorption\n", report.absorptionToAddM2)
        }
        if !report.problemModes.isEmpty {
            out += "  • Watch modes: " + report.problemModes.map { String(format: "%.0f Hz", $0.frequency) }.joined(separator: ", ") + "\n"
        }
        out += "\nACTIONS (highest priority first)\n"
        for (i, item) in report.items.enumerated() {
            out += "  \(i + 1). [\(badge(item.priority))] \(item.title)\n     \(item.detail)\n"
            if let l = item.location {
                out += String(format: "     ↳ at (%.2f, %.2f, %.2f) m\n", l.x, l.y, l.z)
            }
        }
        return out
    }

    private static func badge(_ p: RecPriority) -> String {
        switch p {
        case .high: return "HIGH"
        case .medium: return "MED"
        case .low: return "LOW"
        }
    }
}
