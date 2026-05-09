#pragma once

#include <juce_dsp/juce_dsp.h>

namespace nbs {

class ChaosverbEQ {
public:
  ChaosverbEQ();
  ~ChaosverbEQ() = default;

  void prepare(const juce::dsp::ProcessSpec &spec);
  void reset();

  // Updates coefficients based on parameters if they changed.
  void update(float lowCutHz, float highCutHz, float tiltVal);

  // Processes an interleaved stereo block (in-place)
  void process(int numSamples, float *dataL, float *dataR);

private:
  double currentSampleRate = 48000.0;

  // Low/high cut: SVF TPT filters (zero-delay feedback topology)
  juce::dsp::StateVariableTPTFilter<float> lowCutL, lowCutR;
  juce::dsp::StateVariableTPTFilter<float> highCutL, highCutR;

  // Tilt EQ: low shelf + high shelf (IIR biquad)
  // Wide frequency span (400Hz / 4kHz) with smooth Q for musical character
  juce::dsp::IIR::Filter<float> tiltLowL, tiltLowR;
  juce::dsp::IIR::Filter<float> tiltHighL, tiltHighR;

  // Fixed 200Hz damping — compensates for Clouds reverb internal allpass resonance
  juce::dsp::IIR::Filter<float> dampL200, dampR200;

  juce::SmoothedValue<float> lowCutSmoother;
  juce::SmoothedValue<float> highCutSmoother;
  juce::SmoothedValue<float> tiltSmoother;

  float cachedLowCut = -1.0f;
  float cachedHighCut = -1.0f;
  float cachedTilt = -999.0f;
  float smoothedTiltVal = 0.0f;

  // Internal: recalculate tilt shelf coefficients for a given tilt value
  void updateTiltCoefficients(float tiltVal);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChaosverbEQ)
};

} // namespace nbs
