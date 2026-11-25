#pragma once

#include <cmath>
#include <algorithm>

namespace kalyx::dsp
{

/**
 * Saturateur ADAA (Antiderivative Antialiasing) d'ordre 1 et 2 en précision 64 bits (double).
 *
 * Fonction de transfert sigmoïde algébrique douce :
 *   f(u)  = u / sqrt(1 + u^2)
 *   F1(u) = sqrt(1 + u^2)                                  -> F1(0) = 1.0
 *   F2(u) = 0.5 * (u * sqrt(1 + u^2) + asinh(u))           -> F2(0) = 0.0
 *
 * Garantit un bruit résiduel nul (0.0, < -120 dBFS) sur le silence grâce à :
 * - Initialisation exacte F1_prev = F1(bias) (soit 1.0 quand bias == 0)
 * - Seuil de repli de Taylor à 1e-4 renvoyant directement f((u_n + u_{n-1}) * 0.5)
 * - Silence Gate strict pour |x| < 1e-6 ou drive <= 1e-5.
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

    /**
     * Saturation ADAA d'ordre 1 avec compensation DC exacte et Silence Gate.
     */
    [[nodiscard]] inline float processADAA1(float x, float drive, float bias = 0.0f) noexcept
    {
        const double x64     = static_cast<double>(x);
        const double drive64 = static_cast<double>(drive);
        const double bias64  = static_cast<double>(bias);

        if (std::abs(bias64 - lastBias_) > 1.0e-7)
            reset(bias64);

        if (drive64 <= 1.0e-5 || std::abs(x64) < 1.0e-6)
        {
            uPrev2_ = uPrev_;
            uPrev_  = bias64;
            F1Prev_ = evalF1(bias64);
            F2Prev_ = evalF2(bias64);
            return 0.0f;
        }

        const double u      = drive64 * x64 + bias64;
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

        const double dcOffset = evalF(bias64);
        const double norm     = 1.0 / std::max(evalF(drive64), 0.15);
        const double out      = (y - dcOffset) * norm;
        return std::isfinite(out) ? static_cast<float>(out) : 0.0f;
    }

    /**
     * Saturation ADAA d'ordre 2 (3 points) avec repli d'ordre 1 à 1e-4 et Silence Gate.
     */
    [[nodiscard]] inline float processADAA2(float x, float drive, float bias = 0.0f) noexcept
    {
        const double x64     = static_cast<double>(x);
        const double drive64 = static_cast<double>(drive);
        const double bias64  = static_cast<double>(bias);

        if (std::abs(bias64 - lastBias_) > 1.0e-7)
            reset(bias64);

        if (drive64 <= 1.0e-5 || std::abs(x64) < 1.0e-6)
        {
            uPrev2_ = uPrev_;
            uPrev_  = bias64;
            F1Prev_ = evalF1(bias64);
            F2Prev_ = evalF2(bias64);
            return 0.0f;
        }

        const double u      = drive64 * x64 + bias64;
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
            const double d1 = (F2Curr - F2Prev_) / du0;
            const double F2Prev2 = evalF2(uPrev2_);
            const double d0 = (F2Prev_ - F2Prev2) / du1;
            y = 2.0 * (d1 - d0) / du2;
        }

        uPrev2_ = uPrev_;
        uPrev_  = u;
        F1Prev_ = F1Curr;
        F2Prev_ = F2Curr;

        const double dcOffset = evalF(bias64);
        const double norm     = 1.0 / std::max(evalF(drive64), 0.15);
        const double out      = (y - dcOffset) * norm;
        return std::isfinite(out) ? static_cast<float>(out) : 0.0f;
    }

private:
    double uPrev_    { 0.0 };
    double uPrev2_   { 0.0 };
    double F1Prev_   { 1.0 };
    double F2Prev_   { 0.0 };
    double lastBias_ { 0.0 };
};

} // namespace kalyx::dsp
