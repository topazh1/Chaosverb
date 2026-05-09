#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <cmath>

#include "clouds_reverb.h"

//==============================================================================
/**
 * ChaosverbFDN
 *
 * Drop-in replacement wrapper around the Mutable Instruments Clouds
 * Dattorro plate reverb algorithm. Preserves the exact public interface
 * of the original 8-line FDN so that PluginProcessor.cpp requires
 * no changes.
 *
 * Original FDN parameters are mapped to Clouds controls:
 *   feedbackGain    -> set_time()       (0-1 passthrough)
 *   modDepthSamples -> set_amount()     (normalized by 80)
 *   spectralTilt    -> set_lp()         (bipolar -100..+100 to 0..1)
 *   topologyBlend   -> set_diffusion()  (0-2 range halved to 0-1)
 *   resonance       -> set_input_gain() (0.2 at 0% to 1.0 at 100%)
 *   modRateHz       -> set_lfo_frequency() (sample-rate compensated)
 */
struct ChaosverbFDN {
  //==========================================================================
  // Public constants accessed by PluginProcessor
  static constexpr int kNumLines = 8;

  // Delay lengths — kept for PluginProcessor's T60 feedback gain calculation.
  // These are set in prepare() proportional to sample rate, matching the
  // original FDN behavior so the decay parameter continues to work correctly.
  static constexpr std::array<int, kNumLines> kDelayLengths48k_L = {
      1447, 1621, 1873, 2143, 2311, 2677, 2963, 3191};
  static constexpr std::array<int, kNumLines> kDelayLengths48k_R = {
      1453, 1637, 1889, 2161, 2339, 2693, 2999, 3209};

  // Maximum LFO depth at 48kHz — accessed by PluginProcessor for mod depth scaling
  static constexpr float kMaxLFODepth48k = 80.0f;

  //==========================================================================
  // Public data members accessed by PluginProcessor
  std::array<int, kNumLines> delayLengthsL{};
  std::array<int, kNumLines> delayLengthsR{};
  float maxLFODepthSamples = kMaxLFODepth48k;
  float cachedSpectralTilt = -999.0f;
  float cachedResonance = -1.0f;
  float cachedLFORate_ = -1.0f;

  //==========================================================================
  void prepare(const juce::dsp::ProcessSpec& spec) {
    sampleRate_ = spec.sampleRate;
    const double srRatio = spec.sampleRate / 48000.0;

    // Scale delay lengths proportionally (used by PluginProcessor for T60 calc)
    for (int i = 0; i < kNumLines; ++i) {
      delayLengthsL[i] =
          static_cast<int>(std::ceil(kDelayLengths48k_L[i] * srRatio));
      delayLengthsR[i] =
          static_cast<int>(std::ceil(kDelayLengths48k_R[i] * srRatio));
    }

    // Scale max LFO depth to current sample rate
    maxLFODepthSamples = kMaxLFODepth48k * static_cast<float>(srRatio);

    // Initialize the Clouds reverb engine
    std::fill(std::begin(buffer_), std::end(buffer_), 0.0f);
    reverb_.Init(buffer_);

    // Set sensible defaults. 
    // We fix these values once during initialization, avoiding per-sample setup.
    reverb_.set_amount(1.0f);       // 100% wet output (external DryWetMixer handles mix)
    reverb_.set_time(0.7f);
    reverb_.set_diffusion(0.625f);  // Clouds default plate diffusion
    // LP coefficient: higher = less HF attenuation in feedback loop = longer tails.
    // Original Clouds used 0.7 (designed for 32kHz with short tails).
    // 0.88 preserves broadband energy better, nearly doubling perceived RT60
    // while keeping natural darkening (-3dB at ~17kHz @ 48kHz).
    reverb_.set_lp(0.88f);
    reverb_.set_input_gain(0.5f);

    cachedSpectralTilt = -999.0f;
    cachedResonance = -1.0f;
    cachedLFORate_ = -1.0f;
  }

  //--------------------------------------------------------------------------
  void reset() {
    std::fill(std::begin(buffer_), std::end(buffer_), 0.0f);
    reverb_.Init(buffer_);

    // Restore last-known parameter state
    reverb_.set_amount(currentAmount_);
    reverb_.set_time(currentTime_);
    reverb_.set_diffusion(currentDiffusion_);
    reverb_.set_lp(currentLp_);
    reverb_.set_input_gain(currentInputGain_);
  }

  //==========================================================================
  void prepareLFO(float modRateHz, float modDepthPercent) {
    // Map LFO Rate knob to Clouds internal LFO frequencies.
    // LFO_1 = primary rate, LFO_2 = 0.6x primary (preserving original ratio).
    // SetLFOFrequency calls Init() which resets oscillator phase — only update
    // when the rate actually changes to avoid resetting every block (which
    // prevents the LFOs from oscillating and kills the reverb tail).
    const float sr = static_cast<float>(sampleRate_);
    const float clampedRate = juce::jlimit(0.01f, 20.0f, modRateHz);

    if (std::abs(clampedRate - cachedLFORate_) > 0.001f) {
      cachedLFORate_ = clampedRate;
      reverb_.set_lfo_frequency(clouds::LFO_1, clampedRate / sr);
      reverb_.set_lfo_frequency(clouds::LFO_2, clampedRate * 0.6f / sr);
    }

    // Wire Mod Depth knob (0-100%) to Clouds LFO amplitudes.
    // Clamped to [0,1] — amplitude scaling is bounded inside clouds_reverb.h
    // to stay within delay buffer limits.
    reverb_.set_mod_depth(juce::jlimit(0.0f, 1.0f, modDepthPercent / 100.0f));
  }

  //==========================================================================
  void updateShelfCoefficients(float /*spectralTiltValue*/) {
    // No-op: LP is now controlled by setDamping().
  }

  //==========================================================================
  // Frequency damping: bipolar control for spectral decay shaping.
  //   dampValue < 0: HF damping (Clouds LP coefficient lowered)
  //   dampValue = 0: neutral (LP at 0.88, no HP)
  //   dampValue > 0: LF damping (post-reverb one-pole HP)
  void setDamping(float dampValue) {
    if (dampValue < 0.0f) {
      // HF damping: lower LP coefficient in Clouds feedback loop
      const float t = -dampValue / 100.0f;  // 0 to 1
      const float lp = 0.88f - t * 0.58f;   // 0.88 → 0.30
      currentLp_ = lp;
      reverb_.set_lp(lp);
      dampHPCoeff_ = 0.0f;
    } else if (dampValue > 0.0f) {
      // LF damping: post-reverb one-pole highpass
      currentLp_ = 0.88f;
      reverb_.set_lp(0.88f);
      const float t = dampValue / 100.0f;  // 0 to 1
      const float hpFreq = t * 500.0f;     // 0 → 500Hz
      dampHPCoeff_ = 1.0f - std::exp(
          -juce::MathConstants<float>::twoPi * hpFreq
          / static_cast<float>(sampleRate_));
    } else {
      currentLp_ = 0.88f;
      reverb_.set_lp(0.88f);
      dampHPCoeff_ = 0.0f;
    }
  }

  //==========================================================================
  void updateResonanceCoefficients(float /*resonanceValue*/) {
    // Resonance replaced by external Tape Saturation — fixed input gain.
    currentInputGain_ = 0.5f;
    reverb_.set_input_gain(0.5f);
  }

  //==========================================================================
  void setFeedbackGain(float feedbackGain) {
    // Only feedbackGain (from Decay param) maps to Clouds — rest are fixed
    const float time = juce::jlimit(0.0f, 1.0f, feedbackGain);
    if (std::abs(time - currentTime_) > 0.001f) {
      currentTime_ = time;
      reverb_.set_time(time);
    }
  }

  //==========================================================================
  void processSample(float inputL, float inputR, float& outL, float& outR)
  {
    FloatFrame frame;
    frame.l = inputL;
    frame.r = inputR;
    reverb_.Process(&frame, 1);

    outL = frame.l;
    outR = frame.r;

    // LF damping: one-pole HP applied to reverb output
    if (dampHPCoeff_ > 0.0001f) {
      dampHPStateL_ += dampHPCoeff_ * (outL - dampHPStateL_);
      outL -= dampHPStateL_;
      dampHPStateR_ += dampHPCoeff_ * (outR - dampHPStateR_);
      outR -= dampHPStateR_;
    }
  }

private:
  clouds::CloudsReverb reverb_;
  float                buffer_[16384]{};
  double               sampleRate_ = 48000.0;

  // Smoothed parameter storage for reset restoration
  float currentTime_      = 0.7f;
  float currentAmount_    = 0.5f;
  float currentLp_        = 0.88f;
  float currentDiffusion_ = 0.625f;
  float currentInputGain_ = 0.5f;

  // LF damping state (one-pole HP per channel)
  float dampHPCoeff_  = 0.0f;
  float dampHPStateL_ = 0.0f;
  float dampHPStateR_ = 0.0f;
};
