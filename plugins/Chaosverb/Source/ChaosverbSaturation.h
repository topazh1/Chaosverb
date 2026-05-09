#pragma once

#include <juce_dsp/juce_dsp.h>

namespace nbs {

// Tape-style tanh saturation with analytical auto-gain compensation.
// Uses 1/drive to perfectly compensate small-signal gain increase.
class ChaosverbSaturation {
public:
    ChaosverbSaturation() = default;
    ~ChaosverbSaturation() = default;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    // Process stereo buffer in-place.
    // saturationAmount: 0-100% (0 = bypass, 100 = heavy warmth)
    void process(int numSamples, float* dataL, float* dataR, float saturationAmount);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChaosverbSaturation)
};

} // namespace nbs
