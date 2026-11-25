#include "StereoPhaseDSP.h"
#include <algorithm>
#include <cmath>

namespace kalyx::consoles
{

void StereoPhaseDSP::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(sampleRate, 8000.0);
    reset();
}

void StereoPhaseDSP::reset() noexcept
{
    monoMakerHp1_.reset();
    monoMakerHp2_.reset();
    sideTiltLo_.reset();
    sideTiltHi_.reset();
    apL1_.reset();
    apL2_.reset();
    apR1_.reset();
    apR2_.reset();

    corrLR_ = 0.0;
    corrLL_ = 0.0;
    corrRR_ = 0.0;
    correlation_.store(1.0f, std::memory_order_relaxed);
}

void StereoPhaseDSP::processSample(float& l, float& r,
                                   const kalyx::core::SlotParamArray& p) noexcept
{
    double inL = static_cast<double>(l);
    double inR = static_cast<double>(r);

    if (p[0] <= 0.5f)
    {
        // En bypass, on continue de calculer la corrélation du signal entrant
        const double cCoeff = std::exp(-1.0 / (0.120 * sampleRate_));
        corrLR_ = cCoeff * corrLR_ + (1.0 - cCoeff) * (inL * inR);
        corrLL_ = cCoeff * corrLL_ + (1.0 - cCoeff) * (inL * inL);
        corrRR_ = cCoeff * corrRR_ + (1.0 - cCoeff) * (inR * inR);
        const double denom = std::sqrt(corrLL_ * corrRR_);
        const float corr = (denom > 1.0e-12) ? static_cast<float>(std::clamp(corrLR_ / denom, -1.0, 1.0)) : 1.0f;
        correlation_.store(corr, std::memory_order_relaxed);
        return;
    }

    const bool   msMode     = (p[1] > 0.5f);
    const double width      = std::clamp(static_cast<double>(p[2]) * 2.0, 0.0, 2.0); // 0..200% -> 0..2
    const double monoHz     = kalyx::core::mapLog(p[3], 20.0f, 500.0f);
    const double phaseDeg   = kalyx::core::mapLin(p[4], -180.0f, 180.0f);
    const double sideTiltDb = kalyx::core::mapLin(p[5], -6.0f, 6.0f);
    const double midGainDb  = kalyx::core::mapLin(p[6], -12.0f, 12.0f);
    const double sideGainDb = kalyx::core::mapLin(p[7], -12.0f, 12.0f);
    const bool   invL       = (p[8] > 0.5f);
    const bool   invR       = (p[9] > 0.5f);
    const bool   swapLR     = (p[10] > 0.5f);
    const int    monitor    = std::clamp(static_cast<int>(std::round(p[11] * 2.0f)), 0, 2); // 0=ST, 1=MID, 2=SIDE

    // 1. Inversion de phase L/R & permutation L<>R
    if (invL) inL = -inL;
    if (invR) inR = -inR;
    if (swapLR) std::swap(inL, inR);

    // 2. Rotation de phase continue (-180°..+180°) via All-Pass 2 étages
    if (std::abs(phaseDeg) > 0.5)
    {
        // Fréquence de coupure All-Pass modulée pour couvrir -180°..+180° de façon musicale
        const double absFrac = std::clamp(std::abs(phaseDeg) / 180.0, 0.01, 1.0);
        const double apFreq  = 120.0 + (1.0 - absFrac) * 1800.0;
        const double t       = std::tan(3.14159265358979323846 * apFreq / sampleRate_);
        const double coeff   = (t - 1.0) / (t + 1.0);
        const double sign    = (phaseDeg >= 0.0) ? 1.0 : -1.0;

        if (!msMode)
        {
            const double rotL = apL2_.process(apL1_.process(inL, coeff), coeff) * sign;
            const double rotR = apR2_.process(apR1_.process(inR, coeff), coeff) * sign;
            const double mix  = absFrac;
            inL = inL * (1.0 - mix) + rotL * mix;
            inR = inR * (1.0 - mix) + rotR * mix;
        }
        else
        {
            // En mode M/S, on réaligne R par rapport à L
            const double rotR = apR2_.process(apR1_.process(inR, coeff), coeff) * sign;
            const double mix  = absFrac;
            inR = inR * (1.0 - mix) + rotR * mix;
        }
    }

    // 3. Décodage Mid / Side
    double mid  = (inL + inR) * 0.7071067811865475;
    double side = (inL - inR) * 0.7071067811865475;

    // 4. Mono Maker (Linkwitz-Riley 24 dB/oct sur Side)
    if (monoHz > 20.5)
    {
        monoMakerHp1_.setHighPass(sampleRate_, monoHz, 0.70710678);
        monoMakerHp2_.setHighPass(sampleRate_, monoHz, 0.70710678);
        const float s1 = monoMakerHp1_.processSample(static_cast<float>(side));
        side = static_cast<double>(monoMakerHp2_.processSample(s1));
    }

    // 5. Side Tilt EQ (-6..+6 dB sur Side uniquement)
    if (std::abs(sideTiltDb) > 0.02)
    {
        const double halfG = sideTiltDb * 0.5;
        sideTiltLo_.setLowShelf(sampleRate_, 900.0, 0.48, -halfG);
        sideTiltHi_.setHighShelf(sampleRate_, 900.0, 0.48, halfG);
        side = static_cast<double>(sideTiltHi_.processSample(sideTiltLo_.processSample(static_cast<float>(side))));
    }

    // 6. Stereo Width avec compensation automatique d'énergie + Mid/Side Gain
    const double midGainLin  = std::pow(10.0, midGainDb / 20.0);
    const double sideGainLin = std::pow(10.0, sideGainDb / 20.0);
    const double energyComp  = 1.0 / std::sqrt(0.5 * (1.0 + width * width));

    mid  *= midGainLin * (msMode ? 1.0 : energyComp);
    side *= sideGainLin * width * (msMode ? 1.0 : energyComp);

    // 7. Monitor (0=STEREO, 1=MID, 2=SIDE) & ré-encodage L/R
    double outL = 0.0, outR = 0.0;
    if (monitor == 1) // MID ONLY (mono centré)
    {
        outL = mid * 0.7071067811865475;
        outR = outL;
    }
    else if (monitor == 2) // SIDE ONLY
    {
        outL = side * 0.7071067811865475;
        outR = -outL;
    }
    else
    {
        outL = (mid + side) * 0.7071067811865475;
        outR = (mid - side) * 0.7071067811865475;
    }

    l = std::isfinite(outL) ? static_cast<float>(outL) : 0.0f;
    r = std::isfinite(outR) ? static_cast<float>(outR) : 0.0f;

    // 8. Corrélomètre de Phase temps réel (-1 .. +1) sur la sortie traitée
    const double cCoeff = std::exp(-1.0 / (0.120 * sampleRate_));
    corrLR_ = cCoeff * corrLR_ + (1.0 - cCoeff) * (outL * outR);
    corrLL_ = cCoeff * corrLL_ + (1.0 - cCoeff) * (outL * outL);
    corrRR_ = cCoeff * corrRR_ + (1.0 - cCoeff) * (outR * outR);
    const double denom = std::sqrt(corrLL_ * corrRR_);
    const float corr = (denom > 1.0e-12) ? static_cast<float>(std::clamp(corrLR_ / denom, -1.0, 1.0)) : 1.0f;
    correlation_.store(corr, std::memory_order_relaxed);
}

} // namespace kalyx::consoles
