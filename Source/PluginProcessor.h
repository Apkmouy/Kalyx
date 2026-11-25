#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Core/ConsoleModuleBase.h"
#include "Core/ConsoleRegistry.h"
#include "Core/SlotRackDSP.h"
#include "Core/PresetManager.h"
#include "DSPShared/StereoAndMeters.h"
#include <atomic>
#include <array>
#include <memory>

namespace ParamIDs
{
    // Tranche Fixe 1 : INPUT
    inline constexpr const char* inTrim        = "in_trim";
    inline constexpr const char* inPreampModel = "in_preamp_model";
    inline constexpr const char* inDrive       = "in_drive";
    inline constexpr const char* inPhase       = "in_phase";
    inline constexpr const char* inHpfEn       = "in_hpf_en";
    inline constexpr const char* inHpfFreq     = "in_hpf_freq";
    inline constexpr const char* inLpfEn       = "in_lpf_en";
    inline constexpr const char* inLpfFreq     = "in_lpf_freq";
    inline constexpr const char* inFiltToSc    = "in_filt_to_sc";
    inline constexpr const char* inExtSc       = "in_ext_sc";
    inline constexpr const char* inScListen    = "in_sc_listen";

    // Tranche Fixe 7 : MASTER & Global
    inline constexpr const char* masterAnalogTol = "master_analog_tol";
    inline constexpr const char* masterMix       = "master_mix";
    inline constexpr const char* masterOutGain   = "master_out_gain";
    inline constexpr const char* masterVuMode    = "master_vu_mode"; // 0=IN, 1=OUT, 2=GR
    inline constexpr const char* masterMsMode    = "master_ms_mode"; // 0=L/R, 1=M+S, 2=MID, 3=SIDE
    inline constexpr const char* chassisTheme    = "chassis_theme";  // 0=Night, 1=Day, 2=Kalyx Heritage
    inline constexpr const char* vuRefDb         = "vu_ref_db";
    inline constexpr const char* oversampling    = "oversampling";
    inline constexpr const char* fftMode         = "fft_mode";

    // Helpers d'identifiants APVTS des 5 slots (slotA..slotE, p1..p18, autogain, order)
    inline juce::String slotModuleId(int slotIdx)
    {
        static constexpr char kLetters[5] = { 'A', 'B', 'C', 'D', 'E' };
        return juce::String("slot") + kLetters[std::clamp(slotIdx, 0, 4)] + "_module_id";
    }

    inline juce::String slotParamId(int slotIdx, int paramOneBased)
    {
        static constexpr char kLetters[5] = { 'A', 'B', 'C', 'D', 'E' };
        return juce::String("slot") + kLetters[std::clamp(slotIdx, 0, 4)] + "_p" + juce::String(std::clamp(paramOneBased, 1, 18));
    }

    inline juce::String slotAutogainId(int slotIdx)
    {
        static constexpr char kLetters[5] = { 'A', 'B', 'C', 'D', 'E' };
        return juce::String("slot") + kLetters[std::clamp(slotIdx, 0, 4)] + "_autogain";
    }

    inline juce::String slotOrderId(int visualPos)
    {
        return "slot_order_" + juce::String(std::clamp(visualPos, 0, 4));
    }
} // namespace ParamIDs

class KalyxHybridConsoleProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int kFFTOrder = 11;
    static constexpr int kFFTSize  = 1 << kFFTOrder; // 2048 pts

    KalyxHybridConsoleProcessor();
    ~KalyxHybridConsoleProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Gain Reduction cumulée (dB) exposée pour REAPER / DAW
    [[nodiscard]] float getGainReductionForHost() const noexcept { return mCumGrDb_.load(std::memory_order_relaxed); }

    [[nodiscard]] juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts_; }
    [[nodiscard]] kalyx::core::PresetManager& getPresetManager() noexcept { return presetManager_; }
    [[nodiscard]] kalyx::dsp::StudioMeterSnapshot getMeterSnapshot() const noexcept;
    [[nodiscard]] double getCurrentSampleRate() const noexcept { return currentSampleRate_.load(std::memory_order_relaxed); }

    // Télémétrie lock-free par slot (GR 6-LEDs, Harmonic Drive 5-LEDs, Peak LED)
    [[nodiscard]] float getSlotGrDb(int slotIdx) const noexcept
    {
        return mSlotGrDb_[static_cast<std::size_t>(std::clamp(slotIdx, 0, 4))].load(std::memory_order_relaxed);
    }
    [[nodiscard]] float getSlotDriveDb(int slotIdx) const noexcept
    {
        return mSlotDriveDb_[static_cast<std::size_t>(std::clamp(slotIdx, 0, 4))].load(std::memory_order_relaxed);
    }
    [[nodiscard]] float getSlotPeakDb(int slotIdx) const noexcept
    {
        return mSlotPeakDb_[static_cast<std::size_t>(std::clamp(slotIdx, 0, 4))].load(std::memory_order_relaxed);
    }
    [[nodiscard]] float getSlotThdPct(int slotIdx) const noexcept
    {
        return mSlotThdPct_[static_cast<std::size_t>(std::clamp(slotIdx, 0, 4))].load(std::memory_order_relaxed);
    }
    [[nodiscard]] float getSlotStereoCorrelation(int slotIdx) const noexcept
    {
        return mSlotCorr_[static_cast<std::size_t>(std::clamp(slotIdx, 0, 4))].load(std::memory_order_relaxed);
    }
    void copyPrecisionDynScope(int slotIdx, std::array<kalyx::consoles::PrecisionDyn2UDSP::ScopeSample, kalyx::consoles::PrecisionDyn2UDSP::kScopeSize>& dest) const noexcept;

    bool pullNextFFTBlock(float* dest2048) noexcept;
    bool pullNextPreFFTBlock(float* dest2048) noexcept;

    // Gestion de l'ordre des 5 slots (Slot A..E) & chargement de module avec valeurs par défaut (1U & 2U)
    [[nodiscard]] std::array<int, 5> getSlotOrder() const noexcept;
    void swapVisualSlots(int posA, int posB);
    void move2USlot(int pos, int delta);
    void loadModuleIntoSlotWithDefaults(int slotIdx, kalyx::core::SlotModuleId modId);

    // Résolution 2U Double-Largeur (36 paramètres combinés)
    [[nodiscard]] int getPartnerSlotFor2U(int hostSlotIdx) const noexcept;
    [[nodiscard]] bool isSlotLinkedTo2U(int slotIdx) const noexcept;
    [[nodiscard]] kalyx::core::SlotParamArray2U getSlotParams2UForGUI(int hostSlotIdx) const noexcept;

    [[nodiscard]] uint32_t getSurgicalBandMeta(int slotIdx, int bandIdx) const noexcept;
    void setSurgicalBandMeta(int slotIdx, int bandIdx, uint32_t packed) noexcept;

    // Lecture rapide des paramètres d'un slot pour la GUI (courbe EQ interactive)
    [[nodiscard]] int getSlotModuleId(int slotIdx) const noexcept;
    [[nodiscard]] kalyx::core::SlotParamArray getSlotParamsForGUI(int slotIdx) const noexcept;

    // Verrouillage de section (0=Input, 1..5=Slot A..E, 6=Master)
    void setSectionLocked(int idx, bool locked) noexcept;
    [[nodiscard]] bool isSectionLocked(int idx) const noexcept;

    // Presets d'usine & A/B
    void applyFactoryPreset(int factoryId);
    void toggleABState();
    void copyActiveToInactiveAB();
    [[nodiscard]] bool isSlotBActive() const noexcept { return activeABSlot_ == 1; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void cacheRawParameterPointers() noexcept;
    void processSingleSampleBlock(float* left, float* right,
                                  const float* scLeft, const float* scRight,
                                  int numSamples,
                                  const kalyx::core::InputStripParameters& inP,
                                  const kalyx::core::MasterStripParameters& mastP,
                                  const std::array<int, 5>& slotOrder,
                                  const std::array<int, 5>& slotModIds,
                                  const std::array<kalyx::core::SlotParamArray, 5>& slotParams,
                                  const std::array<bool, 5>& slotAutogain,
                                  double effectiveFs) noexcept;
    void pushFFTSample(float sample) noexcept;
    void pushPreFFTSample(float sample) noexcept;
    void runZeroInputSilenceVerification(double sampleRate) noexcept;

    juce::AudioProcessorValueTreeState apvts_;
    kalyx::core::PresetManager presetManager_;

    // Moteurs DSP pré-alloués dans prepareToPlay() (Zéro allocation dans processBlock)
    kalyx::core::InputStripDSP inputStripDsp_;
    std::array<kalyx::core::SlotUnitDSP, 5> slotDsps_;
    kalyx::dsp::StereoMetersAndAutoGain metersAndAutoGain_;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler2x_;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler4x_;

    // Pointeurs bruts atomiques APVTS (lock-free)
    std::atomic<float>* pInTrim_        { nullptr };
    std::atomic<float>* pInPreamp_      { nullptr };
    std::atomic<float>* pInDrive_       { nullptr };
    std::atomic<float>* pInPhase_       { nullptr };
    std::atomic<float>* pInHpfEn_       { nullptr };
    std::atomic<float>* pInHpfFreq_     { nullptr };
    std::atomic<float>* pInLpfEn_       { nullptr };
    std::atomic<float>* pInLpfFreq_     { nullptr };
    std::atomic<float>* pInFiltToSc_    { nullptr };
    std::atomic<float>* pInExtSc_       { nullptr };
    std::atomic<float>* pInScListen_    { nullptr };

    std::atomic<float>* pMastAnalogTol_ { nullptr };
    std::atomic<float>* pMastMix_       { nullptr };
    std::atomic<float>* pMastOutGain_   { nullptr };
    std::atomic<float>* pMastVuMode_    { nullptr };
    std::atomic<float>* pMastMsMode_    { nullptr };
    std::atomic<float>* pChassisTheme_  { nullptr };
    std::atomic<float>* pVuRefDb_       { nullptr };
    std::atomic<float>* pOversampling_  { nullptr };
    std::atomic<float>* pFftMode_       { nullptr };

    std::array<std::atomic<float>*, 5> pSlotOrder_ {};
    std::array<std::atomic<float>*, 5> pSlotModId_ {};
    std::array<std::atomic<float>*, 5> pSlotAutogain_ {};
    std::array<std::array<std::atomic<float>*, 18>, 5> pSlotParams_ {};

    // Télémétrie atomique Audio -> GUI (300 ms IEC 60268-17 + GR cumulée)
    std::atomic<float> mInPeakL_   { -100.0f }, mInPeakR_   { -100.0f };
    std::atomic<float> mOutPeakL_  { -100.0f }, mOutPeakR_  { -100.0f };
    std::atomic<float> mInVu300L_  { -100.0f }, mInVu300R_  { -100.0f };
    std::atomic<float> mOutVu300L_ { -100.0f }, mOutVu300R_ { -100.0f };
    std::atomic<float> mCumGrDb_   { 0.0f },    mCumGrVuDb_ { 0.0f };
    std::atomic<float> mAutoGainDb_{ 0.0f };
    std::array<std::atomic<float>, 5> mSlotGrDb_    {};
    std::array<std::atomic<float>, 5> mSlotDriveDb_ {};
    std::array<std::atomic<float>, 5> mSlotPeakDb_  {};
    std::array<std::atomic<float>, 5> mSlotThdPct_  {};
    std::array<std::atomic<float>, 5> mSlotCorr_    {};
    std::atomic<double> currentSampleRate_ { 44100.0 };

    // Tampon FFT lock-free (Post & Pre)
    std::array<float, kFFTSize> fftAccum_ {};
    std::array<float, kFFTSize> fftReady_ {};
    int fftWriteIdx_ { 0 };
    std::atomic<bool> fftBlockReady_ { false };

    std::array<float, kFFTSize> preFftAccum_ {};
    std::array<float, kFFTSize> preFftReady_ {};
    int preFftWriteIdx_ { 0 };
    std::atomic<bool> preFftBlockReady_ { false };

    // Verrous de section (7 : Input, Slot A..E, Master) & A/B
    std::array<std::atomic<bool>, 7> sectionLocks_ {};
    std::array<std::array<std::atomic<uint32_t>, 6>, 5> surgicalBandMeta_ {};
    int activeABSlot_ { 0 };
    juce::ValueTree stateA_;
    juce::ValueTree stateB_;

    juce::AudioBuffer<float> scScratchBuffer_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KalyxHybridConsoleProcessor)
};
