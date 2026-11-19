#pragma once

#include "BiquadFilter.h"
#include <cmath>
#include <algorithm>
#include <array>

namespace kalyx::dsp
{

/**
 * Paramètres instantanés (déjà lissés côté AudioProcessor via juce::SmoothedValue)
 * transmis au moteur DSP pur.
 */
struct SSLParameters
{
    // 1. Channel Input, Preamp Saturation (ADAA 64-bit), Filters (ZDF/TPT) & Section 1 Trim
    float inputGainDb     { 0.0f };     // -24 .. +24 dB (LINE TRIM)
    float inputSatDrive   { 0.0f };     // 0.0 .. 1.0 (0% .. 100%)
    bool  phaseInvert     { false };
    bool  hpfEnabled      { true };
    float hpfFreqHz       { 40.0f };    // 20 .. 500 Hz
    float hpfQ            { 0.71f };    // 0.50 .. 4.00
    bool  lpfEnabled      { false };
    float lpfFreqHz       { 18000.0f }; // 2000 .. 20000 Hz
    float lpfQ            { 0.71f };    // 0.50 .. 4.00
    float filtPreTrimDb   { 0.0f };     // -12 .. +12 dB (FILT/PRE TRIM)

    // 2. Dynamics (Gate/Expander + Compressor/Limiter + Dual-RC Auto-Release + Dedicated SC HPF + De-Esser + DYN MAKEUP)
    bool  dynEnabled      { true };
    float gateThreshDb    { -55.0f };   // -70 .. 0 dB
    float gateRangeDb     { 0.0f };     // 0 .. 40 dB (0 = gate off)
    float compThreshDb    { -14.0f };   // -40 .. 0 dB
    float compRatio       { 4.0f };     // 1.0 .. 20.0
    float compAttackMs    { 10.0f };    // 0.5 .. 50 ms
    float compReleaseMs   { 120.0f };   // 20 .. 1000 ms
    bool  compAutoRelease { false };    // True = Circuit Auto-Release RC analogique dual (C1 rapide + C2 lent)
    bool  limiterMode     { false };    // True = Ratio 20:1 Fast Limiter
    float makeupGainDb    { 0.0f };     // -12 .. +24 dB (DYN MAKEUP)

    bool  extSidechain    { false };    // True = Détection Comp/Gate sur bus Sidechain externe
    bool  filtersToSc     { false };    // True = Route HPF/LPF dans la détection Comp/Gate
    bool  scListen        { false };    // True = Solo Sidechain
    float scHpfFreqHz     { 20.0f };    // 20 .. 300 Hz (Filtre Sidechain dédié SC HPF)

    bool  deessEnabled    { false };
    float deessFreqHz     { 6500.0f };  // 4000 .. 10000 Hz
    float deessThreshDb   { -18.0f };   // -40 .. 0 dB

    // 3. Console Color (ADAA 64-bit + Transformer Hysteresis + 4000 E/G + SAT TONE + CROSSTALK + COLOR TRIM)
    float colorAmount     { 0.25f };    // 0.0 .. 1.0 (0% .. 100%)
    bool  colorLinkComp   { true };     // False = Static, True = Linked to Comp/Limiter GR
    bool  circuit4000G    { true };     // False = 4000 E, True = 4000 G
    float satTone         { 0.0f };     // -1.0 (Warm -100%) .. +1.0 (Bright +100%)
    float crosstalk       { 0.0f };     // 0.0 .. 1.0 (0% .. 100%)
    float colorTrimDb     { 0.0f };     // -12 .. +12 dB (COLOR TRIM)

    // 4. Parametric Equaliser (ZDF/TPT SSL 4-Band EQ + LF/HF Bell + EQ PRE DYN + EQ TRIM)
    bool  eqEnabled       { true };
    bool  eqPreDyn        { false };    // Conservé pour compatibilité DAW
    int   moduleOrder     { 0 };        // 0..5 : les 6 permutations de [0=DYNAMICS, 1=COLOR, 2=EQ]
    bool  lfBell          { false };    // False = Low Shelf, True = Peak Bell
    float lfFreqHz        { 80.0f };    // 30 .. 450 Hz
    float lfGainDb        { 0.0f };     // -15 .. +15 dB
    float lmfFreqHz       { 450.0f };   // 200 .. 2500 Hz (Bell)
    float lmfGainDb       { 0.0f };     // -15 .. +15 dB
    float lmfQ            { 1.2f };     // 0.4 .. 4.0
    float hmfFreqHz       { 2500.0f };  // 600 .. 7000 Hz (Bell)
    float hmfGainDb       { 0.0f };     // -15 .. +15 dB
    float hmfQ            { 1.2f };     // 0.4 .. 4.0
    bool  hfBell          { false };    // False = High Shelf, True = Peak Bell
    float hfFreqHz        { 10000.0f }; // 1500 .. 18000 Hz
    float hfGainDb        { 0.0f };     // -15 .. +15 dB
    float eqTrimDb        { 0.0f };     // -12 .. +12 dB (EQ TRIM)

    // 5. Output, True Dynamic Auto-Gain, Mid/Side Routing, Analog Tolerance, Console Mix & Out Fader
    bool  autoGain        { false };
    int   msMode          { 0 };        // 0 = Stereo L/R, 1 = Mid/Side (Both), 2 = M/S (Mid Only), 3 = M/S (Side Only)
    float analogTolerance { 0.25f };    // 0.0 .. 1.0 (0% .. 100% micro-variances déterministes L/R)
    float outputGainDb    { 0.0f };     // -24 .. +12 dB (OUT FADER final)
    float mix             { 1.0f };     // 0.0 .. 1.0 (0% .. 100% CONSOLE MIX)
};

/**
 * Table des 6 permutations possibles des 3 modules centraux :
 *   0 = DYNAMICS & DE-ESSER
 *   1 = CONSOLE COLOR
 *   2 = 4-BAND PARAMETRIC EQ
 */
inline constexpr std::array<std::array<int, 3>, 6> kModulePermutations {{
    {{ 0, 1, 2 }}, // 0: DYN -> COLOR -> EQ (Défaut)
    {{ 0, 2, 1 }}, // 1: DYN -> EQ -> COLOR
    {{ 1, 0, 2 }}, // 2: COLOR -> DYN -> EQ
    {{ 1, 2, 0 }}, // 3: COLOR -> EQ -> DYN
    {{ 2, 0, 1 }}, // 4: EQ -> DYN -> COLOR
    {{ 2, 1, 0 }}  // 5: EQ -> COLOR -> DYN
}};

/**
 * Télémétrie temps réel du moteur DSP.
 */
struct SSLMeterSnapshot
{
    float inputPeakDbL       { -100.0f };
    float inputPeakDbR       { -100.0f };
    float outputPeakDbL      { -100.0f };
    float outputPeakDbR      { -100.0f };
    float inputVuDbL         { -100.0f };
    float inputVuDbR         { -100.0f };
    float outputVuDbL        { -100.0f };
    float outputVuDbR        { -100.0f };
    float inputLufsDbL       { -100.0f };
    float inputLufsDbR       { -100.0f };
    float outputLufsDbL      { -100.0f };
    float outputLufsDbR      { -100.0f };

    float compDetectorDb     { -100.0f };
    float gateDetectorDb     { -100.0f };
    float deessDetectorDb    { -100.0f };

    float compGainRedDb      { 0.0f };
    float gateGainRedDb      { 0.0f };
    float deessGainRedDb     { 0.0f };
    float dynamicColorDrive  { 0.0f };
    float autoGainAppliedDb  { 0.0f };
};

/**
 * Moteur DSP pur de la tranche de console SSL Kalyx v6.
 * Découplé de JUCE, 0 allocation dynamique en traitement temps réel.
 */
class SSLChannelStripDSP
{
public:
    SSLChannelStripDSP() noexcept = default;

    void prepare(double sampleRate, int maxBlockSize) noexcept;
    void reset() noexcept;

    void updateFiltersAndCoefficients(const SSLParameters& params) noexcept;

    void processInputAndPreampStage(const SSLParameters& params,
                                    float& left,
                                    float& right,
                                    float& outPostLineTrimL,
                                    float& outPostLineTrimR) noexcept;

    /**
     * Module central 0 : DYNAMICS & DE-ESSER (Comp/Lim, Gate/Expander, Auto-Release RC, Sidechain, De-Esser, DYN MAKEUP).
     */
    void processDynamicsStage(const SSLParameters& params,
                              float& left,
                              float& right,
                              float extScLeft,
                              float extScRight,
                              float& outCompGrDb,
                              float& outGateGrDb,
                              float& outDeessGrDb,
                              float& outCompDetDb,
                              float& outGateDetDb,
                              float& outDeessDetDb) noexcept;

    /**
     * Module central 1 : CONSOLE COLOR (ADAA 64-bit, Hystérésis Transfo 64-bit, 4000 E/G, SAT TONE, CROSSTALK, COLOR TRIM).
     */
    void processColorStage(const SSLParameters& params,
                           float& left,
                           float& right,
                           float& outEffectiveColor) noexcept;

    /**
     * Module central 2 : 4-BAND PARAMETRIC EQ (ZDF/TPT, 4000 E/G, LF/HF Bell, EQ TRIM).
     */
    void processEqStage(const SSLParameters& params,
                        float& left,
                        float& right) noexcept;

    /**
     * Exécute un module central par son identifiant (0 = DYNAMICS, 1 = COLOR, 2 = EQ).
     */
    void processCentralModuleById(int moduleId,
                                  const SSLParameters& params,
                                  float& left,
                                  float& right,
                                  float extScLeft,
                                  float extScRight,
                                  float& outCompGrDb,
                                  float& outGateGrDb,
                                  float& outDeessGrDb,
                                  float& outEffectiveColor,
                                  float& outCompDetDb,
                                  float& outGateDetDb,
                                  float& outDeessDetDb) noexcept;

    /**
     * Exécute les 3 modules centraux dans l'ordre exact défini par params.moduleOrder (0..5).
     */
    void processAllCentralModulesOrdered(const SSLParameters& params,
                                         float& left,
                                         float& right,
                                         float extScLeft,
                                         float extScRight,
                                         float& outCompGrDb,
                                         float& outGateGrDb,
                                         float& outDeessGrDb,
                                         float& outEffectiveColor,
                                         float& outCompDetDb,
                                         float& outGateDetDb,
                                         float& outDeessDetDb) noexcept;

    void processOutputStage(const SSLParameters& params,
                            float dryLeft,
                            float dryRight,
                            float postLineTrimL,
                            float postLineTrimR,
                            float& left,
                            float& right,
                            float& outAppliedAutoGainDb) noexcept;

    void updateLevelMetersSample(float inL, float inR,
                                 float outL, float outR,
                                 float& outInVuDbL, float& outInVuDbR,
                                 float& outOutVuDbL, float& outOutVuDbR,
                                 float& outInLufsDbL, float& outInLufsDbR,
                                 float& outOutLufsDbL, float& outOutLufsDbR) noexcept;

    [[nodiscard]] static float dbToLinear(float db) noexcept
    {
        return std::pow(10.0f, db * 0.05f);
    }

    [[nodiscard]] static float linearToDb(float lin, float floorDb = -120.0f) noexcept
    {
        const float minLin = std::pow(10.0f, floorDb * 0.05f);
        return 20.0f * std::log10(std::max(std::abs(lin), minLin));
    }

    [[nodiscard]] static float computeCompGainReductionDb(float levelDb, float threshDb, float ratio, bool limiterMode) noexcept;
    [[nodiscard]] static float computeGateAttenuationDb(float levelDb, float gateThreshDb, float gateRangeDb) noexcept;
    [[nodiscard]] static float computeDeessGainReductionDb(float scLevelDb, float deessThreshDb) noexcept;
    [[nodiscard]] static float computeEffectiveEqQ(float nominalQ, float gainDb, bool circuit4000G) noexcept;

    [[nodiscard]] double getSampleRate() const noexcept { return sampleRate_; }

private:
    double sampleRate_ { 48000.0 };
    int    maxBlockSize_ { 512 };

    // Saturateurs ADAA d'ordre 2 en 64 bits (Preamp Sat & Console Color)
    ADAASaturator preampAdaaSaturator_;
    ADAASaturator colorAdaaSaturator_;

    // Filtres DC-Blocker à 5 Hz (post-Preamp Sat et post-Hystérésis Transformateur Color)
    BiquadFilter inputDcBlockerFilter_;
    BiquadFilter dcBlocker5HzFilter_;

    // Filtres HPF & LPF principaux ZDF / TPT
    ZDFStateVariableFilter hpfZdfFilter_;
    ZDFStateVariableFilter lpfZdfFilter_;

    // Filtres HPF & LPF dédiés au Sidechain + SC HPF dédié (20..300 Hz)
    ZDFStateVariableFilter scHpfZdfFilter_;
    ZDFStateVariableFilter scLpfZdfFilter_;
    ZDFStateVariableFilter dedicatedScHpfFilter_;

    // De-Esser : filtre sidechain passe-bande (Q = 2.0) + filtre peaking dynamique ZDF
    BiquadFilter deessSidechainFilter_;
    ZDFStateVariableFilter deessDynamicZdfFilter_;

    // Filtres 4-Band Parametric EQ ZDF / TPT
    ZDFStateVariableFilter lfZdfFilter_;
    ZDFStateVariableFilter lmfZdfFilter_;
    ZDFStateVariableFilter hmfZdfFilter_;
    ZDFStateVariableFilter hfZdfFilter_;
    ZDFStateVariableFilter lmfGyratorDipFilter_;
    ZDFStateVariableFilter hmfGyratorDipFilter_;

    BiquadFilter colorWarmthFilter_;
    BiquadFilter satTonePreFilter_;
    BiquadFilter satTonePostFilter_;
    BiquadFilter crosstalkBandFilter_;

    // États d'hystérésis magnétique de transformateur en double précision (64 bits)
    std::array<double, 2> transformerFluxState_ { 0.0, 0.0 };
    std::array<double, 2> transformerFluxPrev_  { 0.0, 0.0 };
    double transformerFluxAlpha_ { 0.985 };

    // Filtres K-Weighting ITU-R BS.1770-4
    BiquadFilter kWeightInStage1_;
    BiquadFilter kWeightInStage2_;
    BiquadFilter kWeightOutStage1_;
    BiquadFilter kWeightOutStage2_;

    // États des détecteurs d'enveloppe Dynamics
    float compEnvelopeLin_      { 0.0f };
    float compAutoFastCapLin_   { 0.0f };
    float compAutoSlowCapLin_   { 0.0f };
    float gateEnvelopeLin_      { 0.0f };
    float deessEnvelopeLin_     { 0.0f };
    float smoothedCompGrDb_     { 0.0f };
    float lastDeessCutDb_       { 0.0f };
    float lastDeessFreqHz_      { 6500.0f };

    // Intégrateurs temps réel pour AUTO-GAIN dynamique (~300 ms)
    float autoGainRefMeanSq_  { 0.0f };
    float autoGainProcMeanSq_ { 0.0f };
    float smoothedAutoGainDb_ { 0.0f };

    // Intégrateurs temps réel stéréo L/R pour VU (300 ms) et LUFS Momentary (400 ms)
    float vuInMeanSqL_        { 0.0f };
    float vuInMeanSqR_        { 0.0f };
    float vuOutMeanSqL_       { 0.0f };
    float vuOutMeanSqR_       { 0.0f };
    float lufsInMeanSqL_      { 0.0f };
    float lufsInMeanSqR_      { 0.0f };
    float lufsOutMeanSqL_     { 0.0f };
    float lufsOutMeanSqR_     { 0.0f };
    float vuBallisticCoeff_   { 0.999f };
    float lufs400msCoeff_     { 0.999f };

    // Constantes de temps précalculées
    float compAttackCoeff_       { 0.99f };
    float compReleaseCoeff_      { 0.999f };
    float compAutoFastRelCoeff_  { 0.999f };
    float compAutoSlowAttCoeff_  { 0.999f };
    float compAutoSlowRelCoeff_  { 0.9998f };
    float gateAttackCoeff_       { 0.98f };
    float gateReleaseCoeff_      { 0.9995f };
    float deessAttackCoeff_      { 0.98f };
    float deessReleaseCoeff_     { 0.999f };
};

} // namespace kalyx::dsp
