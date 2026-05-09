#include "ChaosverbChorus.h"
#include <cmath>

namespace nbs {

void ChaosverbChorus::prepare(const juce::dsp::ProcessSpec& spec) {
    currentSampleRate = spec.sampleRate;

    juce::dsp::ProcessSpec monoSpec{spec.sampleRate, spec.maximumBlockSize, 1};

    // Max 4ms chorus delay (2.5ms center + 1.2ms depth + headroom)
    const int maxDelaySamples =
        static_cast<int>(std::ceil(0.004 * spec.sampleRate)) + 1;

    chorusDelayL.prepare(monoSpec);
    chorusDelayL.setMaximumDelayInSamples(maxDelaySamples);
    chorusDelayL.setDelay(0.0f);
    chorusDelayL.reset();

    chorusDelayR.prepare(monoSpec);
    chorusDelayR.setMaximumDelayInSamples(maxDelaySamples);
    chorusDelayR.setDelay(0.0f);
    chorusDelayR.reset();

    chorusMixSmoother.reset(currentSampleRate, 0.08); // 80ms smoothing

    // LFO phases with stereo offset
    lfoPhaseL = 0.0f;
    lfoPhaseR = juce::MathConstants<float>::halfPi; // 90-degree offset
}

void ChaosverbChorus::reset() {
    chorusDelayL.reset();
    chorusDelayR.reset();
}

void ChaosverbChorus::process(int numSamples, float* dataL, float* dataR,
                               float modRateHz, float modRateMax) {
    if (dataL == nullptr || dataR == nullptr)
        return;

    // Single knob controls rate, depth, and mix simultaneously.
    // norm: 0 = off, 1 = full chorus effect.
    const float norm = juce::jlimit(0.0f, 1.0f, modRateHz / modRateMax);

    // Below a tiny threshold, target mix = 0 (fully off)
    // Mix scales 0→25% for audible chorus without overwhelming reverb
    const float targetMix = (modRateHz < 0.02f) ? 0.0f : norm * 0.25f;
    chorusMixSmoother.setTargetValue(targetMix);

    // Skip processing when smoother has settled at zero
    if (!chorusMixSmoother.isSmoothing() && targetMix < 0.0001f)
        return;

    const float sr = static_cast<float>(currentSampleRate);
    const float twoPi = juce::MathConstants<float>::twoPi;

    // Knob-driven chorus parameters:
    // - Rate:  0.05 → 1.5 Hz (slow drift to audible movement)
    // - Depth: 0.3  → 1.2 ms (subtle detuning, not vibrato)
    // - Center delay fixed at 2.5ms for smooth blend
    const float chorusRate = 0.05f + norm * 1.45f;
    const float maxDepthMs = 0.3f + norm * 0.9f;
    const float centerDelayMs = 2.5f;

    const float centerDelay = centerDelayMs * 0.001f * sr;
    const float lfoInc = twoPi * chorusRate / sr;
    const float depthSamples = maxDepthMs * 0.001f * sr;

    for (int n = 0; n < numSamples; ++n) {
        const float mix = chorusMixSmoother.getNextValue();

        // Sine LFO — smooth derivative avoids pitch-shift clicks at peaks
        const float modL = depthSamples * std::sin(lfoPhaseL);
        const float modR = depthSamples * std::sin(lfoPhaseR);

        const float delayL = juce::jmax(1.0f, centerDelay + modL);
        const float delayR = juce::jmax(1.0f, centerDelay + modR);

        // Push dry into delay, read modulated
        chorusDelayL.pushSample(0, dataL[n]);
        chorusDelayR.pushSample(0, dataR[n]);
        const float chorusL = chorusDelayL.popSample(0, delayL);
        const float chorusR = chorusDelayR.popSample(0, delayR);

        // Mix: blend dry with chorus signal
        dataL[n] = dataL[n] * (1.0f - mix) + chorusL * mix;
        dataR[n] = dataR[n] * (1.0f - mix) + chorusR * mix;

        lfoPhaseL += lfoInc;
        lfoPhaseR += lfoInc;
        if (lfoPhaseL >= twoPi) lfoPhaseL -= twoPi;
        if (lfoPhaseR >= twoPi) lfoPhaseR -= twoPi;
    }
}

} // namespace nbs
