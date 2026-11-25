#pragma once

#include <cmath>
#include <complex>
#include <algorithm>

namespace et
{
constexpr double kPi = 3.14159265358979323846;

//==============================================================================
/** RBJ biquad coefficients (normalised so a0 == 1). */
struct BiquadCoefs
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    static BiquadCoefs lowPass (double fs, double f, double q)
    {
        f = std::clamp (f, 10.0, fs * 0.49);
        const double w = 2.0 * kPi * f / fs, c = std::cos (w), s = std::sin (w), al = s / (2.0 * q);
        return norm ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }

    static BiquadCoefs highPass (double fs, double f, double q)
    {
        f = std::clamp (f, 10.0, fs * 0.49);
        const double w = 2.0 * kPi * f / fs, c = std::cos (w), s = std::sin (w), al = s / (2.0 * q);
        return norm ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }

    static BiquadCoefs peak (double fs, double f, double q, double gainDb)
    {
        f = std::clamp (f, 10.0, fs * 0.49);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * f / fs, c = std::cos (w), s = std::sin (w), al = s / (2.0 * q);
        return norm (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }

    static BiquadCoefs lowShelf (double fs, double f, double gainDb)
    {
        f = std::clamp (f, 10.0, fs * 0.49);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * f / fs, c = std::cos (w), s = std::sin (w);
        const double al = s / 2.0 * std::sqrt (2.0);   // shelf slope S = 1
        const double sa = 2.0 * std::sqrt (A) * al;
        return norm (A * ((A + 1) - (A - 1) * c + sa),
                     2 * A * ((A - 1) - (A + 1) * c),
                     A * ((A + 1) - (A - 1) * c - sa),
                     (A + 1) + (A - 1) * c + sa,
                     -2 * ((A - 1) + (A + 1) * c),
                     (A + 1) + (A - 1) * c - sa);
    }

    static BiquadCoefs highShelf (double fs, double f, double gainDb)
    {
        f = std::clamp (f, 10.0, fs * 0.49);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * f / fs, c = std::cos (w), s = std::sin (w);
        const double al = s / 2.0 * std::sqrt (2.0);
        const double sa = 2.0 * std::sqrt (A) * al;
        return norm (A * ((A + 1) + (A - 1) * c + sa),
                     -2 * A * ((A - 1) + (A + 1) * c),
                     A * ((A + 1) + (A - 1) * c - sa),
                     (A + 1) - (A - 1) * c + sa,
                     2 * ((A - 1) - (A + 1) * c),
                     (A + 1) - (A - 1) * c - sa);
    }

    /** |H(e^jw)| at a frequency. */
    double magnitude (double fs, double f) const
    {
        const double w = 2.0 * kPi * f / fs;
        const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
        const auto num = b0 + b1 * z1 + b2 * z2;
        const auto den = 1.0 + a1 * z1 + a2 * z2;
        return std::abs (num / den);
    }

private:
    static BiquadCoefs norm (double b0, double b1, double b2, double a0, double a1, double a2)
    {
        BiquadCoefs c;
        c.b0 = b0 / a0; c.b1 = b1 / a0; c.b2 = b2 / a0; c.a1 = a1 / a0; c.a2 = a2 / a0;
        return c;
    }
};

/** Transposed direct form II biquad. */
struct Biquad
{
    BiquadCoefs c;
    double z1 = 0, z2 = 0;

    inline double process (double x) noexcept
    {
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }

    void reset() noexcept { z1 = z2 = 0; }
};

//==============================================================================
struct EqSettings
{
    bool  on  = true;
    float hpF = 20.0f, lpF = 20000.0f;
    float lsF = 120.0f, lsG = 0.0f;
    float b1F = 450.0f, b1G = 0.0f, b1Q = 1.0f;
    float b2F = 3500.0f, b2G = 0.0f, b2Q = 1.0f;
    float hsF = 8000.0f, hsG = 0.0f;

    bool hpActive() const { return hpF > 20.5f; }
    bool lpActive() const { return lpF < 19900.0f; }
    static bool gainActive (float g) { return std::abs (g) > 0.04f; }
};

/** Stereo 6-band EQ: HP, low shelf, 2 bells, high shelf, LP. Used for rendering and for drawing the curve. */
class EqChain
{
public:
    void prepare (double sampleRate, const EqSettings& s)
    {
        fs = sampleRate;
        settings = s;
        for (int ch = 0; ch < 2; ++ch)
        {
            hp[ch].c = BiquadCoefs::highPass (fs, s.hpF, 0.7071);
            lp[ch].c = BiquadCoefs::lowPass  (fs, s.lpF, 0.7071);
            ls[ch].c = BiquadCoefs::lowShelf (fs, s.lsF, s.lsG);
            b1[ch].c = BiquadCoefs::peak (fs, s.b1F, s.b1Q, s.b1G);
            b2[ch].c = BiquadCoefs::peak (fs, s.b2F, s.b2Q, s.b2G);
            hs[ch].c = BiquadCoefs::highShelf (fs, s.hsF, s.hsG);
            hp[ch].reset(); lp[ch].reset(); ls[ch].reset(); b1[ch].reset(); b2[ch].reset(); hs[ch].reset();
        }
    }

    inline double process (int ch, double x) noexcept
    {
        if (! settings.on) return x;
        if (settings.hpActive()) x = hp[ch].process (x);
        if (EqSettings::gainActive (settings.lsG)) x = ls[ch].process (x);
        if (EqSettings::gainActive (settings.b1G)) x = b1[ch].process (x);
        if (EqSettings::gainActive (settings.b2G)) x = b2[ch].process (x);
        if (EqSettings::gainActive (settings.hsG)) x = hs[ch].process (x);
        if (settings.lpActive()) x = lp[ch].process (x);
        return x;
    }

    /** Combined magnitude response in dB at a frequency (for the UI curve). */
    static double responseDb (const EqSettings& s, double fs, double f)
    {
        double m = 1.0;
        if (s.hpActive()) { const auto c = BiquadCoefs::highPass (fs, s.hpF, 0.7071); m *= c.magnitude (fs, f); }
        if (EqSettings::gainActive (s.lsG)) m *= BiquadCoefs::lowShelf (fs, s.lsF, s.lsG).magnitude (fs, f);
        if (EqSettings::gainActive (s.b1G)) m *= BiquadCoefs::peak (fs, s.b1F, s.b1Q, s.b1G).magnitude (fs, f);
        if (EqSettings::gainActive (s.b2G)) m *= BiquadCoefs::peak (fs, s.b2F, s.b2Q, s.b2G).magnitude (fs, f);
        if (EqSettings::gainActive (s.hsG)) m *= BiquadCoefs::highShelf (fs, s.hsF, s.hsG).magnitude (fs, f);
        if (s.lpActive()) m *= BiquadCoefs::lowPass (fs, s.lpF, 0.7071).magnitude (fs, f);
        return 20.0 * std::log10 (std::max (m, 1.0e-6));
    }

private:
    double fs = 48000.0;
    EqSettings settings;
    Biquad hp[2], lp[2], ls[2], b1[2], b2[2], hs[2];
};

} // namespace et
