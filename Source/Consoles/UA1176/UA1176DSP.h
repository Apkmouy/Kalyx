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
 * Moteur DSP 64 bits (double) du compresseur FET Universal Audio 1176 (Rev A / Rev D / Rev G).
 *
 * Mapping sur le réservoir générique du slot (slotX_p1..p18, indexés 0..17) :
 * - p[0]  : 1176 IN (Bypass / Engage)
 * - p[1]  : REVISION (0 = Rev A Bluestripe, 1 = Rev D Blackface, 2 = Rev G Silverface)
 * - p[2]  : INPUT (-24 .. +24 dB, pilote directement le seuil fixe du FET)
 * - p[3]  : OUTPUT (-24 .. +24 dB)
 * - p[4]  : ATTACK (0..1, vers la droite = plus rapide : 800 µs -> 20 µs, ou 10 ms -> 20 µs avec SLOW)
 * - p[5]  : RELEASE (0..1, vers la droite = plus rapide : 1100 ms -> 50 ms)
 * - p[6]  : FET BIAS / Headroom (0..1 : Clean Headroom -> Hot / Saturated)
 * - p[7]  : BLEND (Dry/Wet 0..100 %)
 * - p[8]  : RATIO (0 = 4:1, 1 = 8:1, 2 = 12:1, 3 = 20:1)
 * - p[9]  : ALL-BUTTONS IN / SLAM (Bool)
 * - p[10] : SUPER SLOW ATTACK (Bool, étend la plage d'attaque jusqu'à 10 ms)
 * - p[11] : ROUTING (0 = MONO, 1 = STEREO, 2 = DUAL M, 3 = M/S, 4 = INV)
 *
 * Zéro allocation dynamique, zéro verrou dans processSample(), silence < -120 dBFS sur entrée 0.0.
 */
class UA1176DSP
{
public:
    enum class Revision : int
    {
        RevA_Bluestripe = 0,
        RevD_Blackface  = 1,
        RevG_Silverface = 2
    };

    UA1176DSP() noexcept = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    /**
     * Traite un échantillon stéréo en précision 64 bits (avec suréchantillonnage interne 2x
     * dans la cellule FET feedback et saturation ADAA2) et retourne la réduction de gain (dB >= 0).
     */
    [[nodiscard]] float processSample(float& l, float& r,
                                      float detL, float detR,
                                      const kalyx::core::SlotParamArray& p,
                                      bool extSidechain,
                                      float tolBiasL, float tolBiasR) noexcept;

    // Helpers analytiques partagés entre DSP et Façade UI (sens horaire = plus rapide comme l'original)
    [[nodiscard]] static float mapAttackUs(float norm, bool superSlow) noexcept
    {
        const float n     = std::clamp(norm, 0.0f, 1.0f);
        const float maxUs = superSlow ? 10000.0f : 800.0f;
        constexpr float minUs = 20.0f;
        return maxUs * std::pow(minUs / maxUs, n);
    }

    [[nodiscard]] static float unmapAttackUs(float us, bool superSlow) noexcept
    {
        const float maxUs = superSlow ? 10000.0f : 800.0f;
        constexpr float minUs = 20.0f;
        const float clamped = std::clamp(us, minUs, maxUs);
        return std::clamp(std::log(clamped / maxUs) / std::log(minUs / maxUs), 0.0f, 1.0f);
    }

    [[nodiscard]] static float mapReleaseMs(float norm) noexcept
    {
        const float n = std::clamp(norm, 0.0f, 1.0f);
        constexpr float maxMs = 1100.0f;
        constexpr float minMs = 50.0f;
        return maxMs * std::pow(minMs / maxMs, n);
    }

    [[nodiscard]] static float unmapReleaseMs(float ms) noexcept
    {
        constexpr float maxMs = 1100.0f;
        constexpr float minMs = 50.0f;
        const float clamped = std::clamp(ms, minMs, maxMs);
        return std::clamp(std::log(clamped / maxMs) / std::log(minMs / maxMs), 0.0f, 1.0f);
    }

private:
    [[nodiscard]] double computeFetTargetGrDb(double detDb,
                                              double fixedThreshDb,
                                              double baseRatio,
                                              bool allButtons) const noexcept;

    double sampleRate_ { 44100.0 };

    // États de la boucle Feedback FET 64 bits (2 canaux A/B pour Dual Mono / M-S)
    double envADb_     { 0.0 };
    double envBDb_     { 0.0 };
    double slowRelA_   { 0.0 };
    double slowRelB_   { 0.0 };
    double slamLagA_   { 0.0 };
    double slamLagB_   { 0.0 };
    double prevGainA_  { 1.0 };
    double prevGainB_  { 1.0 };

    // Mémoire pour suréchantillonnage interne 2x de la cellule FET (zéro latence)
    double prevInL_    { 0.0 };
    double prevInR_    { 0.0 };
    double prevDetL_   { 0.0 };
    double prevDetR_   { 0.0 };

    // Étages non-linéaires & coloration historique (Rev A / D / G)
    kalyx::dsp::ADAASaturator satL_, satR_;
    kalyx::dsp::HysteresisStage hystL_, hystR_;
    kalyx::dsp::ZDFStateVariableFilter revAMidL_, revAMidR_;
};

} // namespace kalyx::consoles
