#pragma once

#include "../../Core/ConsoleModuleBase.h"
#include "../../DSPShared/ADAASaturator.h"
#include "../../DSPShared/HysteresisStage.h"
#include "../../DSPShared/StereoAndMeters.h"
#include "../../DSPShared/ZDFFilters.h"
#include <algorithm>
#include <atomic>
#include <cmath>

namespace kalyx::consoles
{

/**
 * Moteur DSP 64 bits (double) du compresseur Empirical Labs Distressor (VCA + Distorsion Harmonique).
 *
 * Mapping sur le réservoir générique du slot (slotX_p1..p18, indexés 0..17) :
 * - p[0]  : DIST IN (Bypass / Engage)
 * - p[1]  : INPUT (-24 .. +24 dB, pilote simultanément la compression et la saturation harmonique)
 * - p[2]  : OUTPUT (-24 .. +24 dB)
 * - p[3]  : ATTACK (50 µs .. 30 ms, logarithmique)
 * - p[4]  : RELEASE (50 ms .. 3.5 s, logarithmique)
 * - p[5]  : RATIO (0=1:1, 1=2:1, 2=3:1, 3=4:1, 4=6:1, 5=10:1, 6=20:1, 7=NUKE)
 * - p[6]  : BRITISH MODE (Bool, coude agressif & pompage type 1176 All-Buttons aux ratios doux 2:1..6:1)
 * - p[7]  : AUDIO HPF (Bool, 80 Hz sur le signal audio)
 * - p[8]  : DET HPF (Bool, 80 Hz sur le détecteur sidechain)
 * - p[9]  : BAND EMPH (Bool, accentuation 6 kHz sur le détecteur)
 * - p[10] : DIST MODE (0 = CLEAN, 1 = DIST 2 paires Class-A ADAA, 2 = DIST 3 impaires bande ADAA)
 * - p[11] : BLEND (Dry/Wet 0 .. 100 %)
 * - p[12] : ROUTING (0 = MONO, 1 = STEREO, 2 = DUAL M, 3 = M/S, 4 = INV)
 *
 * Zéro allocation dynamique, zéro verrou dans processSample(), silence < -120 dBFS sur entrée 0.0.
 */
class DistressorDSP
{
public:
    enum class RatioCurve : int
    {
        Ratio1_1  = 0,
        Ratio2_1  = 1,
        Ratio3_1  = 2,
        Ratio4_1  = 3,
        Ratio6_1  = 4,
        Ratio10_1 = 5,
        Ratio20_1 = 6,
        Nuke      = 7
    };

    enum class DistMode : int
    {
        Clean = 0,
        Dist2 = 1,
        Dist3 = 2
    };

    DistressorDSP() noexcept = default;

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

    [[nodiscard]] float getInstantThdPct() const noexcept
    {
        return lastThdPct_.load(std::memory_order_relaxed);
    }

    // Helpers analytiques partagés entre DSP et UI (50 µs .. 30 ms, 50 ms .. 3.5 s)
    [[nodiscard]] static float mapAttackMs(float norm) noexcept
    {
        return kalyx::core::mapLog(norm, 0.05f, 30.0f);
    }

    [[nodiscard]] static float mapReleaseMs(float norm) noexcept
    {
        return kalyx::core::mapLog(norm, 50.0f, 3500.0f);
    }

private:
    [[nodiscard]] static double computeDistressorTargetGrDb(double detDb,
                                                            double threshDb,
                                                            int ratioIdx,
                                                            bool britishMode) noexcept;

    double sampleRate_ { 44100.0 };

    // Enveloppes VCA 64 bits (2 canaux A/B pour Dual Mono / M-S)
    double envADb_     { 0.0 };
    double envBDb_     { 0.0 };
    double slowRelA_   { 0.0 };
    double slowRelB_   { 0.0 };
    double britLagA_   { 0.0 };
    double britLagB_   { 0.0 };
    double thdEnv_     { 0.0 };

    // Filtres ZDF Audio (80 Hz HPF) & Détecteur (80 Hz HPF + 6 kHz Band Emphasis)
    kalyx::dsp::ZDFStateVariableFilter audioHpfL_, audioHpfR_;
    kalyx::dsp::ZDFStateVariableFilter detHpfL_, detHpfR_;
    kalyx::dsp::ZDFStateVariableFilter bandEmphL_, bandEmphR_;

    // Circuits de saturation harmonique ADAA 64-bit (Dist 2 = paires, Dist 3 = impaires)
    kalyx::dsp::ADAASaturator sat2L_, sat2R_;
    kalyx::dsp::ADAASaturator sat3L_, sat3R_;
    kalyx::dsp::HysteresisStage hyst3L_, hyst3R_;

    std::atomic<float> lastThdPct_ { 0.0f };
};

} // namespace kalyx::consoles
