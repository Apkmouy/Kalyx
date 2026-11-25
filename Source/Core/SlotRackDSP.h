#pragma once

#include "ConsoleModuleBase.h"
#include "../DSPShared/ADAASaturator.h"
#include "../DSPShared/HysteresisStage.h"
#include "../DSPShared/ZDFFilters.h"
#include "../DSPShared/StereoAndMeters.h"
#include "../Consoles/UA1176/UA1176DSP.h"
#include "../Consoles/LA2A/LA2ADSP.h"
#include "../Consoles/Distressor/DistressorDSP.h"
#include "../Consoles/Fairchild/FairchildDSP.h"
#include "../Consoles/KalyxModern/SurgicalEQ2UDSP.h"
#include "../Consoles/KalyxModern/PrecisionDyn2UDSP.h"
#include "../Consoles/KalyxModern/HarmonicSculptorDSP.h"
#include "../Consoles/KalyxModern/StereoPhaseDSP.h"
#include <array>

namespace kalyx::core
{

/**
 * Moteur DSP 64 bits de la tranche fixe INPUT (Préamplis 5 modèles + HPF/LPF ZDF + Phase).
 */
class InputStripDSP
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void processSample(float& l, float& r,
                       float& scL, float& scR,
                       const InputStripParameters& p,
                       float tolBiasL, float tolBiasR) noexcept;

private:
    double sampleRate_ { 44100.0 };
    dsp::ADAASaturator satL_, satR_;
    dsp::HysteresisStage hystL_, hystR_;
    dsp::ZDFStateVariableFilter midFocusL_, midFocusR_;
    dsp::ZDFStateVariableFilter hpfL_, hpfR_;
    dsp::ZDFStateVariableFilter lpfL_, lpfR_;
};

/**
 * Moteur DSP 64 bits autonome pour un emplacement Série 500 (Slot A à E).
 * Pré-alloue tous les états internes dans prepare() -> Zéro allocation dans processBlock().
 * Intègre un Crossfade linéaire sans clic de 15 ms lors d'un changement de module.
 */
class SlotUnitDSP
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void notifyModuleSelection(int newModuleId) noexcept;

    /**
     * Traite un échantillon stéréo dans ce slot et retourne la réduction de gain (dB >= 0) produite.
     */
    [[nodiscard]] float processSample(float& l, float& r,
                                      float scL, float scR,
                                      const SlotParamArray& params,
                                      bool extSidechain,
                                      float tolBiasL, float tolBiasR,
                                      float tolFreqL, float tolFreqR,
                                      bool autoGainOn = false) noexcept;

    [[nodiscard]] float processSample2U(float& l, float& r,
                                        const SlotParamArray2U& params2U,
                                        const std::array<uint32_t, 6>& bandMeta,
                                        bool autoGainOn = false) noexcept;

    [[nodiscard]] int getActiveModuleId() const noexcept { return activeModuleId_; }
    [[nodiscard]] float getLastDriveLevelDb() const noexcept { return lastDriveLevelDb_; }
    [[nodiscard]] float getDistressorThdPct() const noexcept { return distressorDsp_.getInstantThdPct(); }
    [[nodiscard]] float getHarmonicSculptorThdPct() const noexcept { return harmonicSculptorDsp_.getInstantThdPct(); }
    [[nodiscard]] float getStereoCorrelation() const noexcept { return stereoPhaseDsp_.getCorrelation(); }
    [[nodiscard]] const kalyx::consoles::PrecisionDyn2UDSP& getPrecisionDyn2U() const noexcept { return precisionDyn2uDsp_; }
    [[nodiscard]] float getLastAutoGainDb() const noexcept { return lastAutoGainDb_; }

    // Helpers analytiques partagés avec l'affichage de courbe EQ & façades
    [[nodiscard]] static float computeSslEqQ(float baseQ, float gainDb, bool is4000G) noexcept;
    [[nodiscard]] static float computeApi550BQ(float baseQ, float gainDb) noexcept;
    [[nodiscard]] static float quantizeApi550BGain(float gainDb, bool stepped) noexcept;
    [[nodiscard]] static float quantizeApi550BFreq(int bandIdx, float freqHz, bool stepped) noexcept;
    [[nodiscard]] static float quantizeApi2500RelMs(float relMs, bool isFixed) noexcept;
    [[nodiscard]] static float quantizeNeve1073LowFreq(float freqHz, bool stepped) noexcept;
    [[nodiscard]] static float quantizeNeve1073MidFreq(float freqHz, bool stepped) noexcept;
    [[nodiscard]] static float quantizeNeve1073HpfFreq(float freqHz, bool stepped) noexcept;

private:
    [[nodiscard]] float processModuleById(int modId,
                                          float& l, float& r,
                                          float scL, float scR,
                                          const SlotParamArray& params,
                                          bool extSidechain,
                                          float tolBiasL, float tolBiasR,
                                          float tolFreqL, float tolFreqR) noexcept;

    void  procSlotPreamp(float& l, float& r, const SlotParamArray& p, int preampStyle, float tolBiasL, float tolBiasR) noexcept;
    float procUtilityGateDeEsser(float& l, float& r, float detL, float detR, const SlotParamArray& p) noexcept;
    float procSslComp(float& l, float& r, float detL, float detR, const SlotParamArray& p) noexcept;
    float procApi2500Comp(float& l, float& r, float detL, float detR, const SlotParamArray& p) noexcept;
    float procNeve5254Comp(float& l, float& r, float detL, float detR, const SlotParamArray& p) noexcept;
    void  proc4BandEq(float& l, float& r, const SlotParamArray& p, bool isApi550B, float tolFreqL, float tolFreqR) noexcept;
    void  procNeve1073Eq(float& l, float& r, const SlotParamArray& p, float tolFreqL, float tolFreqR) noexcept;
    void  procConsoleColor(float& l, float& r, const SlotParamArray& p, float tolBiasL, float tolBiasR) noexcept;
    float procPrecisionLimiter(float& l, float& r, const SlotParamArray& p) noexcept;

    double sampleRate_    { 44100.0 };
    int activeModuleId_   { static_cast<int>(SlotModuleId::EmptyBypass) };
    int previousModuleId_ { static_cast<int>(SlotModuleId::EmptyBypass) };
    int fadeRemaining_    { 0 };
    int fadeTotal_        { 1 };

    // État Preamp de slot
    InputStripDSP slotPreampDsp_;

    // États Utility Gate / De-Esser
    double gateEnv_       { 1.0 };
    int    gateHoldCount_ { 0 };
    bool   gateOpen_      { true };
    double deessEnvDb_    { 0.0 };
    dsp::ZDFStateVariableFilter deessScL_, deessScR_, deessNotchL_, deessNotchR_;

    // États Compresseurs (SSL / API 2500 / Neve 5254)
    dsp::ZDFStateVariableFilter scHpfL_, scHpfR_;
    double compEnvADb_    { 0.0 };
    double compEnvBDb_    { 0.0 };
    double compFastRc_    { 0.0 };
    double compSlowRc_    { 0.0 };
    double apiPrevGainL_  { 1.0 };
    double apiPrevGainR_  { 1.0 };
    double apiThrustLpL_  { 0.0 };
    double apiThrustLpR_  { 0.0 };
    dsp::ADAASaturator diodeSatL_, diodeSatR_;

    // États Égaliseurs (SSL E/G, API 550B, Neve 1073)
    dsp::ZDFStateVariableFilter eqHpfL_, eqHpfR_;
    dsp::ZDFStateVariableFilter lfL_, lfR_, lmfL_, lmfR_, lmfDipL_, lmfDipR_;
    dsp::ZDFStateVariableFilter hmfL_, hmfR_, hmfDipL_, hmfDipR_, hfL_, hfR_;

    // États Console Color
    dsp::ADAASaturator colSatL_, colSatR_;
    dsp::HysteresisStage colHystL_, colHystR_;
    dsp::ZDFStateVariableFilter colToneLowL_, colToneLowR_, colToneHighL_, colToneHighR_;
    dsp::ZDFStateVariableFilter colSpecL_, colSpecR_;
    dsp::ZDFStateVariableFilter xtalkHpfL_, xtalkHpfR_;

    // États Precision Peak Limiter & télémétrie interne
    double limEnvDb_  { 0.0 };
    double limFastRc_ { 0.0 };
    double limSlowRc_ { 0.0 };
    float  lastDriveLevelDb_ { -100.0f };
    float  lastAutoGainDb_   { 0.0f };
    dsp::StereoMetersAndAutoGain slotAutoGain_;

    // Compresseur FET UA 1176, Opto LA-2A, VCA Distressor, Vari-Mu Fairchild 660/670 & Surgical EQ 2U (pré-alloués dans prepare())
    kalyx::consoles::UA1176DSP      ua1176Dsp_;
    kalyx::consoles::LA2ADSP        la2aDsp_;
    kalyx::consoles::DistressorDSP      distressorDsp_;
    kalyx::consoles::FairchildDSP       fairchildDsp_;
    kalyx::consoles::SurgicalEQ2UDSP    surgicalEq2uDsp_;
    kalyx::consoles::PrecisionDyn2UDSP  precisionDyn2uDsp_;
    kalyx::consoles::HarmonicSculptorDSP harmonicSculptorDsp_;
    kalyx::consoles::StereoPhaseDSP     stereoPhaseDsp_;
};

} // namespace kalyx::core
