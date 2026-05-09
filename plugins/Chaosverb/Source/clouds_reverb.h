// Copyright 2014 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// Clouds Dattorro plate reverb.
// Desktop-patched version: FORMAT_32_BIT, renamed to CloudsReverb.

#ifndef CLOUDS_REVERB_H_
#define CLOUDS_REVERB_H_

#include "clouds_fx_engine.h"

// ---------------------------------------------------------------------------
// stmlib replacement: FloatFrame (stereo pair)
// ---------------------------------------------------------------------------
#ifndef FLOAT_FRAME_DEFINED
#define FLOAT_FRAME_DEFINED
struct FloatFrame { float l; float r; };
#endif

namespace clouds {

class CloudsReverb {
 public:
  CloudsReverb() : amount_(0.0f), input_gain_(0.0f), reverb_time_(0.0f),
                    diffusion_(0.0f), lp_(0.0f),
                    lp_decay_1_(0.0f), lp_decay_2_(0.0f),
                    mod_depth_(0.5f),
                    energy_rms_(0.0f), gain_reduction_(1.0f) {}
  ~CloudsReverb() {}

  void Init(float* buffer) {
    engine_.Init(buffer);
    engine_.SetLFOFrequency(LFO_1, 0.5f / 32000.0f);
    engine_.SetLFOFrequency(LFO_2, 0.3f / 32000.0f);
    lp_ = 0.7f;
    diffusion_ = 0.625f;
    energy_rms_ = 0.0f;
    gain_reduction_ = 1.0f;
  }

  void Process(FloatFrame* in_out, size_t size) {
    // Griesinger/Dattorro topology:
    // 4 AP diffusers on input, then a loop of 2x (2AP + 1Delay).
    // Modulation in first diffuser AP and two long delays.
    typedef E::Reserve<113,
      E::Reserve<162,
      E::Reserve<241,
      E::Reserve<399,
      E::Reserve<1653,
      E::Reserve<2038,
      E::Reserve<3411,
      E::Reserve<1913,
      E::Reserve<1663,
      E::Reserve<4782> > > > > > > > > > Memory;
    E::DelayLine<Memory, 0> ap1;
    E::DelayLine<Memory, 1> ap2;
    E::DelayLine<Memory, 2> ap3;
    E::DelayLine<Memory, 3> ap4;
    E::DelayLine<Memory, 4> dap1a;
    E::DelayLine<Memory, 5> dap1b;
    E::DelayLine<Memory, 6> del1;
    E::DelayLine<Memory, 7> dap2a;
    E::DelayLine<Memory, 8> dap2b;
    E::DelayLine<Memory, 9> del2;
    E::Context c;

    const float kap = diffusion_;
    const float klp = lp_;
    const float krt_base = reverb_time_;
    const float amount = amount_;
    const float gain = input_gain_;

    // Energy-aware feedback reduction coefficients.
    // Target: keep loop energy below ~0.5 RMS (≈ -6dBFS).
    // Attack ~100ms, release ~500ms at 48kHz. Purely linear gain —
    // no waveshaping, no harmonics generated in the feedback loop.
    const float energy_target = 0.25f;   // squared RMS target (0.5^2)
    const float energy_attack  = 0.00002f;  // ~100ms rise at 48kHz
    const float energy_release = 0.000004f; // ~500ms fall at 48kHz

    // Mod depth scales LFO amplitudes within safe buffer bounds:
    //   ap1:  base=113, offset=10 → safe max amplitude ~100
    //   del2: base=4782, offset=4680 → safe max amplitude ~100
    const float md = mod_depth_;
    const float amp1 = 20.0f + md * 80.0f;   // 20–100 samples (ap1 smear)
    const float amp2 = 30.0f + md * 70.0f;   // 30–100 samples (del2 mod)

    float lp_1 = lp_decay_1_;
    float lp_2 = lp_decay_2_;
    float energy = energy_rms_;
    float gr = gain_reduction_;

    while (size--) {
      float wet;
      float apout = 0.0f;
      engine_.Start(&c);

      // Smear AP1 inside the loop.
      c.Interpolate(ap1, 10.0f, LFO_1, amp1, 1.0f);
      c.Write(ap1, 100, 0.0f);

      c.Read(in_out->l + in_out->r, gain);

      // Diffuse through 4 allpasses.
      c.Read(ap1 TAIL, kap);
      c.WriteAllPass(ap1, -kap);
      c.Read(ap2 TAIL, kap);
      c.WriteAllPass(ap2, -kap);
      c.Read(ap3 TAIL, kap);
      c.WriteAllPass(ap3, -kap);
      c.Read(ap4 TAIL, kap);
      c.WriteAllPass(ap4, -kap);
      c.Write(apout);

      // Energy-aware feedback: reduce krt when loop energy exceeds target.
      // This is a slow, smooth gain change — purely linear, no harmonics.
      const float krt = krt_base * gr;

      // Main reverb loop.
      c.Load(apout);
      c.Interpolate(del2, 4680.0f, LFO_2, amp2, krt);
      c.Lp(lp_1, klp);
      c.Read(dap1a TAIL, -kap);
      c.WriteAllPass(dap1a, kap);
      c.Read(dap1b TAIL, kap);
      c.WriteAllPass(dap1b, -kap);
      c.Write(del1, 1.0f);
      c.Write(wet, 0.0f);

      in_out->l += (wet - in_out->l) * amount;

      // Track energy from first half-loop output
      const float e1 = wet * wet;

      c.Load(apout);
      c.Read(del1 TAIL, krt);
      c.Lp(lp_2, klp);
      c.Read(dap2a TAIL, kap);
      c.WriteAllPass(dap2a, -kap);
      c.Read(dap2b TAIL, -kap);
      c.WriteAllPass(dap2b, kap);
      c.Write(del2, 1.0f);
      c.Write(wet, 0.0f);

      in_out->r += (wet - in_out->r) * amount;

      // Track energy from second half-loop output
      const float e2 = wet * wet;

      // Update RMS energy estimate (peak of both halves)
      const float peak_energy = (e1 > e2) ? e1 : e2;
      const float ecoeff = (peak_energy > energy) ? energy_attack : energy_release;
      energy += ecoeff * (peak_energy - energy);

      // Compute gain reduction: smoothly reduce feedback when energy exceeds target
      if (energy > energy_target) {
        // Ratio of target to actual energy, square-rooted for amplitude domain
        float target_gr = energy_target / energy;
        // Sqrt for amplitude-domain scaling (energy is squared)
        if (target_gr < 1.0f) {
          // Fast approximation: sqrt via one Newton-Raphson iteration
          float x = 0.5f * (1.0f + target_gr); // initial guess
          x = 0.5f * (x + target_gr / x);      // one iteration
          target_gr = x;
        }
        // Smooth towards target (don't jump instantly)
        gr += 0.001f * (target_gr - gr);
      } else {
        // Release back towards 1.0 (no reduction)
        gr += 0.0002f * (1.0f - gr);
      }

      ++in_out;
    }

    lp_decay_1_ = lp_1;
    lp_decay_2_ = lp_2;
    energy_rms_ = energy;
    gain_reduction_ = gr;
  }

  inline void set_amount(float amount) {
    amount_ = amount;
  }

  inline void set_input_gain(float input_gain) {
    input_gain_ = input_gain;
  }

  inline void set_time(float reverb_time) {
    reverb_time_ = reverb_time;
  }

  inline void set_diffusion(float diffusion) {
    diffusion_ = diffusion;
  }

  inline void set_lp(float lp) {
    lp_ = lp;
  }

  inline void set_mod_depth(float depth) {
    mod_depth_ = depth;
  }

  inline void set_lfo_frequency(LFOIndex index, float frequency) {
    engine_.SetLFOFrequency(index, frequency);
  }

 private:
  typedef FxEngine<16384, FORMAT_32_BIT> E;
  E engine_;

  float amount_;
  float input_gain_;
  float reverb_time_;
  float diffusion_;
  float lp_;

  float lp_decay_1_;
  float lp_decay_2_;
  float mod_depth_;
  float energy_rms_;
  float gain_reduction_;

  CloudsReverb(const CloudsReverb&) = delete;
  CloudsReverb& operator=(const CloudsReverb&) = delete;
};

}  // namespace clouds

#endif  // CLOUDS_REVERB_H_
