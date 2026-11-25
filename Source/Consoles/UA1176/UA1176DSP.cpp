#include "UA1176DSP.h"
#include <algorithm>
#include <cmath>

namespace kalyx::consoles
{

void UA1176DSP::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(sampleRate, 44100.0);
    reset();
}

void UA1176DSP::reset() noexcept
{
    envADb_    = 0.0;
    envBDb_    = 0.0;
    slowRelA_  = 0.0;
    slowRelB_  = 0.0;
    slamLagA_  = 0.0;
    slamLagB_  = 0.0;
    prevGainA_ = 1.0;
    prevGainB_ = 1.0;
    prevInL_   = 0.0;
    prevInR_   = 0.0;
    prevDetL_  = 0.0;
    prevDetR_  = 0.0;

    satL_.reset();
    satR_.reset();
    hystL_.reset();
    hystR_.reset();
    revAMidL_.reset();
    revAMidR_.reset();
}

double UA1176DSP::computeFetTargetGrDb(double detDb,
                                       double fixedThreshDb,
                                       double baseRatio,
                                       bool allButtons) const noexcept
{
    // En mode All-Buttons In (Slam / British Mode), la polarisation de grille s'effondre :
    // - seuil fixe abaissé (~6 dB plus bas)
    // - courbe de ratio repliée (quasi-limiteur avec transition agressive)
    const double effThresh = allButtons ? (fixedThreshDb - 6.0) : fixedThreshDb;
    const double over      = detDb - effThresh;

    if (allButtons)
    {
        constexpr double slamKnee = 5.5;
        if (over <= -0.5 * slamKnee)
            return 0.0;

        // Courbe repliée caractéristique du mode British (inflexion + quasi-limiteur 18:1)
        constexpr double slamRatio = 18.0;
        const double slope = 1.0 - 1.0 / slamRatio;
        if (over >= 0.5 * slamKnee)
        {
            const double foldDip = 0.85 * std::exp(-0.18 * std::max(0.0, over - 2.5));
            return std::max(0.0, over * slope + foldDip);
        }
        const double x = over + 0.5 * slamKnee;
        return (slope * x * x) / (2.0 * slamKnee);
    }

    // Courbure douce caractéristique du transistor FET + ratio dépendant du programme
    const double baseKnee = (baseRatio <= 4.5)  ? 9.5
                          : (baseRatio <= 8.5)  ? 7.5
                          : (baseRatio <= 12.5) ? 6.0
                                                : 4.5;

    if (over <= -0.5 * baseKnee)
        return 0.0;

    // Raidissement progressif du ratio quand le FET est poussé profondément en pincement
    const double progRatio = baseRatio * (1.0 + 0.08 * std::clamp(over / 14.0, 0.0, 1.5));
    const double slope     = 1.0 - 1.0 / progRatio;

    if (over >= 0.5 * baseKnee)
        return over * slope;

    const double x = over + 0.5 * baseKnee;
    return (slope * x * x) / (2.0 * baseKnee);
}

float UA1176DSP::processSample(float& l, float& r,
                               float detL, float detR,
                               const kalyx::core::SlotParamArray& p,
                               bool extSidechain,
                               float tolBiasL, float tolBiasR) noexcept
{
    using namespace kalyx::core;
    using namespace kalyx::dsp;

    // p[0] : 1176 IN (Bypass / Engage)
    if (p[0] <= 0.5f)
    {
        envADb_ = envBDb_ = 0.0;
        slowRelA_ = slowRelB_ = 0.0;
        slamLagA_ = slamLagB_ = 0.0;
        prevGainA_ = prevGainB_ = 1.0;
        prevInL_ = prevInR_ = 0.0;
        prevDetL_ = prevDetR_ = 0.0;
        (void)satL_.processADAA2(0.0f, 0.0f, 0.0f);
        (void)satR_.processADAA2(0.0f, 0.0f, 0.0f);
        return 0.0f;
    }

    const double dryL = static_cast<double>(l);
    const double dryR = static_cast<double>(r);

    // Silence Gate strict (garantit < -120 dBFS sur entrée silencieuse 0.0)
    if (std::abs(dryL) < 1.0e-6 && std::abs(dryR) < 1.0e-6
        && std::abs(static_cast<double>(detL)) < 1.0e-6
        && std::abs(static_cast<double>(detR)) < 1.0e-6
        && envADb_ < 1.0e-5 && envBDb_ < 1.0e-5)
    {
        envADb_ = envBDb_ = 0.0;
        slowRelA_ = slowRelB_ = 0.0;
        slamLagA_ = slamLagB_ = 0.0;
        prevGainA_ = prevGainB_ = 1.0;
        prevInL_ = prevInR_ = 0.0;
        prevDetL_ = prevDetR_ = 0.0;
        (void)satL_.processADAA2(0.0f, 0.0f, 0.0f);
        (void)satR_.processADAA2(0.0f, 0.0f, 0.0f);
        (void)hystL_.processSample(0.0f, 0.0f, sampleRate_, 0);
        (void)hystR_.processSample(0.0f, 0.0f, sampleRate_, 0);
        l = 0.0f;
        r = 0.0f;
        return 0.0f;
    }

    // Lecture des paramètres normalisés du slot
    const int    revIdx     = mapChoice(p[1], 3); // 0=Rev A, 1=Rev D, 2=Rev G
    const double inputDb    = static_cast<double>(mapLin(p[2], -24.0f, 24.0f));
    const double outputDb   = static_cast<double>(mapLin(p[3], -24.0f, 24.0f));
    const bool   allButtons = (p[9]  > 0.5f);
    const bool   superSlow  = (p[10] > 0.5f);
    const double attUs      = static_cast<double>(mapAttackUs(p[4], superSlow));
    const double relMs      = static_cast<double>(mapReleaseMs(p[5]));
    const double fetBias    = static_cast<double>(std::clamp(p[6], 0.0f, 1.0f)); // 0=Clean, 1=Hot
    const double blend      = static_cast<double>(std::clamp(p[7], 0.0f, 1.0f));
    const int    ratioIdx   = mapChoice(p[8], 4);
    const int    routing    = mapChoice(p[11], 5);

    static constexpr double kRatios[4] = { 4.0, 8.0, 12.0, 20.0 };
    const double baseRatio = kRatios[ratioIdx];

    const bool isMS  = (routing == static_cast<int>(DynamicsRoutingMode::MidSide));
    const bool isInv = (routing == static_cast<int>(DynamicsRoutingMode::InverseDucking));

    // Étage d'entrée : pilote directement le seuil fixe du transistor FET
    const double inLin  = std::pow(10.0, inputDb / 20.0);
    const double outLin = std::pow(10.0, outputDb / 20.0);

    const double drivenL = dryL * inLin;
    const double drivenR = dryR * inLin;

    // En sidechain interne, le détecteur voit le signal piloté par INPUT ; en SC externe, le bus SC.
    const double rawDetL = extSidechain ? static_cast<double>(detL) : drivenL;
    const double rawDetR = extSidechain ? static_cast<double>(detR) : drivenR;

    // Paramètres physiques spécifiques à la Révision (Rev A / Rev D / Rev G)
    double fixedThreshDb = -18.0;
    double baseDrive     = 0.55;
    double baseBias      = 0.045;
    double hystAmount    = 0.18;
    int    hystModel     = 0;

    if (revIdx == static_cast<int>(Revision::RevA_Bluestripe))
    {
        fixedThreshDb = -19.2; // Réponse agressive, accroche plus tôt
        baseDrive     = 0.85;  // Saturation riche harmoniques paires & impaires
        baseBias      = 0.095; // Forte asymétrie FET Classe A
        hystAmount    = 0.34;  // Transformateur d'entrée UTC vintage
        hystModel     = 3;
    }
    else if (revIdx == static_cast<int>(Revision::RevD_Blackface))
    {
        fixedThreshDb = -18.0; // Low-Noise classique de référence
        baseDrive     = 0.55;
        baseBias      = 0.045;
        hystAmount    = 0.16;
        hystModel     = 0;
    }
    else // Rev G (Silverface) : étage d'entrée op-amp sans transformateur
    {
        fixedThreshDb = -17.0; // Propre et rapide sur transitoires modernes
        baseDrive     = 0.32;
        baseBias      = 0.012;
        hystAmount    = 0.0;   // Sans transformateur d'entrée
        hystModel     = 1;
    }

    // FET BIAS (Headroom) : décale légèrement le point de pincement et l'intensité harmonique
    fixedThreshDb += (0.5 - fetBias) * 4.0; // Clean (+2 dB headroom) <-> Hot (-2 dB)

    // Coefficients de balistique au double de la fréquence d'échantillonnage (2x OS interne)
    // pour traiter proprement les attaques ultra-rapides (20 µs) sans aliasing de modulation.
    const double osFs    = sampleRate_ * 2.0;
    const double attSec  = std::max(2.0e-5, attUs * 1.0e-6) * (allButtons ? 1.35 : 1.0);
    const double relSec  = std::max(0.050, relMs * 0.001);
    const double attC    = std::exp(-1.0 / std::max(0.5, attSec * osFs));
    const double relFast = std::exp(-1.0 / std::max(1.0, relSec * (allButtons ? 0.65 : 0.85) * osFs));
    const double relSlow = std::exp(-1.0 / std::max(1.0, relSec * 1.85 * osFs));
    const double lagC    = std::exp(-1.0 / std::max(1.0, 0.0012 * osFs)); // Décalage transitoire All-Buttons

    double accWetL = 0.0;
    double accWetR = 0.0;

    // Boucle 2x suréchantillonnée interne de la cellule Feedback FET
    for (int sub = 0; sub < 2; ++sub)
    {
        const double frac = (sub == 0) ? 0.5 : 1.0;
        const double sInL = prevInL_ + frac * (drivenL - prevInL_);
        const double sInR = prevInR_ + frac * (drivenR - prevInR_);
        const double sDtL = prevDetL_ + frac * (rawDetL - prevDetL_);
        const double sDtR = prevDetR_ + frac * (rawDetR - prevDetR_);

        // Topologie Feedback FET stabilisée ZDF :
        // Le détecteur écoute le signal après la cellule FET (prevGain) pour le caractère feedback,
        // et reconstruit l'excédent continu sans délai (ZDF) afin de garantir les ratios exacts
        // (4:1, 8:1, 12:1, 20:1, All-Buttons In) sans oscillation même à 20 µs / 44.1 kHz.
        double fbDetL = sDtL;
        double fbDetR = sDtR;
        if (!extSidechain)
        {
            if (isMS)
            {
                const double m = 0.5 * (sDtL + sDtR) * prevGainA_;
                const double s = 0.5 * (sDtL - sDtR) * prevGainB_;
                fbDetL = m + s;
                fbDetR = m - s;
            }
            else
            {
                fbDetL = sDtL * prevGainA_;
                fbDetR = sDtR * prevGainB_;
            }
        }

        const auto det = computeUniversalDetectorLevels(static_cast<float>(fbDetL),
                                                        static_cast<float>(fbDetR),
                                                        routing);

        // Reconstruction ZDF continue : en interne, detDb + envDb restitue le niveau de commande exact
        // tout en conservant la dynamique de boucle feedback (0.78 ZDF + 0.22 feedback pur)
        const double zdfDetA = extSidechain ? det.detADb : (det.detADb + 0.82 * envADb_);
        const double zdfDetB = extSidechain ? det.detBDb : (det.detBDb + 0.82 * envBDb_);

        auto calcTarget = [&](double detDb) noexcept -> double
        {
            if (isInv)
            {
                const double over = detDb - fixedThreshDb;
                return (over > 0.0) ? std::min(30.0, over * 1.45) : 0.0;
            }
            return computeFetTargetGrDb(detDb, fixedThreshDb, baseRatio, allButtons);
        };

        double tA = calcTarget(zdfDetA);
        double tB = calcTarget(zdfDetB);

        if (allButtons)
        {
            // Anomalie All-Buttons In : léger retard sur le front initial suivi d'un claquement explosif
            slamLagA_ = lagC * slamLagA_ + (1.0 - lagC) * tA;
            slamLagB_ = lagC * slamLagB_ + (1.0 - lagC) * tB;
            tA = 0.65 * tA + 0.35 * slamLagA_;
            tB = 0.65 * tB + 0.35 * slamLagB_;
        }

        // Relâchement dépendant du programme (double constante RC analogique)
        slowRelA_ = (tA > slowRelA_) ? (attC * slowRelA_ + (1.0 - attC) * tA)
                                     : (relSlow * slowRelA_ + (1.0 - relSlow) * tA);
        slowRelB_ = (tB > slowRelB_) ? (attC * slowRelB_ + (1.0 - attC) * tB)
                                     : (relSlow * slowRelB_ + (1.0 - relSlow) * tB);

        const double progMixA = std::clamp(slowRelA_ / 18.0, 0.0, 0.75);
        const double progMixB = std::clamp(slowRelB_ / 18.0, 0.0, 0.75);
        const double relCA    = (1.0 - progMixA) * relFast + progMixA * relSlow;
        const double relCB    = (1.0 - progMixB) * relFast + progMixB * relSlow;

        envADb_ = (tA > envADb_) ? (attC * envADb_ + (1.0 - attC) * tA)
                                 : (relCA * envADb_ + (1.0 - relCA) * tA);
        envBDb_ = (tB > envBDb_) ? (attC * envBDb_ + (1.0 - attC) * tB)
                                 : (relCB * envBDb_ + (1.0 - relCB) * tB);

        if (envADb_ < 1.0e-5) envADb_ = 0.0;
        if (envBDb_ < 1.0e-5) envBDb_ = 0.0;

        prevGainA_ = std::pow(10.0, -envADb_ / 20.0);
        prevGainB_ = std::pow(10.0, -envBDb_ / 20.0);

        double wL = sInL * prevGainA_;
        double wR = sInR * prevGainB_;
        if (isMS)
        {
            const double mid  = 0.5 * (sInL + sInR) * prevGainA_;
            const double side = 0.5 * (sInL - sInR) * prevGainB_;
            wL = mid + side;
            wR = mid - side;
        }

        // Saturation non-linéaire du canal Drain-Source FET traitée par ADAA2 64 bits
        const double grNow = std::max(envADb_, envBDb_);
        const double biasScale = 0.35 + 1.45 * fetBias;
        const double slamBoost = allButtons ? 1.65 : 1.0;
        const double fetDrive  = std::clamp((baseDrive + grNow * 0.045) * biasScale * slamBoost, 0.0, 3.8);
        const double fetAsym   = baseBias * (0.4 + 1.2 * fetBias) * (allButtons ? 1.5 : 1.0);
        const double satMix    = std::clamp((0.10 + 0.32 * fetBias + grNow * 0.014) * (allButtons ? 1.35 : 1.0), 0.0, 0.72);

        if (fetDrive > 0.02 && satMix > 0.005)
        {
            const double sL = static_cast<double>(satL_.processADAA2(static_cast<float>(wL),
                                                                     static_cast<float>(fetDrive),
                                                                     static_cast<float>(fetAsym + static_cast<double>(tolBiasL))));
            const double sR = static_cast<double>(satR_.processADAA2(static_cast<float>(wR),
                                                                     static_cast<float>(fetDrive),
                                                                     static_cast<float>(fetAsym + static_cast<double>(tolBiasR))));
            wL = (1.0 - satMix) * wL + satMix * sL;
            wR = (1.0 - satMix) * wR + satMix * sR;
        }

        accWetL += 0.5 * wL;
        accWetR += 0.5 * wR;
    }

    prevInL_  = drivenL;
    prevInR_  = drivenR;
    prevDetL_ = rawDetL;
    prevDetR_ = rawDetR;

    float wetL = static_cast<float>(accWetL);
    float wetR = static_cast<float>(accWetR);

    // Hystérésis du transformateur d'entrée/sortie (Rev A fort, Rev D modéré, Rev G absent)
    if (hystAmount > 0.01)
    {
        const float hDrv = static_cast<float>(std::clamp(hystAmount * (0.65 + 0.75 * fetBias), 0.0, 0.9));
        wetL = hystL_.processSample(wetL, hDrv, sampleRate_, hystModel);
        wetR = hystR_.processSample(wetR, hDrv, sampleRate_, hystModel);
    }

    // Rev A (Bluestripe) : légère bosse dans le haut-médium caractéristique (~2.8 kHz)
    if (revIdx == static_cast<int>(Revision::RevA_Bluestripe))
    {
        const double presenceDb = 0.85 + 0.55 * fetBias;
        revAMidL_.setBell(sampleRate_, 2800.0, 0.85, presenceDb);
        revAMidR_.setBell(sampleRate_, 2800.0, 0.85, presenceDb);
        wetL = revAMidL_.processSample(wetL);
        wetR = revAMidR_.processSample(wetR);
    }

    // Étage de sortie + BLEND (Dry/Wet) : OUTPUT s'applique à la voie compressée (wet)
    // afin qu'un réglage INPUT/OUTPUT compensé se mélange à niveau égal avec le signal Dry.
    const double outWetL = static_cast<double>(wetL) * outLin;
    const double outWetR = static_cast<double>(wetR) * outLin;
    const double finalL  = (1.0 - blend) * dryL + blend * outWetL;
    const double finalR  = (1.0 - blend) * dryR + blend * outWetR;

    l = std::isfinite(finalL) ? static_cast<float>(finalL) : 0.0f;
    r = std::isfinite(finalR) ? static_cast<float>(finalR) : 0.0f;

    return static_cast<float>(std::max(envADb_, envBDb_));
}

} // namespace kalyx::consoles
