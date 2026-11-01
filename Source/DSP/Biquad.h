#pragma once

#include <algorithm>
#include <cmath>
#include <complex>

namespace org
{
// RBJ-cookbook biquad, transposed direct form II.
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void reset() noexcept { z1 = z2 = 0.0f; }

    void lowPass (double sr, double f, double q)
    {
        const double w = omega (sr, f), c = std::cos (w), al = std::sin (w) / (2.0 * std::max (0.1, q));
        set ((1.0 - c) * 0.5, 1.0 - c, (1.0 - c) * 0.5, 1.0 + al, -2.0 * c, 1.0 - al);
    }

    void highPass (double sr, double f, double q)
    {
        const double w = omega (sr, f), c = std::cos (w), al = std::sin (w) / (2.0 * std::max (0.1, q));
        set ((1.0 + c) * 0.5, -(1.0 + c), (1.0 + c) * 0.5, 1.0 + al, -2.0 * c, 1.0 - al);
    }

    // constant 0 dB peak gain band-pass
    void bandPass (double sr, double f, double q)
    {
        const double w = omega (sr, f), c = std::cos (w), al = std::sin (w) / (2.0 * std::max (0.1, q));
        set (al, 0.0, -al, 1.0 + al, -2.0 * c, 1.0 - al);
    }

    void peak (double sr, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = omega (sr, f), c = std::cos (w);
        const double al = std::sin (w) / (2.0 * std::max (0.1, q));
        set (1.0 + al * A, -2.0 * c, 1.0 - al * A, 1.0 + al / A, -2.0 * c, 1.0 - al / A);
    }

    void lowShelf (double sr, double f, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = omega (sr, f), c = std::cos (w);
        const double sq = 2.0 * std::sqrt (A) * std::sin (w) / 2.0 * std::sqrt (2.0);
        set (A * ((A + 1.0) - (A - 1.0) * c + sq), 2.0 * A * ((A - 1.0) - (A + 1.0) * c), A * ((A + 1.0) - (A - 1.0) * c - sq),
             (A + 1.0) + (A - 1.0) * c + sq, -2.0 * ((A - 1.0) + (A + 1.0) * c), (A + 1.0) + (A - 1.0) * c - sq);
    }

    void highShelf (double sr, double f, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = omega (sr, f), c = std::cos (w);
        const double sq = 2.0 * std::sqrt (A) * std::sin (w) / 2.0 * std::sqrt (2.0);
        set (A * ((A + 1.0) + (A - 1.0) * c + sq), -2.0 * A * ((A - 1.0) + (A + 1.0) * c), A * ((A + 1.0) + (A - 1.0) * c - sq),
             (A + 1.0) - (A - 1.0) * c + sq, 2.0 * ((A - 1.0) - (A + 1.0) * c), (A + 1.0) - (A - 1.0) * c - sq);
    }

    double magnitudeDb (double f, double sr) const
    {
        const double w = 2.0 * kPi * f / sr;
        const std::complex<double> z1c = std::polar (1.0, -w), z2c = std::polar (1.0, -2.0 * w);
        const auto num = (double) b0 + (double) b1 * z1c + (double) b2 * z2c;
        const auto den = 1.0 + (double) a1 * z1c + (double) a2 * z2c;
        return 20.0 * std::log10 (std::max (1.0e-9, std::abs (num / den)));
    }

private:
    static constexpr double kPi = 3.14159265358979323846;

    static double omega (double sr, double f)
    {
        f = std::min (std::max (f, 10.0), sr * 0.45);
        return 2.0 * kPi * f / sr;
    }

    void set (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }
};

// simple per-sample one-pole smoother
struct Smooth
{
    float v = 0.0f, coef = 1.0f;
    void setTime (double sr, double ms) { coef = ms <= 0.0 ? 1.0f : (float) (1.0 - std::exp (-1.0 / (ms * 0.001 * sr))); }
    inline float next (float target) noexcept { v += (target - v) * coef; return v; }
};
} // namespace org
