#pragma once

#include "../../Core/ConsoleModuleBase.h"
#include "../../DSPShared/ADAASaturator.h"
#include "../../DSPShared/HysteresisStage.h"
#include "../../DSPShared/StereoAndMeters.h"
#include "../../DSPShared/ZDFFilters.h"
#include <algorithm>
#include <cmath>

namespace kalyx::consoles
{

/**
 * Moteur DSP 64 bits (double) du compresseur optique Teletronix LA-2A (Cellule T4B).
 *
 * Mapping sur le réservoir générique du slot (slotX_p1..p18, indexés 0..17) :
 * - p[0] : LA-2A IN (Bypass / Engage)
 * - p[1] : LIMIT / COMP (0 = COMP ~3.5:1, 1 = LIMIT > 10:1)
 * - p[2] : GAIN (-24 .. +24 dB, étage de sortie à lampes 12AX7/12BH7)
 * - p[3] : PEAK REDUCTION (0 .. 100 %, intensité lumineuse envoyée au panneau EL de la cellule T4B)
 * - p[4] : R37 HF EMPHASIS (0 .. 100 %, filtre sidechain haute fréquence sibilance/brillance)
 * - p[5] : CELL AGE (0 % Neuf/T4A rapide .. 100 % Vintage/Usé, inertie & mémoire optique)
 * - p[6] : BLEND (Dry/Wet 0 .. 100 %)
 * - p[7] : ROUTING (0 = MONO, 1 = STEREO, 2 = DUAL M, 3 = M/S, 4 = INV)
 *
 * Zéro allocation dynamique, zéro verrou dans processSample(), silence < -120 dBFS sur entrée 0.0.
 */
class LA2ADSP
{
public:
    LA2ADSP() noexcept = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    /**
     * Traite un échantillon stéréo en précision 64 bits et retourne la réduction de gain (dB >= 0).
     */
    [[nodiscard]] float processSample(float& l, float& r,
                                      float detL, float detR,
                                      const kalyx::core::SlotParamArray& p,
                                      bool extSidechain,
                                      float tolBiasL, float tolBiasR) noexcept;

private:
    [[nodiscard]] static double computeOptoTargetGrDb(double detDb,
                                                      double threshDb,
                                                      bool isLimit,
                                                      double cellAge) noexcept;

    double sampleRate_ { 44100.0 };

    // États de la cellule T4B (2 canaux A/B pour Dual Mono / M-S)
    double fastEnvA_  { 0.0 }; // Étage rapide (~60 ms pour les premiers 50 % de relâchement)
    double fastEnvB_  { 0.0 };
    double slowEnvA_  { 0.0 }; // Queue lente exponentielle (1 s .. 15 s selon mémoire)
    double slowEnvB_  { 0.0 };
    double memoryA_   { 0.0 }; // Historique d'illumination du panneau électroluminescent [0..1]
    double memoryB_   { 0.0 };
    double prevGainA_ { 1.0 };
    double prevGainB_ { 1.0 };

    // Filtre sidechain R37 HF Emphasis
    kalyx::dsp::ZDFStateVariableFilter r37EmphL_, r37EmphR_;

    // Étage de sortie à lampes (12AX7 / 12BH7) + Transformateur HA-100X
    kalyx::dsp::ADAASaturator tubeSatL_, tubeSatR_;
    kalyx::dsp::HysteresisStage ha100xL_, ha100xR_;
};

} // namespace kalyx::consoles
