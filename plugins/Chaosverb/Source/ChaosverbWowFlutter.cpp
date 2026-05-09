#include "ChaosverbWowFlutter.h"

namespace nbs {

ChaosverbWowFlutter::ChaosverbWowFlutter() = default;

void ChaosverbWowFlutter::prepare(const juce::dsp::ProcessSpec &spec) {
  currentSampleRate = spec.sampleRate;

  juce::dsp::ProcessSpec monoSpec{spec.sampleRate, spec.maximumBlockSize, 1};

  // Max 6ms (3ms center + 3ms modulation) — reduced for subtlety
  const int maxWfDelaySamples =
      static_cast<int>(std::ceil(0.006 * spec.sampleRate)) + 1;

  wfDelayLineL.prepare(monoSpec);
  wfDelayLineL.setMaximumDelayInSamples(maxWfDelaySamples);
  wfDelayLineL.setDelay(0.0f);
  wfDelayLineL.reset();

  wfDelayLineR.prepare(monoSpec);
  wfDelayLineR.setMaximumDelayInSamples(maxWfDelaySamples);
  wfDelayLineR.setDelay(0.0f);
  wfDelayLineR.reset();

  wfAmountSmoother.reset(currentSampleRate, 0.10); // 100ms smoothing for zipper-free sweeps

  // Reset wow/flutter LFO phases
  wfWowPhase = 0.0f;
  wfFlutterPhase = 0.0f;
}

void ChaosverbWowFlutter::reset() {
  wfDelayLineL.reset();
  wfDelayLineR.reset();
  wfWowPhase = 0.0f;
  wfFlutterPhase = 0.0f;
}

void ChaosverbWowFlutter::process(int numSamples, float *dataL, float *dataR,
                                  float wfAmount, bool wfEnabled,
                                  float speedMultiplier) {
  if (dataL == nullptr || dataR == nullptr)
    return;

  // Always update smoother target — even when disabled/zero — so transitions
  // ramp smoothly instead of snapping when re-enabled.
  const float targetAmount = (wfEnabled && wfAmount >= 0.001f) ? wfAmount / 100.0f : 0.0f;
  wfAmountSmoother.setTargetValue(targetAmount);

  const float sr = static_cast<float>(currentSampleRate);
  const float maxDepthMs = 2.0f; // reduced from 5ms for subtlety

  const float twoPi = juce::MathConstants<float>::twoPi;
  const float maxDepthSamples = maxDepthMs * 0.001f * sr;
  const float centerDelay = maxDepthSamples; // center point of modulation
  const float twoPiOverSr = twoPi / sr;

  for (int n = 0; n < numSamples; ++n) {
    const float wfNorm = wfAmountSmoother.getNextValue();

    // Smoothly blend delay time: when wfNorm=0, delay=1 sample (near-passthrough).
    // When wfNorm>0, delay ramps up to centerDelay + modulation.
    // This prevents the click caused by jumping between bypass and delay.
    const float activeDelay = 1.0f + wfNorm * (centerDelay - 1.0f);
    const float depthSamples = wfNorm * maxDepthSamples;

    // Wow: slow, deep pitch drift (tape transport instability)
    // Flutter: fast, shallow pitch wobble (head vibration)
    const float wowInc = (0.3f + wfNorm * 0.8f) * speedMultiplier * twoPiOverSr;
    const float flutterInc = (5.0f + wfNorm * 6.0f) * speedMultiplier * twoPiOverSr;

    const float sinWow = std::sin(wfWowPhase);
    const float sinFlutter = std::sin(wfFlutterPhase);
    const float cosWow = std::cos(wfWowPhase);
    const float cosFlutter = std::cos(wfFlutterPhase);

    const float modL = depthSamples * (0.7f * sinWow + 0.3f * sinFlutter);
    const float modR = depthSamples * (0.7f * cosWow + 0.3f * cosFlutter);

    // Always push/pop through delay lines to keep them filled with current audio.
    // When disabled (wfNorm=0), activeDelay=1 (near-passthrough, no latency).
    // When enabled, activeDelay ramps smoothly to centerDelay + modulation.
    wfDelayLineL.pushSample(0, dataL[n]);
    wfDelayLineR.pushSample(0, dataR[n]);
    dataL[n] = wfDelayLineL.popSample(0, juce::jmax(1.0f, activeDelay + modL));
    dataR[n] = wfDelayLineR.popSample(0, juce::jmax(1.0f, activeDelay + modR));

    wfWowPhase += wowInc;
    wfFlutterPhase += flutterInc;

    if (wfWowPhase >= twoPi)
      wfWowPhase -= twoPi;
    if (wfFlutterPhase >= twoPi)
      wfFlutterPhase -= twoPi;
  }
}

} // namespace nbs
