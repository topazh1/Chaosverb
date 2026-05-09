#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>

namespace nbs {

// Pre-reverb spectral shaping compressor with FDR (Frequency Dependent Ratio)
// dynamic EQ and Momentary LUFS auto-gain compensation.
//
// Placed before the reverb engine to reshape the tonal balance of the input
// signal, changing how the reverb responds (e.g. taming lows before they
// muddy the tail, or softening highs before they create harsh reflections).
//
// Signal flow:
//   Input → Peak Detector → Gain Computer (8:1) → Gain Cell → FDR Dynamic EQ
//         → LUFS Auto-Gain → Parallel Blend → Output → Reverb
//
// Auto-gain uses ITU-R BS.1770-4 K-weighted momentary loudness (400ms window)
// to match the compressed signal's perceived loudness to the input, ensuring
// consistent level into the reverb regardless of squash amount.
class ChaosverbSpectralSquash {
public:
    ChaosverbSpectralSquash() = default;
    ~ChaosverbSpectralSquash() = default;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    // Process stereo buffer in-place.
    // squashAmount: -100 to +100 (0 = bypass, abs value = compression intensity)
    void process(int numSamples, float* dataL, float* dataR, float squashAmount);

private:
    double sampleRate_ = 48000.0;

    // Per-sample smoothing of squash blend amount (~15ms ramp)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedSquashNorm_;

    // Wideband compressor: fixed 8:1 ratio, -42dBFS threshold
    float envelope_ = 0.0f;
    float attackCoeff_ = 0.0f;
    float releaseCoeff_ = 0.0f;

    // GR smoothing (~2ms one-pole lowpass for zipper-free coefficient updates)
    float smoothedGR_ = 0.0f;
    float grSmoothCoeff_ = 0.0f;

    // FDR dynamic EQ: 2 bands (low shelf + high shelf)
    static constexpr int kNumFDRBands = 2;

    struct BiquadState {
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    };
    struct BiquadCoeffs {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    };

    std::array<std::array<BiquadState, 2>, kNumFDRBands> fdrStates_{};
    std::array<BiquadCoeffs, kNumFDRBands> fdrCoeffs_{};
    float cachedGRForCoeffs_ = 0.0f;

    // --- LUFS Auto-Gain (ITU-R BS.1770-4) ---
    // K-weighting: Stage 1 = high shelf (~1682Hz, +4dB), Stage 2 = high-pass (~38Hz)
    BiquadCoeffs kWeightStage1_;
    BiquadCoeffs kWeightStage2_;

    // K-weight filter states: [stage][channel], separate for input and compressed paths
    std::array<std::array<BiquadState, 2>, 2> kWeightInputStates_{};   // input path
    std::array<std::array<BiquadState, 2>, 2> kWeightCompStates_{};    // compressed path

    // Momentary LUFS: 400ms exponential mean-square accumulators
    float inputMeanSq_ = 0.0f;
    float compMeanSq_ = 0.0f;
    float lufsIntegCoeff_ = 0.0f;  // one-pole coefficient for 400ms window

    // Smoothed auto-makeup gain (prevents jumps in compensation)
    float autoMakeupGain_ = 1.0f;
    float makeupSmoothCoeff_ = 0.0f;  // ~50ms smoothing on makeup changes

    void updateFDRCoefficients(float grDb);
    void computeKWeightCoefficients();
    static float processBiquad(float input, const BiquadCoeffs& c, BiquadState& s);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChaosverbSpectralSquash)
};

} // namespace nbs
