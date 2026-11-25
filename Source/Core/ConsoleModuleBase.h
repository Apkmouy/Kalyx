#pragma once

#include "../DSPShared/StereoAndMeters.h"
#include "../DSPShared/ZDFFilters.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <cstdint>

namespace kalyx::core
{

inline constexpr int kNumRackSlots  = 5;  // Slot A, B, C, D, E
inline constexpr int kParamsPerSlot = 18; // p1..p18 (prêt pour Fairchild 660/670 et ses 7 lampes NOS)

/**
 * Finition du châssis externe (INPUT, MASTER et Bandeau supérieur) :
 * - 0 : NIGHT MODE (métal noir anodisé)
 * - 1 : DAY MODE (aluminium clair brossé #DCDFE4, polices #12151C)
 * - 2 : KALYX HERITAGE (vert forêt teal foncé #143832 & parme profond #9B5B7A)
 */
enum class ChassisTheme : int
{
    Night         = 0,
    Day           = 1,
    KalyxHeritage = 2
};

/**
 * Modèles de préamplis disponibles dans la tranche fixe INPUT.
 */
enum class PreampModel : int
{
    SSL4000E   = 0,
    SSL4000G   = 1,
    API1608    = 2,
    Neve1073   = 3,
    KalyxClean = 4
};

/**
 * Catalogue complet des modules Série 500 (avec fusion SSL 4000 E/G unifié via bascule physique E/G).
 */
enum class SlotModuleId : int
{
    // Préamplis
    SSL4000Preamp      = 0,  // SSL 4000 Preamp (bascule E/G)
    API2520Preamp      = 1,  // API 2520 Preamp
    Neve1073Preamp     = 2,  // Neve 1073 Marinair
    KalyxCleanPreamp   = 3,  // Kalyx Clean Preamp

    // Compression
    SSL4000Comp        = 4,  // SSL 4000 VCA (bascule E/G)
    API2500Comp        = 5,  // API 2500 Bus Comp
    Neve5254Comp       = 6,  // Neve 5254 Diode-Bridge

    // Égaliseurs (EQ)
    SSL4000EQ          = 7,  // SSL 4000 4-Band EQ (bascule E/G)
    API550BEQ          = 8,  // API 550B 4-Band EQ
    Neve1073EQ         = 9,  // Neve 1073 3-Band EQ

    // Coloration
    ConsoleColor       = 10, // Console Color Stage

    // Utilitaire & Limiteur
    UtilityGateDeEsser = 11, // Utility Gate / De-Esser
    PrecisionLimiter   = 12, // Precision Peak Limiter
    EmptyBypass        = 13, // Empty / Bypass

    // Compression FET / Opto / VCA Hybride / Vari-Mu
    UA1176Comp         = 14, // Universal Audio 1176 (FET)
    LA2AComp           = 15, // Teletronix LA-2A (Opto)
    DistressorComp     = 16, // Empirical Labs Distressor (VCA + Dist 2/3)
    FairchildComp      = 17, // Fairchild 660/670 (Vari-Mu)

    // Gamme Moderne Kalyx Audio (2U Double-Largeur & 1U)
    SurgicalEQ2U       = 18, // Kalyx Surgical EQ (SplitEQ + Dynamic 2U)
    PrecisionDyn2U     = 19, // Kalyx Precision VCA & Transient (2U)
    HarmonicSculptor   = 20, // Kalyx Harmonic Sculptor (1U)
    StereoPhase        = 21, // Kalyx Stereo & Phase Studio (1U)

    Count              = 22
};

using SlotParamArray   = std::array<float, kParamsPerSlot>;
using SlotParamArray2U = std::array<float, kParamsPerSlot * 2>;

[[nodiscard]] constexpr bool isModule2U(SlotModuleId id) noexcept
{
    return id == SlotModuleId::SurgicalEQ2U || id == SlotModuleId::PrecisionDyn2U;
}

[[nodiscard]] constexpr bool isModuleId2U(int id) noexcept
{
    return id == static_cast<int>(SlotModuleId::SurgicalEQ2U)
        || id == static_cast<int>(SlotModuleId::PrecisionDyn2U);
}

struct InputStripParameters
{
    float trimDb      { 0.0f };    // -24 .. +24 dB
    int   preampModel { 0 };       // 0..4 (SSL E, SSL G, API 1608, Neve 1073, Kalyx Clean)
    float drivePct    { 0.0f };    // 0 .. 100 %
    bool  phaseInvert { false };
    bool  hpfEnabled  { false };
    float hpfFreqHz   { 40.0f };   // 20 .. 500 Hz
    bool  lpfEnabled  { false };
    float lpfFreqHz   { 18000.0f };// 2000 .. 20000 Hz
    bool  filtersToSc { false };
    bool  extSidechain{ false };
    bool  scListen    { false };
};

struct MasterStripParameters
{
    float analogTolerance { 25.0f };  // 0 .. 100 %
    float mixPct          { 100.0f }; // 0 .. 100 %
    float outGainDb       { 0.0f };   // -24 .. +12 dB
    int   vuMode          { 1 };      // 0 = IN, 1 = OUT, 2 = GR
    int   msMode          { 0 };      // 0 = LR, 1 = M+S, 2 = MID, 3 = SIDE
    int   chassisTheme    { 0 };      // 0 = Night, 1 = Day, 2 = Kalyx Heritage
    float vuRefDb         { 0.0f };   // 0 VU = 0 dBFS par défaut (calibrable -24..0 dBFS)
    int   oversampling    { 0 };      // 0 = 1x, 1 = 2x, 2 = 4x
    int   fftMode         { 0 };      // 0 = POST EQ, 1 = PRE EQ, 2 = OFF
};

struct SlotModuleDescriptor
{
    SlotModuleId id         { SlotModuleId::EmptyBypass };
    const char*  shortName  { "Empty" };
    const char*  displayName{ "Empty / Bypass" };
    const char*  category   { "Utilitaire & Limiteur" };
    juce::uint32 bgArgb     { 0xff1e2024u };
    juce::uint32 accentArgb { 0xff9b5b7au };
    bool         darkText   { false };
};

// Helpers de conversion normalisé [0..1] <-> plage physique
[[nodiscard]] inline float mapLin(float norm, float minV, float maxV) noexcept
{
    return minV + std::clamp(norm, 0.0f, 1.0f) * (maxV - minV);
}

[[nodiscard]] inline float mapLog(float norm, float minV, float maxV) noexcept
{
    const float n = std::clamp(norm, 0.0f, 1.0f);
    return minV * std::pow(maxV / std::max(minV, 1.0e-5f), n);
}

[[nodiscard]] inline int mapChoice(float norm, int numChoices) noexcept
{
    const int n = std::max(numChoices, 1);
    return std::clamp(static_cast<int>(std::floor(std::clamp(norm, 0.0f, 0.9999f) * static_cast<float>(n))), 0, n - 1);
}

[[nodiscard]] inline float unmapLin(float plain, float minV, float maxV) noexcept
{
    return std::clamp((plain - minV) / std::max(maxV - minV, 1.0e-6f), 0.0f, 1.0f);
}

[[nodiscard]] inline float unmapLog(float plain, float minV, float maxV) noexcept
{
    const float clamped = std::clamp(plain, minV, maxV);
    return std::clamp(std::log(clamped / minV) / std::log(maxV / minV), 0.0f, 1.0f);
}

[[nodiscard]] inline float choiceToNorm(int idx, int numChoices) noexcept
{
    if (numChoices <= 1) return 0.0f;
    return (static_cast<float>(std::clamp(idx, 0, numChoices - 1)) + 0.5f) / static_cast<float>(numChoices);
}

} // namespace kalyx::core
