#pragma once

#include <cmath>
#include <algorithm>

namespace kalyx::dsp
{

/**
 * Filtre Biquad IIR Transposed Direct Form II (TDF-II) 64-bit & Évaluateur analytique de courbe EQ.
 */
class BiquadFilter
{
public:
    struct Coeffs
    {
        double b0 { 1.0 };
        double b1 { 0.0 };
        double b2 { 0.0 };
        double a1 { 0.0 };
        double a2 { 0.0 };
    };

    BiquadFilter() noexcept = default;

    void reset() noexcept
    {
        z1_ = 0.0;
        z2_ = 0.0;
    }

    void setCoefficients(const Coeffs& c) noexcept
    {
        coeffs_ = c;
    }

    [[nodiscard]] const Coeffs& getCoefficients() const noexcept
    {
        return coeffs_;
    }

    [[nodiscard]] inline float processSample(float x) noexcept
    {
        const double in = static_cast<double>(x);
        const double y  = coeffs_.b0 * in + z1_;
        z1_ = coeffs_.b1 * in - coeffs_.a1 * y + z2_;
        z2_ = coeffs_.b2 * in - coeffs_.a2 * y;

        if (!std::isfinite(y))
        {
            reset();
            return 0.0f;
        }
        return static_cast<float>(y);
    }

    static Coeffs makeHighPass(double sampleRate, double freqHz, double q = 0.70710678) noexcept
    {
        const double fs = std::max(sampleRate, 8000.0);
        const double fc = std::clamp(freqHz, 10.0, 0.48 * fs);
        const double Q  = std::clamp(q, 0.25, 12.0);
        const double w0 = 2.0 * 3.14159265358979323846 * fc / fs;
        const double cw = std::cos(w0);
        const double sw = std::sin(w0);
        const double al = sw / (2.0 * Q);
        const double a0Inv = 1.0 / (1.0 + al);

        return {
            ((1.0 + cw) * 0.5) * a0Inv,
            -(1.0 + cw) * a0Inv,
            ((1.0 + cw) * 0.5) * a0Inv,
            (-2.0 * cw) * a0Inv,
            (1.0 - al) * a0Inv
        };
    }

    static Coeffs makeLowPass(double sampleRate, double freqHz, double q = 0.70710678) noexcept
    {
        const double fs = std::max(sampleRate, 8000.0);
        const double fc = std::clamp(freqHz, 20.0, 0.48 * fs);
        const double Q  = std::clamp(q, 0.25, 12.0);
        const double w0 = 2.0 * 3.14159265358979323846 * fc / fs;
        const double cw = std::cos(w0);
        const double sw = std::sin(w0);
        const double al = sw / (2.0 * Q);
        const double a0Inv = 1.0 / (1.0 + al);

        return {
            ((1.0 - cw) * 0.5) * a0Inv,
            (1.0 - cw) * a0Inv,
            ((1.0 - cw) * 0.5) * a0Inv,
            (-2.0 * cw) * a0Inv,
            (1.0 - al) * a0Inv
        };
    }

    static Coeffs makePeakEQ(double sampleRate, double freqHz, double gainDb, double q) noexcept
    {
        const double fs = std::max(sampleRate, 8000.0);
        const double fc = std::clamp(freqHz, 15.0, 0.48 * fs);
        const double Q  = std::clamp(q, 0.2, 16.0);
        const double A  = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * 3.14159265358979323846 * fc / fs;
        const double cw = std::cos(w0);
        const double sw = std::sin(w0);
        const double al = sw / (2.0 * Q);
        const double a0Inv = 1.0 / (1.0 + al / A);

        return {
            (1.0 + al * A) * a0Inv,
            (-2.0 * cw) * a0Inv,
            (1.0 - al * A) * a0Inv,
            (-2.0 * cw) * a0Inv,
            (1.0 - al / A) * a0Inv
        };
    }

    static Coeffs makeLowShelf(double sampleRate, double freqHz, double gainDb, double q = 0.70710678) noexcept
    {
        const double fs = std::max(sampleRate, 8000.0);
        const double fc = std::clamp(freqHz, 15.0, 0.48 * fs);
        const double Q  = std::clamp(q, 0.25, 4.0);
        const double A  = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * 3.14159265358979323846 * fc / fs;
        const double cw = std::cos(w0);
        const double sw = std::sin(w0);
        const double al = sw / (2.0 * Q);
        const double twoSqrtAAlpha = 2.0 * std::sqrt(A) * al;
        const double a0Inv = 1.0 / ((A + 1.0) + (A - 1.0) * cw + twoSqrtAAlpha);

        return {
            (A * ((A + 1.0) - (A - 1.0) * cw + twoSqrtAAlpha)) * a0Inv,
            (2.0 * A * ((A - 1.0) - (A + 1.0) * cw)) * a0Inv,
            (A * ((A + 1.0) - (A - 1.0) * cw - twoSqrtAAlpha)) * a0Inv,
            (-2.0 * ((A - 1.0) + (A + 1.0) * cw)) * a0Inv,
            ((A + 1.0) + (A - 1.0) * cw - twoSqrtAAlpha) * a0Inv
        };
    }

    static Coeffs makeHighShelf(double sampleRate, double freqHz, double gainDb, double q = 0.70710678) noexcept
    {
        const double fs = std::max(sampleRate, 8000.0);
        const double fc = std::clamp(freqHz, 20.0, 0.48 * fs);
        const double Q  = std::clamp(q, 0.25, 4.0);
        const double A  = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * 3.14159265358979323846 * fc / fs;
        const double cw = std::cos(w0);
        const double sw = std::sin(w0);
        const double al = sw / (2.0 * Q);
        const double twoSqrtAAlpha = 2.0 * std::sqrt(A) * al;
        const double a0Inv = 1.0 / ((A + 1.0) - (A - 1.0) * cw + twoSqrtAAlpha);

        return {
            (A * ((A + 1.0) + (A - 1.0) * cw + twoSqrtAAlpha)) * a0Inv,
            (-2.0 * A * ((A - 1.0) + (A + 1.0) * cw)) * a0Inv,
            (A * ((A + 1.0) + (A - 1.0) * cw - twoSqrtAAlpha)) * a0Inv,
            (2.0 * ((A - 1.0) - (A + 1.0) * cw)) * a0Inv,
            ((A + 1.0) - (A - 1.0) * cw - twoSqrtAAlpha) * a0Inv
        };
    }

    static float getMagnitudeDbAtFrequency(const Coeffs& c, double freqHz, double sampleRate) noexcept
    {
        const double fs = std::max(sampleRate, 8000.0);
        const double w  = 2.0 * 3.14159265358979323846 * std::clamp(freqHz, 10.0, 0.49 * fs) / fs;
        const double c1 = std::cos(w);
        const double s1 = std::sin(w);
        const double c2 = std::cos(2.0 * w);
        const double s2 = std::sin(2.0 * w);

        const double numRe = c.b0 + c.b1 * c1 + c.b2 * c2;
        const double numIm = -(c.b1 * s1 + c.b2 * s2);
        const double denRe = 1.0 + c.a1 * c1 + c.a2 * c2;
        const double denIm = -(c.a1 * s1 + c.a2 * s2);

        const double numSq = numRe * numRe + numIm * numIm;
        const double denSq = std::max(denRe * denRe + denIm * denIm, 1.0e-18);
        return static_cast<float>(10.0 * std::log10(std::max(numSq / denSq, 1.0e-12)));
    }

private:
    Coeffs coeffs_ {};
    double z1_ { 0.0 };
    double z2_ { 0.0 };
};

/**
 * Filtre ZDF / TPT (Zero-Delay Feedback / Topology-Preserving Transform) 64-bit
 * par intégration trapézoïdale d'Andy Simper (Cytomic).
 * Inconditionnellement stable sous modulation rapide de fréquence et de Q, sans cramping à Nyquist.
 */
class ZDFStateVariableFilter
{
public:
    ZDFStateVariableFilter() noexcept = default;

    void reset() noexcept
    {
        ic1eq_ = 0.0;
        ic2eq_ = 0.0;
    }

    void setHighPass(double sampleRate, double freqHz, double q = 0.70710678) noexcept
    {
        const double fs = std::max(sampleRate, 8000.0);
        const double fc = std::clamp(freqHz, 10.0, 0.48 * fs);
        const double Q  = std::clamp(q, 0.30, 12.0);

        g_  = std::tan(3.14159265358979323846 * fc / fs);
        k_  = 1.0 / Q;
        a1_ = 1.0 / (1.0 + g_ * (g_ + k_));
        a2_ = g_ * a1_;
        a3_ = g_ * a2_;
        m0_ = 1.0;
        m1_ = -k_;
        m2_ = -1.0;
    }

    void setLowPass(double sampleRate, double freqHz, double q = 0.70710678) noexcept
    {
        const double fs = std::max(sampleRate, 8000.0);
        const double fc = std::clamp(freqHz, 20.0, 0.48 * fs);
        const double Q  = std::clamp(q, 0.30, 12.0);

        g_  = std::tan(3.14159265358979323846 * fc / fs);
        k_  = 1.0 / Q;
        a1_ = 1.0 / (1.0 + g_ * (g_ + k_));
        a2_ = g_ * a1_;
        a3_ = g_ * a2_;
        m0_ = 0.0;
        m1_ = 0.0;
        m2_ = 1.0;
    }

    void setBell(double sampleRate, double freqHz, double q, double gainDb) noexcept
    {
        const double fs = std::max(sampleRate, 8000.0);
        const double fc = std::clamp(freqHz, 15.0, 0.48 * fs);
        const double Q  = std::clamp(q, 0.20, 16.0);
        const double A  = std::pow(10.0, gainDb / 40.0);

        g_  = std::tan(3.14159265358979323846 * fc / fs);
        k_  = 1.0 / (Q * A);
        a1_ = 1.0 / (1.0 + g_ * (g_ + k_));
        a2_ = g_ * a1_;
        a3_ = g_ * a2_;
        m0_ = 1.0;
        m1_ = k_ * (A * A - 1.0);
        m2_ = 0.0;
    }

    void setLowShelf(double sampleRate, double freqHz, double q, double gainDb) noexcept
    {
        const double fs = std::max(sampleRate, 8000.0);
        const double fc = std::clamp(freqHz, 15.0, 0.48 * fs);
        const double Q  = std::clamp(q, 0.25, 4.0);
        const double A  = std::pow(10.0, gainDb / 40.0);

        g_  = std::tan(3.14159265358979323846 * fc / fs) / std::sqrt(A);
        k_  = 1.0 / Q;
        a1_ = 1.0 / (1.0 + g_ * (g_ + k_));
        a2_ = g_ * a1_;
        a3_ = g_ * a2_;
        m0_ = 1.0;
        m1_ = k_ * (A - 1.0);
        m2_ = A * A - 1.0;
    }

    void setHighShelf(double sampleRate, double freqHz, double q, double gainDb) noexcept
    {
        const double fs = std::max(sampleRate, 8000.0);
        const double fc = std::clamp(freqHz, 20.0, 0.48 * fs);
        const double Q  = std::clamp(q, 0.25, 4.0);
        const double A  = std::pow(10.0, gainDb / 40.0);

        g_  = std::tan(3.14159265358979323846 * fc / fs) * std::sqrt(A);
        k_  = 1.0 / Q;
        a1_ = 1.0 / (1.0 + g_ * (g_ + k_));
        a2_ = g_ * a1_;
        a3_ = g_ * a2_;
        m0_ = A * A;
        m1_ = k_ * (1.0 - A) * A;
        m2_ = 1.0 - A * A;
    }

    void setNotch(double sampleRate, double freqHz, double q) noexcept
    {
        const double fs = std::max(sampleRate, 8000.0);
        const double fc = std::clamp(freqHz, 15.0, 0.48 * fs);
        const double Q  = std::clamp(q, 0.25, 24.0);

        g_  = std::tan(3.14159265358979323846 * fc / fs);
        k_  = 1.0 / Q;
        a1_ = 1.0 / (1.0 + g_ * (g_ + k_));
        a2_ = g_ * a1_;
        a3_ = g_ * a2_;
        m0_ = 1.0;
        m1_ = -k_;
        m2_ = 0.0;
    }

    void setBandPass(double sampleRate, double freqHz, double q) noexcept
    {
        const double fs = std::max(sampleRate, 8000.0);
        const double fc = std::clamp(freqHz, 15.0, 0.48 * fs);
        const double Q  = std::clamp(q, 0.20, 18.0);

        g_  = std::tan(3.14159265358979323846 * fc / fs);
        k_  = 1.0 / Q;
        a1_ = 1.0 / (1.0 + g_ * (g_ + k_));
        a2_ = g_ * a1_;
        a3_ = g_ * a2_;
        m0_ = 0.0;
        m1_ = k_;
        m2_ = 0.0;
    }

    [[nodiscard]] inline float processSample(float x) noexcept
    {
        const double v0 = static_cast<double>(x);
        const double v3 = v0 - ic2eq_;
        const double v1 = a1_ * ic1eq_ + a2_ * v3;
        const double v2 = ic2eq_ + a2_ * ic1eq_ + a3_ * v3;
        ic1eq_ = 2.0 * v1 - ic1eq_;
        ic2eq_ = 2.0 * v2 - ic2eq_;

        const double out = m0_ * v0 + m1_ * v1 + m2_ * v2;
        if (!std::isfinite(out))
        {
            reset();
            return 0.0f;
        }
        return static_cast<float>(out);
    }

private:
    double ic1eq_ { 0.0 };
    double ic2eq_ { 0.0 };
    double g_     { 0.0 };
    double k_     { 1.41421356 };
    double a1_    { 0.0 };
    double a2_    { 0.0 };
    double a3_    { 0.0 };
    double m0_    { 1.0 };
    double m1_    { 0.0 };
    double m2_    { 0.0 };
};

} // namespace kalyx::dsp
