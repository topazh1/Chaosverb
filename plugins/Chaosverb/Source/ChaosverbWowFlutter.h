#pragma once

#include <juce_dsp/juce_dsp.h>

namespace nbs {

class ChaosverbWowFlutter {
public:
  ChaosverbWowFlutter();
  ~ChaosverbWowFlutter() = default;

  void prepare(const juce::dsp::ProcessSpec &spec);
  void reset();

  // Processes an interleaved stereo block (in-place)
  void process(int numSamples, float *dataL, float *dataR, float wfAmount,
               bool wfEnabled, float speedMultiplier = 1.0f);

private:
  double currentSampleRate = 48000.0;

  juce::dsp::DelayLine<float,
                       juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>
      wfDelayLineL;
  juce::dsp::DelayLine<float,
                       juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>
      wfDelayLineR;

  juce::SmoothedValue<float> wfAmountSmoother;

  float wfWowPhase = 0.0f;
  float wfFlutterPhase = 0.0f;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChaosverbWowFlutter)
};

} // namespace nbs
