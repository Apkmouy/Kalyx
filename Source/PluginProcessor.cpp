#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace
{
    inline float toDbClamped(float linear) noexcept
    {
        return (linear > 1.0e-5f) ? (20.0f * std::log10(linear)) : -100.0f;
    }

    inline void setParamPlain(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float plainVal)
    {
        if (auto* param = apvts.getParameter(id))
            param->setValueNotifyingHost(param->convertTo0to1(plainVal));
    }
} // namespace

KalyxHybridConsoleProcessor::KalyxHybridConsoleProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "KALYX_HYBRID_5SLOT_PARAMS", createParameterLayout()),
      presetManager_(apvts_)
{
    for (auto& l : sectionLocks_)
        l.store(false, std::memory_order_relaxed);

    const auto defMeta = kalyx::consoles::SurgicalEQ2UDSP::getDefaultBandMeta();
    for (int s = 0; s < 5; ++s)
        for (int b = 0; b < 6; ++b)
            surgicalBandMeta_[static_cast<std::size_t>(s)][static_cast<std::size_t>(b)].store(defMeta[static_cast<std::size_t>(b)], std::memory_order_relaxed);

    cacheRawParameterPointers();
    presetManager_.setFactoryPresetCallback([this](int fid) { applyFactoryPreset(fid); });

    // Configuration Init par défaut (Slot A=Gate/DeEsser, B=SSL E Comp, C=SSL E EQ, D=Color, E=Limiter)
    applyFactoryPreset(0);
    stateA_ = apvts_.copyState();
    stateB_ = apvts_.copyState();
}

juce::AudioProcessorValueTreeState::ParameterLayout KalyxHybridConsoleProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.reserve(130);

    auto addChoice = [&](const juce::String& id, const juce::String& name, const juce::StringArray& choices, int defIdx)
    {
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(id, 1), name, choices, defIdx));
    };

    auto addBool = [&](const juce::String& id, const juce::String& name, bool defVal)
    {
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(id, 1), name, defVal));
    };

    auto addFloat = [&](const juce::String& id, const juce::String& name,
                        float minV, float maxV, float step, float skew, float defV,
                        const juce::String& suffix = {})
    {
        juce::NormalisableRange<float> r(minV, maxV, step);
        r.setSkewForCentre(skew);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(id, 1), name, r, defV,
            juce::AudioParameterFloatAttributes().withLabel(suffix)));
    };

    auto addLinFloat = [&](const juce::String& id, const juce::String& name,
                           float minV, float maxV, float step, float defV,
                           const juce::String& suffix = {})
    {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(id, 1), name,
            juce::NormalisableRange<float>(minV, maxV, step), defV,
            juce::AudioParameterFloatAttributes().withLabel(suffix)));
    };

    // -------------------------------------------------------------------------
    // Tranche Fixe 1 : INPUT
    // -------------------------------------------------------------------------
    addLinFloat(ParamIDs::inTrim,        "Input Trim",    -24.0f, 24.0f, 0.1f, 0.0f, " dB");
    addChoice  (ParamIDs::inPreampModel, "Preamp Model",
                { "SSL 4000 E", "SSL 4000 G", "API 1608 (2520)", "Neve 1073 (Marinair)", "Kalyx Clean" }, 0);
    addLinFloat(ParamIDs::inDrive,       "Preamp Drive",   0.0f, 100.0f, 0.5f, 0.0f, " %");
    addBool    (ParamIDs::inPhase,       "Phase Invert",   false);
    addBool    (ParamIDs::inHpfEn,       "HPF In",         false);
    addFloat   (ParamIDs::inHpfFreq,     "HPF Freq",      20.0f, 500.0f, 1.0f, 100.0f, 40.0f, " Hz");
    addBool    (ParamIDs::inLpfEn,       "LPF In",         false);
    addFloat   (ParamIDs::inLpfFreq,     "LPF Freq",    2000.0f, 20000.0f, 10.0f, 7500.0f, 18000.0f, " Hz");
    addBool    (ParamIDs::inFiltToSc,    "Filters To SC",  false);
    addBool    (ParamIDs::inExtSc,       "Ext Sidechain",  false);
    addBool    (ParamIDs::inScListen,    "SC Listen",      false);

    // -------------------------------------------------------------------------
    // 5 Emplacements Série 500 Interchangeables (Slot A..E : 18 paramètres normalisés chacun)
    // -------------------------------------------------------------------------
    static constexpr int kDefaultSlotMods[5] = {
        static_cast<int>(kalyx::core::SlotModuleId::UtilityGateDeEsser), // Slot A
        static_cast<int>(kalyx::core::SlotModuleId::SSL4000Comp),        // Slot B
        static_cast<int>(kalyx::core::SlotModuleId::SSL4000EQ),          // Slot C
        static_cast<int>(kalyx::core::SlotModuleId::ConsoleColor),       // Slot D
        static_cast<int>(kalyx::core::SlotModuleId::PrecisionLimiter)    // Slot E
    };

    const auto modNames = kalyx::core::ModuleRegistry::getDisplayNames();
    for (int s = 0; s < 5; ++s)
    {
        addChoice(ParamIDs::slotOrderId(s), "Slot Order " + juce::String(s + 1),
                  { "Slot A", "Slot B", "Slot C", "Slot D", "Slot E" }, s);

        addChoice(ParamIDs::slotModuleId(s), "Slot " + juce::String::charToString(static_cast<juce::juce_wchar>('A' + s)) + " Module",
                  modNames, kDefaultSlotMods[s]);

        addBool(ParamIDs::slotAutogainId(s),
                "Slot " + juce::String::charToString(static_cast<juce::juce_wchar>('A' + s)) + " Auto-Gain",
                false);

        const auto defaults = kalyx::core::ModuleRegistry::getDefaultParamsForModule(
            static_cast<kalyx::core::SlotModuleId>(kDefaultSlotMods[s]));

        for (int p = 1; p <= 18; ++p)
        {
            const juce::String pid = ParamIDs::slotParamId(s, p);
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(pid, 1), pid,
                juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f),
                defaults[static_cast<std::size_t>(p - 1)]));
        }
    }

    // -------------------------------------------------------------------------
    // Tranche Fixe 7 : MASTER & Global
    // -------------------------------------------------------------------------
    addLinFloat(ParamIDs::masterAnalogTol, "Analog Tolerance", 0.0f, 100.0f, 1.0f, 25.0f, " %");
    addLinFloat(ParamIDs::masterMix,       "Console Mix",      0.0f, 100.0f, 0.5f, 100.0f, " %");
    addLinFloat(ParamIDs::masterOutGain,   "Output Fader",   -24.0f, 12.0f, 0.1f, 0.0f, " dB");
    addChoice  (ParamIDs::masterVuMode,    "VU Mode",         { "IN", "OUT", "GR" }, 1);
    addChoice  (ParamIDs::masterMsMode,    "Stereo Matrix",   { "LR STEREO", "M/S (M+S)", "M/S (MID)", "M/S (SIDE)" }, 0);
    addChoice  (ParamIDs::chassisTheme,    "Chassis Finish",  { "NIGHT", "DAY", "KALYX HERITAGE" }, 0);
    addLinFloat(ParamIDs::vuRefDb,         "0 VU Reference", -24.0f, 0.0f, 0.5f, -18.0f, " dBFS");
    addChoice  (ParamIDs::oversampling,    "Oversampling",    { "1x (Off)", "2x IIR", "4x IIR" }, 0);
    addChoice  (ParamIDs::fftMode,         "FFT Mode",        { "POST EQ", "PRE EQ", "FFT OFF" }, 0);

    return { params.begin(), params.end() };
}

void KalyxHybridConsoleProcessor::cacheRawParameterPointers() noexcept
{
    pInTrim_        = apvts_.getRawParameterValue(ParamIDs::inTrim);
    pInPreamp_      = apvts_.getRawParameterValue(ParamIDs::inPreampModel);
    pInDrive_       = apvts_.getRawParameterValue(ParamIDs::inDrive);
    pInPhase_       = apvts_.getRawParameterValue(ParamIDs::inPhase);
    pInHpfEn_       = apvts_.getRawParameterValue(ParamIDs::inHpfEn);
    pInHpfFreq_     = apvts_.getRawParameterValue(ParamIDs::inHpfFreq);
    pInLpfEn_       = apvts_.getRawParameterValue(ParamIDs::inLpfEn);
    pInLpfFreq_     = apvts_.getRawParameterValue(ParamIDs::inLpfFreq);
    pInFiltToSc_    = apvts_.getRawParameterValue(ParamIDs::inFiltToSc);
    pInExtSc_       = apvts_.getRawParameterValue(ParamIDs::inExtSc);
    pInScListen_    = apvts_.getRawParameterValue(ParamIDs::inScListen);

    pMastAnalogTol_ = apvts_.getRawParameterValue(ParamIDs::masterAnalogTol);
    pMastMix_       = apvts_.getRawParameterValue(ParamIDs::masterMix);
    pMastOutGain_   = apvts_.getRawParameterValue(ParamIDs::masterOutGain);
    pMastVuMode_    = apvts_.getRawParameterValue(ParamIDs::masterVuMode);
    pMastMsMode_    = apvts_.getRawParameterValue(ParamIDs::masterMsMode);
    pChassisTheme_  = apvts_.getRawParameterValue(ParamIDs::chassisTheme);
    pVuRefDb_       = apvts_.getRawParameterValue(ParamIDs::vuRefDb);
    pOversampling_  = apvts_.getRawParameterValue(ParamIDs::oversampling);
    pFftMode_       = apvts_.getRawParameterValue(ParamIDs::fftMode);

    for (int s = 0; s < 5; ++s)
    {
        pSlotOrder_[static_cast<std::size_t>(s)]    = apvts_.getRawParameterValue(ParamIDs::slotOrderId(s));
        pSlotModId_[static_cast<std::size_t>(s)]    = apvts_.getRawParameterValue(ParamIDs::slotModuleId(s));
        pSlotAutogain_[static_cast<std::size_t>(s)] = apvts_.getRawParameterValue(ParamIDs::slotAutogainId(s));
        for (int p = 0; p < 18; ++p)
            pSlotParams_[static_cast<std::size_t>(s)][static_cast<std::size_t>(p)] =
                apvts_.getRawParameterValue(ParamIDs::slotParamId(s, p + 1));
    }
}

void KalyxHybridConsoleProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const double safeFs = std::max(sampleRate, 44100.0);
    currentSampleRate_.store(safeFs, std::memory_order_relaxed);

    inputStripDsp_.prepare(safeFs);
    for (int s = 0; s < 5; ++s)
    {
        slotDsps_[static_cast<std::size_t>(s)].prepare(safeFs);
        slotDsps_[static_cast<std::size_t>(s)].notifyModuleSelection(getSlotModuleId(s));
    }
    metersAndAutoGain_.prepare(safeFs);

    oversampler2x_ = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    oversampler4x_ = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    oversampler2x_->initProcessing(static_cast<std::size_t>(std::max(samplesPerBlock, 64)));
    oversampler4x_->initProcessing(static_cast<std::size_t>(std::max(samplesPerBlock, 64)));

    scScratchBuffer_.setSize(2, std::max(samplesPerBlock, 64) * 4, false, true, true);
    fftAccum_.fill(0.0f);
    fftReady_.fill(0.0f);
    fftWriteIdx_ = 0;
    fftBlockReady_.store(false, std::memory_order_relaxed);

    preFftAccum_.fill(0.0f);
    preFftReady_.fill(0.0f);
    preFftWriteIdx_ = 0;
    preFftBlockReady_.store(false, std::memory_order_relaxed);

    runZeroInputSilenceVerification(safeFs);
}

void KalyxHybridConsoleProcessor::releaseResources()
{
    inputStripDsp_.reset();
    for (auto& s : slotDsps_)
        s.reset();
    metersAndAutoGain_.reset();
}

bool KalyxHybridConsoleProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;
    if (mainIn != mainOut)
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto& sc = layouts.inputBuses[1];
        if (!sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void KalyxHybridConsoleProcessor::pushFFTSample(float sample) noexcept
{
    fftAccum_[static_cast<std::size_t>(fftWriteIdx_)] = sample;
    ++fftWriteIdx_;
    if (fftWriteIdx_ >= kFFTSize)
    {
        fftWriteIdx_ = 0;
        if (!fftBlockReady_.load(std::memory_order_acquire))
        {
            std::memcpy(fftReady_.data(), fftAccum_.data(), sizeof(float) * static_cast<std::size_t>(kFFTSize));
            fftBlockReady_.store(true, std::memory_order_release);
        }
    }
}

bool KalyxHybridConsoleProcessor::pullNextFFTBlock(float* dest2048) noexcept
{
    if (!fftBlockReady_.load(std::memory_order_acquire))
        return false;
    std::memcpy(dest2048, fftReady_.data(), sizeof(float) * static_cast<std::size_t>(kFFTSize));
    fftBlockReady_.store(false, std::memory_order_release);
    return true;
}

void KalyxHybridConsoleProcessor::pushPreFFTSample(float sample) noexcept
{
    preFftAccum_[static_cast<std::size_t>(preFftWriteIdx_)] = sample;
    ++preFftWriteIdx_;
    if (preFftWriteIdx_ >= kFFTSize)
    {
        preFftWriteIdx_ = 0;
        if (!preFftBlockReady_.load(std::memory_order_acquire))
        {
            std::memcpy(preFftReady_.data(), preFftAccum_.data(), sizeof(float) * static_cast<std::size_t>(kFFTSize));
            preFftBlockReady_.store(true, std::memory_order_release);
        }
    }
}

bool KalyxHybridConsoleProcessor::pullNextPreFFTBlock(float* dest2048) noexcept
{
    if (!preFftBlockReady_.load(std::memory_order_acquire))
        return false;
    std::memcpy(dest2048, preFftReady_.data(), sizeof(float) * static_cast<std::size_t>(kFFTSize));
    preFftBlockReady_.store(false, std::memory_order_release);
    return true;
}

void KalyxHybridConsoleProcessor::runZeroInputSilenceVerification(double sampleRate) noexcept
{
    kalyx::core::InputStripParameters testIn;
    testIn.drivePct = 100.0f;
    float maxErr = 0.0f;

    kalyx::core::InputStripDSP tmpIn;
    kalyx::core::SlotUnitDSP tmpSlot;
    tmpIn.prepare(sampleRate);
    tmpSlot.prepare(sampleRate);

    for (int m = 0; m < static_cast<int>(kalyx::core::SlotModuleId::Count); ++m)
    {
        testIn.preampModel = m % 5;
        tmpSlot.notifyModuleSelection(m);
        auto defP = kalyx::core::ModuleRegistry::getDefaultParamsForModule(static_cast<kalyx::core::SlotModuleId>(m));
        defP[0] = 1.0f;
        defP[2] = 1.0f;

        for (int n = 0; n < 128; ++n)
        {
            float l = 0.0f, r = 0.0f, scL = 0.0f, scR = 0.0f;
            tmpIn.processSample(l, r, scL, scR, testIn, 0.02f, -0.02f);
            (void)tmpSlot.processSample(l, r, scL, scR, defP, false, 0.02f, -0.02f, 1.01f, 0.99f);
            maxErr = std::max({ maxErr, std::abs(l), std::abs(r) });
        }
        tmpIn.reset();
        tmpSlot.reset();
    }
    jassert(maxErr < 1.0e-6f);
    (void)maxErr;
}

void KalyxHybridConsoleProcessor::processSingleSampleBlock(float* left, float* right,
                                                           const float* scLeft, const float* scRight,
                                                           int numSamples,
                                                           const kalyx::core::InputStripParameters& inP,
                                                           const kalyx::core::MasterStripParameters& mastP,
                                                           const std::array<int, 5>& slotOrder,
                                                           const std::array<int, 5>& slotModIds,
                                                           const std::array<kalyx::core::SlotParamArray, 5>& slotParams,
                                                           const std::array<bool, 5>& slotAutogain,
                                                           double /*effectiveFs*/) noexcept
{
    for (int s = 0; s < 5; ++s)
        slotDsps_[static_cast<std::size_t>(s)].notifyModuleSelection(slotModIds[static_cast<std::size_t>(s)]);

    const float tolNorm    = std::clamp(mastP.analogTolerance * 0.01f, 0.0f, 1.0f);
    const float tolBiasL   =  0.018f * tolNorm;
    const float tolBiasR   = -0.015f * tolNorm;
    const float tolFreqL   = 1.0f + 0.014f * tolNorm;
    const float tolFreqR   = 1.0f - 0.012f * tolNorm;
    const float outGainLin = std::pow(10.0f, mastP.outGainDb / 20.0f);
    const float mixWet     = std::clamp(mastP.mixPct * 0.01f, 0.0f, 1.0f);

    float peakInL = 0.0f, peakInR = 0.0f;
    float peakOutL = 0.0f, peakOutR = 0.0f;
    float maxBlockGrDb = 0.0f;
    float blockSlotGr[5]      = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    float blockSlotDriveDb[5] = { -100.0f, -100.0f, -100.0f, -100.0f, -100.0f };
    float blockSlotPeak[5]    = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

    for (int n = 0; n < numSamples; ++n)
    {
        const float dryL = left[n];
        const float dryR = right[n];
        peakInL = std::max(peakInL, std::abs(dryL));
        peakInR = std::max(peakInR, std::abs(dryR));
        metersAndAutoGain_.updateInputBallistics(dryL, dryR);

        float l = dryL, r = dryR;
        float scL = (inP.extSidechain && scLeft != nullptr) ? scLeft[n] : l;
        float scR = (inP.extSidechain && scRight != nullptr) ? scRight[n] : r;

        // 1. Tranche Fixe INPUT (Preamp + HPF/LPF)
        inputStripDsp_.processSample(l, r, scL, scR, inP, tolBiasL, tolBiasR);

        float sampleTotalGr = 0.0f;
        if (inP.scListen)
        {
            l = (scLeft != nullptr) ? scLeft[n] : scL;
            r = (scRight != nullptr) ? scRight[n] : scR;
        }
        else
        {
            // Encodage M/S global si actif
            float procA = l, procB = r;
            const bool globalMS = (mastP.msMode != 0);
            if (globalMS)
            {
                procA = 0.5f * (l + r);
                procB = 0.5f * (l - r);
            }

            // 2. Exécution des 5 Slots Série 500 (A à E) dans l'ordre défini par slotOrder (1U & 2U)
            bool fftPushed = false;
            bool preFftPushed = false;
            const float inputMonoPre = 0.5f * (procA + procB);
            for (int pos = 0; pos < 5; ++pos)
            {
                const int sIdx  = std::clamp(slotOrder[static_cast<std::size_t>(pos)], 0, 4);
                const int modId = slotModIds[static_cast<std::size_t>(sIdx)];
                const bool is2U = kalyx::core::isModuleId2U(modId) && (pos < 4);
                const bool isEq = (modId == static_cast<int>(kalyx::core::SlotModuleId::SurgicalEQ2U))
                               || (modId >= static_cast<int>(kalyx::core::SlotModuleId::SSL4000EQ) &&
                                   modId <= static_cast<int>(kalyx::core::SlotModuleId::Neve1073EQ));

                if (isEq && !preFftPushed && mastP.fftMode != 2)
                {
                    pushPreFFTSample(0.5f * (procA + procB));
                    preFftPushed = true;
                }

                if (isEq && mastP.fftMode == 1 && !fftPushed)
                {
                    pushFFTSample(0.5f * (procA + procB));
                    fftPushed = true;
                }

                float sGr = 0.0f;
                if (is2U)
                {
                    const int partnerIdx = std::clamp(slotOrder[static_cast<std::size_t>(pos + 1)], 0, 4);
                    kalyx::core::SlotParamArray2U p2u {};
                    for (int k = 0; k < 18; ++k)
                    {
                        p2u[static_cast<std::size_t>(k)]      = slotParams[static_cast<std::size_t>(sIdx)][static_cast<std::size_t>(k)];
                        p2u[static_cast<std::size_t>(18 + k)] = slotParams[static_cast<std::size_t>(partnerIdx)][static_cast<std::size_t>(k)];
                    }
                    std::array<uint32_t, 6> meta {};
                    for (int b = 0; b < 6; ++b)
                        meta[static_cast<std::size_t>(b)] = surgicalBandMeta_[static_cast<std::size_t>(sIdx)][static_cast<std::size_t>(b)].load(std::memory_order_relaxed);

                    sGr = slotDsps_[static_cast<std::size_t>(sIdx)].processSample2U(
                        procA, procB, p2u, meta, slotAutogain[static_cast<std::size_t>(sIdx)]);
                    ++pos; // Le slot partenaire est fusionné dans ce module 2U
                }
                else
                {
                    sGr = slotDsps_[static_cast<std::size_t>(sIdx)].processSample(
                        procA, procB, scL, scR,
                        slotParams[static_cast<std::size_t>(sIdx)],
                        inP.extSidechain,
                        tolBiasL, tolBiasR, tolFreqL, tolFreqR,
                        slotAutogain[static_cast<std::size_t>(sIdx)]);
                }

                sampleTotalGr += sGr;
                blockSlotGr[sIdx]      = std::max(blockSlotGr[sIdx], sGr);
                blockSlotDriveDb[sIdx] = std::max(blockSlotDriveDb[sIdx], slotDsps_[static_cast<std::size_t>(sIdx)].getLastDriveLevelDb());
                blockSlotPeak[sIdx]    = std::max(blockSlotPeak[sIdx], std::max(std::abs(procA), std::abs(procB)));

                if (isEq && mastP.fftMode == 0 && !fftPushed)
                {
                    pushFFTSample(0.5f * (procA + procB));
                    fftPushed = true;
                }
            }

            if (!preFftPushed && mastP.fftMode != 2)
                pushPreFFTSample(inputMonoPre);
            if (!fftPushed && mastP.fftMode != 2)
                pushFFTSample(0.5f * (procA + procB));

            if (globalMS)
            {
                const float midOut  = (mastP.msMode == 3) ? 0.0f : procA;
                const float sideOut = (mastP.msMode == 2) ? 0.0f : procB;
                l = midOut + sideOut;
                r = midOut - sideOut;
            }
            else
            {
                l = procA; r = procB;
            }
        }

        // 3. Tranche Fixe MASTER : Mix -> Out Fader (Auto-Gain est désormais géré individuellement par slot)
        l = ((1.0f - mixWet) * dryL + mixWet * l) * outGainLin;
        r = ((1.0f - mixWet) * dryR + mixWet * r) * outGainLin;

        l = std::isfinite(l) ? std::clamp(l, -4.0f, 4.0f) : 0.0f;
        r = std::isfinite(r) ? std::clamp(r, -4.0f, 4.0f) : 0.0f;

        left[n]  = l;
        right[n] = r;

        peakOutL     = std::max(peakOutL, std::abs(l));
        peakOutR     = std::max(peakOutR, std::abs(r));
        maxBlockGrDb = std::max(maxBlockGrDb, sampleTotalGr);
        metersAndAutoGain_.updateOutputAndGrBallistics(l, r, sampleTotalGr);
    }

    kalyx::dsp::StudioMeterSnapshot snap {};
    metersAndAutoGain_.populateSnapshot(snap);
    mInPeakL_.store(toDbClamped(peakInL),   std::memory_order_relaxed);
    mInPeakR_.store(toDbClamped(peakInR),   std::memory_order_relaxed);
    mOutPeakL_.store(toDbClamped(peakOutL), std::memory_order_relaxed);
    mOutPeakR_.store(toDbClamped(peakOutR), std::memory_order_relaxed);
    mInVu300L_.store(snap.inputVu300DbL,    std::memory_order_relaxed);
    mInVu300R_.store(snap.inputVu300DbR,    std::memory_order_relaxed);
    mOutVu300L_.store(snap.outputVu300DbL,  std::memory_order_relaxed);
    mOutVu300R_.store(snap.outputVu300DbR,  std::memory_order_relaxed);
    mCumGrDb_.store(maxBlockGrDb,           std::memory_order_relaxed);
    mCumGrVuDb_.store(snap.cumulativeGrVuDb,std::memory_order_relaxed);
    mAutoGainDb_.store(0.0f,                std::memory_order_relaxed);
    for (int s = 0; s < 5; ++s)
    {
        mSlotGrDb_[static_cast<std::size_t>(s)].store(blockSlotGr[s],                  std::memory_order_relaxed);
        mSlotDriveDb_[static_cast<std::size_t>(s)].store(blockSlotDriveDb[s],          std::memory_order_relaxed);
        mSlotPeakDb_[static_cast<std::size_t>(s)].store(toDbClamped(blockSlotPeak[s]), std::memory_order_relaxed);
        const float thd = (slotModIds[static_cast<std::size_t>(s)] == static_cast<int>(kalyx::core::SlotModuleId::HarmonicSculptor))
            ? slotDsps_[static_cast<std::size_t>(s)].getHarmonicSculptorThdPct()
            : slotDsps_[static_cast<std::size_t>(s)].getDistressorThdPct();
        mSlotThdPct_[static_cast<std::size_t>(s)].store(thd, std::memory_order_relaxed);
        mSlotCorr_[static_cast<std::size_t>(s)].store(slotDsps_[static_cast<std::size_t>(s)].getStereoCorrelation(), std::memory_order_relaxed);
    }
}

void KalyxHybridConsoleProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainInput  = getBusBuffer(buffer, true, 0);
    auto mainOutput = getBusBuffer(buffer, false, 0);
    const int numSamples = mainOutput.getNumSamples();
    if (numSamples <= 0) return;

    const int inCh  = mainInput.getNumChannels();
    const int outCh = mainOutput.getNumChannels();
    for (int ch = inCh; ch < outCh; ++ch)
        mainOutput.clear(ch, 0, numSamples);
    if (outCh == 0 || inCh == 0) return;

    auto rd = [](std::atomic<float>* ptr) noexcept { return ptr ? ptr->load(std::memory_order_relaxed) : 0.0f; };

    kalyx::core::InputStripParameters inP;
    inP.trimDb       = rd(pInTrim_);
    inP.preampModel  = std::clamp(static_cast<int>(std::round(rd(pInPreamp_))), 0, 4);
    inP.drivePct     = rd(pInDrive_);
    inP.phaseInvert  = rd(pInPhase_) > 0.5f;
    inP.hpfEnabled   = rd(pInHpfEn_) > 0.5f;
    inP.hpfFreqHz    = rd(pInHpfFreq_);
    inP.lpfEnabled   = rd(pInLpfEn_) > 0.5f;
    inP.lpfFreqHz    = rd(pInLpfFreq_);
    inP.filtersToSc  = rd(pInFiltToSc_) > 0.5f;
    inP.extSidechain = rd(pInExtSc_) > 0.5f;
    inP.scListen     = rd(pInScListen_) > 0.5f;

    kalyx::core::MasterStripParameters mastP;
    mastP.analogTolerance = rd(pMastAnalogTol_);
    mastP.mixPct          = rd(pMastMix_);
    mastP.outGainDb       = rd(pMastOutGain_);
    mastP.vuMode          = std::clamp(static_cast<int>(std::round(rd(pMastVuMode_))), 0, 2);
    mastP.msMode          = std::clamp(static_cast<int>(std::round(rd(pMastMsMode_))), 0, 3);
    mastP.chassisTheme    = std::clamp(static_cast<int>(std::round(rd(pChassisTheme_))), 0, 2);
    mastP.vuRefDb         = rd(pVuRefDb_);
    mastP.oversampling    = std::clamp(static_cast<int>(std::round(rd(pOversampling_))), 0, 2);
    mastP.fftMode         = std::clamp(static_cast<int>(std::round(rd(pFftMode_))), 0, 2);

    std::array<int, 5> slotOrder {};
    std::array<int, 5> slotModIds {};
    std::array<bool, 5> slotAutogain {};
    std::array<kalyx::core::SlotParamArray, 5> slotParams {};
    const int maxMod = static_cast<int>(kalyx::core::SlotModuleId::Count) - 1;
    for (int s = 0; s < 5; ++s)
    {
        slotOrder[static_cast<std::size_t>(s)]    = std::clamp(static_cast<int>(std::round(rd(pSlotOrder_[static_cast<std::size_t>(s)]))), 0, 4);
        slotModIds[static_cast<std::size_t>(s)]   = std::clamp(static_cast<int>(std::round(rd(pSlotModId_[static_cast<std::size_t>(s)]))), 0, maxMod);
        slotAutogain[static_cast<std::size_t>(s)] = rd(pSlotAutogain_[static_cast<std::size_t>(s)]) > 0.5f;
        for (int p = 0; p < 18; ++p)
            slotParams[static_cast<std::size_t>(s)][static_cast<std::size_t>(p)] = rd(pSlotParams_[static_cast<std::size_t>(s)][static_cast<std::size_t>(p)]);
    }

    const float* scL = nullptr;
    const float* scR = nullptr;
    if (getBusCount(true) > 1 && getBus(true, 1)->isEnabled())
    {
        auto scBus = getBusBuffer(buffer, true, 1);
        if (scBus.getNumChannels() >= 1)
        {
            scL = scBus.getReadPointer(0);
            scR = (scBus.getNumChannels() >= 2) ? scBus.getReadPointer(1) : scL;
        }
    }
    else if (buffer.getNumChannels() >= 4)
    {
        scL = buffer.getReadPointer(2);
        scR = buffer.getReadPointer(3);
    }

    for (int ch = 0; ch < std::min(inCh, outCh); ++ch)
        if (mainOutput.getWritePointer(ch) != mainInput.getReadPointer(ch))
            mainOutput.copyFrom(ch, 0, mainInput, ch, 0, numSamples);

    float* outL = mainOutput.getWritePointer(0);
    float* outR = (outCh >= 2) ? mainOutput.getWritePointer(1) : scScratchBuffer_.getWritePointer(0);
    if (outCh == 1 && numSamples <= scScratchBuffer_.getNumSamples())
        std::memcpy(outR, outL, sizeof(float) * static_cast<std::size_t>(numSamples));

    const double baseFs = currentSampleRate_.load(std::memory_order_relaxed);
    if (mastP.oversampling == 0 || outCh < 2)
    {
        processSingleSampleBlock(outL, outR, scL, scR, numSamples, inP, mastP, slotOrder, slotModIds, slotParams, slotAutogain, baseFs);
    }
    else
    {
        auto* os = (mastP.oversampling == 1) ? oversampler2x_.get() : oversampler4x_.get();
        const int factor = (mastP.oversampling == 1) ? 2 : 4;
        juce::dsp::AudioBlock<float> baseBlock(mainOutput);
        auto upBlock = os->processSamplesUp(baseBlock);
        processSingleSampleBlock(upBlock.getChannelPointer(0), upBlock.getChannelPointer(1),
                                 nullptr, nullptr, static_cast<int>(upBlock.getNumSamples()),
                                 inP, mastP, slotOrder, slotModIds, slotParams, slotAutogain, baseFs * static_cast<double>(factor));
        os->processSamplesDown(baseBlock);
    }
}

kalyx::dsp::StudioMeterSnapshot KalyxHybridConsoleProcessor::getMeterSnapshot() const noexcept
{
    kalyx::dsp::StudioMeterSnapshot s;
    s.inputPeakDbL      = mInPeakL_.load(std::memory_order_relaxed);
    s.inputPeakDbR      = mInPeakR_.load(std::memory_order_relaxed);
    s.outputPeakDbL     = mOutPeakL_.load(std::memory_order_relaxed);
    s.outputPeakDbR     = mOutPeakR_.load(std::memory_order_relaxed);
    s.inputVu300DbL     = mInVu300L_.load(std::memory_order_relaxed);
    s.inputVu300DbR     = mInVu300R_.load(std::memory_order_relaxed);
    s.outputVu300DbL    = mOutVu300L_.load(std::memory_order_relaxed);
    s.outputVu300DbR    = mOutVu300R_.load(std::memory_order_relaxed);
    s.cumulativeGrDb    = mCumGrDb_.load(std::memory_order_relaxed);
    s.cumulativeGrVuDb  = mCumGrVuDb_.load(std::memory_order_relaxed);
    s.autoGainAppliedDb = mAutoGainDb_.load(std::memory_order_relaxed);
    return s;
}

void KalyxHybridConsoleProcessor::copyPrecisionDynScope(
    int slotIdx,
    std::array<kalyx::consoles::PrecisionDyn2UDSP::ScopeSample, kalyx::consoles::PrecisionDyn2UDSP::kScopeSize>& dest) const noexcept
{
    const int idx = std::clamp(slotIdx, 0, 4);
    slotDsps_[static_cast<std::size_t>(idx)].getPrecisionDyn2U().copyScopeHistory(dest);
}

std::array<int, 5> KalyxHybridConsoleProcessor::getSlotOrder() const noexcept
{
    std::array<int, 5> order {{ 0, 1, 2, 3, 4 }};
    for (int i = 0; i < 5; ++i)
        order[static_cast<std::size_t>(i)] = std::clamp(
            static_cast<int>(std::round(pSlotOrder_[static_cast<std::size_t>(i)]->load(std::memory_order_relaxed))), 0, 4);
    return order;
}

void KalyxHybridConsoleProcessor::swapVisualSlots(int posA, int posB)
{
    if (posA < 0 || posA > 4 || posB < 0 || posB > 4 || posA == posB)
        return;
    auto order = getSlotOrder();
    std::swap(order[static_cast<std::size_t>(posA)], order[static_cast<std::size_t>(posB)]);
    for (int i = 0; i < 5; ++i)
        setParamPlain(apvts_, ParamIDs::slotOrderId(i), static_cast<float>(order[static_cast<std::size_t>(i)]));
}

void KalyxHybridConsoleProcessor::move2USlot(int pos, int delta)
{
    auto order = getSlotOrder();
    if (pos < 0 || pos >= 4)
        return;
    if (delta < 0 && pos > 0)
    {
        const int leftSlot = order[static_cast<std::size_t>(pos - 1)];
        order[static_cast<std::size_t>(pos - 1)] = order[static_cast<std::size_t>(pos)];
        order[static_cast<std::size_t>(pos)]     = order[static_cast<std::size_t>(pos + 1)];
        order[static_cast<std::size_t>(pos + 1)] = leftSlot;
    }
    else if (delta > 0 && pos + 2 <= 4)
    {
        const int rightSlot = order[static_cast<std::size_t>(pos + 2)];
        order[static_cast<std::size_t>(pos + 2)] = order[static_cast<std::size_t>(pos + 1)];
        order[static_cast<std::size_t>(pos + 1)] = order[static_cast<std::size_t>(pos)];
        order[static_cast<std::size_t>(pos)]     = rightSlot;
    }
    for (int i = 0; i < 5; ++i)
        setParamPlain(apvts_, ParamIDs::slotOrderId(i), static_cast<float>(order[static_cast<std::size_t>(i)]));
}

int KalyxHybridConsoleProcessor::getPartnerSlotFor2U(int hostSlotIdx) const noexcept
{
    const int s = std::clamp(hostSlotIdx, 0, 4);
    if (!kalyx::core::isModuleId2U(getSlotModuleId(s)))
        return -1;
    const auto order = getSlotOrder();
    for (int pos = 0; pos < 4; ++pos)
        if (order[static_cast<std::size_t>(pos)] == s)
            return order[static_cast<std::size_t>(pos + 1)];
    return -1;
}

bool KalyxHybridConsoleProcessor::isSlotLinkedTo2U(int slotIdx) const noexcept
{
    const int s = std::clamp(slotIdx, 0, 4);
    const auto order = getSlotOrder();
    for (int pos = 1; pos < 5; ++pos)
        if (order[static_cast<std::size_t>(pos)] == s)
            return kalyx::core::isModuleId2U(getSlotModuleId(order[static_cast<std::size_t>(pos - 1)]));
    return false;
}

kalyx::core::SlotParamArray2U KalyxHybridConsoleProcessor::getSlotParams2UForGUI(int hostSlotIdx) const noexcept
{
    kalyx::core::SlotParamArray2U out {};
    const auto pHost = getSlotParamsForGUI(hostSlotIdx);
    const int partner = getPartnerSlotFor2U(hostSlotIdx);
    const auto pPart = (partner >= 0) ? getSlotParamsForGUI(partner) : kalyx::core::SlotParamArray {};
    for (int k = 0; k < 18; ++k)
    {
        out[static_cast<std::size_t>(k)]      = pHost[static_cast<std::size_t>(k)];
        out[static_cast<std::size_t>(18 + k)] = pPart[static_cast<std::size_t>(k)];
    }
    return out;
}

uint32_t KalyxHybridConsoleProcessor::getSurgicalBandMeta(int slotIdx, int bandIdx) const noexcept
{
    const int s = std::clamp(slotIdx, 0, 4);
    const int b = std::clamp(bandIdx, 0, 5);
    return surgicalBandMeta_[static_cast<std::size_t>(s)][static_cast<std::size_t>(b)].load(std::memory_order_relaxed);
}

void KalyxHybridConsoleProcessor::setSurgicalBandMeta(int slotIdx, int bandIdx, uint32_t packed) noexcept
{
    const int s = std::clamp(slotIdx, 0, 4);
    const int b = std::clamp(bandIdx, 0, 5);
    surgicalBandMeta_[static_cast<std::size_t>(s)][static_cast<std::size_t>(b)].store(packed, std::memory_order_relaxed);
}

void KalyxHybridConsoleProcessor::loadModuleIntoSlotWithDefaults(int slotIdx, kalyx::core::SlotModuleId modId)
{
    const int s = std::clamp(slotIdx, 0, 4);
    if (isSectionLocked(s + 1)) return;

    if (kalyx::core::isModule2U(modId))
    {
        auto order = getSlotOrder();
        int pos = 0;
        for (int i = 0; i < 5; ++i)
            if (order[static_cast<std::size_t>(i)] == s) { pos = i; break; }

        if (pos == 4)
        {
            swapVisualSlots(3, 4);
            order = getSlotOrder();
            pos = 3;
        }
        const int partner = order[static_cast<std::size_t>(pos + 1)];
        const auto defs2u = kalyx::core::ModuleRegistry::getDefaultParams2UForModule(modId);

        setParamPlain(apvts_, ParamIDs::slotModuleId(s), static_cast<float>(static_cast<int>(modId)));
        setParamPlain(apvts_, ParamIDs::slotModuleId(partner), static_cast<float>(static_cast<int>(kalyx::core::SlotModuleId::EmptyBypass)));
        for (int p = 1; p <= 18; ++p)
        {
            setParamPlain(apvts_, ParamIDs::slotParamId(s, p),       defs2u[static_cast<std::size_t>(p - 1)]);
            setParamPlain(apvts_, ParamIDs::slotParamId(partner, p), defs2u[static_cast<std::size_t>(18 + p - 1)]);
        }

        const auto defMeta = kalyx::consoles::SurgicalEQ2UDSP::getDefaultBandMeta();
        for (int b = 0; b < 6; ++b)
            surgicalBandMeta_[static_cast<std::size_t>(s)][static_cast<std::size_t>(b)].store(defMeta[static_cast<std::size_t>(b)], std::memory_order_relaxed);
        return;
    }

    setParamPlain(apvts_, ParamIDs::slotModuleId(s), static_cast<float>(static_cast<int>(modId)));
    const auto defs = kalyx::core::ModuleRegistry::getDefaultParamsForModule(modId);
    for (int p = 1; p <= 18; ++p)
        setParamPlain(apvts_, ParamIDs::slotParamId(s, p), defs[static_cast<std::size_t>(p - 1)]);
}

int KalyxHybridConsoleProcessor::getSlotModuleId(int slotIdx) const noexcept
{
    const int s = std::clamp(slotIdx, 0, 4);
    const int maxMod = static_cast<int>(kalyx::core::SlotModuleId::Count) - 1;
    return std::clamp(static_cast<int>(std::round(pSlotModId_[static_cast<std::size_t>(s)]->load(std::memory_order_relaxed))), 0, maxMod);
}

kalyx::core::SlotParamArray KalyxHybridConsoleProcessor::getSlotParamsForGUI(int slotIdx) const noexcept
{
    const int s = std::clamp(slotIdx, 0, 4);
    kalyx::core::SlotParamArray arr {};
    for (int p = 0; p < 18; ++p)
        arr[static_cast<std::size_t>(p)] = pSlotParams_[static_cast<std::size_t>(s)][static_cast<std::size_t>(p)]->load(std::memory_order_relaxed);
    return arr;
}

void KalyxHybridConsoleProcessor::setSectionLocked(int idx, bool locked) noexcept
{
    if (idx >= 0 && idx < 7)
        sectionLocks_[static_cast<std::size_t>(idx)].store(locked, std::memory_order_relaxed);
}

bool KalyxHybridConsoleProcessor::isSectionLocked(int idx) const noexcept
{
    if (idx >= 0 && idx < 7)
        return sectionLocks_[static_cast<std::size_t>(idx)].load(std::memory_order_relaxed);
    return false;
}

int KalyxHybridConsoleProcessor::getNumPrograms() { return static_cast<int>(presetManager_.getPresets().size()); }
int KalyxHybridConsoleProcessor::getCurrentProgram() { return presetManager_.getCurrentPresetIndex(); }
void KalyxHybridConsoleProcessor::setCurrentProgram(int index) { presetManager_.loadPresetByIndex(index); }
const juce::String KalyxHybridConsoleProcessor::getProgramName(int index)
{
    const auto& all = presetManager_.getPresets();
    if (index >= 0 && index < static_cast<int>(all.size()))
        return all[static_cast<std::size_t>(index)].name;
    return "Init";
}
void KalyxHybridConsoleProcessor::changeProgramName(int, const juce::String&) {}

void KalyxHybridConsoleProcessor::applyFactoryPreset(int factoryId)
{
    using Mod = kalyx::core::SlotModuleId;
    for (int i = 0; i < 5; ++i)
        setParamPlain(apvts_, ParamIDs::slotOrderId(i), static_cast<float>(i));

    if (!isSectionLocked(0))
    {
        setParamPlain(apvts_, ParamIDs::inTrim, 0.0f);
        setParamPlain(apvts_, ParamIDs::inDrive, 0.0f);
        setParamPlain(apvts_, ParamIDs::inHpfEn, 0.0f);
        setParamPlain(apvts_, ParamIDs::inLpfEn, 0.0f);
    }

    auto loadSlot = [&](int s, Mod m) { loadModuleIntoSlotWithDefaults(s, m); };

    switch (factoryId)
    {
        case 1: // SSL 4000 E Vocal Strip
            if (!isSectionLocked(0)) { setParamPlain(apvts_, ParamIDs::inPreampModel, 0.0f); setParamPlain(apvts_, ParamIDs::inHpfEn, 1.0f); setParamPlain(apvts_, ParamIDs::inHpfFreq, 90.0f); }
            loadSlot(0, Mod::UtilityGateDeEsser); setParamPlain(apvts_, ParamIDs::slotParamId(0, 6), 1.0f);
            loadSlot(1, Mod::SSL4000Comp);        setParamPlain(apvts_, ParamIDs::slotParamId(1, 11), 0.0f); // SSL E
            loadSlot(2, Mod::SSL4000EQ);          setParamPlain(apvts_, ParamIDs::slotParamId(2, 15), 0.0f); // SSL E
            loadSlot(3, Mod::ConsoleColor);
            loadSlot(4, Mod::PrecisionLimiter);
            break;

        case 2: // API 1608 / 2500 Drum Bus
            if (!isSectionLocked(0)) { setParamPlain(apvts_, ParamIDs::inPreampModel, 2.0f); setParamPlain(apvts_, ParamIDs::inDrive, 35.0f); }
            loadSlot(0, Mod::UtilityGateDeEsser);
            loadSlot(1, Mod::API2500Comp); setParamPlain(apvts_, ParamIDs::slotParamId(1, 7), kalyx::core::choiceToNorm(2, 3)); // Thrust Loud
            loadSlot(2, Mod::API550BEQ);
            loadSlot(3, Mod::ConsoleColor); setParamPlain(apvts_, ParamIDs::slotParamId(3, 2), kalyx::core::choiceToNorm(2, 5)); // API 2503
            loadSlot(4, Mod::PrecisionLimiter); setParamPlain(apvts_, ParamIDs::slotParamId(4, 1), 1.0f);
            break;

        case 3: // Neve 1073 / 5254 Vintage
            if (!isSectionLocked(0)) { setParamPlain(apvts_, ParamIDs::inPreampModel, 3.0f); setParamPlain(apvts_, ParamIDs::inDrive, 45.0f); }
            loadSlot(0, Mod::UtilityGateDeEsser);
            loadSlot(1, Mod::Neve5254Comp);
            loadSlot(2, Mod::Neve1073EQ);
            loadSlot(3, Mod::ConsoleColor); setParamPlain(apvts_, ParamIDs::slotParamId(3, 2), kalyx::core::choiceToNorm(3, 5)); // Neve Silk
            loadSlot(4, Mod::PrecisionLimiter);
            break;

        case 4: // API Punch + SSL Glue (Hybride)
            if (!isSectionLocked(0)) { setParamPlain(apvts_, ParamIDs::inPreampModel, 2.0f); setParamPlain(apvts_, ParamIDs::inDrive, 30.0f); }
            loadSlot(0, Mod::API2500Comp);
            loadSlot(1, Mod::SSL4000Comp); setParamPlain(apvts_, ParamIDs::slotParamId(1, 11), 1.0f); // SSL G Glue
            loadSlot(2, Mod::API550BEQ);
            loadSlot(3, Mod::ConsoleColor); setParamPlain(apvts_, ParamIDs::slotParamId(3, 2), kalyx::core::choiceToNorm(2, 5));
            loadSlot(4, Mod::PrecisionLimiter); setParamPlain(apvts_, ParamIDs::slotParamId(4, 1), 1.0f);
            break;

        case 5: // Neve Warmth + 550B Air (Hybride)
            if (!isSectionLocked(0)) { setParamPlain(apvts_, ParamIDs::inPreampModel, 3.0f); setParamPlain(apvts_, ParamIDs::inDrive, 40.0f); }
            loadSlot(0, Mod::UtilityGateDeEsser);
            loadSlot(1, Mod::Neve5254Comp);
            loadSlot(2, Mod::API550BEQ);
            loadSlot(3, Mod::ConsoleColor); setParamPlain(apvts_, ParamIDs::slotParamId(3, 2), kalyx::core::choiceToNorm(3, 5));
            loadSlot(4, Mod::PrecisionLimiter);
            break;

        case 6: // 12-String Hybrid
            if (!isSectionLocked(0)) { setParamPlain(apvts_, ParamIDs::inPreampModel, 3.0f); setParamPlain(apvts_, ParamIDs::inHpfEn, 1.0f); setParamPlain(apvts_, ParamIDs::inHpfFreq, 85.0f); }
            loadSlot(0, Mod::UtilityGateDeEsser);
            loadSlot(1, Mod::API2500Comp);
            loadSlot(2, Mod::Neve1073EQ);
            loadSlot(3, Mod::ConsoleColor);
            loadSlot(4, Mod::PrecisionLimiter);
            break;

        case 7: // SSL G Glue + Peak Ceiling (Mastering)
            if (!isSectionLocked(0)) { setParamPlain(apvts_, ParamIDs::inPreampModel, 1.0f); }
            loadSlot(0, Mod::SSL4000EQ);   setParamPlain(apvts_, ParamIDs::slotParamId(0, 15), 1.0f); // SSL G
            loadSlot(1, Mod::SSL4000Comp); setParamPlain(apvts_, ParamIDs::slotParamId(1, 11), 1.0f); // SSL G
            setParamPlain(apvts_, ParamIDs::slotParamId(1, 6), 1.0f); // Auto-Rel
            loadSlot(2, Mod::ConsoleColor); setParamPlain(apvts_, ParamIDs::slotParamId(2, 2), kalyx::core::choiceToNorm(1, 5));
            loadSlot(3, Mod::PrecisionLimiter); setParamPlain(apvts_, ParamIDs::slotParamId(3, 1), 1.0f);
            loadSlot(4, Mod::EmptyBypass);
            break;

        case 8: // Kalyx Heritage Clean Master
            if (!isSectionLocked(0)) { setParamPlain(apvts_, ParamIDs::inPreampModel, 4.0f); }
            setParamPlain(apvts_, ParamIDs::chassisTheme, 2.0f);
            loadSlot(0, Mod::UtilityGateDeEsser);
            loadSlot(1, Mod::Neve5254Comp);
            loadSlot(2, Mod::SSL4000EQ);   setParamPlain(apvts_, ParamIDs::slotParamId(2, 15), 1.0f); // SSL G
            loadSlot(3, Mod::ConsoleColor); setParamPlain(apvts_, ParamIDs::slotParamId(3, 2), kalyx::core::choiceToNorm(4, 5)); // Kalyx
            loadSlot(4, Mod::PrecisionLimiter); setParamPlain(apvts_, ParamIDs::slotParamId(4, 1), 1.0f);
            break;

        case 0: // Init (5-Slot Studio)
        default:
            if (!isSectionLocked(0)) { setParamPlain(apvts_, ParamIDs::inPreampModel, 0.0f); }
            loadSlot(0, Mod::UtilityGateDeEsser);
            loadSlot(1, Mod::SSL4000Comp);
            loadSlot(2, Mod::SSL4000EQ);
            loadSlot(3, Mod::ConsoleColor);
            loadSlot(4, Mod::PrecisionLimiter);
            break;
    }
}

void KalyxHybridConsoleProcessor::toggleABState()
{
    if (activeABSlot_ == 0)
    {
        stateA_ = apvts_.copyState();
        activeABSlot_ = 1;
        if (stateB_.isValid()) apvts_.replaceState(stateB_.createCopy());
    }
    else
    {
        stateB_ = apvts_.copyState();
        activeABSlot_ = 0;
        if (stateA_.isValid()) apvts_.replaceState(stateA_.createCopy());
    }
}

void KalyxHybridConsoleProcessor::copyActiveToInactiveAB()
{
    if (activeABSlot_ == 0) stateB_ = apvts_.copyState();
    else                    stateA_ = apvts_.copyState();
}

void KalyxHybridConsoleProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts_.copyState();
    for (int s = 0; s < 5; ++s)
        for (int b = 0; b < 6; ++b)
            state.setProperty("seq2u_meta_" + juce::String(s) + "_" + juce::String(b),
                              static_cast<int>(surgicalBandMeta_[static_cast<std::size_t>(s)][static_cast<std::size_t>(b)].load(std::memory_order_relaxed)),
                              nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void KalyxHybridConsoleProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts_.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml(*xml);
        const auto defMeta = kalyx::consoles::SurgicalEQ2UDSP::getDefaultBandMeta();
        for (int s = 0; s < 5; ++s)
        {
            for (int b = 0; b < 6; ++b)
            {
                const juce::Identifier prop("seq2u_meta_" + juce::String(s) + "_" + juce::String(b));
                const uint32_t val = tree.hasProperty(prop)
                                   ? static_cast<uint32_t>(static_cast<int>(tree.getProperty(prop)))
                                   : defMeta[static_cast<std::size_t>(b)];
                surgicalBandMeta_[static_cast<std::size_t>(s)][static_cast<std::size_t>(b)].store(val, std::memory_order_relaxed);
            }
        }
        apvts_.replaceState(tree);
    }
}

juce::AudioProcessorEditor* KalyxHybridConsoleProcessor::createEditor()
{
    return new KalyxHybridConsoleEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KalyxHybridConsoleProcessor();
}
