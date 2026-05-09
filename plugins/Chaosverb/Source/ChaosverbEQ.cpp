#include "ChaosverbEQ.h"

namespace nbs {

ChaosverbEQ::ChaosverbEQ() = default;

void ChaosverbEQ::prepare(const juce::dsp::ProcessSpec &spec) {
  currentSampleRate = spec.sampleRate;

  juce::dsp::ProcessSpec monoSpec{spec.sampleRate, spec.maximumBlockSize, 1};

  lowCutL.prepare(monoSpec);
  lowCutL.setType(juce::dsp::StateVariableTPTFilterType::highpass);
  lowCutL.setResonance(0.65f);  // Gentle analog-style bump at cutoff
  lowCutR.prepare(monoSpec);
  lowCutR.setType(juce::dsp::StateVariableTPTFilterType::highpass);
  lowCutR.setResonance(0.65f);

  highCutL.prepare(monoSpec);
  highCutL.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
  highCutL.setResonance(0.55f);  // Slight warmth at cutoff
  highCutR.prepare(monoSpec);
  highCutR.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
  highCutR.setResonance(0.55f);

  tiltLowL.prepare(monoSpec);
  tiltLowR.prepare(monoSpec);
  tiltHighL.prepare(monoSpec);
  tiltHighR.prepare(monoSpec);

  // Fixed 200Hz damping: bell cut -4dB @ 200Hz, Q=0.9 (wider for smoother correction)
  // Compensates for Clouds reverb internal allpass resonance around 200Hz
  dampL200.prepare(monoSpec);
  dampR200.prepare(monoSpec);
  auto damp200Coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
      currentSampleRate, 200.0f, 0.9f,
      juce::Decibels::decibelsToGain(-4.0f));
  *dampL200.coefficients = *damp200Coeffs;
  *dampR200.coefficients = *damp200Coeffs;

  lowCutSmoother.reset(currentSampleRate, 0.05);   // 50ms smoothing
  highCutSmoother.reset(currentSampleRate, 0.15);   // 150ms for zipper-free sweeps
  tiltSmoother.reset(currentSampleRate, 0.08);       // 80ms tilt smoothing

  cachedLowCut = -1.0f;
  cachedHighCut = -1.0f;
  cachedTilt = -999.0f;
  smoothedTiltVal = 0.0f;
}

void ChaosverbEQ::reset() {
  lowCutL.reset();
  lowCutR.reset();
  highCutL.reset();
  highCutR.reset();
  tiltLowL.reset();
  tiltLowR.reset();
  tiltHighL.reset();
  tiltHighR.reset();
  dampL200.reset();
  dampR200.reset();
}

void ChaosverbEQ::updateTiltCoefficients(float tiltVal) {
  const float t = tiltVal / 100.0f;
  // ±9dB range (increased from ±6dB) for more dramatic character shaping
  const float tiltGainDb = t * 9.0f;

  // Wider frequency spread: 400Hz low / 4kHz high (was 600/3000)
  // Lower Q (0.5) for smoother, broader shelves with less ringing
  auto lowShelf = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
      currentSampleRate, 400.0f, 0.5f,
      juce::Decibels::decibelsToGain(-tiltGainDb));
  auto highShelf = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
      currentSampleRate, 4000.0f, 0.5f,
      juce::Decibels::decibelsToGain(tiltGainDb));

  *tiltLowL.coefficients = *lowShelf;
  *tiltLowR.coefficients = *lowShelf;
  *tiltHighL.coefficients = *highShelf;
  *tiltHighR.coefficients = *highShelf;
}

void ChaosverbEQ::update(float lowCutHz, float highCutHz, float tiltVal) {
  if (std::abs(lowCutHz - cachedLowCut) > 0.5f) {
    cachedLowCut = lowCutHz;
    lowCutSmoother.setTargetValue(lowCutHz);
  }

  if (std::abs(highCutHz - cachedHighCut) > 1.0f) {
    cachedHighCut = highCutHz;
    highCutSmoother.setTargetValue(highCutHz);
  }

  if (std::abs(tiltVal - cachedTilt) > 0.1f) {
    cachedTilt = tiltVal;
    tiltSmoother.setTargetValue(tiltVal);
  }
}

void ChaosverbEQ::process(int numSamples, float *dataL, float *dataR) {
  if (dataL == nullptr || dataR == nullptr)
    return;

  // Optimization: setCutoffFrequency triggers tan() coefficient recalculation.
  // Only call per-sample when smoother is actively interpolating;
  // otherwise set once per block and skip the per-sample overhead.
  const bool lowSmoothing = lowCutSmoother.isSmoothing();
  const bool highSmoothing = highCutSmoother.isSmoothing();
  const bool tiltSmoothing = tiltSmoother.isSmoothing();

  if (!lowSmoothing) {
    const float freq = lowCutSmoother.getCurrentValue();
    lowCutL.setCutoffFrequency(freq);
    lowCutR.setCutoffFrequency(freq);
  }
  if (!highSmoothing) {
    const float freq = highCutSmoother.getCurrentValue();
    highCutL.setCutoffFrequency(freq);
    highCutR.setCutoffFrequency(freq);
  }
  if (!tiltSmoothing) {
    // Update once per block when not smoothing
    const float tv = tiltSmoother.getCurrentValue();
    if (std::abs(tv - smoothedTiltVal) > 0.05f) {
      smoothedTiltVal = tv;
      updateTiltCoefficients(tv);
    }
  }

  // Tilt coefficient update interval during smoothing (~every 32 samples)
  constexpr int kTiltUpdateInterval = 32;
  int tiltCounter = 0;

  for (int n = 0; n < numSamples; ++n) {
    if (lowSmoothing) {
      const float v = lowCutSmoother.getNextValue();
      lowCutL.setCutoffFrequency(v);
      lowCutR.setCutoffFrequency(v);
    }
    if (highSmoothing) {
      const float v = highCutSmoother.getNextValue();
      highCutL.setCutoffFrequency(v);
      highCutR.setCutoffFrequency(v);
    }
    if (tiltSmoothing) {
      const float tv = tiltSmoother.getNextValue();
      if (++tiltCounter >= kTiltUpdateInterval) {
        tiltCounter = 0;
        smoothedTiltVal = tv;
        updateTiltCoefficients(tv);
      }
    }

    dataL[n] = lowCutL.processSample(0, dataL[n]);
    dataR[n] = lowCutR.processSample(0, dataR[n]);

    dataL[n] = highCutL.processSample(0, dataL[n]);
    dataR[n] = highCutR.processSample(0, dataR[n]);

    // 200Hz resonance damping (fixed, always active)
    dataL[n] = dampL200.processSample(dataL[n]);
    dataR[n] = dampR200.processSample(dataR[n]);

    dataL[n] = tiltLowL.processSample(dataL[n]);
    dataL[n] = tiltHighL.processSample(dataL[n]);

    dataR[n] = tiltLowR.processSample(dataR[n]);
    dataR[n] = tiltHighR.processSample(dataR[n]);
  }
}

} // namespace nbs
