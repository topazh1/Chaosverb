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
// Base class for building reverbs.
// Desktop-patched version: stmlib dependencies replaced with inline equivalents.

#ifndef CLOUDS_FX_ENGINE_H_
#define CLOUDS_FX_ENGINE_H_

#include <algorithm>
#include <cmath>
#include <cstdint>

// ---------------------------------------------------------------------------
// stmlib replacement: MAKE_INTEGRAL_FRACTIONAL macro
// Splits a float x into integer and fractional parts.
// ---------------------------------------------------------------------------
#define MAKE_INTEGRAL_FRACTIONAL(x) \
  int32_t x ## _integral = static_cast<int32_t>(x); \
  float x ## _fractional = x - static_cast<float>(x ## _integral);

namespace clouds {

// ---------------------------------------------------------------------------
// stmlib replacement: Clip16 (only used by FORMAT_12_BIT / FORMAT_16_BIT)
// ---------------------------------------------------------------------------
inline int32_t Clip16(int32_t x) {
  if (x < -32768) return -32768;
  if (x >  32767) return  32767;
  return x;
}

// ---------------------------------------------------------------------------
// stmlib replacement: CosineOscillator
// Quadrature IIR oscillator producing values in [0, 1].
// ---------------------------------------------------------------------------
enum CosineOscillatorMode {
  COSINE_OSCILLATOR_APPROXIMATE,
  COSINE_OSCILLATOR_EXACT
};

class CosineOscillator {
 public:
  CosineOscillator() : y1_(0.0f), y0_(0.0f),
                        iir_coefficient_(0.0f), initial_amplitude_(0.0f) {}
  ~CosineOscillator() {}

  template<CosineOscillatorMode mode>
  inline void Init(float frequency) {
    if (mode == COSINE_OSCILLATOR_APPROXIMATE) {
      InitApproximate(frequency);
    } else {
      iir_coefficient_ = 2.0f * cosf(2.0f * static_cast<float>(M_PI) * frequency);
      initial_amplitude_ = iir_coefficient_ * 0.25f;
    }
    Start();
  }

  inline void InitApproximate(float frequency) {
    float sign = 16.0f;
    frequency -= 0.25f;
    if (frequency < 0.0f) {
      frequency = -frequency;
    } else {
      if (frequency > 0.5f) {
        frequency -= 0.5f;
      } else {
        sign = -16.0f;
      }
    }
    iir_coefficient_ = sign * frequency * (1.0f - 2.0f * frequency);
    initial_amplitude_ = iir_coefficient_ * 0.25f;
  }

  inline void Start() {
    y1_ = initial_amplitude_;
    y0_ = 0.5f;
  }

  inline float value() const {
    return y1_ + 0.5f;
  }

  inline float Next() {
    float temp = y0_;
    y0_ = iir_coefficient_ * y0_ - y1_;
    y1_ = temp;
    return temp + 0.5f;
  }

 private:
  float y1_;
  float y0_;
  float iir_coefficient_;
  float initial_amplitude_;

  CosineOscillator(const CosineOscillator&) = delete;
  CosineOscillator& operator=(const CosineOscillator&) = delete;
};

// ---------------------------------------------------------------------------
// FxEngine core
// ---------------------------------------------------------------------------

#define TAIL , -1

enum Format {
  FORMAT_12_BIT,
  FORMAT_16_BIT,
  FORMAT_32_BIT
};

enum LFOIndex {
  LFO_1,
  LFO_2
};

template<Format format>
struct DataType { };

template<>
struct DataType<FORMAT_12_BIT> {
  typedef uint16_t T;

  static inline float Decompress(T value) {
    return static_cast<float>(static_cast<int16_t>(value)) / 4096.0f;
  }

  static inline T Compress(float value) {
    return static_cast<uint16_t>(
        Clip16(static_cast<int32_t>(value * 4096.0f)));
  }
};

template<>
struct DataType<FORMAT_16_BIT> {
  typedef uint16_t T;

  static inline float Decompress(T value) {
    return static_cast<float>(static_cast<int16_t>(value)) / 32768.0f;
  }

  static inline T Compress(float value) {
    return static_cast<uint16_t>(
        Clip16(static_cast<int32_t>(value * 32768.0f)));
  }
};

template<>
struct DataType<FORMAT_32_BIT> {
  typedef float T;

  static inline float Decompress(T value) {
    return value;
  }

  static inline T Compress(float value) {
    return value;
  }
};

template<
    size_t size,
    Format format = FORMAT_12_BIT>
class FxEngine {
 public:
  typedef typename DataType<format>::T T;
  FxEngine() : write_ptr_(0), buffer_(nullptr) {}
  ~FxEngine() {}

  void Init(T* buffer) {
    buffer_ = buffer;
    Clear();
  }

  void Clear() {
    std::fill(&buffer_[0], &buffer_[size], T{});
    write_ptr_ = 0;
  }

  struct Empty { };

  template<int32_t l, typename T = Empty>
  struct Reserve {
    typedef T Tail;
    enum {
      length = l
    };
  };

  template<typename Memory, int32_t index>
  struct DelayLine {
    enum {
      length = DelayLine<typename Memory::Tail, index - 1>::length,
      base = DelayLine<Memory, index - 1>::base + DelayLine<Memory, index - 1>::length + 1
    };
  };

  template<typename Memory>
  struct DelayLine<Memory, 0> {
    enum {
      length = Memory::length,
      base = 0
    };
  };

  class Context {
   friend class FxEngine;
   public:
    Context() : accumulator_(0.0f), previous_read_(0.0f), buffer_(nullptr), write_ptr_(0) {
      lfo_value_[0] = 0.0f;
      lfo_value_[1] = 0.0f;
    }
    ~Context() {}

    inline void Load(float value) {
      accumulator_ = value;
    }

    inline void Read(float value, float scale) {
      accumulator_ += value * scale;
    }

    inline void Read(float value) {
      accumulator_ += value;
    }

    inline void Write(float& value) {
      value = accumulator_;
    }

    inline void Write(float& value, float scale) {
      value = accumulator_;
      accumulator_ *= scale;
    }

    template<typename D>
    inline void Write(D& d, int32_t offset, float scale) {
      static_assert(D::base + D::length <= size, "delay memory full");
      T w = DataType<format>::Compress(accumulator_);
      if (offset == -1) {
        buffer_[(write_ptr_ + D::base + D::length - 1) & MASK] = w;
      } else {
        buffer_[(write_ptr_ + D::base + offset) & MASK] = w;
      }
      accumulator_ *= scale;
    }

    template<typename D>
    inline void Write(D& d, float scale) {
      Write(d, 0, scale);
    }

    template<typename D>
    inline void WriteAllPass(D& d, int32_t offset, float scale) {
      Write(d, offset, scale);
      accumulator_ += previous_read_;
    }

    template<typename D>
    inline void WriteAllPass(D& d, float scale) {
      WriteAllPass(d, 0, scale);
    }

    template<typename D>
    inline void Read(D& d, int32_t offset, float scale) {
      static_assert(D::base + D::length <= size, "delay memory full");
      T r;
      if (offset == -1) {
        r = buffer_[(write_ptr_ + D::base + D::length - 1) & MASK];
      } else {
        r = buffer_[(write_ptr_ + D::base + offset) & MASK];
      }
      float r_f = DataType<format>::Decompress(r);
      previous_read_ = r_f;
      accumulator_ += r_f * scale;
    }

    template<typename D>
    inline void Read(D& d, float scale) {
      Read(d, 0, scale);
    }

    inline void Lp(float& state, float coefficient) {
      state += coefficient * (accumulator_ - state);
      accumulator_ = state;
    }

    inline void Hp(float& state, float coefficient) {
      state += coefficient * (accumulator_ - state);
      accumulator_ -= state;
    }

    // Soft limiter for feedback loop energy control.
    // Below threshold: unity gain (transparent).
    // Above threshold: tanh-shaped compression toward ceiling.
    // Output is strictly bounded to (-ceiling, +ceiling).
    // Uses Padé [3,3] approximant with hard clamp (Padé diverges beyond |x|>3).
    inline void SoftLimit(float threshold, float ceiling) {
      float x = accumulator_;
      float ax = (x > 0.0f) ? x : -x;
      if (ax > threshold) {
        float knee = ceiling - threshold;
        float norm = (ax - threshold) / knee;
        float saturated;
        if (norm >= 3.0f) {
          saturated = 1.0f;  // hard clamp — Padé is inaccurate beyond 3
        } else {
          float n2 = norm * norm;
          saturated = norm * (27.0f + n2) / (27.0f + 9.0f * n2);
        }
        float out = threshold + saturated * knee;
        accumulator_ = (x > 0.0f) ? out : -out;
      }
    }

    template<typename D>
    inline void Interpolate(D& d, float offset, float scale) {
      static_assert(D::base + D::length <= size, "delay memory full");
      MAKE_INTEGRAL_FRACTIONAL(offset);
      float a = DataType<format>::Decompress(
          buffer_[(write_ptr_ + offset_integral + D::base) & MASK]);
      float b = DataType<format>::Decompress(
          buffer_[(write_ptr_ + offset_integral + D::base + 1) & MASK]);
      float x = a + (b - a) * offset_fractional;
      previous_read_ = x;
      accumulator_ += x * scale;
    }

    template<typename D>
    inline void Interpolate(
        D& d, float offset, LFOIndex index, float amplitude, float scale) {
      static_assert(D::base + D::length <= size, "delay memory full");
      offset += amplitude * lfo_value_[index];
      MAKE_INTEGRAL_FRACTIONAL(offset);
      float a = DataType<format>::Decompress(
          buffer_[(write_ptr_ + offset_integral + D::base) & MASK]);
      float b = DataType<format>::Decompress(
          buffer_[(write_ptr_ + offset_integral + D::base + 1) & MASK]);
      float x = a + (b - a) * offset_fractional;
      previous_read_ = x;
      accumulator_ += x * scale;
    }

   private:
    float accumulator_;
    float previous_read_;
    float lfo_value_[2];
    T* buffer_;
    int32_t write_ptr_;

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
  };

  inline void SetLFOFrequency(LFOIndex index, float frequency) {
    lfo_[index].template Init<COSINE_OSCILLATOR_APPROXIMATE>(
        frequency * 32.0f);
  }

  inline void Start(Context* c) {
    --write_ptr_;
    if (write_ptr_ < 0) {
      write_ptr_ += static_cast<int32_t>(size);
    }
    c->accumulator_ = 0.0f;
    c->previous_read_ = 0.0f;
    c->buffer_ = buffer_;
    c->write_ptr_ = write_ptr_;
    if ((write_ptr_ & 31) == 0) {
      c->lfo_value_[0] = lfo_[0].Next();
      c->lfo_value_[1] = lfo_[1].Next();
    } else {
      c->lfo_value_[0] = lfo_[0].value();
      c->lfo_value_[1] = lfo_[1].value();
    }
  }

 private:
  enum {
    MASK = size - 1
  };

  int32_t write_ptr_;
  T* buffer_;
  CosineOscillator lfo_[2];

  FxEngine(const FxEngine&) = delete;
  FxEngine& operator=(const FxEngine&) = delete;
};

}  // namespace clouds

#endif  // CLOUDS_FX_ENGINE_H_
