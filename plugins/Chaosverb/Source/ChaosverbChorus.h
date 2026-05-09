#pragma once

#include <juce_dsp/juce_dsp.h>

namespace nbs {

// Subtle stereo chorus driven by the Mod Rate parameter.
// At modRate = 0: fully bypassed (no pitch/chorus artifacts).
// As modRate increases: a gentle chorus with slow LFO movement
// is crossfaded in, independent from the WowFlutter section.
class ChaosverbChorus {
public:
    ChaosverbChorus() = default;
    ~ChaosverbChorus() = default;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    // Process stereo buffer in-place.
    // modRateHz: 0 = bypass, >0 = increasing chorus presence
    // modRateMax: maximum modRate value (for normalization)
    void process(int numSamples, float* dataL, float* dataR,
                 float modRateHz, float modRateMax);

private:
    double currentSampleRate = 48000.0;

    // Stereo delay lines for chorus modulation
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>
        chorusDelayL, chorusDelayR;

    // Smooth the chorus mix to prevent zipper noise
    juce::SmoothedValue<float> chorusMixSmoother;

    // LFO phases — offset for stereo decorrelation
    float lfoPhaseL = 0.0f;
    float lfoPhaseR = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChaosverbChorus)
};

} // namespace nbs
