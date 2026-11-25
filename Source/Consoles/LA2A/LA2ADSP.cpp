#include "LA2ADSP.h"
#include <algorithm>
#include <cmath>

namespace kalyx::consoles
{

void LA2ADSP::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(sampleRate, 44100.0);
    reset();
}

void LA2ADSP::reset() noexcept
{
    fastEnvA_  = fastEnvB_  = 0.0;
    slowEnvA_  = slowEnvB_  = 0.0;
    memoryA_   = memoryB_   = 0.0;
    prevGainA_ = prevGainB_ = 1.0;

    r37EmphL_.reset();
    r37EmphR_.reset();
    tubeSatL_.reset();
    tubeSatR_.reset();
    ha100xL_.reset();
    ha100xR_.reset();
}

double LA2ADSP::computeOptoTargetGrDb(double detDb,
                                       double threshDb,
                                       bool isLimit,
                                       double cellAge) noexcept
{
    const double over = detDb - threshDb;

    // Coude optique progressif (plus doux avec le vieillissement de la cellule T4B)
    const double kneeDb = (isLimit ? 7.0 : 12.0) + cellAge * 4.0;
    if (over <= -0.5 * kneeDb)
        return 0.0;

    // Ratio optique dépendant du niveau :
    // - COMP : compression douce ~3.2:1 à 4.0:1
    // - LIMIT : rapport élevé > 10:1 (12:1 à 16:1)
    const double baseRatio = isLimit ? (12.0 + 4.0 * std::clamp(over / 18.0, 0.0, 1.0))
                                     : (3.2 + 0.8 * std::clamp(over / 16.0, 0.0, 1.0));
    const double slope = 1.0 - 1.0 / baseRatio;

    if (over >= 0.5 * kneeDb)
        return over * slope;

    const double x = over + 0.5 * kneeDb;
    return (slope * x * x) / (2.0 * kneeDb);
}

float LA2ADSP::processSample(float& l, float& r,
                             float detL, float detR,
                             const kalyx::core::SlotParamArray& p,
                             bool extSidechain,
                             float tolBiasL, float tolBiasR) noexcept
{
    using namespace kalyx::core;
    using namespace kalyx::dsp;

    // p[0] : LA-2A IN (Bypass / Engage)
    if (p[0] <= 0.5f)
    {
        fastEnvA_ = fastEnvB_ = 0.0;
        slowEnvA_ = slowEnvB_ = 0.0;
        memoryA_  = memoryB_  = 0.0;
        prevGainA_ = prevGainB_ = 1.0;
        (void)tubeSatL_.processADAA2(0.0f, 0.0f, 0.0f);
        (void)tubeSatR_.processADAA2(0.0f, 0.0f, 0.0f);
        return 0.0f;
    }

    const double dryL = static_cast<double>(l);
    const double dryR = static_cast<double>(r);

    // Silence Gate strict (garantit < -120 dBFS sur entrée silencieuse 0.0)
    if (std::abs(dryL) < 1.0e-6 && std::abs(dryR) < 1.0e-6
        && std::abs(static_cast<double>(detL)) < 1.0e-6
        && std::abs(static_cast<double>(detR)) < 1.0e-6
        && fastEnvA_ < 1.0e-5 && fastEnvB_ < 1.0e-5
        && slowEnvA_ < 1.0e-5 && slowEnvB_ < 1.0e-5)
    {
        fastEnvA_ = fastEnvB_ = 0.0;
        slowEnvA_ = slowEnvB_ = 0.0;
        memoryA_  = memoryB_  = 0.0;
        prevGainA_ = prevGainB_ = 1.0;
        (void)tubeSatL_.processADAA2(0.0f, 0.0f, 0.0f);
        (void)tubeSatR_.processADAA2(0.0f, 0.0f, 0.0f);
        (void)ha100xL_.processSample(0.0f, 0.0f, sampleRate_, 3);
        (void)ha100xR_.processSample(0.0f, 0.0f, sampleRate_, 3);
        l = 0.0f;
        r = 0.0f;
        return 0.0f;
    }

    // Paramètres normalisés du slot
    const bool   isLimit   = (p[1] > 0.5f); // 0 = COMP, 1 = LIMIT
    const double gainDb    = static_cast<double>(mapLin(p[2], -24.0f, 24.0f));
    const double peakRed   = static_cast<double>(std::clamp(p[3], 0.0f, 1.0f));
    const double r37Emph   = static_cast<double>(std::clamp(p[4], 0.0f, 1.0f));
    const double cellAge   = static_cast<double>(std::clamp(p[5], 0.0f, 1.0f));
    const double blend     = static_cast<double>(std::clamp(p[6], 0.0f, 1.0f));
    const int    routing   = mapChoice(p[7], 5);

    const bool isMS  = (routing == static_cast<int>(DynamicsRoutingMode::MidSide));
    const bool isInv = (routing == static_cast<int>(DynamicsRoutingMode::InverseDucking));

    // 1. R37 HF Emphasis (filtre sidechain continu 0..100 % accentuant les hautes fréquences vers le panneau EL)
    double scInL = extSidechain ? static_cast<double>(detL) : dryL;
    double scInR = extSidechain ? static_cast<double>(detR) : dryR;

    if (r37Emph > 1.0e-4)
    {
        const double shelfDb = r37Emph * 9.5;
        r37EmphL_.setHighShelf(sampleRate_, 2200.0, 0.7071, shelfDb);
        r37EmphR_.setHighShelf(sampleRate_, 2200.0, 0.7071, shelfDb);
        scInL = static_cast<double>(r37EmphL_.processSample(static_cast<float>(scInL)));
        scInR = static_cast<double>(r37EmphR_.processSample(static_cast<float>(scInR)));
    }

    // 2. PEAK REDUCTION (0..100 %) pilote l'intensité lumineuse envoyée à la cellule T4B
    // À 0 % -> seuil très haut (+6 dB), quasi aucune compression ; à 45 % -> ~ -13 dBFS ; à 100 % -> -36 dBFS
    const double prDriveDb = -6.0 + peakRed * 42.0;
    const double effThresh = -18.0 - (prDriveDb - 12.0); // [-36 dB .. +6 dB] selon Peak Reduction

    const auto det = computeUniversalDetectorLevels(static_cast<float>(scInL),
                                                    static_cast<float>(scInR),
                                                    routing);

    auto calcTarget = [&](double detDb) noexcept -> double
    {
        if (peakRed <= 1.0e-4)
            return 0.0;
        if (isInv)
        {
            const double over = detDb - effThresh;
            return (over > 0.0) ? std::min(30.0, over * 1.35) : 0.0;
        }
        return computeOptoTargetGrDb(detDb, effThresh, isLimit, cellAge);
    };

    const double tA = calcTarget(det.detADb);
    const double tB = calcTarget(det.detBDb);

    // 3. Cellule Opto-Électrique T4B (Attaque douce ~10 ms + Double étage non-linéaire + Effet de Mémoire)
    // Attaque : ~10 ms en moyenne (7 ms T4A neuve -> 15 ms cellule usée, légèrement plus vive sur fortes crêtes)
    const double progAttScaleA = 1.0 - 0.25 * std::clamp(tA / 20.0, 0.0, 1.0);
    const double progAttScaleB = 1.0 - 0.25 * std::clamp(tB / 20.0, 0.0, 1.0);
    const double baseAttSec    = 0.007 + 0.008 * cellAge; // 7..15 ms (moyenne ~10 ms)
    const double attCA = std::exp(-1.0 / std::max(1.0, baseAttSec * progAttScaleA * sampleRate_));
    const double attCB = std::exp(-1.0 / std::max(1.0, baseAttSec * progAttScaleB * sampleRate_));

    // Historique d'illumination du panneau EL (effet de mémoire T4B)
    const double memChargeC = std::exp(-1.0 / std::max(1.0, 0.220 * sampleRate_));
    const double memDecayC  = std::exp(-1.0 / std::max(1.0, (2.5 + 3.0 * cellAge) * sampleRate_));
    const double instLumA   = std::clamp(tA / 16.0, 0.0, 1.0);
    const double instLumB   = std::clamp(tB / 16.0, 0.0, 1.0);
    memoryA_ = (instLumA > memoryA_) ? (memChargeC * memoryA_ + (1.0 - memChargeC) * instLumA)
                                     : (memDecayC  * memoryA_ + (1.0 - memDecayC)  * instLumA);
    memoryB_ = (instLumB > memoryB_) ? (memChargeC * memoryB_ + (1.0 - memChargeC) * instLumB)
                                     : (memDecayC  * memoryB_ + (1.0 - memDecayC)  * instLumB);

    // Premier étage de relâchement : premiers 50 % rapides (~60 ms en moyenne, 45..85 ms selon Cell Age)
    const double fastRelSec = 0.045 + 0.040 * cellAge;
    const double fastRelC   = std::exp(-1.0 / std::max(1.0, fastRelSec * sampleRate_));

    // Second étage : queue lente exponentielle de 1 s à 15 s selon l'historique d'illumination et Cell Age
    const double slowRelSecA = (1.0 + 14.0 * std::clamp(memoryA_, 0.0, 1.0)) * (0.65 + 0.70 * cellAge);
    const double slowRelSecB = (1.0 + 14.0 * std::clamp(memoryB_, 0.0, 1.0)) * (0.65 + 0.70 * cellAge);
    const double slowRelCA   = std::exp(-1.0 / std::max(1.0, slowRelSecA * sampleRate_));
    const double slowRelCB   = std::exp(-1.0 / std::max(1.0, slowRelSecB * sampleRate_));

    fastEnvA_ = (tA > fastEnvA_) ? (attCA * fastEnvA_ + (1.0 - attCA) * tA)
                                 : (fastRelC * fastEnvA_ + (1.0 - fastRelC) * tA);
    fastEnvB_ = (tB > fastEnvB_) ? (attCB * fastEnvB_ + (1.0 - attCB) * tB)
                                 : (fastRelC * fastEnvB_ + (1.0 - fastRelC) * tB);

    slowEnvA_ = (tA > slowEnvA_) ? (attCA * slowEnvA_ + (1.0 - attCA) * tA)
                                 : (slowRelCA * slowEnvA_ + (1.0 - slowRelCA) * tA);
    slowEnvB_ = (tB > slowEnvB_) ? (attCB * slowEnvB_ + (1.0 - attCB) * tB)
                                 : (slowRelCB * slowEnvB_ + (1.0 - slowRelCB) * tB);

    if (fastEnvA_ < 1.0e-5) fastEnvA_ = 0.0;
    if (fastEnvB_ < 1.0e-5) fastEnvB_ = 0.0;
    if (slowEnvA_ < 1.0e-5) slowEnvA_ = 0.0;
    if (slowEnvB_ < 1.0e-5) slowEnvB_ = 0.0;

    // Combinaison exacte 50/50 des deux étages : les premiers 50 % récupèrent en ~60 ms,
    // tandis que les 50 % restants s'éteignent sur la queue lente (1 s .. 15 s).
    const double envADb = 0.50 * fastEnvA_ + 0.50 * slowEnvA_;
    const double envBDb = 0.50 * fastEnvB_ + 0.50 * slowEnvB_;

    prevGainA_ = std::pow(10.0, -envADb / 20.0);
    prevGainB_ = std::pow(10.0, -envBDb / 20.0);

    double wetL = dryL * prevGainA_;
    double wetR = dryR * prevGainB_;
    if (isMS)
    {
        const double mid  = 0.5 * (dryL + dryR) * prevGainA_;
        const double side = 0.5 * (dryL - dryR) * prevGainB_;
        wetL = mid + side;
        wetR = mid - side;
    }

    // 4. Étage de sortie à lampes (12AX7 / 12BH7) + Transformateur HA-100X (harmoniques paires via ADAA2)
    const double outLin    = std::pow(10.0, gainDb / 20.0);
    const double tubeDrive = std::clamp(0.45 + 0.025 * std::max(envADb, envBDb) + 0.15 * std::max(0.0, gainDb / 24.0), 0.25, 1.8);
    const double tubeBias  = 0.068; // Asymétrie douce 12AX7/12BH7 -> richesse en harmoniques paires

    const double satL = static_cast<double>(tubeSatL_.processADAA2(static_cast<float>(wetL),
                                                                   static_cast<float>(tubeDrive),
                                                                   static_cast<float>(tubeBias + static_cast<double>(tolBiasL))));
    const double satR = static_cast<double>(tubeSatR_.processADAA2(static_cast<float>(wetR),
                                                                   static_cast<float>(tubeDrive),
                                                                   static_cast<float>(tubeBias + static_cast<double>(tolBiasR))));
    wetL = 0.72 * wetL + 0.28 * satL;
    wetR = 0.72 * wetR + 0.28 * satR;

    // Transformateur de sortie HA-100X (hystérésis magnétique douce)
    float txL = ha100xL_.processSample(static_cast<float>(wetL * outLin), 0.22f, sampleRate_, 3);
    float txR = ha100xR_.processSample(static_cast<float>(wetR * outLin), 0.22f, sampleRate_, 3);

    const double finalL = (1.0 - blend) * dryL + blend * static_cast<double>(txL);
    const double finalR = (1.0 - blend) * dryR + blend * static_cast<double>(txR);

    l = std::isfinite(finalL) ? static_cast<float>(finalL) : 0.0f;
    r = std::isfinite(finalR) ? static_cast<float>(finalR) : 0.0f;

    return static_cast<float>(std::max(envADb, envBDb));
}

} // namespace kalyx::consoles
