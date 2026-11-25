#pragma once

#include "../../Core/ConsoleModuleBase.h"
#include "../../DSPShared/ADAASaturator.h"
#include "../../DSPShared/HysteresisStage.h"
#include "../../DSPShared/StereoAndMeters.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace kalyx::consoles
{

/**
 * Moteur DSP 64 bits (double) du compresseur à lampes Fairchild 660 / 670 (Variable-Mu).
 *
 * Mapping sur le réservoir générique du slot (slotX_p1..p18, indexés 0..17) :
 * - p[0]  : 670 IN (Bypass / Engage)
 * - p[1]  : INPUT GAIN (-24 .. +24 dB)
 * - p[2]  : AC THRESHOLD (0 .. 100 %, polarisation de grille Vari-Mu)
 * - p[3]  : TIME CONST (0..1 -> TC 1..6 crantés en mode VINTAGE, ou interpolation continue en mode MODERN)
 * - p[4]  : OUTPUT (-24 .. +24 dB)
 * - p[5]  : MATCHING L/R (0 .. 100 %, appairage stéréo seuil & relâchement)
 * - p[6]  : MATCHING P/P (0 .. 100 %, symétrie Push-Pull -> asymétrie harmoniques paires ADAA 64-bit)
 * - p[7]  : BLEND (Dry/Wet 0 .. 100 %)
 * - p[8]  : NOS TUBE (0..6 : 7 lampes NOS : Thomson 5749, Brimar CV4009, GE 5749, RCA 6386 Black, GE 5-Star 6386, RCA 6BC8, Mullard ECC82)
 * - p[9]  : ROUTING (0 = MONO, 1 = STEREO, 2 = DUAL M, 3 = M/S, 4 = INV)
 * - p[10] : VINTAGE / MODERN (1 = VINTAGE TC 1..6 crantés, 0 = MODERN continu)
 *
 * Zéro allocation dynamique, zéro verrou dans processSample(), silence < -120 dBFS sur entrée 0.0.
 */
class FairchildDSP
{
public:
    enum class NosTube : int
    {
        Thomson5749FSE  = 0,
        BrimarCV4009    = 1,
        GE5749          = 2,
        RCA6386Black    = 3,
        GE5Star6386     = 4,
        RCA6BC8         = 5,
        MullardECC82    = 6
    };

    FairchildDSP() noexcept = default;

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

    struct TubeProfile
    {
        const char* shortName;
        const char* signature;
        double kneeSoftness;
        double maxRatio;
        double satDrive;
        double ironDrive;
    };

    [[nodiscard]] static const TubeProfile& getTubeProfile(int tubeIdx) noexcept;

private:
    [[nodiscard]] static double computeVariMuTargetGrDb(double detDb,
                                                        double effThreshDb,
                                                        const TubeProfile& tube) noexcept;

    double sampleRate_ { 44100.0 };

    // Enveloppes Vari-Mu 64 bits (2 canaux A/B pour Dual Mono, M/S et Matching L/R)
    double envADb_     { 0.0 };
    double envBDb_     { 0.0 };
    double slowEnvA_   { 0.0 };
    double slowEnvB_   { 0.0 };
    double memA_       { 0.0 };
    double memB_       { 0.0 };

    // Étages non-linéaires 64-bit ADAA (Push-Pull + lampes NOS) & transformateurs de sortie
    kalyx::dsp::ADAASaturator satL_, satR_;
    kalyx::dsp::HysteresisStage ironL_, ironR_;
};

} // namespace kalyx::consoles
