import Foundation

/// Parses REW's "Export measurement as text" files into a `FrequencyResponse`.
///
/// REW text format (stable across 5.x):
///   * comment / metadata lines begin with `*` (occasionally `#`)
///   * data rows are `Freq(Hz)  SPL(dB)  Phase(deg)`, whitespace- or comma-delimited
///   * frequency points may be log-spaced (e.g. 48/96 PPO) — never assumed linear
///
/// NOTE: tuned against REW's documented format. When a real sample export is
/// added to `samples/`, verify delimiter/column order here (see `detectDelimiter`).
public enum REWTextParser {

    public enum ParseError: Error, Equatable {
        case tooFewPoints(Int)
        case nonMonotonicFrequency
        case noNumericData
    }

    public struct Result: Equatable {
        public let response: FrequencyResponse
        public let meta: MeasurementMeta
    }

    public static func parse(_ text: String, minPoints: Int = 8) throws -> Result {
        var meta = MeasurementMeta()
        var points: [ResponsePoint] = []

        for rawLine in text.split(whereSeparator: \.isNewline) {
            let line = rawLine.trimmingCharacters(in: .whitespaces)
            if line.isEmpty { continue }

            if line.hasPrefix("*") || line.hasPrefix("#") {
                harvestMeta(from: line, into: &meta)
                continue
            }

            let fields = split(line)
            // A header/label row like "Freq(Hz) SPL(dB) Phase(deg)" — skip.
            guard let f = Double(fields.first ?? ""), fields.count >= 2 else { continue }
            guard let spl = Double(fields[1]) else { continue }
            let phase = fields.count >= 3 ? Double(fields[2]) : nil
            points.append(ResponsePoint(frequency: f, spl: spl, phase: phase))
        }

        guard !points.isEmpty else { throw ParseError.noNumericData }
        guard points.count >= minPoints else { throw ParseError.tooFewPoints(points.count) }
        // REW exports ascending frequency; reject corrupted/interleaved files.
        for i in points.indices.dropLast() where points[i + 1].frequency <= points[i].frequency {
            throw ParseError.nonMonotonicFrequency
        }
        return Result(response: FrequencyResponse(points: points), meta: meta)
    }

    /// Detect the field delimiter (tab → comma → whitespace) and split.
    private static func split(_ line: String) -> [String] {
        let sep: Character = line.contains("\t") ? "\t" : (line.contains(",") ? "," : " ")
        return line.split(whereSeparator: { $0 == sep || $0 == " " })
            .map { $0.trimmingCharacters(in: .whitespaces) }
            .filter { !$0.isEmpty }
    }

    private static func harvestMeta(from line: String, into meta: inout MeasurementMeta) {
        let lower = line.lowercased()
        if lower.contains("rew") && meta.rewVersion == nil {
            meta.rewVersion = line.trimmingCharacters(in: CharacterSet(charactersIn: "*# "))
        }
        if lower.contains("dated") || lower.contains("date") {
            meta.date = value(after: ":", in: line)
        }
        if lower.contains("smoothing") {
            meta.smoothing = value(after: ":", in: line)
        }
    }

    private static func value(after sep: Character, in line: String) -> String? {
        guard let idx = line.firstIndex(of: sep) else { return nil }
        let v = line[line.index(after: idx)...].trimmingCharacters(in: .whitespaces)
        return v.isEmpty ? nil : v
    }
}
