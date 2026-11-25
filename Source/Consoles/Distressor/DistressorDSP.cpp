#include "DistressorDSP.h"

namespace kalyx::consoles
{

void DistressorDSP::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(sampleRate, 8000.0);

    // Filtres fixes calibrés sur le matériel Empirical Labs (80 Hz HPF, 6 kHz Band Emphasis)
    audioHpfL_.setHighPass(sampleRate_, 80.0, 0.7071);
    audioHpfR_.setHighPass(sampleRate_, 80.0, 0.7071);
    detHpfL_.setHighPass  (sampleRate_, 80.0, 0.7071);
    detHpfR_.setHighPass  (sampleRate_, 80.0, 0.7071);
    bandEmphL_.setBell    (sampleRate_, 6000.0, 1.15, 7.5);
    bandEmphR_.setBell    (sampleRate_, 6000.0, 1.15, 7.5);

    reset();
}

void DistressorDSP::reset() noexcept
{
    envADb_ = envBDb_ = 0.0;
    slowRelA_ = slowRelB_ = 0.0;
    britLagA_ = britLagB_ = 0.0;
    thdEnv_ = 0.0;

    audioHpfL_.reset();
    audioHpfR_.reset();
    detHpfL_.reset();
    detHpfR_.reset();
    bandEmphL_.reset();
    bandEmphR_.reset();

    sat2L_.reset(0.0);
    sat2R_.reset(0.0);
    sat3L_.reset(0.0);
    sat3R_.reset(0.0);
    hyst3L_.reset();
    hyst3R_.reset();

    lastThdPct_.store(0.0f, std::memory_order_relaxed);
}

double DistressorDSP::computeDistressorTargetGrDb(double detDb,
                                                  double threshDb,
                                                  int ratioIdx,
                                                  bool britishMode) noexcept
{
    // 1:1 : réchauffeur harmonique pur sans compression
    if (ratioIdx <= 0)
        return 0.0;

    static constexpr double kRatios[8] = { 1.0, 2.0, 3.0, 4.0, 6.0, 10.0, 20.0, 100.0 };
    const int idx = std::clamp(ratioIdx, 0, 7);
    const double baseRatio = kRatios[idx];
    const bool isNuke      = (idx == 7);
    const bool isBrit      = britishMode && (idx >= 1 && idx <= 4); // British Mode sur ratios doux 2:1..6:1

    const double effThresh = isNuke ? (threshDb - 4.0) : (isBrit ? (threshDb - 3.5) : threshDb);
    const double over = detDb - effThresh;

    if (isNuke)
    {
        // Limiteur brickwall ultra-agressif
        constexpr double knee = 2.0;
        if (over <= -0.5 * knee) return 0.0;
        constexpr double slope = 1.0 - 1.0 / 100.0;
        if (over >= 0.5 * knee)  return over * slope;
        const double x = over + 0.5 * knee;
        return (slope * x * x) / (2.0 * knee);
    }

    if (isBrit)
    {
        // Coude agressif + repli inspiré du 1176 All-Buttons In
        constexpr double knee = 5.0;
        if (over <= -0.5 * knee) return 0.0;
        const double slope = 1.0 - 1.0 / std::max(baseRatio * 2.2, 12.0);
        if (over >= 0.5 * knee)
        {
            const double fold = 0.75 * std::exp(-0.16 * std::max(0.0, over - 2.0));
            return std::max(0.0, over * slope + fold);
        }
        const double x = over + 0.5 * knee;
        return (slope * x * x) / (2.0 * knee);
    }

    // Coude adaptatif selon le ratio (doux sur 2:1/3:1, plus ferme sur 10:1/20:1)
    const double knee = (idx <= 2) ? 10.0 : ((idx <= 4) ? 7.5 : 4.5);
    if (over <= -0.5 * knee)
        return 0.0;

    const double slope = 1.0 - 1.0 / baseRatio;
    if (over >= 0.5 * knee)
        return over * slope;

    const double x = over + 0.5 * knee;
    return (slope * x * x) / (2.0 * knee);
}

float DistressorDSP::processSample(float& l, float& r,
                                   float detL, float detR,
                                   const kalyx::core::SlotParamArray& p,
                                   bool extSidechain,
                                   float tolBiasL, float tolBiasR) noexcept
{
    using namespace kalyx::core;
    const bool engaged = (p[0] > 0.5f);
    if (!engaged)
    {
        lastThdPct_.store(0.0f, std::memory_order_relaxed);
        return 0.0f;
    }

    const float dryL = l;
    const float dryR = r;

    // Silence Gate strict (< -120 dBFS sur entrée 0.0)
    if (std::abs(dryL) < 1.0e-6f && std::abs(dryR) < 1.0e-6f
        && envADb_ < 1.0e-5 && envBDb_ < 1.0e-5)
    {
        envADb_ = envBDb_ = slowRelA_ = slowRelB_ = britLagA_ = britLagB_ = 0.0;
        thdEnv_ = 0.0;
        (void) sat2L_.processADAA1(0.0f, 0.0f, 0.0f);
        (void) sat2R_.processADAA1(0.0f, 0.0f, 0.0f);
        (void) sat3L_.processADAA1(0.0f, 0.0f, 0.0f);
        (void) sat3R_.processADAA1(0.0f, 0.0f, 0.0f);
        lastThdPct_.store(0.0f, std::memory_order_relaxed);
        return 0.0f;
    }

    const double inputDb     = static_cast<double>(mapLin(p[1], -24.0f, 24.0f));
    const double outputDb    = static_cast<double>(mapLin(p[2], -24.0f, 24.0f));
    const double attackMs    = static_cast<double>(mapAttackMs(p[3]));
    const double releaseMs   = static_cast<double>(mapReleaseMs(p[4]));
    const int    ratioIdx    = mapChoice(p[5], 8);
    const bool   britishMode = (p[6] > 0.5f);
    const bool   audioHpf    = (p[7] > 0.5f);
    const bool   detHpf      = (p[8] > 0.5f);
    const bool   bandEmph    = (p[9] > 0.5f);
    const int    distMode    = mapChoice(p[10], 3);
    const double blend       = static_cast<double>(std::clamp(p[11], 0.0f, 1.0f));
    const int    routing     = mapChoice(p[12], 5);

    const double inLin  = std::pow(10.0, inputDb / 20.0);
    const double outLin = std::pow(10.0, outputDb / 20.0);

    // 1. Étage d'entrée & Audio HPF (80 Hz)
    double sigL = static_cast<double>(dryL) * inLin;
    double sigR = static_cast<double>(dryR) * inLin;

    if (audioHpf)
    {
        sigL = static_cast<double>(audioHpfL_.processSample(static_cast<float>(sigL)));
        sigR = static_cast<double>(audioHpfR_.processSample(static_cast<float>(sigR)));
    }

    // 2. Détecteur Sidechain (interne après INPUT ou externe) + DET HPF + BAND EMPH (6 kHz)
    double dL = extSidechain ? static_cast<double>(detL) : sigL;
    double dR = extSidechain ? static_cast<double>(detR) : sigR;

    if (detHpf)
    {
        dL = static_cast<double>(detHpfL_.processSample(static_cast<float>(dL)));
        dR = static_cast<double>(detHpfR_.processSample(static_cast<float>(dR)));
    }
    if (bandEmph)
    {
        dL = static_cast<double>(bandEmphL_.processSample(static_cast<float>(dL)));
        dR = static_cast<double>(bandEmphR_.processSample(static_cast<float>(dR)));
    }

    // 3. Routage 5 modes (MONO, STEREO, DUAL M, M/S, INV)
    double procL = sigL, procR = sigR;
    double scA = dL, scB = dR;

    if (routing == 0) // MONO
    {
        const double m = 0.5 * (sigL + sigR);
        procL = procR = m;
        scA = scB = 0.5 * (dL + dR);
    }
    else if (routing == 3) // M/S
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

    const bool dualPath = (routing == 2 || routing == 3); // Dual Mono ou M/S
    constexpr double fixedThreshDb = -18.0;

    // 4. Balistique 64-bit VCA (attaque 50 µs..30 ms, relâchement 50 ms..3.5 s)
    const double attSec  = std::max(5.0e-5, attackMs * 0.001);
    const double relSec  = std::max(0.050, releaseMs * 0.001);
    const bool   isBrit  = britishMode && (ratioIdx >= 1 && ratioIdx <= 4);
    const double attC    = std::exp(-1.0 / std::max(0.5, attSec * sampleRate_));
    const double relFast = std::exp(-1.0 / std::max(1.0, relSec * (isBrit ? 0.60 : 0.85) * sampleRate_));
    const double relSlow = std::exp(-1.0 / std::max(1.0, relSec * 1.90 * sampleRate_));
    const double lagC    = std::exp(-1.0 / std::max(1.0, 0.0014 * sampleRate_));

    auto updateEnv = [&](double detValDb, double& envDb, double& slowEnv, double& britLag)
    {
        double target = computeDistressorTargetGrDb(detValDb, fixedThreshDb, ratioIdx, britishMode);
        if (isBrit)
        {
            britLag = lagC * britLag + (1.0 - lagC) * target;
            target = 0.65 * target + 0.35 * britLag;
        }
        slowEnv = (target > slowEnv)
                      ? (attC * slowEnv + (1.0 - attC) * target)
                      : (relSlow * slowEnv + (1.0 - relSlow) * target);
        const double progMix = std::clamp(slowEnv / 18.0, 0.0, 0.65);
        const double rc = (1.0 - progMix) * relFast + progMix * relSlow;
        envDb = (target > envDb)
                    ? (attC * envDb + (1.0 - attC) * target)
                    : (rc * envDb + (1.0 - rc) * target);
        if (envDb < 1.0e-5) envDb = 0.0;
    };

    double grDisplayDb = 0.0;
    if (dualPath)
    {
        const double detADb = 20.0 * std::log10(std::max(std::abs(scA), 1.0e-7));
        const double detBDb = 20.0 * std::log10(std::max(std::abs(scB), 1.0e-7));
        updateEnv(detADb, envADb_, slowRelA_, britLagA_);
        updateEnv(detBDb, envBDb_, slowRelB_, britLagB_);
        procL *= std::pow(10.0, -envADb_ / 20.0);
        procR *= std::pow(10.0, -envBDb_ / 20.0);
        grDisplayDb = std::max(envADb_, envBDb_);
    }
    else
    {
        const double detLinkedDb = 20.0 * std::log10(std::max({ std::abs(scA), std::abs(scB), 1.0e-7 }));
        updateEnv(detLinkedDb, envADb_, slowRelA_, britLagA_);
        envBDb_ = envADb_;
        const double g = std::pow(10.0, -envADb_ / 20.0);
        procL *= g;
        procR *= g;
        grDisplayDb = envADb_;
    }

    // Reconversion M/S -> L/R si nécessaire
    if (routing == 3)
    {
        const double m = procL;
        const double s = procR;
        procL = m + s;
        procR = m - s;
    }

    // 5. Circuits de Distorsion Harmonique (CLEAN / DIST 2 / DIST 3) via ADAA 64-bit
    const double inputDriveNorm = std::clamp((inputDb + 24.0) / 48.0, 0.0, 1.0);
    const double peakSig        = std::max(std::abs(procL), std::abs(procR));
    double instThdPct           = 0.0;

    if (distMode == 1) // DIST 2 : harmoniques paires chaleureuses (Class-A) via ADAA asymétrique
    {
        const float drv2 = static_cast<float>(0.65 + 1.45 * inputDriveNorm);
        const float asymL = static_cast<float>(0.22 + 0.16 * inputDriveNorm + static_cast<double>(tolBiasL));
        const float asymR = static_cast<float>(0.22 + 0.16 * inputDriveNorm + static_cast<double>(tolBiasR));

        const float sL = sat2L_.processADAA1(static_cast<float>(procL), drv2, asymL);
        const float sR = sat2R_.processADAA1(static_cast<float>(procR), drv2, asymR);
        constexpr double mix2 = 0.55;
        procL = (1.0 - mix2) * procL + mix2 * static_cast<double>(sL);
        procR = (1.0 - mix2) * procR + mix2 * static_cast<double>(sR);

        instThdPct = std::clamp(peakSig * (1.8 + 4.2 * inputDriveNorm), 0.0, 12.0);
    }
    else if (distMode == 2) // DIST 3 : harmoniques impaires riches (bande magnétique) via ADAA symétrique + hystérésis
    {
        const float drv3 = static_cast<float>(0.90 + 1.95 * inputDriveNorm);
        const float sL = sat3L_.processADAA1(static_cast<float>(procL), drv3, tolBiasL * 0.2f);
        const float sR = sat3R_.processADAA1(static_cast<float>(procR), drv3, tolBiasR * 0.2f);
        const float hDrv = 0.35f + 0.45f * static_cast<float>(inputDriveNorm);
        const float hL = hyst3L_.processSample(sL, hDrv, sampleRate_, 4);
        const float hR = hyst3R_.processSample(sR, hDrv, sampleRate_, 4);
        constexpr double mix3 = 0.65;
        procL = (1.0 - mix3) * procL + mix3 * static_cast<double>(hL);
        procR = (1.0 - mix3) * procR + mix3 * static_cast<double>(hR);

        instThdPct = std::clamp(peakSig * (2.6 + 6.0 * inputDriveNorm), 0.0, 15.0);
    }
    else
    {
        // CLEAN : reset silencieux des saturateurs
        (void) sat2L_.processADAA1(0.0f, 0.0f, 0.0f);
        (void) sat2R_.processADAA1(0.0f, 0.0f, 0.0f);
        (void) sat3L_.processADAA1(0.0f, 0.0f, 0.0f);
        (void) sat3R_.processADAA1(0.0f, 0.0f, 0.0f);
        instThdPct = 0.0;
    }

    // Lissage léger de l'indicateur THD pour les voyants 1% / 3%
    const double thdC = std::exp(-1.0 / std::max(1.0, 0.040 * sampleRate_));
    thdEnv_ = (instThdPct > thdEnv_) ? (0.85 * thdEnv_ + 0.15 * instThdPct)
                                     : (thdC * thdEnv_ + (1.0 - thdC) * instThdPct);
    lastThdPct_.store(static_cast<float>(thdEnv_), std::memory_order_relaxed);

    // 6. Gain de sortie & Blend Dry/Wet
    const double wetL = procL * outLin;
    const double wetR = procR * outLin;
    const double outL = (1.0 - blend) * static_cast<double>(dryL) + blend * wetL;
    const double outR = (1.0 - blend) * static_cast<double>(dryR) + blend * wetR;

    l = std::isfinite(outL) ? static_cast<float>(outL) : 0.0f;
    r = std::isfinite(outR) ? static_cast<float>(outR) : 0.0f;
    return static_cast<float>(grDisplayDb);
}

} // namespace kalyx::consoles
