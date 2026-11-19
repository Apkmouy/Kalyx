#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/SSLChannelStripDSP.h"
#include <atomic>
#include <array>
#include <memory>

namespace ParamIDs
{
    inline constexpr const char* inputGain       = "inputGain";
    inline constexpr const char* inputSatDrive   = "input_sat_drive";
    inline constexpr const char* phaseInvert     = "phaseInvert";

    inline constexpr const char* hpfEnabled      = "hpfEnabled";
    inline constexpr const char* hpfFreq         = "hpfFreq";
    inline constexpr const char* hpfQ            = "hpf_q";
    inline constexpr const char* lpfEnabled      = "lpfEnabled";
    inline constexpr const char* lpfFreq         = "lpfFreq";
    inline constexpr const char* lpfQ            = "lpf_q";
    inline constexpr const char* filtPreTrim     = "filt_pre_trim";

    inline constexpr const char* dynEnabled      = "dynEnabled";
    inline constexpr const char* gateThresh      = "gateThresh";
    inline constexpr const char* gateRange       = "gateRange";
    inline constexpr const char* compThresh      = "compThresh";
    inline constexpr const char* compRatio       = "compRatio";
    inline constexpr const char* compAttack      = "compAttack";
    inline constexpr const char* compRelease     = "compRelease";
    inline constexpr const char* compAutoRelease = "comp_auto_rel";
    inline constexpr const char* limiterMode     = "limiterMode";
    inline constexpr const char* makeupGain      = "makeupGain"; // DYN MAKEUP (Section 2)

    inline constexpr const char* extSidechain    = "ext_sc";
    inline constexpr const char* filtersToSc     = "filters_to_sc";
    inline constexpr const char* scListen        = "sc_listen";
    inline constexpr const char* scHpfFreq       = "sc_hpf_freq"; // 20 .. 300 Hz

    inline constexpr const char* deessIn         = "deess_in";
    inline constexpr const char* deessFreq       = "deess_freq";
    inline constexpr const char* deessThresh     = "deess_thresh";

    inline constexpr const char* colorAmount     = "colorAmount";
    inline constexpr const char* colorLinkComp   = "colorLinkComp";
    inline constexpr const char* circuitMode     = "circuit_mode"; // false = 4000 E, true = 4000 G
    inline constexpr const char* satTone         = "sat_tone";     // -100% (Warm) .. +100% (Bright)
    inline constexpr const char* crosstalk       = "crosstalk";    // 0% .. 100%
    inline constexpr const char* colorTrim       = "color_trim";   // -12 .. +12 dB

    inline constexpr const char* eqEnabled       = "eqEnabled";
    inline constexpr const char* eqPreDyn        = "eq_pre_dyn";
    inline constexpr const char* moduleOrder     = "module_order"; // 0..5 : 6 permutations de [DYN, COLOR, EQ]
    inline constexpr const char* lfBell          = "lf_bell";
    inline constexpr const char* lfFreq          = "lfFreq";
    inline constexpr const char* lfGain          = "lfGain";
    inline constexpr const char* lmfFreq         = "lmfFreq";
    inline constexpr const char* lmfGain         = "lmfGain";
    inline constexpr const char* lmfQ            = "lmfQ";
    inline constexpr const char* hmfFreq         = "hmfFreq";
    inline constexpr const char* hmfGain         = "hmfGain";
    inline constexpr const char* hmfQ            = "hmfQ";
    inline constexpr const char* hfBell          = "hf_bell";
    inline constexpr const char* hfFreq          = "hfFreq";
    inline constexpr const char* hfGain          = "hfGain";
    inline constexpr const char* eqTrim          = "eq_trim";      // -12 .. +12 dB

    inline constexpr const char* autoGain        = "autoGain";
    inline constexpr const char* msMode          = "ms_mode";      // 0 = L/R, 1 = M/S (Both), 2 = M/S (Mid), 3 = M/S (Side)
    inline constexpr const char* analogTolerance = "analog_tol";   // 0 .. 100 %
    inline constexpr const char* outputGain      = "outputGain";   // OUT FADER final (Section 5)
    inline constexpr const char* mix             = "mix";          // CONSOLE MIX

    // Paramètres globaux Settings
    inline constexpr const char* oversampling    = "oversampling"; // 0 = 1x (Off), 1 = 2x, 2 = 4x
    inline constexpr const char* meterMode       = "meter_mode";   // 0 = dBFS, 1 = VU (dB), 2 = LUFS
    inline constexpr const char* vuRefDb         = "vu_ref_db";    // -24.0 .. -6.0 dBFS (défaut -18.0 dBFS)
    inline constexpr const char* fftMode         = "fft_mode";     // 0 = POST, 1 = PRE, 2 = OFF
    inline constexpr const char* dayMode         = "day_mode";     // false = Night Mode, true = Day Mode
} // namespace ParamIDs

class KalyxSSLConsoleProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int kFFTOrder = 11;
    static constexpr int kFFTSize  = 1 << kFFTOrder; // 2048 points

    KalyxSSLConsoleProcessor();
    ~KalyxSSLConsoleProcessor() override = default;

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

    int getNumPrograms() override { return 5; }
    int getCurrentProgram() override { return currentProgram_; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    [[nodiscard]] juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts_; }
    [[nodiscard]] static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Télémétrie lock-free (Audio Thread -> GUI Thread) via std::atomic<float>
    [[nodiscard]] kalyx::dsp::SSLMeterSnapshot getMeterSnapshot() const noexcept;
    [[nodiscard]] kalyx::dsp::SSLParameters getCurrentParametersForGUI() const noexcept;
    [[nodiscard]] double getCurrentSampleRate() const noexcept { return currentSampleRate_.load(std::memory_order_relaxed); }

    // Extraction lock-free d'un bloc de 2048 échantillons pour l'analyseur FFT du graphe EQ
    [[nodiscard]] bool pullNextFFTBlock(float* destBuffer2048) noexcept;

    // Gestion du verrouillage de section (Parameter Lock : 0=INPUT, 1=DYNAMICS, 2=COLOR, 3=EQ, 4=OUTPUT)
    void setSectionLocked(int sectionIndex, bool locked) noexcept;
    [[nodiscard]] bool isSectionLocked(int sectionIndex) const noexcept;

    // Réorganisation libre des 3 modules centraux (slot 0, 1, 2 <-> direction -1 ou +1)
    [[nodiscard]] int getModuleOrderIndex() const noexcept;
    void setModuleOrderIndex(int newOrderIdx) noexcept;
    void swapCentralModuleSlots(int slotA, int slotB) noexcept;

    // Gestion Presets d'usine & Comparateur A/B (respectant le verrouillage de section)
    void applyFactoryPreset(int presetIndex);
    void toggleABState();
    void copyActiveToInactiveAB();
    [[nodiscard]] bool isSlotBActive() const noexcept { return isSlotB_; }

private:
    void updateSmoothedTargetsFromAPVTS() noexcept;
    void setParamNormalizedValue(const char* paramID, float plainValue);
    void pushSampleToFFTFifo(float monoSample) noexcept;
    void runZeroInputSilenceVerification(double sampleRate) noexcept;
    [[nodiscard]] static int getParameterSectionIndex(const juce::String& paramID) noexcept;
    void applyValueTreeRespectingLocks(const juce::ValueTree& sourceState);

    juce::AudioProcessorValueTreeState apvts_;

    kalyx::dsp::SSLChannelStripDSP dspBaseEngine_;
    std::array<kalyx::dsp::SSLChannelStripDSP, 3> dspNonlinearEngines_;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler2x_;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler4x_;
    juce::AudioBuffer<float> preAllocatedWorkBuffer_;
    juce::AudioBuffer<float> preAllocatedDryBuffer_;
    juce::AudioBuffer<float> preAllocatedRefBuffer_;
    int activeOversamplingIndex_ { 0 };

    // FIFO lock-free pour l'analyseur FFT
    std::array<float, kFFTSize> fftFifo_ {};
    std::array<float, kFFTSize> fftTransferBlock_ {};
    int fftFifoIndex_ { 0 };
    std::atomic<bool> nextFFTBlockReady_ { false };

    // Verrouillage des 5 sections (Parameter Lock)
    std::array<std::atomic<bool>, 5> sectionLocked_ { false, false, false, false, false };

    // Comparateur A/B & Presets
    juce::ValueTree slotAState_;
    juce::ValueTree slotBState_;
    bool isSlotB_ { false };
    int currentProgram_ { 0 };

    // Pointeurs atomiques bruts pré-cachés dans le constructeur
    std::atomic<float>* pInputGain_       { nullptr };
    std::atomic<float>* pInputSatDrive_   { nullptr };
    std::atomic<float>* pPhaseInvert_     { nullptr };
    std::atomic<float>* pHpfEnabled_      { nullptr };
    std::atomic<float>* pHpfFreq_         { nullptr };
    std::atomic<float>* pHpfQ_            { nullptr };
    std::atomic<float>* pLpfEnabled_      { nullptr };
    std::atomic<float>* pLpfFreq_         { nullptr };
    std::atomic<float>* pLpfQ_            { nullptr };
    std::atomic<float>* pFiltPreTrim_     { nullptr };
    std::atomic<float>* pDynEnabled_      { nullptr };
    std::atomic<float>* pGateThresh_      { nullptr };
    std::atomic<float>* pGateRange_       { nullptr };
    std::atomic<float>* pCompThresh_      { nullptr };
    std::atomic<float>* pCompRatio_       { nullptr };
    std::atomic<float>* pCompAttack_      { nullptr };
    std::atomic<float>* pCompRelease_     { nullptr };
    std::atomic<float>* pCompAutoRelease_ { nullptr };
    std::atomic<float>* pLimiterMode_     { nullptr };
    std::atomic<float>* pMakeupGain_      { nullptr };
    std::atomic<float>* pExtSidechain_    { nullptr };
    std::atomic<float>* pFiltersToSc_     { nullptr };
    std::atomic<float>* pScListen_        { nullptr };
    std::atomic<float>* pScHpfFreq_       { nullptr };
    std::atomic<float>* pDeessIn_         { nullptr };
    std::atomic<float>* pDeessFreq_       { nullptr };
    std::atomic<float>* pDeessThresh_     { nullptr };
    std::atomic<float>* pColorAmount_     { nullptr };
    std::atomic<float>* pColorLinkComp_   { nullptr };
    std::atomic<float>* pCircuitMode_     { nullptr };
    std::atomic<float>* pSatTone_         { nullptr };
    std::atomic<float>* pCrosstalk_       { nullptr };
    std::atomic<float>* pColorTrim_       { nullptr };
    std::atomic<float>* pEqEnabled_       { nullptr };
    std::atomic<float>* pEqPreDyn_        { nullptr };
    std::atomic<float>* pModuleOrder_     { nullptr };
    std::atomic<float>* pLfBell_          { nullptr };
    std::atomic<float>* pLfFreq_          { nullptr };
    std::atomic<float>* pLfGain_          { nullptr };
    std::atomic<float>* pLmfFreq_         { nullptr };
    std::atomic<float>* pLmfGain_         { nullptr };
    std::atomic<float>* pLmfQ_            { nullptr };
    std::atomic<float>* pHmfFreq_         { nullptr };
    std::atomic<float>* pHmfGain_         { nullptr };
    std::atomic<float>* pHmfQ_            { nullptr };
    std::atomic<float>* pHfBell_          { nullptr };
    std::atomic<float>* pHfFreq_          { nullptr };
    std::atomic<float>* pHfGain_          { nullptr };
    std::atomic<float>* pEqTrim_          { nullptr };
    std::atomic<float>* pAutoGain_        { nullptr };
    std::atomic<float>* pMsMode_          { nullptr };
    std::atomic<float>* pAnalogTolerance_ { nullptr };
    std::atomic<float>* pOutputGain_      { nullptr };
    std::atomic<float>* pMix_             { nullptr };
    std::atomic<float>* pOversampling_    { nullptr };
    std::atomic<float>* pMeterMode_       { nullptr };
    std::atomic<float>* pVuRefDb_         { nullptr };
    std::atomic<float>* pFftMode_         { nullptr };
    std::atomic<float>* pDayMode_         { nullptr };

    using LinearSmoothed = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;
    LinearSmoothed smoothInputGain_;
    LinearSmoothed smoothInputSatDrive_;
    LinearSmoothed smoothHpfFreq_;
    LinearSmoothed smoothHpfQ_;
    LinearSmoothed smoothLpfFreq_;
    LinearSmoothed smoothLpfQ_;
    LinearSmoothed smoothFiltPreTrim_;
    LinearSmoothed smoothGateThresh_;
    LinearSmoothed smoothGateRange_;
    LinearSmoothed smoothCompThresh_;
    LinearSmoothed smoothCompRatio_;
    LinearSmoothed smoothCompAttack_;
    LinearSmoothed smoothCompRelease_;
    LinearSmoothed smoothMakeupGain_;
    LinearSmoothed smoothScHpfFreq_;
    LinearSmoothed smoothDeessFreq_;
    LinearSmoothed smoothDeessThresh_;
    LinearSmoothed smoothColorAmount_;
    LinearSmoothed smoothSatTone_;
    LinearSmoothed smoothCrosstalk_;
    LinearSmoothed smoothColorTrim_;
    LinearSmoothed smoothLfFreq_;
    LinearSmoothed smoothLfGain_;
    LinearSmoothed smoothLmfFreq_;
    LinearSmoothed smoothLmfGain_;
    LinearSmoothed smoothLmfQ_;
    LinearSmoothed smoothHmfFreq_;
    LinearSmoothed smoothHmfGain_;
    LinearSmoothed smoothHmfQ_;
    LinearSmoothed smoothHfFreq_;
    LinearSmoothed smoothHfGain_;
    LinearSmoothed smoothEqTrim_;
    LinearSmoothed smoothAnalogTolerance_;
    LinearSmoothed smoothOutputGain_;
    LinearSmoothed smoothMix_;

    std::atomic<float> meterInputPeakDbL_      { -100.0f };
    std::atomic<float> meterInputPeakDbR_      { -100.0f };
    std::atomic<float> meterOutputPeakDbL_     { -100.0f };
    std::atomic<float> meterOutputPeakDbR_     { -100.0f };
    std::atomic<float> meterInputVuDbL_        { -100.0f };
    std::atomic<float> meterInputVuDbR_        { -100.0f };
    std::atomic<float> meterOutputVuDbL_       { -100.0f };
    std::atomic<float> meterOutputVuDbR_       { -100.0f };
    std::atomic<float> meterInputLufsDbL_      { -100.0f };
    std::atomic<float> meterInputLufsDbR_      { -100.0f };
    std::atomic<float> meterOutputLufsDbL_     { -100.0f };
    std::atomic<float> meterOutputLufsDbR_     { -100.0f };

    std::atomic<float> meterCompDetectorDb_    { -100.0f };
    std::atomic<float> meterGateDetectorDb_    { -100.0f };
    std::atomic<float> meterDeessDetectorDb_   { -100.0f };

    std::atomic<float> meterCompGrDb_          { 0.0f };
    std::atomic<float> meterGateGrDb_          { 0.0f };
    std::atomic<float> meterDeessGrDb_         { 0.0f };
    std::atomic<float> meterDynamicColorDrive_ { 0.0f };
    std::atomic<float> meterAutoGainDb_        { 0.0f };
    std::atomic<double> currentSampleRate_     { 48000.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KalyxSSLConsoleProcessor)
};
