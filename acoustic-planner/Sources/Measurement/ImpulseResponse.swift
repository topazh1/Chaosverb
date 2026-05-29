import Foundation
import Acoustics

/// A decoded impulse response (mono, normalised to Double).
public struct ImpulseResponse: Equatable {
    public let samples: [Double]
    public let sampleRate: Double
    public init(samples: [Double], sampleRate: Double) {
        self.samples = samples; self.sampleRate = sampleRate
    }
}

/// Minimal WAV reader for REW impulse-response exports.
/// Supports PCM 16/24/32-bit integer and 32-bit IEEE float; downmixes to mono.
public enum WAVDecoder {

    public enum DecodeError: Error, Equatable {
        case notRIFF, missingFmt, missingData, unsupportedFormat(Int, bits: Int)
    }

    public static func decode(_ data: Data) throws -> ImpulseResponse {
        let b = [UInt8](data)
        func u32(_ o: Int) -> UInt32 { UInt32(b[o]) | UInt32(b[o+1])<<8 | UInt32(b[o+2])<<16 | UInt32(b[o+3])<<24 }
        func u16(_ o: Int) -> Int { Int(b[o]) | Int(b[o+1])<<8 }

        guard b.count > 44, b[0]==0x52, b[1]==0x49, b[2]==0x46, b[3]==0x46 else { throw DecodeError.notRIFF } // "RIFF"

        // Walk chunks after the 12-byte RIFF/WAVE header.
        var off = 12
        var fmt: (format: Int, channels: Int, rate: Double, bits: Int)?
        var dataRange: Range<Int>?
        while off + 8 <= b.count {
            let id = String(bytes: b[off..<off+4], encoding: .ascii) ?? ""
            let size = Int(u32(off+4))
            let body = off + 8
            if id == "fmt " {
                fmt = (u16(body), u16(body+2), Double(u32(body+4)), u16(body+14))
            } else if id == "data" {
                dataRange = body..<min(body+size, b.count)
            }
            off = body + size + (size & 1)   // chunks are word-aligned
        }
        guard let f = fmt else { throw DecodeError.missingFmt }
        guard let range = dataRange else { throw DecodeError.missingData }

        // Supported: PCM int 16/24/32 (format 1) and IEEE float 32 (format 3).
        let supported = (f.format == 1 && [16, 24, 32].contains(f.bits)) || (f.format == 3 && f.bits == 32)
        guard supported else { throw DecodeError.unsupportedFormat(f.format, bits: f.bits) }

        let bytesPerSample = f.bits / 8
        let frameSize = bytesPerSample * f.channels
        guard frameSize > 0, f.channels > 0 else { throw DecodeError.unsupportedFormat(f.format, bits: f.bits) }

        func sample(_ o: Int) -> Double {
            switch (f.format, f.bits) {
            case (1, 16):
                let v = Int16(bitPattern: UInt16(b[o]) | UInt16(b[o+1])<<8)
                return Double(v) / 32768.0
            case (1, 24):
                var v = Int(b[o]) | Int(b[o+1])<<8 | Int(b[o+2])<<16
                if v & 0x800000 != 0 { v -= 0x1000000 }
                return Double(v) / 8388608.0
            case (1, 32):
                let v = Int32(bitPattern: u32(o))
                return Double(v) / 2147483648.0
            case (3, 32):
                return Double(Float(bitPattern: u32(o)))
            default:
                return 0
            }
        }

        var mono: [Double] = []
        mono.reserveCapacity((range.count) / frameSize)
        var p = range.lowerBound
        while p + frameSize <= range.upperBound {
            var acc = 0.0
            for ch in 0..<f.channels { acc += sample(p + ch * bytesPerSample) }
            mono.append(acc / Double(f.channels))
            p += frameSize
        }
        return ImpulseResponse(samples: mono, sampleRate: f.rate)
    }
}

public enum ReverbDecay {

    /// RT60 from the Schroeder energy-decay curve, fitting the slope between
    /// `lowerDb` and `upperDb` (default −5…−25 dB = T20). Returns nil if the
    /// IR doesn't decay far enough to fit.
    public static func rt60(_ ir: ImpulseResponse, lowerDb: Double = -5, upperDb: Double = -25) -> Double? {
        let n = ir.samples.count
        guard n > 1, ir.sampleRate > 0 else { return nil }

        // Backward-integrated energy decay curve.
        var edc = [Double](repeating: 0, count: n)
        var acc = 0.0
        for i in stride(from: n - 1, through: 0, by: -1) {
            acc += ir.samples[i] * ir.samples[i]
            edc[i] = acc
        }
        let ref = edc[0]
        guard ref > 0 else { return nil }

        var tLower: Double?, tUpper: Double?
        for i in 0..<n {
            let db = 10 * log10(edc[i] / ref)
            if tLower == nil, db <= lowerDb { tLower = Double(i) / ir.sampleRate }
            if db <= upperDb { tUpper = Double(i) / ir.sampleRate; break }
        }
        guard let t0 = tLower, let t1 = tUpper, t1 > t0 else { return nil }
        let slope = (upperDb - lowerDb) / (t1 - t0)   // dB/s (negative)
        return -60.0 / slope
    }

    /// RT60 per octave band, by band-pass filtering the IR then running Schroeder.
    public static func rt60ByBand(_ ir: ImpulseResponse) -> [OctaveBand: Double] {
        var result: [OctaveBand: Double] = [:]
        for band in OctaveBand.allCases {
            let filtered = ImpulseResponse(
                samples: Biquad.bandpass(ir.samples, fs: ir.sampleRate, f0: band.frequency, q: 1.414),
                sampleRate: ir.sampleRate)
            if let rt = rt60(filtered) { result[band] = rt }
        }
        return result
    }
}

/// RBJ-cookbook constant-skirt band-pass biquad (Direct Form I).
enum Biquad {
    static func bandpass(_ x: [Double], fs: Double, f0: Double, q: Double) -> [Double] {
        guard fs > 0, f0 < fs / 2 else { return x }
        let w0 = 2 * Double.pi * f0 / fs
        let alpha = sin(w0) / (2 * q)
        let b0 = alpha, b1 = 0.0, b2 = -alpha
        let a0 = 1 + alpha, a1 = -2 * cos(w0), a2 = 1 - alpha
        let nb0 = b0/a0, nb1 = b1/a0, nb2 = b2/a0, na1 = a1/a0, na2 = a2/a0

        var y = [Double](repeating: 0, count: x.count)
        var x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0
        for i in x.indices {
            let out = nb0*x[i] + nb1*x1 + nb2*x2 - na1*y1 - na2*y2
            x2 = x1; x1 = x[i]; y2 = y1; y1 = out
            y[i] = out
        }
        return y
    }
}
