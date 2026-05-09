#include "ChaosverbSpectralSquash.h"
#include <cmath>

namespace nbs {

// FDR band configuration — pre-computed K values from target ratios.
// K[b] = (1/R_f[b] - 1/R_main) / (1 - 1/R_main), clamped >= 0
// R_main = 8:1 (aggressive compression)
//
// FDR curve: relax compression on lows and highs, full 8:1 on mids.
//   Low shelf  200Hz: R_f=3.0:1 → K = (0.333 - 0.125) / 0.875 = 0.238
//   High shelf 4kHz:  R_f=2.5:1 → K = (0.4   - 0.125) / 0.875 = 0.314
struct FDRBandConfig {
    float freq;
    float Q;
    float K;
    bool isLowShelf;
};

static constexpr FDRBandConfig kFDRBands[2] = {
    { 200.0f,  0.707f, 0.238f, true  },
    { 4000.0f, 0.707f, 0.314f, false },
};

// Compressor constants — aggressive settings for clearly audible compression
static constexpr float kThresholdDb = -42.0f;
static constexpr float kRatio = 8.0f;
static constexpr float kRatioRecip = 1.0f / kRatio;  // 0.125

void ChaosverbSpectralSquash::prepare(const juce::dsp::ProcessSpec& spec) {
    sampleRate_ = spec.sampleRate;
    const float sr = static_cast<float>(spec.sampleRate);

    attackCoeff_   = 1.0f - std::exp(-1.0f / (0.002f * sr));  // ~2ms attack (faster grab)
    releaseCoeff_  = 1.0f - std::exp(-1.0f / (0.040f * sr));  // ~40ms release (tighter)
    grSmoothCoeff_ = 1.0f - std::exp(-1.0f / (0.002f * sr));  // ~2ms GR smoothing

    smoothedSquashNorm_.reset(spec.sampleRate, 0.015);  // 15ms ramp for click-free blend
    smoothedSquashNorm_.setCurrentAndTargetValue(0.0f);

    // LUFS integration: 400ms momentary window (exponential approximation)
    lufsIntegCoeff_ = 1.0f - std::exp(-1.0f / (0.4f * sr));

    // Makeup gain smoothing: ~50ms to prevent jumps
    makeupSmoothCoeff_ = 1.0f - std::exp(-1.0f / (0.050f * sr));

    // Compute K-weighting filter coefficients for this sample rate
    computeKWeightCoefficients();

    reset();
}

void ChaosverbSpectralSquash::reset() {
    envelope_ = 0.0f;
    smoothedGR_ = 0.0f;
    cachedGRForCoeffs_ = 0.0f;

    for (auto& band : fdrStates_)
        for (auto& ch : band)
            ch = {};

    // Initialize all FDR biquads to unity (flat passthrough)
    for (auto& c : fdrCoeffs_)
        c = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    // Reset K-weighting filter states
    for (auto& stage : kWeightInputStates_)
        for (auto& ch : stage)
            ch = {};
    for (auto& stage : kWeightCompStates_)
        for (auto& ch : stage)
            ch = {};

    // Reset LUFS accumulators
    inputMeanSq_ = 0.0f;
    compMeanSq_ = 0.0f;
    autoMakeupGain_ = 1.0f;
}

void ChaosverbSpectralSquash::computeKWeightCoefficients() {
    // ITU-R BS.1770-4 K-weighting filter, two stages:
    //   Stage 1: High shelf (models head acoustic effect)
    //            fc = 1681.97 Hz, gain = +4.0 dB, Q = 0.7072
    //   Stage 2: High-pass (revised low-frequency, "RLB" weighting)
    //            fc = 38.14 Hz, Q = 0.5003
    //
    // Coefficients computed analytically for arbitrary sample rate.

    const double sr = sampleRate_;
    const double pi = juce::MathConstants<double>::pi;
    const double twoPi = juce::MathConstants<double>::twoPi;

    // --- Stage 1: High shelf, fc=1681.97Hz, gain=+4.0dB ---
    {
        const double fc = 1681.974450955533;
        const double gainDb = 3.999843853973347;
        const double Q = 0.7071752369554196;

        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = twoPi * fc / sr;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double alpha = sinW0 / (2.0 * Q);
        const double sqrtA = std::sqrt(A);
        const double twoSqrtAAlpha = 2.0 * sqrtA * alpha;

        // High shelf coefficients (Audio EQ Cookbook)
        const double a0 = (A + 1.0) - (A - 1.0) * cosW0 + twoSqrtAAlpha;
        const double b0 = A * ((A + 1.0) + (A - 1.0) * cosW0 + twoSqrtAAlpha);
        const double b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosW0);
        const double b2 = A * ((A + 1.0) + (A - 1.0) * cosW0 - twoSqrtAAlpha);
        const double a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cosW0);
        const double a2 = (A + 1.0) - (A - 1.0) * cosW0 - twoSqrtAAlpha;

        const double invA0 = 1.0 / a0;
        kWeightStage1_.b0 = static_cast<float>(b0 * invA0);
        kWeightStage1_.b1 = static_cast<float>(b1 * invA0);
        kWeightStage1_.b2 = static_cast<float>(b2 * invA0);
        kWeightStage1_.a1 = static_cast<float>(a1 * invA0);
        kWeightStage1_.a2 = static_cast<float>(a2 * invA0);
    }

    // --- Stage 2: High-pass (RLB), fc=38.14Hz ---
    {
        const double fc = 38.13547087602444;
        const double Q = 0.5003270373238773;

        const double w0 = twoPi * fc / sr;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double alpha = sinW0 / (2.0 * Q);

        // HPF coefficients (Audio EQ Cookbook)
        const double a0 = 1.0 + alpha;
        const double b0 = (1.0 + cosW0) / 2.0;
        const double b1 = -(1.0 + cosW0);
        const double b2 = (1.0 + cosW0) / 2.0;
        const double a1 = -2.0 * cosW0;
        const double a2 = 1.0 - alpha;

        const double invA0 = 1.0 / a0;
        kWeightStage2_.b0 = static_cast<float>(b0 * invA0);
        kWeightStage2_.b1 = static_cast<float>(b1 * invA0);
        kWeightStage2_.b2 = static_cast<float>(b2 * invA0);
        kWeightStage2_.a1 = static_cast<float>(a1 * invA0);
        kWeightStage2_.a2 = static_cast<float>(a2 * invA0);
    }
}

void ChaosverbSpectralSquash::updateFDRCoefficients(float grDb) {
    // grDb is positive when compression is active (dB of gain reduction).
    // Each FDR band boosts by grDb * K[b] dB to partially undo the wideband
    // compression at that frequency, achieving a lower effective ratio.
    const double sr = sampleRate_;

    for (int b = 0; b < kNumFDRBands; ++b) {
        const auto& band = kFDRBands[b];
        const float boostDb = grDb * band.K;

        // Negligible boost → set to unity (avoids denormal coefficients)
        if (std::abs(boostDb) < 0.01f) {
            fdrCoeffs_[b] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
            continue;
        }

        // Audio EQ Cookbook: A = 10^(dBgain/40) for shelf filters
        const double A = std::pow(10.0, static_cast<double>(boostDb) / 40.0);
        const double w0 = juce::MathConstants<double>::twoPi * band.freq / sr;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double alpha = sinW0 / (2.0 * band.Q);
        const double sqrtA = std::sqrt(A);
        const double twoSqrtAAlpha = 2.0 * sqrtA * alpha;

        double b0, b1, b2, a0, a1, a2;

        if (band.isLowShelf) {
            a0 =        (A + 1.0) + (A - 1.0) * cosW0 + twoSqrtAAlpha;
            b0 = A  * ((A + 1.0) - (A - 1.0) * cosW0 + twoSqrtAAlpha);
            b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cosW0);
            b2 = A  * ((A + 1.0) - (A - 1.0) * cosW0 - twoSqrtAAlpha);
            a1 = -2.0  * ((A - 1.0) + (A + 1.0) * cosW0);
            a2 =        (A + 1.0) + (A - 1.0) * cosW0 - twoSqrtAAlpha;
        } else {
            // High shelf
            a0 =        (A + 1.0) - (A - 1.0) * cosW0 + twoSqrtAAlpha;
            b0 = A  * ((A + 1.0) + (A - 1.0) * cosW0 + twoSqrtAAlpha);
            b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosW0);
            b2 = A  * ((A + 1.0) + (A - 1.0) * cosW0 - twoSqrtAAlpha);
            a1 = 2.0   * ((A - 1.0) - (A + 1.0) * cosW0);
            a2 =        (A + 1.0) - (A - 1.0) * cosW0 - twoSqrtAAlpha;
        }

        const double invA0 = 1.0 / a0;
        fdrCoeffs_[b].b0 = static_cast<float>(b0 * invA0);
        fdrCoeffs_[b].b1 = static_cast<float>(b1 * invA0);
        fdrCoeffs_[b].b2 = static_cast<float>(b2 * invA0);
        fdrCoeffs_[b].a1 = static_cast<float>(a1 * invA0);
        fdrCoeffs_[b].a2 = static_cast<float>(a2 * invA0);
    }
}

float ChaosverbSpectralSquash::processBiquad(float input, const BiquadCoeffs& c,
                                              BiquadState& s) {
    const float out = c.b0 * input + c.b1 * s.x1 + c.b2 * s.x2
                    - c.a1 * s.y1 - c.a2 * s.y2;
    s.x2 = s.x1; s.x1 = input;
    s.y2 = s.y1; s.y1 = out;
    return out;
}

void ChaosverbSpectralSquash::process(int numSamples, float* dataL, float* dataR,
                                       float squashAmount) {
    if (dataL == nullptr || dataR == nullptr)
        return;

    const float targetNorm = std::abs(squashAmount) / 100.0f;
    smoothedSquashNorm_.setTargetValue(targetNorm);

    // Fast exit: smoother already at zero and target is zero — true bypass
    if (targetNorm < 0.005f && smoothedSquashNorm_.getCurrentValue() < 0.005f) {
        smoothedSquashNorm_.setCurrentAndTargetValue(0.0f);
        return;
    }

    const float threshLin = juce::Decibels::decibelsToGain(kThresholdDb);

    // Update FDR biquad coefficients once per block from previous block's
    // smoothed GR. One-block latency (~2.7ms at 48kHz/128) is inaudible.
    if (std::abs(smoothedGR_ - cachedGRForCoeffs_) > 0.05f) {
        cachedGRForCoeffs_ = smoothedGR_;
        updateFDRCoefficients(smoothedGR_);
    }

    for (int n = 0; n < numSamples; ++n) {
        const float squashNorm = smoothedSquashNorm_.getNextValue();

        const float dryL = dataL[n];
        const float dryR = dataR[n];

        // --- 1. Linked stereo peak detection ---
        const float peak = std::max(std::abs(dryL), std::abs(dryR));
        const float envCoeff = (peak > envelope_) ? attackCoeff_ : releaseCoeff_;
        envelope_ += envCoeff * (peak - envelope_);

        // --- 2. Gain computer: hard-knee 8:1 ---
        float grDb = 0.0f;
        if (envelope_ > threshLin) {
            const float envDb = 20.0f * std::log10(juce::jmax(1e-10f, envelope_));
            const float overDb = envDb - kThresholdDb;
            grDb = overDb * (1.0f - kRatioRecip);
        }

        // --- 3. Smooth GR for coefficient update ---
        smoothedGR_ += grSmoothCoeff_ * (grDb - smoothedGR_);

        // --- 4. Compressed signal (no fixed makeup — LUFS auto-gain handles it) ---
        const float gainLin = std::exp(-grDb * 0.11512925f);
        float compL = dryL * gainLin;
        float compR = dryR * gainLin;

        // --- 5. FDR dynamic EQ on compressed signal only ---
        for (int b = 0; b < kNumFDRBands; ++b) {
            compL = processBiquad(compL, fdrCoeffs_[b], fdrStates_[b][0]);
            compR = processBiquad(compR, fdrCoeffs_[b], fdrStates_[b][1]);
        }

        // --- 6. LUFS auto-gain: K-weight both paths, measure, compensate ---
        // K-weight the input (dry) signal
        float kwInL = processBiquad(dryL, kWeightStage1_, kWeightInputStates_[0][0]);
        float kwInR = processBiquad(dryR, kWeightStage1_, kWeightInputStates_[0][1]);
        kwInL = processBiquad(kwInL, kWeightStage2_, kWeightInputStates_[1][0]);
        kwInR = processBiquad(kwInR, kWeightStage2_, kWeightInputStates_[1][1]);

        // K-weight the compressed signal
        float kwCompL = processBiquad(compL, kWeightStage1_, kWeightCompStates_[0][0]);
        float kwCompR = processBiquad(compR, kWeightStage1_, kWeightCompStates_[0][1]);
        kwCompL = processBiquad(kwCompL, kWeightStage2_, kWeightCompStates_[1][0]);
        kwCompR = processBiquad(kwCompR, kWeightStage2_, kWeightCompStates_[1][1]);

        // Accumulate mean-square (stereo sum, 400ms exponential window)
        const float inSq = kwInL * kwInL + kwInR * kwInR;
        const float compSq = kwCompL * kwCompL + kwCompR * kwCompR;
        inputMeanSq_ += lufsIntegCoeff_ * (inSq - inputMeanSq_);
        compMeanSq_ += lufsIntegCoeff_ * (compSq - compMeanSq_);

        // Compute target makeup gain: match compressed LUFS to input LUFS
        float targetMakeup = 1.0f;
        if (compMeanSq_ > 1e-12f) {
            targetMakeup = std::sqrt(inputMeanSq_ / compMeanSq_);
            // Clamp to reasonable range: max +24dB boost, no cut below unity
            targetMakeup = juce::jlimit(1.0f, 16.0f, targetMakeup);
        }

        // Smooth the makeup gain (~50ms) to prevent modulation artifacts
        autoMakeupGain_ += makeupSmoothCoeff_ * (targetMakeup - autoMakeupGain_);

        // Apply auto-gain to compressed signal
        compL *= autoMakeupGain_;
        compR *= autoMakeupGain_;

        // --- 7. Parallel blend: smoothed squashNorm controls dry/wet ---
        dataL[n] = dryL * (1.0f - squashNorm) + compL * squashNorm;
        dataR[n] = dryR * (1.0f - squashNorm) + compR * squashNorm;
    }
}

} // namespace nbs
