#pragma once

#include <cmath>
#include <algorithm>
#include <array>

namespace kalyx::dsp
{

/**
 * Filtre Biquad IIR (Transposed Direct Form II) 100% temps réel,
 * zéro allocation dynamique, découplé de JUCE.
 */
class BiquadFilter
{
public:
    struct Coefficients
    {
        float b0 { 1.0f };
        float b1 { 0.0f };
        float b2 { 0.0f };
        float a1 { 0.0f };
        float a2 { 0.0f };
    };

    BiquadFilter() noexcept = default;

    void reset() noexcept
    {
        z1_ = { 0.0f, 0.0f };
        z2_ = { 0.0f, 0.0f };
    }

    void setCoefficients(const Coefficients& coeffs) noexcept
    {
        c_ = coeffs;
    }

    [[nodiscard]] const Coefficients& getCoefficients() const noexcept
    {
        return c_;
    }

    [[nodiscard]] float processSample(float x, int channel) noexcept
    {
        const std::size_t ch = (channel > 0) ? 1u : 0u;
        const float y = c_.b0 * x + z1_[ch];
        z1_[ch] = c_.b1 * x - c_.a1 * y + z2_[ch];
        z2_[ch] = c_.b2 * x - c_.a2 * y;
        return y;
    }

    [[nodiscard]] static Coefficients makeHighPass(double sampleRate, float freqHz, float q = 0.70710678f) noexcept
    {
        const double fs = std::max(sampleRate, 8000.0);
        const double fc = std::clamp(static_cast<double>(freqHz), 5.0, 0.48 * fs);
        const double w0 = 2.0 * 3.14159265358979323846 * fc / fs;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double alpha = sinW0 / (2.0 * std::max(static_cast<double>(q), 0.1));

        const double b0 = (1.0 + cosW0) * 0.5;
        const double b1 = -(1.0 + cosW0);
        const double b2 = (1.0 + cosW0) * 0.5;
        const double a0 = 1.0 + alpha;
        const double a1 = -2.0 * cosW0;
        const double a2 = 1.0 - alpha;

        const double invA0 = 1.0 / a0;
        return {
            static_cast<float>(b0 * invA0),
            static_cast<float>(b1 * invA0),
            static_cast<float>(b2 * invA0),
            static_cast<float>(a1 * invA0),
            static_cast<float>(a2 * invA0)
        };
    }

    [[nodiscard]] static Coefficients makeLowPass(double sampleRate, float freqHz, float q = 0.70710678f) noexcept
    {
        const double fs = std::max(sampleRate, 8000.0);
        const double fc = std::clamp(static_cast<double>(freqHz), 20.0, 0.48 * fs);
        const double w0 = 2.0 * 3.14159265358979323846 * fc / fs;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double alpha = sinW0 / (2.0 * std::max(static_cast<double>(q), 0.1));

        const double b0 = (1.0 - cosW0) * 0.5;
        const double b1 = 1.0 - cosW0;
        const double b2 = (1.0 - cosW0) * 0.5;
        const double a0 = 1.0 + alpha;
        const double a1 = -2.0 * cosW0;
        const double a2 = 1.0 - alpha;

        const double invA0 = 1.0 / a0;
        return {
            static_cast<float>(b0 * invA0),
            static_cast<float>(b1 * invA0),
            static_cast<float>(b2 * invA0),
            static_cast<float>(a1 * invA0),
            static_cast<float>(a2 * invA0)
        };
    }

    [[nodiscard]] static Coefficients makeBandPass(double sampleRate, float freqHz, float q = 2.0f) noexcept
    {
        const double fs = std::max(sampleRate, 8000.0);
        const double fc = std::clamp(static_cast<double>(freqHz), 20.0, 0.48 * fs);
        const double w0 = 2.0 * 3.14159265358979323846 * fc / fs;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double alpha = sinW0 / (2.0 * std::max(static_cast<double>(q), 0.1));

        const double b0 = alpha;
        const double b1 = 0.0;
        const double b2 = -alpha;
        const double a0 = 1.0 + alpha;
        const double a1 = -2.0 * cosW0;
        const double a2 = 1.0 - alpha;

        const double invA0 = 1.0 / a0;
        return {
            static_cast<float>(b0 * invA0),
            static_cast<float>(b1 * invA0),
            static_cast<float>(b2 * invA0),
            static_cast<float>(a1 * invA0),
            static_cast<float>(a2 * invA0)
        };
    }

    [[nodiscard]] static Coefficients makePeakEQ(double sampleRate, float freqHz, float gainDb, float q) noexcept
    {
        const double fs = std::max(sampleRate, 8000.0);
        const double fc = std::clamp(static_cast<double>(freqHz), 15.0, 0.48 * fs);
        const double A = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
        const double w0 = 2.0 * 3.14159265358979323846 * fc / fs;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double alpha = sinW0 / (2.0 * std::max(static_cast<double>(q), 0.1));

        const double b0 = 1.0 + alpha * A;
        const double b1 = -2.0 * cosW0;
        const double b2 = 1.0 - alpha * A;
        const double a0 = 1.0 + alpha / A;
        const double a1 = -2.0 * cosW0;
        const double a2 = 1.0 - alpha / A;

        const double invA0 = 1.0 / a0;
        return {
            static_cast<float>(b0 * invA0),
            static_cast<float>(b1 * invA0),
            static_cast<float>(b2 * invA0),
            static_cast<float>(a1 * invA0),
            static_cast<float>(a2 * invA0)
        };
    }

    [[nodiscard]] static Coefficients makeLowShelf(double sampleRate, float freqHz, float gainDb, float q = 0.70710678f) noexcept
    {
        const double fs = std::max(sampleRate, 8000.0);
        const double fc = std::clamp(static_cast<double>(freqHz), 15.0, 0.48 * fs);
        const double A = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
        const double w0 = 2.0 * 3.14159265358979323846 * fc / fs;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double alpha = sinW0 / (2.0 * std::max(static_cast<double>(q), 0.1));
        const double twoSqrtAAlpha = 2.0 * std::sqrt(A) * alpha;

        const double b0 = A * ((A + 1.0) - (A - 1.0) * cosW0 + twoSqrtAAlpha);
        const double b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cosW0);
        const double b2 = A * ((A + 1.0) - (A - 1.0) * cosW0 - twoSqrtAAlpha);
        const double a0 = (A + 1.0) + (A - 1.0) * cosW0 + twoSqrtAAlpha;
        const double a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cosW0);
        const double a2 = (A + 1.0) + (A - 1.0) * cosW0 - twoSqrtAAlpha;

        const double invA0 = 1.0 / a0;
        return {
            static_cast<float>(b0 * invA0),
            static_cast<float>(b1 * invA0),
            static_cast<float>(b2 * invA0),
            static_cast<float>(a1 * invA0),
            static_cast<float>(a2 * invA0)
        };
    }

    [[nodiscard]] static Coefficients makeHighShelf(double sampleRate, float freqHz, float gainDb, float q = 0.70710678f) noexcept
    {
        const double fs = std::max(sampleRate, 8000.0);
        const double fc = std::clamp(static_cast<double>(freqHz), 15.0, 0.48 * fs);
        const double A = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
        const double w0 = 2.0 * 3.14159265358979323846 * fc / fs;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double alpha = sinW0 / (2.0 * std::max(static_cast<double>(q), 0.1));
        const double twoSqrtAAlpha = 2.0 * std::sqrt(A) * alpha;

        const double b0 = A * ((A + 1.0) + (A - 1.0) * cosW0 + twoSqrtAAlpha);
        const double b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosW0);
        const double b2 = A * ((A + 1.0) + (A - 1.0) * cosW0 - twoSqrtAAlpha);
        const double a0 = (A + 1.0) - (A - 1.0) * cosW0 + twoSqrtAAlpha;
        const double a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cosW0);
        const double a2 = (A + 1.0) - (A - 1.0) * cosW0 - twoSqrtAAlpha;

        const double invA0 = 1.0 / a0;
        return {
            static_cast<float>(b0 * invA0),
            static_cast<float>(b1 * invA0),
            static_cast<float>(b2 * invA0),
            static_cast<float>(a1 * invA0),
            static_cast<float>(a2 * invA0)
        };
    }

    [[nodiscard]] static float getMagnitudeDbAtFrequency(const Coefficients& c, double freqHz, double sampleRate) noexcept
    {
        const double fs = std::max(sampleRate, 8000.0);
        const double w = 2.0 * 3.14159265358979323846 * std::clamp(freqHz, 10.0, 0.49 * fs) / fs;
        const double cosW = std::cos(w);
        const double cos2W = std::cos(2.0 * w);
        const double sinW = std::sin(w);
        const double sin2W = std::sin(2.0 * w);

        const double numReal = static_cast<double>(c.b0) + static_cast<double>(c.b1) * cosW + static_cast<double>(c.b2) * cos2W;
        const double numImag = -static_cast<double>(c.b1) * sinW - static_cast<double>(c.b2) * sin2W;
        const double denReal = 1.0 + static_cast<double>(c.a1) * cosW + static_cast<double>(c.a2) * cos2W;
        const double denImag = -static_cast<double>(c.a1) * sinW - static_cast<double>(c.a2) * sin2W;

        const double numMagSq = numReal * numReal + numImag * numImag;
        const double denMagSq = std::max(denReal * denReal + denImag * denImag, 1e-18);
        const double mag = std::sqrt(numMagSq / denMagSq);
        return static_cast<float>(20.0 * std::log10(std::max(mag, 1e-6)));
    }

private:
    Coefficients c_ {};
    std::array<float, 2> z1_ { 0.0f, 0.0f };
    std::array<float, 2> z2_ { 0.0f, 0.0f };
};

/**
 * Filtre State-Variable Zero-Delay Feedback (ZDF / TPT - Topology-Preserving Transform de Zavalishin).
 * États internes en double précision (64 bits) pour une stabilité inconditionnelle de 44.1 kHz à 192 kHz.
 */
class ZDFStateVariableFilter
{
public:
    struct Coefficients
    {
        double a1 { 0.0 };
        double a2 { 0.0 };
        double a3 { 0.0 };
        double m0 { 1.0 };
        double m1 { 0.0 };
        double m2 { 0.0 };
    };

    ZDFStateVariableFilter() noexcept = default;

    void reset() noexcept
    {
        ic1eq_ = { 0.0, 0.0 };
        ic2eq_ = { 0.0, 0.0 };
    }

    void setCoefficients(const Coefficients& cCh0, const Coefficients& cCh1) noexcept
    {
        c_[0] = cCh0;
        c_[1] = cCh1;
    }

    void setCoefficients(const Coefficients& cBoth) noexcept
    {
        c_[0] = cBoth;
        c_[1] = cBoth;
    }

    [[nodiscard]] float processSample(float v0In, int channel) noexcept
    {
        const std::size_t ch = (channel > 0) ? 1u : 0u;
        const auto& c = c_[ch];
        const double v0 = static_cast<double>(v0In);
        const double v3 = v0 - ic2eq_[ch];
        const double v1 = c.a1 * ic1eq_[ch] + c.a2 * v3;
        const double v2 = ic2eq_[ch] + c.a2 * ic1eq_[ch] + c.a3 * v3;
        ic1eq_[ch] = 2.0 * v1 - ic1eq_[ch];
        ic2eq_[ch] = 2.0 * v2 - ic2eq_[ch];
        return static_cast<float>(c.m0 * v0 + c.m1 * v1 + c.m2 * v2);
    }

    [[nodiscard]] static Coefficients makeHighPass(double sampleRate, float freqHz, float q = 0.70710678f) noexcept
    {
        const double fs = std::max(sampleRate, 8000.0);
        const double fc = std::clamp(static_cast<double>(freqHz), 10.0, 0.48 * fs);
        const double g  = std::tan(3.14159265358979323846 * fc / fs);
        const double k  = 1.0 / std::max(static_cast<double>(q), 0.15);
        const double a1 = 1.0 / (1.0 + g * (g + k));
        const double a2 = g * a1;
        const double a3 = g * a2;
        return { a1, a2, a3, 1.0, -k, -1.0 };
    }

    [[nodiscard]] static Coefficients makeLowPass(double sampleRate, float freqHz, float q = 0.70710678f) noexcept
    {
        const double fs = std::max(sampleRate, 8000.0);
        const double fc = std::clamp(static_cast<double>(freqHz), 20.0, 0.48 * fs);
        const double g  = std::tan(3.14159265358979323846 * fc / fs);
        const double k  = 1.0 / std::max(static_cast<double>(q), 0.15);
        const double a1 = 1.0 / (1.0 + g * (g + k));
        const double a2 = g * a1;
        const double a3 = g * a2;
        return { a1, a2, a3, 0.0, 0.0, 1.0 };
    }

    [[nodiscard]] static Coefficients makeBell(double sampleRate, float freqHz, float gainDb, float q) noexcept
    {
        const double fs = std::max(sampleRate, 8000.0);
        const double fc = std::clamp(static_cast<double>(freqHz), 15.0, 0.48 * fs);
        const double A  = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
        const double g  = std::tan(3.14159265358979323846 * fc / fs);
        const double k  = 1.0 / (std::max(static_cast<double>(q), 0.15) * A);
        const double a1 = 1.0 / (1.0 + g * (g + k));
        const double a2 = g * a1;
        const double a3 = g * a2;
        return { a1, a2, a3, 1.0, k * (A * A - 1.0), 0.0 };
    }

    [[nodiscard]] static Coefficients makeLowShelf(double sampleRate, float freqHz, float gainDb, float q = 0.70710678f) noexcept
    {
        const double fs = std::max(sampleRate, 8000.0);
        const double fc = std::clamp(static_cast<double>(freqHz), 15.0, 0.48 * fs);
        const double A  = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
        const double g  = std::tan(3.14159265358979323846 * fc / fs) / std::sqrt(A);
        const double k  = 1.0 / std::max(static_cast<double>(q), 0.15);
        const double a1 = 1.0 / (1.0 + g * (g + k));
        const double a2 = g * a1;
        const double a3 = g * a2;
        return { a1, a2, a3, 1.0, k * (A - 1.0), A * A - 1.0 };
    }

    [[nodiscard]] static Coefficients makeHighShelf(double sampleRate, float freqHz, float gainDb, float q = 0.70710678f) noexcept
    {
        const double fs = std::max(sampleRate, 8000.0);
        const double fc = std::clamp(static_cast<double>(freqHz), 15.0, 0.48 * fs);
        const double A  = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
        const double g  = std::tan(3.14159265358979323846 * fc / fs) * std::sqrt(A);
        const double k  = 1.0 / std::max(static_cast<double>(q), 0.15);
        const double a1 = 1.0 / (1.0 + g * (g + k));
        const double a2 = g * a1;
        const double a3 = g * a2;
        return { a1, a2, a3, A * A, k * (1.0 - A) * A, 1.0 - A * A };
    }

private:
    std::array<Coefficients, 2> c_ {};
    std::array<double, 2> ic1eq_ { 0.0, 0.0 };
    std::array<double, 2> ic2eq_ { 0.0, 0.0 };
};

/**
 * Saturateur ADAA (Antiderivative Antialiasing) d'ordre 1 et d'ordre 2 en double précision (64 bits).
 * - f(u)  = u / sqrt(1 + u^2)
 * - F1(u) = sqrt(1 + u^2)                         (noter que F1(0) = 1.0 !)
 * - F2(u) = 0.5 * (u * sqrt(1 + u^2) + asinh(u))  (F2(0) = 0.0)
 * - Initialise u_prev = 0.0, u_prev2 = 0.0, F1_prev = 1.0 (F1(0)), F2_prev = 0.0.
 * - Repli de Taylor lorsque |u_n - u_{n-1}| < 1e-4 renvoyant directement f((u_n + u_{n-1})/2).
 * - Garantit formellement saturate(0.0) == 0.0 dans 100 % des cas.
 */
class ADAASaturator
{
public:
    ADAASaturator() noexcept
    {
        reset();
    }

    void reset(double initialBias = 0.0) noexcept
    {
        for (std::size_t ch = 0; ch < 2; ++ch)
        {
            uPrev_[ch]       = initialBias;
            uPrev2_[ch]      = initialBias;
            F1Prev_[ch]      = F1(initialBias); // F1(0.0) == 1.0
            F2Prev_[ch]      = F2(initialBias); // F2(0.0) == 0.0
            initialized_[ch] = (std::abs(initialBias) > 0.0);
        }
    }

    [[nodiscard]] static double f(double u) noexcept
    {
        return u / std::sqrt(1.0 + u * u);
    }

    [[nodiscard]] static double F1(double u) noexcept
    {
        return std::sqrt(1.0 + u * u);
    }

    [[nodiscard]] static double F2(double u) noexcept
    {
        return 0.5 * (u * std::sqrt(1.0 + u * u) + std::asinh(u));
    }

    [[nodiscard]] static double evalADAA1(double u, double uPrev, double F1u, double F1Prev) noexcept
    {
        constexpr double eps = 1e-4;
        const double du = u - uPrev;
        if (std::abs(du) < eps)
            return f(0.5 * (u + uPrev));
        return (F1u - F1Prev) / du;
    }

    [[nodiscard]] static double evalADAA2(double u, double uPrev, double uPrev2,
                                          double F2u, double F2Prev, double F2Prev2) noexcept
    {
        constexpr double eps = 1e-4;
        const double du01 = u - uPrev;
        const double du12 = uPrev - uPrev2;
        const double du02 = u - uPrev2;

        if (std::abs(du01) < eps || std::abs(du12) < eps || std::abs(du02) < eps)
        {
            const double mid01 = 0.5 * (u + uPrev);
            const double mid12 = 0.5 * (uPrev + uPrev2);
            return evalADAA1(mid01, mid12, F1(mid01), F1(mid12));
        }

        const double term1 = (F2u - F2Prev) / du01;
        const double term2 = (F2Prev - F2Prev2) / du12;
        return 2.0 * (term1 - term2) / du02;
    }

    [[nodiscard]] float processSampleOrder2(float xIn, float driveIn, float biasIn, int channel) noexcept
    {
        const std::size_t ch = (channel > 0) ? 1u : 0u;
        const double x     = static_cast<double>(xIn);
        const double drive = static_cast<double>(driveIn);
        const double bias  = static_cast<double>(biasIn);

        if (!initialized_[ch])
        {
            uPrev_[ch]       = bias;
            uPrev2_[ch]      = bias;
            F1Prev_[ch]      = F1(bias);
            F2Prev_[ch]      = F2(bias);
            initialized_[ch] = true;
        }

        // Gate de silence strict : si l'entrée est au silence (|x| < 1e-6) et l'historique est au repos autour de bias,
        // on maintient l'état exact sur bias et on renvoie 0.0f sans le moindre résidu DC ou bruit numérique.
        if (std::abs(x) < 1e-6 && std::abs(uPrev_[ch] - bias) < 1e-6 && std::abs(uPrev2_[ch] - bias) < 1e-6)
        {
            uPrev_[ch]  = bias;
            uPrev2_[ch] = bias;
            F1Prev_[ch] = F1(bias);
            F2Prev_[ch] = F2(bias);
            return 0.0f;
        }

        const double u      = drive * x + bias;
        const double F1u    = F1(u);
        const double F2u    = F2(u);
        const double F2Prev2 = F2(uPrev2_[ch]);

        const double rawSat = evalADAA2(u, uPrev_[ch], uPrev2_[ch], F2u, F2Prev_[ch], F2Prev2);
        const double y      = rawSat - f(bias);

        uPrev2_[ch] = uPrev_[ch];
        uPrev_[ch]  = u;
        F1Prev_[ch] = F1u;
        F2Prev_[ch] = F2u;

        return static_cast<float>(y);
    }

private:
    std::array<double, 2> uPrev_       { 0.0, 0.0 };
    std::array<double, 2> uPrev2_      { 0.0, 0.0 };
    std::array<double, 2> F1Prev_      { 1.0, 1.0 }; // F1(0.0) = 1.0
    std::array<double, 2> F2Prev_      { 0.0, 0.0 }; // F2(0.0) = 0.0
    std::array<bool, 2>   initialized_ { false, false };
};

} // namespace kalyx::dsp
