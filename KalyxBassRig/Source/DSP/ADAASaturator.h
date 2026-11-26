#pragma once

#include <algorithm>
#include <cmath>

namespace kalyx::bass::dsp
{

/**
 * Saturateur ADAA (Antiderivative Antialiasing) d'ordre 1 et 2 en précision 64 bits (double).
 * - F1(0) = 1.0
 * - Repli de Taylor à 1e-4 : f((u_n + u_{n-1}) * 0.5)
 * - Bypass strict sous |x| < 1e-6 ou drive <= 1e-5 => bruit résiduel < -120 dBFS sur silence.
 */
class ADAASaturator
{
public:
    ADAASaturator() noexcept = default;

    void reset(double bias = 0.0) noexcept
    {
        uPrev_    = bias;
        uPrev2_   = bias;
        F1Prev_   = evalF1(bias);
        F2Prev_   = evalF2(bias);
        lastBias_ = bias;
    }

    [[nodiscard]] static inline double evalF(double u) noexcept
    {
        return u / std::sqrt(1.0 + u * u);
    }

    [[nodiscard]] static inline double evalF1(double u) noexcept
    {
        return std::sqrt(1.0 + u * u);
    }

    [[nodiscard]] static inline double evalF2(double u) noexcept
    {
        return 0.5 * (u * std::sqrt(1.0 + u * u) + std::asinh(u));
    }

    [[nodiscard]] inline double processADAA1(double x, double drive, double bias = 0.0) noexcept
    {
        if (std::abs(bias - lastBias_) > 1.0e-7)
            reset(bias);

        if (drive <= 1.0e-5 || std::abs(x) < 1.0e-6)
        {
            uPrev2_ = uPrev_;
            uPrev_  = bias;
            F1Prev_ = evalF1(bias);
            F2Prev_ = evalF2(bias);
            return 0.0;
        }

        const double u      = drive * x + bias;
        const double F1Curr = evalF1(u);
        const double du     = u - uPrev_;

        double y = 0.0;
        if (std::abs(du) < 1.0e-4)
            y = evalF(0.5 * (u + uPrev_));
        else
            y = (F1Curr - F1Prev_) / du;

        uPrev2_ = uPrev_;
        uPrev_  = u;
        F1Prev_ = F1Curr;

        const double dcOffset = evalF(bias);
        const double norm     = 1.0 / std::max(evalF(drive), 0.15);
        const double out      = (y - dcOffset) * norm;
        return std::isfinite(out) ? out : 0.0;
    }

    [[nodiscard]] inline double processADAA2(double x, double drive, double bias = 0.0) noexcept
    {
        if (std::abs(bias - lastBias_) > 1.0e-7)
            reset(bias);

        if (drive <= 1.0e-5 || std::abs(x) < 1.0e-6)
        {
            uPrev2_ = uPrev_;
            uPrev_  = bias;
            F1Prev_ = evalF1(bias);
            F2Prev_ = evalF2(bias);
            return 0.0;
        }

        const double u      = drive * x + bias;
        const double F1Curr = evalF1(u);
        const double F2Curr = evalF2(u);

        const double du0 = u - uPrev_;
        const double du1 = uPrev_ - uPrev2_;
        const double du2 = u - uPrev2_;

        double y = 0.0;
        if (std::abs(du2) < 1.0e-3 || std::abs(du0) < 1.0e-4 || std::abs(du1) < 1.0e-4)
        {
            if (std::abs(du0) < 1.0e-4)
                y = evalF(0.5 * (u + uPrev_));
            else
                y = (F1Curr - F1Prev_) / du0;
        }
        else
        {
            const double d1      = (F2Curr - F2Prev_) / du0;
            const double F2Prev2 = evalF2(uPrev2_);
            const double d0      = (F2Prev_ - F2Prev2) / du1;
            y = 2.0 * (d1 - d0) / du2;
        }

        uPrev2_ = uPrev_;
        uPrev_  = u;
        F1Prev_ = F1Curr;
        F2Prev_ = F2Curr;

        const double dcOffset = evalF(bias);
        const double norm     = 1.0 / std::max(evalF(drive), 0.15);
        const double out      = (y - dcOffset) * norm;
        return std::isfinite(out) ? out : 0.0;
    }

private:
    double uPrev_    { 0.0 };
    double uPrev2_   { 0.0 };
    double F1Prev_   { 1.0 };
    double F2Prev_   { 0.0 };
    double lastBias_ { 0.0 };
};

} // namespace kalyx::bass::dsp
