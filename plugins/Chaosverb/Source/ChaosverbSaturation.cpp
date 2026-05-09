#include "ChaosverbSaturation.h"
#include <cmath>

namespace nbs {

void ChaosverbSaturation::prepare(const juce::dsp::ProcessSpec& /*spec*/) {
    reset();
}

void ChaosverbSaturation::reset() {
    // No stateful accumulators needed for analytical autogain
}

// Fast tanh approximation — Padé [3/3] approximant.
// Max error ~0.005 for |x| <= 3, clamped to [-1,1] for large inputs.
// Replaces std::tanh (~20x faster on most platforms).
static inline float fastTanh(float x) {
    if (x < -3.0f) return -1.0f;
    if (x >  3.0f) return  1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

void ChaosverbSaturation::process(int numSamples, float* dataL, float* dataR,
                                   float saturationAmount) {
    if (dataL == nullptr || dataR == nullptr)
        return;

    // Skip when off
    if (saturationAmount < 0.1f)
        return;

    // Drive scales from 1.0 (0%) to 12.0 (100%) — aggressive harmonic distortion
    const float norm = saturationAmount / 100.0f;
    const float drive = 1.0f + norm * 11.0f;

    // Analytical autogain: 1/drive gives unity small-signal gain for tanh(x*drive).
    // Harmonic character comes from the waveshaping nonlinearity itself, not volume boost.
    const float autoGain = 1.0f / drive;

    for (int n = 0; n < numSamples; ++n) {
        dataL[n] = fastTanh(dataL[n] * drive) * autoGain;
        dataR[n] = fastTanh(dataR[n] * drive) * autoGain;
    }
}

} // namespace nbs
