#include "FairchildDSP.h"

namespace kalyx::consoles
{

const FairchildDSP::TubeProfile& FairchildDSP::getTubeProfile(int tubeIdx) noexcept
{
    static constexpr std::array<TubeProfile, 7> kTubes {{
        { "THOMSON 5749 FSE", "Dense Mids & Punchy Transients",     11.0,  5.5, 0.72, 0.35 },
        { "BRIMAR CV4009",    "Warm Low-Mid & Airy Leveling",       16.0,  3.8, 0.58, 0.30 },
        { "GE 5749",          "Dark American Vintage Grit",         12.0,  4.8, 0.95, 0.42 },
        { "RCA 6386 BLACK",   "3D Depth & Silky Liquid Glue",       15.0,  4.5, 0.65, 0.36 },
        { "GE 5-STAR 6386",   "Hi-Def Linear Bus Precision",        13.0,  4.2, 0.32, 0.22 },
        { "RCA 6BC8",         "Analog Weight & Firm Crest",         10.0,  6.2, 0.68, 0.38 },
        { "MULLARD ECC82",    "Holographic Iron & Open Dynamics",   18.0,  2.6, 0.40, 0.60 }
    }};
    return kTubes[static_cast<std::size_t>(std::clamp(tubeIdx, 0, 6))];
}

void FairchildDSP::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(sampleRate, 8000.0);
    reset();
}

void FairchildDSP::reset() noexcept
{
    envADb_ = envBDb_ = 0.0;
    slowEnvA_ = slowEnvB_ = 0.0;
    memA_ = memB_ = 0.0;
    satL_.reset(0.0);
    satR_.reset(0.0);
    ironL_.reset();
    ironR_.reset();
}

double FairchildDSP::computeVariMuTargetGrDb(double detDb,
                                             double effThreshDb,
                                             const TubeProfile& tube) noexcept
{
    // Courbe Vari-Mu progressive sans seuil dur : la pente augmente continuellement avec l'overdrive
    const double over = detDb - effThreshDb;
    const double knee = tube.kneeSoftness;
    if (over <= -0.5 * knee)
        return 0.0;

    // Ratio progressif Vari-Mu (1.2:1 près du seuil -> maxRatio en forte compression)
    const double prog = std::clamp((over + 0.5 * knee) / 24.0, 0.0, 1.0);
    const double effRatio = 1.25 + (tube.maxRatio - 1.25) * prog;
    const double slope = 1.0 - 1.0 / effRatio;

    if (over >= 0.5 * knee)
        return over * slope;

    const double x = over + 0.5 * knee;
    return (slope * x * x) / (2.0 * knee);
}

float FairchildDSP::processSample(float& l, float& r,
                                  float detL, float detR,
                                  const kalyx::core::SlotParamArray& p,
                                  bool extSidechain,
                                  float tolBiasL, float tolBiasR) noexcept
{
    using namespace kalyx::core;
    const bool engaged = (p[0] > 0.5f);
    if (!engaged)
        return 0.0f;

    const float dryL = l;
    const float dryR = r;

    // Silence Gate strict (< -120 dBFS sur entrée 0.0)
    if (std::abs(dryL) < 1.0e-6f && std::abs(dryR) < 1.0e-6f
        && envADb_ < 1.0e-5 && envBDb_ < 1.0e-5)
    {
        envADb_ = envBDb_ = slowEnvA_ = slowEnvB_ = memA_ = memB_ = 0.0;
        (void) satL_.processADAA1(0.0f, 0.0f, 0.0f);
        (void) satR_.processADAA1(0.0f, 0.0f, 0.0f);
        return 0.0f;
    }

    const double inputDb    = static_cast<double>(mapLin(p[1], -24.0f, 24.0f));
    const double acThresh   = static_cast<double>(std::clamp(p[2], 0.0f, 1.0f));
    const bool   isVintage  = (p[10] > 0.5f);
    const double outputDb   = static_cast<double>(mapLin(p[4], -24.0f, 24.0f));
    const double matchLR    = static_cast<double>(std::clamp(p[5], 0.0f, 1.0f));
    const double matchPP    = static_cast<double>(std::clamp(p[6], 0.0f, 1.0f));
    const double blend      = static_cast<double>(std::clamp(p[7], 0.0f, 1.0f));
    const int    tubeIdx    = mapChoice(p[8], 7);
    const int    routing    = mapChoice(p[9], 5);

    const auto& tube = getTubeProfile(tubeIdx);

    // 6 Constantes de Temps Historiques (TC 1..6)
    static constexpr double kTcAttMs[6]      = { 0.20, 0.20, 0.40, 0.80, 0.40, 0.20 };
    static constexpr double kTcFastRelMs[6]  = { 300.0, 800.0, 2000.0, 5000.0, 2000.0, 300.0 };
    static constexpr double kTcSlowRelMs[6]  = { 300.0, 800.0, 2000.0, 5000.0, 10000.0, 25000.0 };
    static constexpr double kTcProgWeight[6] = { 0.0, 0.0, 0.0, 0.0, 0.55, 0.72 };

    double attMs = 0.20, fastRelMs = 800.0, slowRelMs = 800.0, progW = 0.0;
    if (isVintage)
    {
        const int tc = mapChoice(p[3], 6);
        attMs     = kTcAttMs[tc];
        fastRelMs = kTcFastRelMs[tc];
        slowRelMs = kTcSlowRelMs[tc];
        progW     = kTcProgWeight[tc];
    }
    else
    {
        // Interpolation continue entre TC 1 et TC 6 en mode MODERN
        const double pos = std::clamp(static_cast<double>(p[3]), 0.0, 1.0) * 5.0;
        const int    i0  = std::clamp(static_cast<int>(std::floor(pos)), 0, 4);
        const double f   = pos - static_cast<double>(i0);
        attMs     = kTcAttMs[i0]      + f * (kTcAttMs[i0 + 1]      - kTcAttMs[i0]);
        fastRelMs = kTcFastRelMs[i0]  + f * (kTcFastRelMs[i0 + 1]  - kTcFastRelMs[i0]);
        slowRelMs = kTcSlowRelMs[i0]  + f * (kTcSlowRelMs[i0 + 1]  - kTcSlowRelMs[i0]);
        progW     = kTcProgWeight[i0] + f * (kTcProgWeight[i0 + 1] - kTcProgWeight[i0]);
    }

    const double inLin  = std::pow(10.0, inputDb / 20.0);
    const double outLin = std::pow(10.0, outputDb / 20.0);

    double sigL = static_cast<double>(dryL) * inLin;
    double sigR = static_cast<double>(dryR) * inLin;
    double dL   = extSidechain ? static_cast<double>(detL) : sigL;
    double dR   = extSidechain ? static_cast<double>(detR) : sigR;

    // Routage 5 modes
    double procL = sigL, procR = sigR;
    double scA = dL, scB = dR;

    if (routing == 0) // MONO
    {
        const double m = 0.5 * (sigL + sigR);
        procL = procR = m;
        scA = scB = 0.5 * (dL + dR);
    }
    else if (routing == 3) // M/S (Lat/Vert comme le 670 !)
    {
        procL = 0.5 * (sigL + sigR);
        procR = 0.5 * (sigL - sigR);
        scA   = 0.5 * (dL + dR);
        scB   = 0.5 * (dL - dR);
    }
    else if (routing == 4) // INV
    {
        procR = -sigR;
        scB   = -dR;
    }

    // AC THRESHOLD (0..100 %) -> seuil effectif -2 dBFS .. -34 dBFS
    const double baseThreshDb = -2.0 - acThresh * 32.0;

    // MATCHING L/R : micro-variances calibrées L vs R sur seuil & relâchement pour largeur 3D
    const double lrMismatch = 1.0 - matchLR;
    const double threshA    = baseThreshDb - lrMismatch * 1.4;
    const double threshB    = baseThreshDb + lrMismatch * 1.4;
    const double relScaleA  = 1.0 - lrMismatch * 0.16;
    const double relScaleB  = 1.0 + lrMismatch * 0.16;

    const bool dualPath = (routing == 2 || routing == 3 || lrMismatch > 0.01);

    auto updateChannelEnv = [&](double detValDb, double effThresh, double relMul,
                                double& envDb, double& slowEnv, double& mem)
    {
        const double target = computeVariMuTargetGrDb(detValDb, effThresh, tube);
        const double attC   = std::exp(-1.0 / std::max(0.5, attMs * 0.001 * sampleRate_));
        const double fRelC  = std::exp(-1.0 / std::max(1.0, fastRelMs * relMul * 0.001 * sampleRate_));
        const double sRelC  = std::exp(-1.0 / std::max(1.0, slowRelMs * relMul * 0.001 * sampleRate_));

        const double lum  = std::clamp(target / 14.0, 0.0, 1.0);
        const double chgC = std::exp(-1.0 / std::max(1.0, 0.120 * sampleRate_));
        const double decC = std::exp(-1.0 / std::max(1.0, 1.800 * sampleRate_));
        mem = (lum > mem) ? (chgC * mem + (1.0 - chgC) * lum) : (decC * mem + (1.0 - decC) * lum);

        slowEnv = (target > slowEnv) ? (attC * slowEnv + (1.0 - attC) * target)
                                     : (sRelC * slowEnv + (1.0 - sRelC) * target);
        const double mixSlow = progW * std::clamp(mem, 0.0, 1.0);
        const double relC = (1.0 - mixSlow) * fRelC + mixSlow * sRelC;

        envDb = (target > envDb) ? (attC * envDb + (1.0 - attC) * target)
                                 : (relC * envDb + (1.0 - relC) * target);
        if (envDb < 1.0e-5) envDb = 0.0;
    };

    double grDisplayDb = 0.0;
    if (dualPath)
    {
        const double detADb = 20.0 * std::log10(std::max(std::abs(scA), 1.0e-7));
        const double detBDb = 20.0 * std::log10(std::max(std::abs(scB), 1.0e-7));
        // En stéréo avec L/R mismatch, on garde un couplage partiel (70% max + 30% indépendant)
        const double linkedDb = std::max(detADb, detBDb);
        const double effDetA  = (routing == 2 || routing == 3) ? detADb : (0.70 * linkedDb + 0.30 * detADb);
        const double effDetB  = (routing == 2 || routing == 3) ? detBDb : (0.70 * linkedDb + 0.30 * detBDb);

        updateChannelEnv(effDetA, threshA, relScaleA, envADb_, slowEnvA_, memA_);
        updateChannelEnv(effDetB, threshB, relScaleB, envBDb_, slowEnvB_, memB_);
        procL *= std::pow(10.0, -envADb_ / 20.0);
        procR *= std::pow(10.0, -envBDb_ / 20.0);
        grDisplayDb = std::max(envADb_, envBDb_);
    }
    else
    {
        const double detLinkedDb = 20.0 * std::log10(std::max({ std::abs(scA), std::abs(scB), 1.0e-7 }));
        updateChannelEnv(detLinkedDb, threshA, 1.0, envADb_, slowEnvA_, memA_);
        envBDb_ = envADb_;
        const double g = std::pow(10.0, -envADb_ / 20.0);
        procL *= g;
        procR *= g;
        grDisplayDb = envADb_;
    }

    if (routing == 3) // Reconversion Lat/Vert M/S -> L/R
    {
        const double m = procL;
        const double s = procR;
        procL = m + s;
        procR = m - s;
    }

    // MATCHING P/P (Push-Pull) + Coloration Lampe NOS via ADAA 64-bit + Transformateurs
    const double ppImbalance = 1.0 - matchPP;
    const float  tubeDrv     = static_cast<float>(std::clamp(tube.satDrive * (0.55 + 0.04 * grDisplayDb + 0.35 * ppImbalance), 0.0, 2.5));
    const float  biasL       = static_cast<float>(ppImbalance * 0.32 + static_cast<double>(tolBiasL));
    const float  biasR       = static_cast<float>(ppImbalance * 0.32 + static_cast<double>(tolBiasR));

    const float sL = satL_.processADAA1(static_cast<float>(procL), tubeDrv, biasL);
    const float sR = satR_.processADAA1(static_cast<float>(procR), tubeDrv, biasR);
    const double satMix = std::clamp(0.18 + 0.38 * ppImbalance, 0.15, 0.60);
    procL = (1.0 - satMix) * procL + satMix * static_cast<double>(sL);
    procR = (1.0 - satMix) * procR + satMix * static_cast<double>(sR);

    const float ironDrv = static_cast<float>(tube.ironDrive);
    procL = static_cast<double>(ironL_.processSample(static_cast<float>(procL), ironDrv, sampleRate_, 3));
    procR = static_cast<double>(ironR_.processSample(static_cast<float>(procR), ironDrv, sampleRate_, 3));

    const double wetL = procL * outLin;
    const double wetR = procR * outLin;
    const double outL = (1.0 - blend) * static_cast<double>(dryL) + blend * wetL;
    const double outR = (1.0 - blend) * static_cast<double>(dryR) + blend * wetR;

    l = std::isfinite(outL) ? static_cast<float>(outL) : 0.0f;
    r = std::isfinite(outR) ? static_cast<float>(outR) : 0.0f;
    return static_cast<float>(grDisplayDb);
}

} // namespace kalyx::consoles
