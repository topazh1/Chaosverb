#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Parameter layout — 22 parameters: 12 Float + 10 Bool
// JUCE 8 requires juce::ParameterID { "id", 1 } format (not bare strings)
//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
ChaosverbAudioProcessor::createParameterLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;

  // -------------------------------------------------------------------------
  // Float parameters (12) — Audio controls, all lockable via mutation system
  // (except mutationInterval and crossfadeSpeed which are never randomized)
  // -------------------------------------------------------------------------

  // topology — FDN feedback matrix structure (0–100, linear, default 50)
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"topology", 1}, "Topology",
      juce::NormalisableRange<float>(0.0f, 100.0f, 0.0f, 1.0f), 50.0f));

  // decay — Reverb tail length (0.1–60s, logarithmic skew 0.35, default 4.0)
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"decay", 1}, "Decay",
      juce::NormalisableRange<float>(0.1f, 60.0f, 0.0f, 0.35f), 4.0f, "s"));

  // preDelay — Bipolar: left=BPM-synced divisions, center=0ms, right=free time
  // Range: -500 to +500 (symmetric = 12 o'clock at 0ms). Linear skew.
  // Negative = BPM sync mode, Positive = milliseconds.
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"preDelay", 1}, "Pre-Delay",
      juce::NormalisableRange<float>(-500.0f, 500.0f, 0.0f, 1.0f), 0.0f));

  // density — Echo density / diffusion (0–100%, linear, default 60)
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"density", 1}, "Density",
      juce::NormalisableRange<float>(0.0f, 100.0f, 0.0f, 1.0f), 60.0f, "%"));

  // spectralTilt — Per-band independent decay scaling (-100 to +100, linear,
  // default 0)
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"spectralTilt", 1}, "Spectral Tilt",
      juce::NormalisableRange<float>(-100.0f, 100.0f, 0.0f, 1.0f), 0.0f));

  // resonance — Narrow resonant feedback peaks in tail (0–100%, linear, default
  // 0)
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"resonance", 1}, "Resonance",
      juce::NormalisableRange<float>(0.0f, 100.0f, 0.0f, 1.0f), 0.0f, "%"));

  // modRate — Chorus rate (0–2Hz, logarithmic skew 0.4, default 0.3).
  // At 0 = bypass (no chorus/modulation), above 0 = subtle chorus.
  // Range capped at 2Hz for soft, slow chorus character.
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"modRate", 1}, "Chorus",
      juce::NormalisableRange<float>(0.0f, 2.0f, 0.0f, 0.4f), 0.3f, "Hz"));

  // modDepth — Amount of delay line length modulation (0–100%, linear, default
  // 20)
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"modDepth", 1}, "Mod Depth",
      juce::NormalisableRange<float>(0.0f, 100.0f, 0.0f, 1.0f), 50.0f, "%"));

  // flutterSpeed — Flutter LFO speed multiplier (-100 to +100, bipolar, default 0)
  // CCW = 0.25x speed (slower), Center = 1x, CW = 4x speed (faster)
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"flutterSpeed", 1}, "F. Speed",
      juce::NormalisableRange<float>(-100.0f, 100.0f, 0.0f, 1.0f), 0.0f));

  // mutationInterval — dual-mode fader.
  //   SYNC mode (intervalMode=0): integer 0–10 mapped to BPM divisions.
  //   MS mode   (intervalMode=1): continuous 0–5000, value IS milliseconds.
  // Range 0–5000 accommodates both modes. In SYNC mode, only 0–10 are used.
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"mutationInterval", 1}, "Mutation Interval",
      juce::NormalisableRange<float>(0.0f, 5000.0f), 2500.0f));  // SYNC step 5 = 1 BAR

  // intervalMode — 0 = SYNC (BPM-synced divisions), 1 = MS (free milliseconds)
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"intervalMode", 1}, "Interval Mode",
      juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 0.0f));

  // crossfadeSpeed — dual-mode like interval. 0–5000 range.
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"crossfadeSpeed", 1}, "Crossfade Speed",
      juce::NormalisableRange<float>(0.0f, 5000.0f), 2500.0f));  // SYNC step 5 = 1 BAR

  // crossfadeMode — 0 = SYNC, 1 = MS
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"crossfadeMode", 1}, "Crossfade Mode",
      juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 0.0f));

  // mutationAmount — Controls how drastic randomization is (0–100%, default 50)
  // At 0%: no change. At 100%: random within ±50% of current value (clamped).
  // The value represents the total range width around each parameter's current
  // normalized position.
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"mutationAmount", 1}, "Mutation Amount",
      juce::NormalisableRange<float>(0.0f, 100.0f, 0.0f, 1.0f), 50.0f, "%"));

  // width — Stereo spread of wet signal (0–400%, linear, default 100)
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"width", 1}, "Width",
      juce::NormalisableRange<float>(0.0f, 400.0f, 0.0f, 1.0f), 100.0f, "%"));

  // mix — Dry/wet balance (0–100%, linear, default 50)
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"mix", 1}, "Mix",
      juce::NormalisableRange<float>(0.0f, 100.0f, 0.0f, 1.0f), 50.0f, "%"));

  // outputLevel — Output gain trim (-12 to +24 dB, piecewise linear, default 0dB)
  // Piecewise: norm 0..0.5 → -12..0 dB, norm 0.5..1 → 0..+24 dB
  // This centers 0dB at 12 o'clock on the knob.
  {
    juce::NormalisableRange<float> levelRange(
        -12.0f, 24.0f,
        [](float, float, float norm) -> float {
          if (norm <= 0.5f)
            return -12.0f + (norm / 0.5f) * 12.0f;
          return ((norm - 0.5f) / 0.5f) * 24.0f;
        },
        [](float, float, float value) -> float {
          if (value <= 0.0f)
            return ((value + 12.0f) / 12.0f) * 0.5f;
          return 0.5f + (value / 24.0f) * 0.5f;
        },
        [](float, float, float value) -> float {
          return value;
        });
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputLevel", 1}, "Output Level",
        levelRange, 0.0f, "dB"));
  }

  // -------------------------------------------------------------------------
  // Output EQ parameters (3) — post-FDN wet signal shaping
  // -------------------------------------------------------------------------

  // lowCut — Highpass frequency on wet output (20–2000Hz, log skew, default 80)
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"lowCut", 1}, "Low Cut",
      juce::NormalisableRange<float>(20.0f, 2000.0f, 0.0f, 0.35f), 80.0f, "Hz"));

  // highCut — Lowpass frequency on wet output (1k–20kHz, log skew, default 10k)
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"highCut", 1}, "High Cut",
      juce::NormalisableRange<float>(1000.0f, 20000.0f, 0.0f, 0.35f), 10000.0f,
      "Hz"));

  // tilt — Output tilt EQ (-100 to +100, negative=warm/dark, positive=bright,
  // default -10)
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"tilt", 1}, "Tilt",
      juce::NormalisableRange<float>(-100.0f, 100.0f, 0.0f, 1.0f), -10.0f));

  // -------------------------------------------------------------------------
  // Wow & Flutter parameter (1) — combined depth+speed control
  // -------------------------------------------------------------------------

  // wowFlutterAmount — Combined wow/flutter intensity (0–100%, default 0)
  // Scales both modulation depth and rate together for a single-knob control.
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"wowFlutterAmount", 1}, "Wow & Flutter",
      juce::NormalisableRange<float>(0.0f, 100.0f, 0.0f, 1.0f), 0.0f, "%"));

  // duckingAmount — Wet signal ducking based on input envelope (0–100%, default
  // 0) When input is loud, the wet reverb signal is reduced proportionally. 0%
  // = no ducking, 100% = full ducking (wet goes silent during loud input).
  // Skew 3.0 = exponential knob feel (more resolution at low values).
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"duckingAmount", 1}, "Ducking",
      juce::NormalisableRange<float>(0.0f, 100.0f, 0.0f, 3.0f), 0.0f, "%"));

  // duckAttack — Ducking envelope attack time in ms (1–100ms, default 5)
  // Controls how fast the ducking responds to transients.
  // Short = instant grab, Long = gradual onset.
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"duckAttack", 1}, "Duck Attack",
      juce::NormalisableRange<float>(1.0f, 100.0f, 0.1f, 3.0f), 5.0f, "ms"));

  // duckRelease — Ducking envelope release time in ms (20–1000ms, default 510)
  // Controls how fast the wet signal returns after the dry input drops.
  // Short = punchy/responsive, Long = smooth/gradual. Default at midpoint.
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"duckRelease", 1}, "Duck Release",
      juce::NormalisableRange<float>(20.0f, 1000.0f, 1.0f, 2.0f), 510.0f,
      "ms"));

  // gravity — Frequency damping (-100 to +100, default 0)
  // Negative = HF damping (highs decay faster), Positive = LF damping (lows decay faster)
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"gravity", 1}, "Damping",
      juce::NormalisableRange<float>(-100.0f, 100.0f, 0.0f, 1.0f), 0.0f));

  // saturation — Tape saturation with auto-gain (0-100%, default 0)
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"saturation", 1}, "Saturation",
      juce::NormalisableRange<float>(0.0f, 100.0f, 0.0f, 1.0f), 0.0f, "%"));

  // -------------------------------------------------------------------------
  // Bool parameters (16) — Mutation lock controls + toggles
  // -------------------------------------------------------------------------

  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"topologyLock", 1}, "Topology Lock", false));

  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"decayLock", 1}, "Decay Lock", false));

  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"preDelayLock", 1}, "Pre-Delay Lock", false));

  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"densityLock", 1}, "Density Lock", false));

  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"spectralTiltLock", 1}, "Spectral Tilt Lock", false));

  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"resonanceLock", 1}, "Resonance Lock", false));

  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"modRateLock", 1}, "Chorus Lock", false));

  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"modDepthLock", 1}, "Mod Depth Lock", false));

  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"widthLock", 1}, "Width Lock", false));

  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"mixLock", 1}, "Mix Lock", false));

  // Tone lock controls (3) — protect tone params from mutation
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"lowCutLock", 1}, "Low Cut Lock", false));

  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"highCutLock", 1}, "High Cut Lock", false));

  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"tiltLock", 1}, "Tilt Lock", false));

  // Wow & Flutter lock + enable toggle
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"wowFlutterAmountLock", 1}, "Wow & Flutter Amount Lock",
      false));

  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"wowFlutterEnabled", 1}, "Wow & Flutter Enabled",
      false));

  // Output level lock
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"outputLevelLock", 1}, "Output Level Lock", false));

  // Ducking amount lock
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"duckingAmountLock", 1}, "Ducking Lock", false));

  // Duck attack lock
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"duckAttackLock", 1}, "Duck Attack Lock", false));

  // Duck release lock
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"duckReleaseLock", 1}, "Duck Release Lock", false));

  // Gravity (Swell) lock
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"gravityLock", 1}, "Gravity Lock", false));

  // Saturation lock
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"saturationLock", 1}, "Saturation Lock", false));

  // Flutter Speed lock
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"flutterSpeedLock", 1}, "F. Speed Lock", false));

  // bypass — Global effect bypass (default: off = effect active)
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"bypass", 1}, "Bypass", false));

  return layout;
}

//==============================================================================
ChaosverbAudioProcessor::ChaosverbAudioProcessor()
    : AudioProcessor(
          BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "Parameters", createParameterLayout()) {

  topologyParam = parameters.getRawParameterValue("topology");
  decayParam = parameters.getRawParameterValue("decay");
  preDelayParam = parameters.getRawParameterValue("preDelay");
  densityParam = parameters.getRawParameterValue("density");
  spectralTiltParam = parameters.getRawParameterValue("spectralTilt");
  resonanceParam = parameters.getRawParameterValue("resonance");
  modRateParam = parameters.getRawParameterValue("modRate");
  modDepthParam = parameters.getRawParameterValue("modDepth");
  widthParam = parameters.getRawParameterValue("width");
  mixParam = parameters.getRawParameterValue("mix");
  crossfadeSpeedParam = parameters.getRawParameterValue("crossfadeSpeed");
  lowCutParam = parameters.getRawParameterValue("lowCut");
  highCutParam = parameters.getRawParameterValue("highCut");
  tiltParam = parameters.getRawParameterValue("tilt");
  wfAmountParam = parameters.getRawParameterValue("wowFlutterAmount");
  wfEnabledParam = parameters.getRawParameterValue("wowFlutterEnabled");
  outputLevelParam = parameters.getRawParameterValue("outputLevel");
  duckingAmountParam = parameters.getRawParameterValue("duckingAmount");
  duckAttackParam = parameters.getRawParameterValue("duckAttack");
  duckReleaseParam = parameters.getRawParameterValue("duckRelease");
  saturationParam = parameters.getRawParameterValue("saturation");
  flutterSpeedParam = parameters.getRawParameterValue("flutterSpeed");
  gravityParam = parameters.getRawParameterValue("gravity");
  bypassParam = parameters.getRawParameterValue("bypass");
  mutationIntervalParam = parameters.getRawParameterValue("mutationInterval");
  intervalModeParam = parameters.getRawParameterValue("intervalMode");
  crossfadeModeParam = parameters.getRawParameterValue("crossfadeMode");

  // License: load cached state, kick off background re-verify if key on file.
  licenseManager.initialize();

  // User-saved defaults: apply to fresh instance.
  // Host's setStateInformation (if loading saved project) will overwrite
  // these, which is the correct behavior — project state always wins.
  {
    juce::PropertiesFile::Options opts;
    opts.applicationName     = "Chaosverb";
    opts.filenameSuffix      = ".defaults";
    opts.osxLibrarySubFolder = "Application Support";
    juce::ApplicationProperties props;
    props.setStorageParameters(opts);
    auto* user = props.getUserSettings();
    if (user->getAllProperties().size() > 0) {
      for (auto* param : parameters.processor.getParameters()) {
        if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*>(param)) {
          const auto id = rangedParam->getParameterID();
          if (user->containsKey(id)) {
            const float norm = (float) user->getDoubleValue(id);
            rangedParam->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, norm));
          }
        }
      }
    }
  }
}

ChaosverbAudioProcessor::~ChaosverbAudioProcessor() {
  // Stop timers before destruction to prevent callbacks into a partially
  // destroyed object. Must happen before any member destruction.
  glideTimerObj.stopTimer();
  mutationTimerObj.stopTimer();
}

//==============================================================================
void ChaosverbAudioProcessor::prepareToPlay(double sampleRate,
                                            int samplesPerBlock) {
  currentSampleRate = sampleRate;

  juce::dsp::ProcessSpec spec;
  spec.sampleRate = sampleRate;
  spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
  spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

  const double srRatio = sampleRate / 48000.0;

  // --- Pre-delay: max 250ms + 1 sample headroom ---
  // Max: 4000ms (accommodates 1/1 note at 60 BPM + free time 500ms)
  const int maxPreDelaySamples =
      static_cast<int>(std::ceil(4.0 * sampleRate)) + 1;
  preDelayLine.prepare(spec);
  preDelayLine.setMaximumDelayInSamples(maxPreDelaySamples);
  preDelayLine.setDelay(0.0f);
  preDelayLine.reset();

  // --- Allpass diffuser: scale delays to actual sample rate ---
  for (int i = 0; i < kNumDiffuserStages; ++i) {
    int allpassDelayLength =
        static_cast<int>(std::ceil(kAllpassLengths48k[i] * srRatio));
    diffLines[i].prepare(spec);
    diffLines[i].setMaximumDelayInSamples(allpassDelayLength + 1);
    diffLines[i].setDelay(static_cast<float>(allpassDelayLength));
    diffLines[i].reset();
  }

  // --- Input bandwidth limiter: one-pole LPF at 7kHz ---
  inputBWCoeff =
      1.0f - std::exp(-juce::MathConstants<float>::twoPi * kInputBandwidthHz /
                      static_cast<float>(sampleRate));
  inputBWState.fill(0.0f);

  // --- Dry/wet mixer ---
  dryWetMixer.prepare(spec);
  dryWetMixer.setMixingRule(juce::dsp::DryWetMixingRule::sin3dB);
  dryWetMixer.setWetMixProportion(0.5f);

  // --- Output EQ filters ---
  juce::dsp::ProcessSpec monoSpec{
      sampleRate, static_cast<juce::uint32>(samplesPerBlock), 1};
  outputEQ.prepare(spec);

  // --- Haas delay: max 25ms on R channel for broadband stereo width ---
  const int maxHaasSamples =
      static_cast<int>(std::ceil(0.025 * sampleRate)) + 1;
  haasDelayLine.prepare(monoSpec);
  haasDelayLine.setMaximumDelayInSamples(maxHaasSamples);
  haasDelayLine.setDelay(0.0f);
  haasDelayLine.reset();

  // --- Wow & Flutter ---
  wowFlutter.prepare(spec);

  // --- Chorus (independent from Flutter, driven by Mod Rate) ---
  chorus.prepare(spec);

  // --- Ducking envelope follower ---
  // Attack/Release: user-controlled via duckAttack/duckRelease parameters
  duckEnvelope = 0.0f;
  duckAttackCoeff =
      1.0f - std::exp(-1.0f / (0.005f * static_cast<float>(sampleRate)));
  duckReleaseCoeff =
      1.0f - std::exp(-1.0f / (0.150f * static_cast<float>(sampleRate)));
  lastDuckAttackMs = 5.0f;
  lastDuckReleaseMs = 150.0f;

  // --- Both FDN instances: prepare together ---
  fdnA.prepare(spec);
  fdnB.prepare(spec);

  // Cache mean FDN loop time (used in per-block feedback gain calc).
  // Depends only on sample rate + delay lengths set during fdnA.prepare(),
  // so safe to compute once after prepare().
  {
    float meanDelaySamples = 0.0f;
    for (int i = 0; i < ChaosverbFDN::kNumLines; ++i)
      meanDelaySamples += static_cast<float>(fdnA.delayLengthsL[i] + fdnA.delayLengthsR[i]) * 0.5f;
    meanDelaySamples /= static_cast<float>(ChaosverbFDN::kNumLines);
    cachedMeanLoopTime = meanDelaySamples / static_cast<float>(sampleRate);
  }

  // Cache resonance smoothing coefficient (50ms one-pole IIR).
  // Sample-rate-only dependency — set once.
  cachedResoSmoothCoeff = 1.0f - std::exp(-1.0f / (0.050f * static_cast<float>(sampleRate)));

  // --- Spectral Squash + Saturation ---
  spectralSquash.prepare(spec);
  saturation.prepare(spec);

  // (Swell state removed in v6.0.0 — damping now lives in ChaosverbFDN)

  widthSmoother.reset(sampleRate, 0.05);     // 50ms smoothing
  haasDelaySmoother.reset(sampleRate, 0.05); // 50ms smoothing for Haas delay

  // --- Crossfade state machine: reset to Idle, A is active ---
  xfadeState = CrossfadeState::Idle;
  crossfadePhase = 0.0f;
  crossfadePhaseInc = 0.0f;
  fdnAIsActive = true;

  // Clear any pending mutation signal from previous session
  mutationPending.store(false, std::memory_order_relaxed);

  // --- Mutation timer: start (or restart) with current interval ---
  // Record when the timer starts so getRemainingTimeMs() has a valid baseline.
  // juce::Timer::startTimer() is thread-safe — it uses the internal TimerThread
  // lock (not the message thread) to register the timer. The *callback* fires
  // on the message thread. This pattern is standard in JUCE plugins where
  // prepareToPlay() may be called from the audio thread.
  lastMutationTimeMs.store(juce::Time::getMillisecondCounterHiRes(), std::memory_order_relaxed);
  beatGridNeedsReset.store(true, std::memory_order_relaxed);
  lastIntervalVal_ = 0.0f;

  // Only start the mutation timer if it was enabled (default: stopped)
  if (mutationTimerRunning_.load()) {
    // 50ms poll — fast enough for ms-scale bipolar interval values
    mutationTimerObj.startTimer(50);
  }
}

void ChaosverbAudioProcessor::releaseResources() {
  preDelayLine.reset();
  for (int i = 0; i < kNumDiffuserStages; ++i)
    diffLines[i].reset();
  outputEQ.reset();
  haasDelayLine.reset();
  wowFlutter.reset();
  chorus.reset();
  spectralSquash.reset();
  saturation.reset();
  inputBWState.fill(0.0f);
  dryWetMixer.reset();
  fdnA.reset();
  fdnB.reset();
}

//==============================================================================
// Shared BPM-sync helper: maps a normalized value (0..1) from the bipolar
// parameter's left side to a musical beat fraction (in quarter notes).
// Used by both crossfade speed and mutation interval.
// Maps integer step (1–10) to a multiplier of (60000/bpm).
// ms = multiplier * (60000 / bpm).
// Convert raw parameter value (0–5000) to SYNC step (0–10).
// JS uses normalised (0–1) × 10 → steps 0–10.  Raw = normalised × 5000,
// so step = round(raw / 500).
static int rawToSyncStep(float rawVal) {
  return juce::roundToInt(juce::jlimit(0.0f, 5000.0f, rawVal) / 500.0f);
}

static float bpmSyncMultiplier(int step) {
  switch (step) {
    case 1:  return 0.0625f;  // 1/16
    case 2:  return 0.125f;   // 1/8
    case 3:  return 0.25f;    // 1/4
    case 4:  return 0.5f;     // 1/2
    case 5:  return 1.0f;     // 1 BAR
    case 6:  return 2.0f;     // 2 BAR
    case 7:  return 3.0f;     // 3 BAR
    case 8:  return 4.0f;     // 4 BAR
    case 9:  return 6.0f;     // 6 BAR
    case 10: return 8.0f;     // 8 BAR
    default: return 0.0f;     // OFF
  }
}

//==============================================================================
// Convert crossfade speed to milliseconds. Supports SYNC and MS modes.
static float computeCrossfadeSpeedMs(float val, float bpm, bool msMode) {
  if (msMode)
    return (val < 1.0f) ? 0.0f : val;
  const int step = rawToSyncStep(val);
  if (step <= 0) return 0.0f;
  const float quarterNoteMs = 60000.0f / juce::jmax(20.0f, bpm);
  return quarterNoteMs * bpmSyncMultiplier(step);
}

//==============================================================================
// Phase 4.4 — Mutation Timer System
//==============================================================================

void ChaosverbAudioProcessor::triggerMutation() {
  // Guard: if a crossfade trigger is already pending (not yet picked up by the
  // audio thread), skip this mutation. This prevents overlapping state changes
  // when the timer fires faster than the audio thread can process them.
  if (mutationPending.load(std::memory_order_acquire))
    return;

  // Record mutation timestamp for the countdown timer BEFORE setting params.
  lastMutationTimeMs.store(juce::Time::getMillisecondCounterHiRes(), std::memory_order_relaxed);

  // Read all lock states (message thread — APVTS reads are always safe here)
  const bool decayLocked =
      parameters.getRawParameterValue("decayLock")->load() > 0.5f;
  const bool preDelayLocked =
      parameters.getRawParameterValue("preDelayLock")->load() > 0.5f;
  const bool densityLocked =
      parameters.getRawParameterValue("densityLock")->load() > 0.5f;
  const bool spectralTiltLocked =
      parameters.getRawParameterValue("spectralTiltLock")->load() > 0.5f;
  const bool saturationLocked =
      parameters.getRawParameterValue("saturationLock")->load() > 0.5f;
  const bool modRateLocked =
      parameters.getRawParameterValue("modRateLock")->load() > 0.5f;
  const bool flutterSpeedLocked =
      parameters.getRawParameterValue("flutterSpeedLock")->load() > 0.5f;
  const bool widthLocked =
      parameters.getRawParameterValue("widthLock")->load() > 0.5f;
  const bool mixLocked =
      parameters.getRawParameterValue("mixLock")->load() > 0.5f;
  const bool lowCutLocked =
      parameters.getRawParameterValue("lowCutLock")->load() > 0.5f;
  const bool highCutLocked =
      parameters.getRawParameterValue("highCutLock")->load() > 0.5f;
  const bool tiltLocked =
      parameters.getRawParameterValue("tiltLock")->load() > 0.5f;
  const bool wfAmountLocked =
      parameters.getRawParameterValue("wowFlutterAmountLock")->load() > 0.5f;
  const bool outputLevelLocked =
      parameters.getRawParameterValue("outputLevelLock")->load() > 0.5f;
  const bool duckingLocked =
      parameters.getRawParameterValue("duckingAmountLock")->load() > 0.5f;
  const bool gravityLocked =
      parameters.getRawParameterValue("gravityLock")->load() > 0.5f;

  // Use JUCE system random — uniform distribution, no musical weighting.
  juce::Random &rng = juce::Random::getSystemRandom();

  // Read mutation amount (0..100) → normalize to 0..1 range width
  const float mutationAmountPct =
      parameters.getRawParameterValue("mutationAmount")->load();
  const float halfRange = (mutationAmountPct / 100.0f) * 0.5f;

  // Stop any existing glide before starting a new one
  glideTimerObj.stopTimer();
  numActiveGlides = 0;

  // Build glide targets: for each unlocked param, generate a random target
  // within ±halfRange of the current normalized value, clamped to [0, 1].
  auto addGlide = [&](const char *paramID, bool locked) {
    if (locked)
      return;
    if (auto *param = parameters.getParameter(paramID)) {
      const float currentNorm = param->getValue();
      const float lo = juce::jlimit(0.0f, 1.0f, currentNorm - halfRange);
      const float hi = juce::jlimit(0.0f, 1.0f, currentNorm + halfRange);
      const float targetNorm = lo + rng.nextFloat() * (hi - lo);
      glideTargets[static_cast<size_t>(numActiveGlides++)] = {
          param, currentNorm, targetNorm};
    }
  };

  addGlide("decay", decayLocked);
  addGlide("preDelay", preDelayLocked);
  addGlide("density", densityLocked);
  addGlide("spectralTilt", spectralTiltLocked);
  addGlide("saturation", saturationLocked);
  addGlide("modRate", modRateLocked);
  addGlide("flutterSpeed", flutterSpeedLocked);
  addGlide("width", widthLocked);
  addGlide("mix", mixLocked);
  addGlide("lowCut", lowCutLocked);
  addGlide("highCut", highCutLocked);
  addGlide("tilt", tiltLocked);
  addGlide("wowFlutterAmount", wfAmountLocked);
  addGlide("outputLevel", outputLevelLocked);
  addGlide("duckingAmount", duckingLocked);
  addGlide("gravity", gravityLocked);

  // Glide duration matches crossfade speed for synchronized visual + audio
  // transition
  const bool xfMsMode = crossfadeModeParam && crossfadeModeParam->load() > 0.5f;
  const float crossfadeMs = computeCrossfadeSpeedMs(
      crossfadeSpeedParam->load(), hostBPM.load(std::memory_order_relaxed), xfMsMode);
  constexpr float kGlideTickMs = 16.0f; // ~60Hz update rate

  if (numActiveGlides > 0 && crossfadeMs > kGlideTickMs) {
    // Start smooth glide: knobs animate toward targets over crossfade duration
    glidePhase = 0.0f;
    glidePhaseInc = kGlideTickMs / crossfadeMs;
    glideTimerObj.startTimer(static_cast<int>(kGlideTickMs));
  } else {
    // Instant (crossfade <= 16ms): write targets immediately
    for (size_t i = 0; i < static_cast<size_t>(numActiveGlides); ++i)
      glideTargets[i].param->setValueNotifyingHost(glideTargets[i].targetNorm);
  }

  // Crossfade to the new parameter state (even if all params were locked —
  // the crossfade still fires, FDN-B gets the same values, no audible change).
  triggerCrossfade();
}

//==============================================================================
// Glide timer callback — smoothly interpolates parameters toward mutation
// targets
void ChaosverbAudioProcessor::GlideTimer::timerCallback() {
  processor.glidePhase += processor.glidePhaseInc;

  if (processor.glidePhase >= 1.0f) {
    // Glide complete: snap to final targets
    for (size_t i = 0; i < static_cast<size_t>(processor.numActiveGlides); ++i)
      processor.glideTargets[i].param->setValueNotifyingHost(
          processor.glideTargets[i].targetNorm);
    stopTimer();
    return;
  }

  // Smoothstep for natural-feeling motion (ease in/out)
  const float t = processor.glidePhase;
  const float smooth = t * t * (3.0f - 2.0f * t);

  for (size_t i = 0; i < static_cast<size_t>(processor.numActiveGlides); ++i) {
    const auto &g = processor.glideTargets[i];
    const float val = g.startNorm + smooth * (g.targetNorm - g.startNorm);
    g.param->setValueNotifyingHost(val);
  }
}

double ChaosverbAudioProcessor::getRemainingTimeMs() const {
  const float val = mutationIntervalParam->load();
  const bool msMode = isIntervalModeMs();

  // OFF check
  if (msMode) {
    if (val < 1.0f) return -1.0;
  } else {
    if (rawToSyncStep(val) <= 0) return -1.0;
  }

  // Timer not running: show full interval duration
  if (!mutationTimerRunning_.load())
    return computeMutationIntervalMs();

  // MS mode: wall-clock countdown
  if (msMode) {
    const double baseline = lastMutationTimeMs.load(std::memory_order_relaxed);
    if (baseline == 0.0) return computeMutationIntervalMs();
    const double intervalMs = static_cast<double>(val);
    const double elapsed = juce::Time::getMillisecondCounterHiRes() - baseline;
    return juce::jlimit(0.0, intervalMs, intervalMs - elapsed);
  }

  // SYNC mode: waiting for transport
  if (!intervalIsBpmSynced.load(std::memory_order_relaxed))
    return -2.0;

  // SYNC mode: countdown from audio thread
  return juce::jmax(0.0, remainingMsBpmSync.load(std::memory_order_relaxed));
}

//==============================================================================
double ChaosverbAudioProcessor::computeMutationIntervalMs() const {
  const float val = mutationIntervalParam->load();
  if (isIntervalModeMs()) {
    // MS mode: value IS milliseconds directly
    return (val < 1.0f) ? 0.0 : static_cast<double>(val);
  }
  // SYNC mode: raw value → step 0–10 → BPM-synced
  const int step = rawToSyncStep(val);
  if (step <= 0) return 0.0;
  const float bpm = juce::jmax(20.0f, hostBPM.load(std::memory_order_relaxed));
  const float qMs = 60000.0f / bpm;
  return static_cast<double>(qMs * bpmSyncMultiplier(step));
}

double ChaosverbAudioProcessor::computeMutationIntervalPpq() const {
  if (isIntervalModeMs()) return 0.0; // MS mode doesn't use PPQ
  const int step = rawToSyncStep(mutationIntervalParam->load());
  if (step <= 0) return 0.0;
  return static_cast<double>(bpmSyncMultiplier(step));
}

void ChaosverbAudioProcessor::setMutationTimerRunning(bool running) {
  mutationTimerRunning_.store(running);

  if (running) {
    lastMutationTimeMs.store(juce::Time::getMillisecondCounterHiRes(),
                             std::memory_order_relaxed);
    beatGridNeedsReset.store(true, std::memory_order_relaxed);
    lastIntervalVal_ = 0.0f;
    mutationTimerObj.startTimer(50);
  } else {
    mutationTimerObj.stopTimer();
  }
}

bool ChaosverbAudioProcessor::isMutationTimerRunning() const {
  return mutationTimerRunning_.load();
}

//==============================================================================
// Allpass diffuser helper (called from processBlock per-sample)
//==============================================================================

//==============================================================================

void ChaosverbAudioProcessor::applyStereoWidth(int numSamples, float *dataL,
                                               float *dataR, float widthGain) {
  if (dataL == nullptr || dataR == nullptr) return;
  widthSmoother.setTargetValue(widthGain);
  // Identity when width=1.0 (M+S=L, M-S=R) — skip if smoother settled.
  if (!widthSmoother.isSmoothing() && std::abs(widthGain - 1.0f) < 1e-4f)
    return;
  for (int n = 0; n < numSamples; ++n) {
    const float currentWidth = widthSmoother.getNextValue();
    const float mid = (dataL[n] + dataR[n]) * 0.5f;
    const float side = (dataL[n] - dataR[n]) * 0.5f;
    dataL[n] = mid + side * currentWidth;
    dataR[n] = mid - side * currentWidth;
  }
}

float ChaosverbAudioProcessor::processDiffuserSample(float input, int channel,
                                                     int numActiveStages) {
  float x = input;
  for (int stage = 0; stage < numActiveStages; ++stage) {
    auto &dl = diffLines[stage];
    const float delayed = dl.popSample(channel);
    const float v = x - kAllpassCoeff * delayed;
    dl.pushSample(channel, v);
    x = delayed + kAllpassCoeff * v;
  }
  return x;
}

//==============================================================================
void ChaosverbAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                           juce::MidiBuffer &midiMessages) {
  juce::ScopedNoDenormals noDenormals;
  juce::ignoreUnused(midiMessages);

  const int totalNumInputChannels = getTotalNumInputChannels();
  const int totalNumOutputChannels = getTotalNumOutputChannels();

  for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
    buffer.clear(i, 0, buffer.getNumSamples());

  // -------------------------------------------------------------------------
  // License gate — silence when not licensed and no active trial.
  // Trial mode (within 7 days of trial start) = fully functional.
  // -------------------------------------------------------------------------
  if (!licenseManager.isActive()) {
    buffer.clear();
    return;
  }

  // -------------------------------------------------------------------------
  // Bypass — pass audio through unprocessed
  // -------------------------------------------------------------------------
  const bool bypassed = bypassParam->load() >= 0.5f;
  if (bypassed)
    return;

  const int numSamples = buffer.getNumSamples();
  const int numChannels = juce::jmin(buffer.getNumChannels(), 2);

  // -------------------------------------------------------------------------
  // Read host playhead ONCE — extract BPM, PPQ, time signature, transport.
  // Used by crossfade speed, interval scheduling, and pre-delay calculations.
  // -------------------------------------------------------------------------
  float bpmEarly = 120.0f;
  double hostPpq = -1.0;
  double hostBarStartPpq = -1.0;
  int hostTimeSigNum = 4;
  int hostTimeSigDen = 4;
  bool hostTransportPlaying = false;
  if (auto *ph = getPlayHead()) {
    if (auto p = ph->getPosition()) {
      if (auto b = p->getBpm())
        bpmEarly = juce::jmax(20.0f, static_cast<float>(*b));
      if (auto ppqPos = p->getPpqPosition())
        hostPpq = *ppqPos;
      if (auto barStart = p->getPpqPositionOfLastBarStart())
        hostBarStartPpq = *barStart;
      if (auto ts = p->getTimeSignature()) {
        hostTimeSigNum = ts->numerator;
        hostTimeSigDen = ts->denominator;
      }
      hostTransportPlaying = p->getIsPlaying();
    }
  }
  hostBPM.store(bpmEarly, std::memory_order_relaxed);

  // -------------------------------------------------------------------------
  // Read all parameters — atomic loads, fully real-time safe
  // -------------------------------------------------------------------------
  const float decaySeconds = decayParam->load();
  const float preDelayVal = preDelayParam->load();
  const float densityPercent = densityParam->load();
  const float spectralTiltVal = spectralTiltParam->load();
  const float resonanceVal = resonanceParam->load();
  const float modRateHz = modRateParam->load();
  const float modDepthPercent = modDepthParam->load();
  const float widthPercent = widthParam->load();
  const float mixPercent = mixParam->load();
  const float crossfadeSpeedVal = crossfadeSpeedParam->load();
  const bool xfMsMode2 = crossfadeModeParam && crossfadeModeParam->load() > 0.5f;
  const float crossfadeSpeedMs = computeCrossfadeSpeedMs(
      crossfadeSpeedVal, hostBPM.load(std::memory_order_relaxed), xfMsMode2);
  const float lowCutHz = lowCutParam->load();
  const float highCutHz = highCutParam->load();
  const float tiltVal = tiltParam->load();
  const float wfAmount = wfAmountParam->load();
  const bool wfEnabled = wfEnabledParam->load() > 0.5f;
  const float outputLevelDb = outputLevelParam->load();
  const float duckingAmount = duckingAmountParam->load();
  const float duckAttackMs = duckAttackParam->load();
  const float duckReleaseMs = duckReleaseParam->load();
  const float saturationAmount = saturationParam->load();
  const float flutterSpeedVal = flutterSpeedParam->load();
  const float flutterSpeedMult = std::pow(4.0f, flutterSpeedVal / 100.0f);
  const float dampingVal = gravityParam->load();  // -100 to +100

  // Recompute duck attack coefficient only when parameter changes
  if (std::abs(duckAttackMs - lastDuckAttackMs) > 0.001f) {
    lastDuckAttackMs = duckAttackMs;
    duckAttackCoeff = 1.0f - std::exp(-1.0f / (duckAttackMs * 0.001f *
                                                 static_cast<float>(currentSampleRate)));
  }

  // Recompute duck release coefficient only when parameter changes
  if (std::abs(duckReleaseMs - lastDuckReleaseMs) > 0.001f) {
    lastDuckReleaseMs = duckReleaseMs;
    duckReleaseCoeff = 1.0f - std::exp(-1.0f / (duckReleaseMs * 0.001f *
                                                  static_cast<float>(currentSampleRate)));
  }

  // -------------------------------------------------------------------------
  // Check mutation pending flag (set by message thread, cleared here)
  // Guard: ignore if crossfade already in progress
  // -------------------------------------------------------------------------
  if (mutationPending.load(std::memory_order_acquire) &&
      xfadeState == CrossfadeState::Idle) {
    mutationPending.store(false, std::memory_order_release);
    xfadeState = CrossfadeState::Ramping;
    crossfadePhase = 0.0f;

    // Freeze current params as the outgoing snapshot.
    // activeSnapshot has last block's params (before mutation wrote new APVTS
    // values). The outgoing FDN keeps reverberating with these old params
    // during crossfade.
    outgoingSnapshot = activeSnapshot;

    // Do NOT reset the incoming FDN — it already has reverb energy from running
    // in parallel with the outgoing FDN. Keeping its state allows the crossfade
    // to genuinely blend between old and new reverb characters, making the
    // crossfade speed parameter audible. Resetting would cause the incoming FDN
    // to start from silence, producing a volume dip instead of a smooth morph.

    const float sr = static_cast<float>(currentSampleRate);
    if (crossfadeSpeedMs <= 0.0f) {
      crossfadePhaseInc = 1.0f;
    } else {
      crossfadePhaseInc = 1.0f / (crossfadeSpeedMs * 0.001f * sr);
    }
  }

  // -------------------------------------------------------------------------
  // Precompute per-block DSP parameters (avoids recomputing per-sample)
  // -------------------------------------------------------------------------

  const float sr = static_cast<float>(currentSampleRate);

  // Use playhead snapshot taken at top of processBlock (single read).
  {
    const float bpm = hostBPM.load(std::memory_order_relaxed);
    const double ppq = hostPpq;
    const double barStartPpq = hostBarStartPpq;
    const bool transportPlaying = hostTransportPlaying;
    // Bar length in quarter notes (e.g., 4/4 = 4.0, 3/4 = 3.0, 6/8 = 3.0)
    const double barLenPpq = static_cast<double>(hostTimeSigNum) * 4.0 / static_cast<double>(hostTimeSigDen);

    // --- BPM-synced mutation scheduling (skipped in MS mode) ---
    const bool msMode = isIntervalModeMs();
    const int intervalStep = msMode ? 0 : rawToSyncStep(mutationIntervalParam->load());
    const bool hasPpq = (ppq >= 0.0);
    const bool bpmSyncActive = !msMode && (intervalStep > 0) && hasPpq && transportPlaying;
    intervalIsBpmSynced.store(bpmSyncActive, std::memory_order_relaxed);

    // Detect events that require rescheduling to the next bar boundary
    const bool transportJustStarted = transportPlaying && !wasTransportPlaying_;
    const bool ppqJumpedBack = hasPpq && ppq < lastPpq_ - 0.5;
    const float intervalFloat = static_cast<float>(intervalStep);
    const bool intervalChanged =
        std::abs(intervalFloat - lastIntervalVal_.load(std::memory_order_relaxed)) > 0.5f;

    if (transportJustStarted || ppqJumpedBack)
      beatGridNeedsReset.store(true, std::memory_order_relaxed);

    wasTransportPlaying_ = transportPlaying;
    if (hasPpq) lastPpq_ = ppq;
    if (intervalChanged) lastIntervalVal_ = intervalFloat;

    if (bpmSyncActive && mutationTimerRunning_.load(std::memory_order_relaxed)) {
      const double intervalPpq = static_cast<double>(bpmSyncMultiplier(intervalStep));

      if (intervalPpq > 0.0) {
        const bool needsReset =
            beatGridNeedsReset.load(std::memory_order_relaxed) || intervalChanged;

        // --- RESET: schedule first mutation at next bar + interval ---
        if (needsReset) {
          beatGridNeedsReset.store(false, std::memory_order_relaxed);

          // Find the next bar boundary from current position
          double nextBar;
          if (barStartPpq >= 0.0 && barStartPpq <= ppq + 0.1) {
            nextBar = barStartPpq + barLenPpq;
            while (nextBar <= ppq + 0.01)
              nextBar += barLenPpq;
          } else {
            nextBar = std::ceil((ppq + 0.01) / barLenPpq) * barLenPpq;
          }

          nextMutationPpq.store(nextBar + intervalPpq, std::memory_order_relaxed);
        }

        // --- CHECK: has ppq reached the target? ---
        const double target = nextMutationPpq.load(std::memory_order_relaxed);
        if (target > 0.0 && ppq >= target - 0.02) {
          // Signal message thread to fire mutation (never call triggerMutation
          // from audio thread — it uses setValueNotifyingHost + juce::Random)
          bpmSyncMutationDue_.store(true, std::memory_order_release);
          // After fire: next mutation is exactly one interval later
          // (no bar wait — stays on rhythmic grid from this point)
          nextMutationPpq.store(target + intervalPpq, std::memory_order_relaxed);
        }

        // --- COUNTDOWN: remaining time in ms for UI ---
        const double remain = nextMutationPpq.load(std::memory_order_relaxed) - ppq;
        remainingMsBpmSync.store(
            (remain > 0.0 && bpm > 0.0f)
                ? remain * 60000.0 / static_cast<double>(bpm)
                : 0.0,
            std::memory_order_relaxed);
      }
    }
  }

  // Pre-delay: bipolar — right of center=free time (ms), left=BPM-synced
  // divisions
  float preDelaySamples = 0.0f;
  if (preDelayVal >= 0.0f) {
    // Right of center: free time in ms (0-500ms)
    preDelaySamples = preDelayVal / 1000.0f * sr;
  } else {
    // Left of center: BPM-synced note divisions
    const float bpm = hostBPM.load(std::memory_order_relaxed);
    const float quarterNoteMs = 60000.0f / bpm;
    const float t = -preDelayVal / 500.0f; // 0..1 (0=off, 1=whole note)

    // Quantize to nearest note division
    float beatFraction = 0.0f;
    if (t > 0.9f)
      beatFraction = 4.0f; // 1/1
    else if (t > 0.8f)
      beatFraction = 2.0f; // 1/2
    else if (t > 0.7f)
      beatFraction = 1.5f; // 1/4d
    else if (t > 0.6f)
      beatFraction = 1.0f; // 1/4
    else if (t > 0.5f)
      beatFraction = 0.75f; // 1/8d
    else if (t > 0.4f)
      beatFraction = 0.5f; // 1/8
    else if (t > 0.3f)
      beatFraction = 0.375f; // 1/16d
    else if (t > 0.2f)
      beatFraction = 0.25f; // 1/16
    else if (t > 0.1f)
      beatFraction = 0.125f; // 1/32

    preDelaySamples = (quarterNoteMs * beatFraction) / 1000.0f * sr;
  }

  preDelaySamples = juce::jmax(0.0f, preDelaySamples);
  preDelayLine.setDelay(preDelaySamples);

  // Feedback gain from RT60 formula. Uses cached FDN mean loop time
  // (computed once in prepareToPlay — sample-rate dependent only).
  const float safeDecay = juce::jmax(0.01f, decaySeconds);
  const float rawGain = std::exp(-6.91f * cachedMeanLoopTime / safeDecay);
  const float feedbackGain = juce::jmin(rawGain, 0.88f);

  // Density -> diffuser stages + FDN in-loop allpass coefficient scaling
  // Wider thresholds = audible grains at low end, lush dense reverb at high end
  int numActiveDiffuserStages;
  if (densityPercent < 25.0f)
    numActiveDiffuserStages = 0;
  else if (densityPercent < 45.0f)
    numActiveDiffuserStages = 1;
  else if (densityPercent < 60.0f)
    numActiveDiffuserStages = 2;
  else if (densityPercent < 80.0f)
    numActiveDiffuserStages = 3;
  else
    numActiveDiffuserStages = 4;

  // Topology blend — fixed, ignored by ChaosverbFDN (Clouds engine uses fixed controls)
  const float topologyBlend = 0.625f;

  // LFO depth in samples
  const float modDepthSamples =
      (modDepthPercent / 100.0f) * fdnA.maxLFODepthSamples;

  // Resonance smoothing coefficient (~50ms one-pole IIR) — cached in prepareToPlay
  const float resoSmoothCoeff = cachedResoSmoothCoeff;

  // Stereo width gain: 0%=0.0, 100%=1.0, 400%=4.0
  const float widthGain = widthPercent / 100.0f;

  // Haas delay: scales from 0ms at width=0% to 25ms at width=400%
  // Adds temporal decorrelation to the R channel for broadband stereo width
  const float maxHaasMs = 25.0f;
  const float haasMs = (widthPercent / 400.0f) * maxHaasMs;
  const float haasDelaySamples = juce::jmax(0.0f, (haasMs / 1000.0f) * sr);

  // -------------------------------------------------------------------------
  // Update FDN coefficients: when idle both FDNs track live params;
  // during crossfade only the incoming FDN gets new coefficients so the
  // outgoing FDN keeps reverberating with its old character.
  // -------------------------------------------------------------------------
  if (xfadeState == CrossfadeState::Idle) {
    if (std::abs(spectralTiltVal - fdnA.cachedSpectralTilt) > 0.01f) {
      fdnA.updateShelfCoefficients(spectralTiltVal);
      fdnB.updateShelfCoefficients(spectralTiltVal);
    }
    if (std::abs(resonanceVal - fdnA.cachedResonance) > 0.01f) {
      fdnA.updateResonanceCoefficients(resonanceVal);
      fdnB.updateResonanceCoefficients(resonanceVal);
    }

    // Only update LFO when modRate > 0 (at 0, modulation is fully bypassed)
    if (modRateHz > 0.001f) {
      fdnA.prepareLFO(modRateHz, modDepthPercent);
      fdnB.prepareLFO(modRateHz, modDepthPercent);
    }

    // Frequency damping: HF damping (left) or LF damping (right)
    fdnA.setDamping(dampingVal);
    fdnB.setDamping(dampingVal);

    // Track live per-sample params for future crossfade snapshot
    activeSnapshot = {feedbackGain, topologyBlend, modDepthSamples,
                      resoSmoothCoeff};
  } else {
    // Crossfading: only update incoming FDN with new (mutated) params.
    // Outgoing FDN keeps its frozen coefficients from before the mutation.
    ChaosverbFDN &incomingFDN = fdnAIsActive ? fdnB : fdnA;
    incomingFDN.updateShelfCoefficients(spectralTiltVal);
    incomingFDN.updateResonanceCoefficients(resonanceVal);
    if (modRateHz > 0.001f)
      incomingFDN.prepareLFO(modRateHz, modDepthPercent);
    incomingFDN.setDamping(dampingVal);
  }

  // Pre-set Reverb Time parameter outside the tight audio loop 
  // (avoiding per-sample parameter checking logic)
  if (xfadeState == CrossfadeState::Ramping) {
    const auto &outSnap = outgoingSnapshot;
    if (fdnAIsActive) {
      fdnA.setFeedbackGain(outSnap.feedbackGain);
      fdnB.setFeedbackGain(feedbackGain);
    } else {
      fdnA.setFeedbackGain(feedbackGain);
      fdnB.setFeedbackGain(outSnap.feedbackGain);
    }
  } else {
    fdnA.setFeedbackGain(feedbackGain);
    fdnB.setFeedbackGain(feedbackGain);
  }

  // Update output EQ coefficients when parameters change
  outputEQ.update(lowCutHz, highCutHz, tiltVal);

  // Set DryWetMixer mix ratio
  dryWetMixer.setWetMixProportion(
      juce::jlimit(0.0f, 1.0f, mixPercent / 100.0f));

  // Ducking amount: 0.0 = off, 1.0 = full ducking
  // Exponential curve: low values = subtle, high values = extreme (near-silence)
  // duckingNorm^2 * 20 gives: 25% → 1.25x (subtle), 50% → 5x (moderate),
  // 75% → 11.25x (strong), 100% → 20x (extreme, nearly silent during transients)
  const float duckingNorm = duckingAmount / 100.0f;
  const float duckAmt = duckingNorm * duckingNorm * 20.0f;

  // -------------------------------------------------------------------------
  // Push dry signal into DryWetMixer before in-place wet processing
  // -------------------------------------------------------------------------
  {
    juce::dsp::AudioBlock<float> block(buffer);
    dryWetMixer.pushDrySamples(block);
  }

  // Get write pointers
  float *dataL = (numChannels > 0) ? buffer.getWritePointer(0) : nullptr;
  float *dataR = (numChannels > 1) ? buffer.getWritePointer(1) : nullptr;

  // Spectral Squash: pre-reverb spectral shaping on input signal.
  // Compresses lows/highs differently (FDR) to reshape the tonal balance
  // entering the reverb, changing how the reverb responds to the input.
  // LUFS auto-gain ensures consistent level into the reverb regardless of
  // squash amount, so tail length and density stay stable.
  spectralSquash.process(numSamples, dataL, dataR, spectralTiltVal);

  // -------------------------------------------------------------------------
  // Sample-by-sample processing loop
  //
  // Flow per sample:
  //   1. Pre-delay (shared, stereo)
  //   2. Allpass diffuser (shared, stereo)
  //   3. Both FDNs process diffused input with per-FDN params:
  //      - During crossfade: outgoing FDN uses frozen snapshot (old params),
  //        incoming FDN uses live params (new mutated values)
  //      - When idle: both FDNs use identical live params
  //   4. Equal-power crossfade blend:
  //      wetL = gainOut * outgoingL + gainIn * incomingL
  //      wetR = gainOut * outgoingR + gainIn * incomingR
  //   5. Advance crossfade phase; handle state transitions
  //
  // After all samples:
  //   6. Stereo width M/S matrix on wet buffer
  //   7. DryWetMixer blend
  // -------------------------------------------------------------------------
  // -------------------------------------------------------------------------
  // Precompute per-block constants (hoisted from per-sample loop)
  // -------------------------------------------------------------------------

  // Crossfade: precompute incremental rotation to avoid per-sample trig.
  // cos(phase + inc) = cos(phase)*cos(inc) - sin(phase)*sin(inc)
  // sin(phase + inc) = sin(phase)*cos(inc) + cos(phase)*sin(inc)
  float xfGainOut = 1.0f, xfGainIn = 0.0f;
  float xfCosInc = 1.0f, xfSinInc = 0.0f;
  if (xfadeState == CrossfadeState::Ramping) {
    const float halfPi = juce::MathConstants<float>::halfPi;
    xfGainOut = std::cos(crossfadePhase * halfPi);
    xfGainIn  = std::sin(crossfadePhase * halfPi);
    xfCosInc  = std::cos(crossfadePhaseInc * halfPi);
    xfSinInc  = std::sin(crossfadePhaseInc * halfPi);
  }

  // Hoist invariant: fdnAIsActive doesn't change mid-block in idle state.
  // Crossfade transitions only happen at block boundaries.
  const bool aActiveAtBlockStart = fdnAIsActive;

  for (int n = 0; n < numSamples; ++n) {
    // --- 1. Pre-delay (shared stereo) ---
    const float rawL = (dataL != nullptr) ? dataL[n] : 0.0f;
    const float rawR = (dataR != nullptr) ? dataR[n] : 0.0f;

    // --- Track dry signal envelope for ducking (sidechain from dry input) ---
    {
      const float dryPeak = std::max(std::abs(rawL), std::abs(rawR));
      const float dCoeff =
          (dryPeak > duckEnvelope) ? duckAttackCoeff : duckReleaseCoeff;
      duckEnvelope += dCoeff * (dryPeak - duckEnvelope);
    }

    preDelayLine.pushSample(0, rawL);
    preDelayLine.pushSample(1, rawR);
    float sigL = preDelayLine.popSample(0);
    float sigR = preDelayLine.popSample(1);

    // --- 2. Allpass diffuser (shared stereo) ---
    if (numActiveDiffuserStages > 0) {
      sigL = processDiffuserSample(sigL, 0, numActiveDiffuserStages);
      sigR = processDiffuserSample(sigR, 1, numActiveDiffuserStages);
    }

    // --- 2b. Input bandwidth limiter (9kHz one-pole LPF) ---
    // Prevents harsh HF from entering FDN recirculation
    inputBWState[0] += inputBWCoeff * (sigL - inputBWState[0]);
    sigL = inputBWState[0];
    inputBWState[1] += inputBWCoeff * (sigR - inputBWState[1]);
    sigR = inputBWState[1];

    // --- 3. Both FDNs process (both always run for crossfade continuity) ---
    float wetAL = 0.0f, wetAR = 0.0f;
    float wetBL = 0.0f, wetBR = 0.0f;
    fdnA.processSample(sigL, sigR, wetAL, wetAR);
    fdnB.processSample(sigL, sigR, wetBL, wetBR);

    // --- 4. Equal-power crossfade blend ---
    float wetL, wetR;
    if (xfadeState == CrossfadeState::Ramping) {
      // Use precomputed incremental rotation gains (no per-sample trig)
      if (fdnAIsActive) {
        wetL = xfGainOut * wetAL + xfGainIn * wetBL;
        wetR = xfGainOut * wetAR + xfGainIn * wetBR;
      } else {
        wetL = xfGainOut * wetBL + xfGainIn * wetAL;
        wetR = xfGainOut * wetBR + xfGainIn * wetAR;
      }

      // Rotate gains for next sample
      const float newOut = xfGainOut * xfCosInc - xfGainIn * xfSinInc;
      const float newIn  = xfGainOut * xfSinInc + xfGainIn * xfCosInc;
      xfGainOut = newOut;
      xfGainIn = newIn;
    } else {
      wetL = aActiveAtBlockStart ? wetAL : wetBL;
      wetR = aActiveAtBlockStart ? wetAR : wetBR;
    }

    // --- 4b. Apply ducking to wet signal (sidechained from dry input) ---
    if (duckingNorm >= 0.001f) {
      const float duckGain =
          juce::jlimit(0.0f, 1.0f, 1.0f - duckAmt * duckEnvelope);
      wetL *= duckGain;
      wetR *= duckGain;
    }

    if (dataL != nullptr)
      dataL[n] = wetL;
    if (dataR != nullptr)
      dataR[n] = wetR;

    // --- 5. Advance crossfade phase and handle state transitions ---
    if (xfadeState == CrossfadeState::Ramping) {
      crossfadePhase += crossfadePhaseInc;

      if (crossfadePhase >= 1.0f) {
        crossfadePhase = 0.0f;

        // Swap roles: incoming FDN becomes active
        fdnAIsActive = !fdnAIsActive;
        xfadeState = CrossfadeState::Idle;
      }
    }
  }

  // -------------------------------------------------------------------------
  // 5b. Haas delay on R channel — temporal decorrelation for broadband width.
  //     Applied AFTER crossfade, BEFORE M/S width processing.
  //     At width=0% delay is 0 (pass-through), scaling to 12ms at 300%.
  // -------------------------------------------------------------------------
  if (dataR != nullptr) {
    haasDelaySmoother.setTargetValue(haasDelaySamples);
    // Skip when target is zero AND smoother has settled — no-op delay.
    const bool haasActive = haasDelaySmoother.isSmoothing() || haasDelaySamples > 0.5f;
    if (haasActive) {
      for (int n = 0; n < numSamples; ++n) {
        haasDelayLine.pushSample(0, dataR[n]);
        const float smoothedHaas = haasDelaySmoother.getNextValue();
        dataR[n] = haasDelayLine.popSample(0, juce::jmax(0.0f, smoothedHaas));
      }
    }
  }

  applyStereoWidth(numSamples, dataL, dataR, widthGain);

  saturation.process(numSamples, dataL, dataR, saturationAmount);

  outputEQ.process(numSamples, dataL, dataR);

  // Chorus: subtle stereo chorus driven by Mod Rate, independent from Flutter
  chorus.process(numSamples, dataL, dataR, modRateHz, 2.0f);

  wowFlutter.process(numSamples, dataL, dataR, wfAmount, wfEnabled, flutterSpeedMult);

  // Ducking now applied inline in the sample loop (sidechained from dry input)

  // -------------------------------------------------------------------------
  // 7. Apply output level gain trim to WET signal only (before dry/wet mix)
  // -------------------------------------------------------------------------
  if (std::abs(outputLevelDb) > 0.01f) {
    const float outputGain = juce::Decibels::decibelsToGain(outputLevelDb);
    buffer.applyGain(outputGain);
  }

  // -------------------------------------------------------------------------
  // 8. Mix wet signal back with dry via DryWetMixer
  // -------------------------------------------------------------------------
  {
    juce::dsp::AudioBlock<float> block(buffer);
    dryWetMixer.mixWetSamples(block);
  }
}

//==============================================================================
juce::AudioProcessorEditor *ChaosverbAudioProcessor::createEditor() {
  return new ChaosverbAudioProcessorEditor(*this);
}

//==============================================================================
void ChaosverbAudioProcessor::getStateInformation(juce::MemoryBlock &destData) {
  auto state = parameters.copyState();
  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, destData);
}

void ChaosverbAudioProcessor::setStateInformation(const void *data,
                                                  int sizeInBytes) {
  std::unique_ptr<juce::XmlElement> xmlState(
      getXmlFromBinary(data, sizeInBytes));

  if (xmlState != nullptr && xmlState->hasTagName(parameters.state.getType()))
    parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
// Plugin factory function — required by JUCE plugin infrastructure
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new ChaosverbAudioProcessor();
}
