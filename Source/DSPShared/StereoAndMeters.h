#pragma once

#include <cmath>
#include <algorithm>
#include <array>

namespace kalyx::dsp
{

/**
 * Photographie de télémétrie temps réel (Audio -> GUI via std::atomic<float> lock-free) :
 * - VU-mètre analogique 300 ms IEC 60268-17 (IN L/R, OUT L/R, GR cumulée)
 * - Crêtes instantanées IN / OUT pour LED Clip
 * - Compensation AUTO-GAIN active.
 */
struct StudioMeterSnapshot
{
    float inputPeakDbL      { -100.0f };
    float inputPeakDbR      { -100.0f };
    float outputPeakDbL     { -100.0f };
    float outputPeakDbR     { -100.0f };

    // Balistique RMS stricte 300 ms (IEC 60268-17)
    float inputVu300DbL     { -100.0f };
    float inputVu300DbR     { -100.0f };
    float outputVu300DbL    { -100.0f };
    float outputVu300DbR    { -100.0f };

    // Réduction de gain cumulée (dB positifs, ex: 6.5 dB de réduction)
    float cumulativeGrDb    { 0.0f };
    float cumulativeGrVuDb  { 0.0f }; // Lissé 300 ms pour l'aiguille VU en mode GR
    float autoGainAppliedDb { 0.0f };
};

/**
 * 5 Modes Universels de Routage et Détection des Compresseurs :
 * 0 = Mono, 1 = Stereo Linked, 2 = Dual Mono, 3 = Mid/Side, 4 = Inverse / Ducking
 */
enum class DynamicsRoutingMode : int
{
    Mono           = 0,
    StereoLinked   = 1,
    DualMono       = 2,
    MidSide        = 3,
    InverseDucking = 4
};

struct UniversalDetectorLevels
{
    double detADb   { -100.0 };
    double detBDb   { -100.0 };
    double maxDetDb { -100.0 };
};

[[nodiscard]] inline UniversalDetectorLevels computeUniversalDetectorLevels(float detL, float detR, int routingMode) noexcept
{
    UniversalDetectorLevels out;
    const double dL = static_cast<double>(detL);
    const double dR = static_cast<double>(detR);

    if (routingMode == static_cast<int>(DynamicsRoutingMode::Mono))
    {
        const double mono = std::max(std::abs(0.5 * (dL + dR)), 1.0e-7);
        const double db   = 20.0 * std::log10(mono);
        out.detADb = out.detBDb = out.maxDetDb = db;
    }
    else if (routingMode == static_cast<int>(DynamicsRoutingMode::DualMono))
    {
        out.detADb   = 20.0 * std::log10(std::max(std::abs(dL), 1.0e-7));
        out.detBDb   = 20.0 * std::log10(std::max(std::abs(dR), 1.0e-7));
        out.maxDetDb = std::max(out.detADb, out.detBDb);
    }
    else if (routingMode == static_cast<int>(DynamicsRoutingMode::MidSide))
    {
        const double mid  = std::max(std::abs(0.5 * (dL + dR)), 1.0e-7);
        const double side = std::max(std::abs(0.5 * (dL - dR)), 1.0e-7);
        out.detADb   = 20.0 * std::log10(mid);
        out.detBDb   = 20.0 * std::log10(side);
        out.maxDetDb = std::max(out.detADb, out.detBDb);
    }
    else // StereoLinked (1) ou InverseDucking (4)
    {
        const double linked = std::max({ std::abs(dL), std::abs(dR), 1.0e-7 });
        const double db     = 20.0 * std::log10(linked);
        out.detADb = out.detBDb = out.maxDetDb = db;
    }

    return out;
}

/**
 * Moteur de mesure analogique IEC 60268-17 (300 ms RMS) et Auto-Gain avec gel sous -60 dBFS.
 */
class StereoMetersAndAutoGain
{
public:
    StereoMetersAndAutoGain() noexcept = default;

    void prepare(double sampleRate) noexcept
    {
        sampleRate_ = std::max(sampleRate, 44100.0);
        // IEC 60268-17 : temps de montée/descente 99% en 300 ms => tau = 0.300 / 4.605
        const double vuTau = 0.300 / 4.605;
        vu300Alpha_          = std::exp(-1.0 / (vuTau * sampleRate_));
        agRmsAlpha_          = std::exp(-1.0 / (0.350 * sampleRate_));
        agSmoothAlpha_       = std::exp(-1.0 / (0.180 * sampleRate_));
        agReturnToZeroAlpha_ = std::exp(-1.0 / (1.500 * sampleRate_));
        reset();
    }

    void reset() noexcept
    {
        inVuSqL_  = 0.0; inVuSqR_  = 0.0;
        outVuSqL_ = 0.0; outVuSqR_ = 0.0;
        grVu300_  = 0.0;
        agRefSq_  = 0.0;
        agProcSq_ = 0.0;
        agLinear_ = 1.0;
        silenceCounterSamples_ = 0;
    }

    inline void updateInputBallistics(float inL, float inR) noexcept
    {
        const double l2 = static_cast<double>(inL) * static_cast<double>(inL);
        const double r2 = static_cast<double>(inR) * static_cast<double>(inR);
        inVuSqL_ = vu300Alpha_ * inVuSqL_ + (1.0 - vu300Alpha_) * l2;
        inVuSqR_ = vu300Alpha_ * inVuSqR_ + (1.0 - vu300Alpha_) * r2;
    }

    inline void updateOutputAndGrBallistics(float outL, float outR, float totalGrDb) noexcept
    {
        const double l2 = static_cast<double>(outL) * static_cast<double>(outL);
        const double r2 = static_cast<double>(outR) * static_cast<double>(outR);
        outVuSqL_ = vu300Alpha_ * outVuSqL_ + (1.0 - vu300Alpha_) * l2;
        outVuSqR_ = vu300Alpha_ * outVuSqR_ + (1.0 - vu300Alpha_) * r2;
        grVu300_  = vu300Alpha_ * grVu300_  + (1.0 - vu300Alpha_) * static_cast<double>(std::max(0.0f, totalGrDb));
    }

    /**
     * Auto-Gain continu entrée/sortie :
     * - Fige la mesure sous -60 dBFS (1e-6 en puissance)
     * - Ramène doucement à 0.0 dB après 2 secondes de silence.
     */
    [[nodiscard]] inline float computeAutoGainSample(float refL, float refR,
                                                     float procL, float procR,
                                                     bool enabled,
                                                     float& outAppliedDb) noexcept
    {
        if (!enabled)
        {
            agLinear_    = agSmoothAlpha_ * agLinear_ + (1.0 - agSmoothAlpha_) * 1.0;
            outAppliedDb = 0.0f;
            return static_cast<float>(agLinear_);
        }

        const double refSq  = 0.5 * (static_cast<double>(refL) * refL + static_cast<double>(refR) * refR);
        const double procSq = 0.5 * (static_cast<double>(procL) * procL + static_cast<double>(procR) * procR);

        if (refSq > 1.0e-6)
        {
            silenceCounterSamples_ = 0;
            agRefSq_  = agRmsAlpha_ * agRefSq_  + (1.0 - agRmsAlpha_) * refSq;
            agProcSq_ = agRmsAlpha_ * agProcSq_ + (1.0 - agRmsAlpha_) * procSq;

            if (agRefSq_ > 1.0e-6 && agProcSq_ > 1.0e-7)
            {
                const double ratio      = std::sqrt(agRefSq_ / agProcSq_);
                const double targetGain = std::clamp(ratio, 0.12589254, 7.94328235); // +/- 18 dB max
                agLinear_ = agSmoothAlpha_ * agLinear_ + (1.0 - agSmoothAlpha_) * targetGain;
            }
        }
        else
        {
            ++silenceCounterSamples_;
            if (static_cast<double>(silenceCounterSamples_) > 2.0 * sampleRate_)
                agLinear_ = agReturnToZeroAlpha_ * agLinear_ + (1.0 - agReturnToZeroAlpha_) * 1.0;
        }

        outAppliedDb = static_cast<float>(20.0 * std::log10(std::max(agLinear_, 1.0e-4)));
        return static_cast<float>(agLinear_);
    }

    void populateSnapshot(StudioMeterSnapshot& snap) const noexcept
    {
        auto sqToDb = [](double sq) noexcept -> float
        {
            return (sq > 1.0e-10) ? static_cast<float>(10.0 * std::log10(sq)) : -100.0f;
        };
        snap.inputVu300DbL    = sqToDb(inVuSqL_);
        snap.inputVu300DbR    = sqToDb(inVuSqR_);
        snap.outputVu300DbL   = sqToDb(outVuSqL_);
        snap.outputVu300DbR   = sqToDb(outVuSqR_);
        snap.cumulativeGrVuDb = static_cast<float>(grVu300_);
    }

private:
    double sampleRate_          { 44100.0 };
    double vu300Alpha_          { 0.999 };
    double agRmsAlpha_          { 0.999 };
    double agSmoothAlpha_       { 0.999 };
    double agReturnToZeroAlpha_ { 0.9999 };

    double inVuSqL_  { 0.0 }, inVuSqR_  { 0.0 };
    double outVuSqL_ { 0.0 }, outVuSqR_ { 0.0 };
    double grVu300_  { 0.0 };

    double agRefSq_  { 0.0 };
    double agProcSq_ { 0.0 };
    double agLinear_ { 1.0 };
    int silenceCounterSamples_ { 0 };
};

} // namespace kalyx::dsp
