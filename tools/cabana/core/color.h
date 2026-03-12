#pragma once

#include <cstdint>
#include <algorithm>
#include <cmath>

struct CabanaColor {
  uint8_t r = 0, g = 0, b = 0, a = 255;

  CabanaColor() = default;
  CabanaColor(int r_, int g_, int b_, int a_ = 255)
    : r(std::clamp(r_, 0, 255)), g(std::clamp(g_, 0, 255)),
      b(std::clamp(b_, 0, 255)), a(std::clamp(a_, 0, 255)) {}

  static CabanaColor fromHsvF(float h, float s, float v, float a = 1.0f) {
    // HSV to RGB conversion
    float c = v * s;
    float x = c * (1.0f - std::fabs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
    float m = v - c;
    float rf, gf, bf;
    int hi = static_cast<int>(h * 6.0f) % 6;
    switch (hi) {
      case 0: rf = c; gf = x; bf = 0; break;
      case 1: rf = x; gf = c; bf = 0; break;
      case 2: rf = 0; gf = c; bf = x; break;
      case 3: rf = 0; gf = x; bf = c; break;
      case 4: rf = x; gf = 0; bf = c; break;
      default: rf = c; gf = 0; bf = x; break;
    }
    return CabanaColor(
      static_cast<int>((rf + m) * 255.0f + 0.5f),
      static_cast<int>((gf + m) * 255.0f + 0.5f),
      static_cast<int>((bf + m) * 255.0f + 0.5f),
      static_cast<int>(a * 255.0f + 0.5f)
    );
  }

  int red() const { return r; }
  int green() const { return g; }
  int blue() const { return b; }
  int alpha() const { return a; }

  float redF() const { return r / 255.0f; }
  float greenF() const { return g / 255.0f; }
  float blueF() const { return b / 255.0f; }
  float alphaF() const { return a / 255.0f; }

  void setAlphaF(float af) { a = static_cast<uint8_t>(std::clamp(af, 0.0f, 1.0f) * 255.0f + 0.5f); }
  void setAlpha(int av) { a = static_cast<uint8_t>(std::clamp(av, 0, 255)); }

  CabanaColor darker(int factor = 200) const {
    float f = 100.0f / std::max(factor, 1);
    return CabanaColor(
      static_cast<int>(r * f + 0.5f),
      static_cast<int>(g * f + 0.5f),
      static_cast<int>(b * f + 0.5f), a);
  }

  CabanaColor lighter(int factor = 150) const {
    if (factor <= 0) return *this;
    if (factor < 100) return darker(10000 / factor);
    // Convert to HSV, scale V, convert back
    float rf = redF(), gf = greenF(), bf = blueF();
    float cmax = std::max({rf, gf, bf});
    float cmin = std::min({rf, gf, bf});
    float v = cmax * factor / 100.0f;
    if (v > 1.0f) {
      // Desaturate towards white
      float s_factor = 1.0f - (v - 1.0f);
      if (s_factor < 0) s_factor = 0;
      float delta = cmax - cmin;
      float s = cmax > 0 ? delta / cmax : 0;
      s *= s_factor;
      float h = 0;
      if (delta > 0) {
        if (cmax == rf) h = std::fmod((gf - bf) / delta, 6.0f);
        else if (cmax == gf) h = (bf - rf) / delta + 2.0f;
        else h = (rf - gf) / delta + 4.0f;
        h /= 6.0f;
        if (h < 0) h += 1.0f;
      }
      return CabanaColor::fromHsvF(h, s, std::min(v, 1.0f), alphaF());
    }
    // Simple scale
    return CabanaColor(
      std::min(255, static_cast<int>(r * factor / 100.0f + 0.5f)),
      std::min(255, static_cast<int>(g * factor / 100.0f + 0.5f)),
      std::min(255, static_cast<int>(b * factor / 100.0f + 0.5f)), a);
  }

  bool isValid() const { return true; }  // always valid (unlike QColor which can be invalid)

};
