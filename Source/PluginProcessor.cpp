#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <memory>
#include <cstring>

KalyxSSLConsoleProcessor::KalyxSSLConsoleProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input",     juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output",   juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "KALYX_SSL_PARAMETERS", createParameterLayout())
{
    pInputGain_       = apvts_.getRawParameterValue(ParamIDs::inputGain);
    pInputSatDrive_   = apvts_.getRawParameterValue(ParamIDs::inputSatDrive);
    pPhaseInvert_     = apvts_.getRawParameterValue(ParamIDs::phaseInvert);
    pHpfEnabled_      = apvts_.getRawParameterValue(ParamIDs::hpfEnabled);
    pHpfFreq_         = apvts_.getRawParameterValue(ParamIDs::hpfFreq);
    pHpfQ_            = apvts_.getRawParameterValue(ParamIDs::hpfQ);
    pLpfEnabled_      = apvts_.getRawParameterValue(ParamIDs::lpfEnabled);
    pLpfFreq_         = apvts_.getRawParameterValue(ParamIDs::lpfFreq);
    pLpfQ_            = apvts_.getRawParameterValue(ParamIDs::lpfQ);
    pFiltPreTrim_     = apvts_.getRawParameterValue(ParamIDs::filtPreTrim);
    pDynEnabled_      = apvts_.getRawParameterValue(ParamIDs::dynEnabled);
    pGateThresh_      = apvts_.getRawParameterValue(ParamIDs::gateThresh);
    pGateRange_       = apvts_.getRawParameterValue(ParamIDs::gateRange);
    pCompThresh_      = apvts_.getRawParameterValue(ParamIDs::compThresh);
    pCompRatio_       = apvts_.getRawParameterValue(ParamIDs::compRatio);
    pCompAttack_      = apvts_.getRawParameterValue(ParamIDs::compAttack);
    pCompRelease_     = apvts_.getRawParameterValue(ParamIDs::compRelease);
    pCompAutoRelease_ = apvts_.getRawParameterValue(ParamIDs::compAutoRelease);
    pLimiterMode_     = apvts_.getRawParameterValue(ParamIDs::limiterMode);
    pMakeupGain_      = apvts_.getRawParameterValue(ParamIDs::makeupGain);
    pExtSidechain_    = apvts_.getRawParameterValue(ParamIDs::extSidechain);
    pFiltersToSc_     = apvts_.getRawParameterValue(ParamIDs::filtersToSc);
    pScListen_        = apvts_.getRawParameterValue(ParamIDs::scListen);
    pScHpfFreq_       = apvts_.getRawParameterValue(ParamIDs::scHpfFreq);
    pDeessIn_         = apvts_.getRawParameterValue(ParamIDs::deessIn);
    pDeessFreq_       = apvts_.getRawParameterValue(ParamIDs::deessFreq);
    pDeessThresh_     = apvts_.getRawParameterValue(ParamIDs::deessThresh);
    pColorAmount_     = apvts_.getRawParameterValue(ParamIDs::colorAmount);
    pColorLinkComp_   = apvts_.getRawParameterValue(ParamIDs::colorLinkComp);
    pCircuitMode_     = apvts_.getRawParameterValue(ParamIDs::circuitMode);
    pSatTone_         = apvts_.getRawParameterValue(ParamIDs::satTone);
    pCrosstalk_       = apvts_.getRawParameterValue(ParamIDs::crosstalk);
    pColorTrim_       = apvts_.getRawParameterValue(ParamIDs::colorTrim);
    pEqEnabled_       = apvts_.getRawParameterValue(ParamIDs::eqEnabled);
    pEqPreDyn_        = apvts_.getRawParameterValue(ParamIDs::eqPreDyn);
    pModuleOrder_     = apvts_.getRawParameterValue(ParamIDs::moduleOrder);
    pLfBell_          = apvts_.getRawParameterValue(ParamIDs::lfBell);
    pLfFreq_          = apvts_.getRawParameterValue(ParamIDs::lfFreq);
    pLfGain_          = apvts_.getRawParameterValue(ParamIDs::lfGain);
    pLmfFreq_         = apvts_.getRawParameterValue(ParamIDs::lmfFreq);
    pLmfGain_         = apvts_.getRawParameterValue(ParamIDs::lmfGain);
    pLmfQ_            = apvts_.getRawParameterValue(ParamIDs::lmfQ);
    pHmfFreq_         = apvts_.getRawParameterValue(ParamIDs::hmfFreq);
    pHmfGain_         = apvts_.getRawParameterValue(ParamIDs::hmfGain);
    pHmfQ_            = apvts_.getRawParameterValue(ParamIDs::hmfQ);
    pHfBell_          = apvts_.getRawParameterValue(ParamIDs::hfBell);
    pHfFreq_          = apvts_.getRawParameterValue(ParamIDs::hfFreq);
    pHfGain_          = apvts_.getRawParameterValue(ParamIDs::hfGain);
    pEqTrim_          = apvts_.getRawParameterValue(ParamIDs::eqTrim);
    pAutoGain_        = apvts_.getRawParameterValue(ParamIDs::autoGain);
    pMsMode_          = apvts_.getRawParameterValue(ParamIDs::msMode);
    pAnalogTolerance_ = apvts_.getRawParameterValue(ParamIDs::analogTolerance);
    pOutputGain_      = apvts_.getRawParameterValue(ParamIDs::outputGain);
    pMix_             = apvts_.getRawParameterValue(ParamIDs::mix);
    pOversampling_    = apvts_.getRawParameterValue(ParamIDs::oversampling);
    pMeterMode_       = apvts_.getRawParameterValue(ParamIDs::meterMode);
    pVuRefDb_         = apvts_.getRawParameterValue(ParamIDs::vuRefDb);
    pFftMode_         = apvts_.getRawParameterValue(ParamIDs::fftMode);
    pDayMode_         = apvts_.getRawParameterValue(ParamIDs::dayMode);

    for (auto& lock : sectionLocked_)
        lock.store(false, std::memory_order_relaxed);

    oversampler2x_ = std::make_unique<juce::dsp::Oversampling<float>>(
        4, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampler4x_ = std::make_unique<juce::dsp::Oversampling<float>>(
        4, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);

    slotAState_ = apvts_.copyState();
    slotBState_ = apvts_.copyState();
}

juce::AudioProcessorValueTreeState::ParameterLayout KalyxSSLConsoleProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    enum class ValueFormat { Decibels, Hertz, Ratio, Percent, QFactor, Milliseconds, ToneTilt };

    auto makeCalibratedFloat = [](const char* id, const char* name,
                                  float minVal, float maxVal, float defVal,
                                  float centerPointVal, ValueFormat fmt)
    {
        juce::NormalisableRange<float> range(minVal, maxVal, 0.01f);
        if (centerPointVal > minVal && centerPointVal < maxVal)
            range.setSkewForCentre(centerPointVal);

        auto stringFromVal = [fmt](float v, int) -> juce::String
        {
            switch (fmt)
            {
                case ValueFormat::Decibels:
                    return juce::String(v, 1) + " dB";
                case ValueFormat::Hertz:
                    if (v >= 1000.0f)
                        return juce::String(v * 0.001f, (v >= 10000.0f) ? 1 : 2) + " kHz";
                    return juce::String(juce::roundToInt(v)) + " Hz";
                case ValueFormat::Ratio:
                    return juce::String(v, 1) + ":1";
                case ValueFormat::Percent:
                    return juce::String(juce::roundToInt(v)) + " %";
                case ValueFormat::QFactor:
                    return juce::String(v, 2) + " Q";
                case ValueFormat::Milliseconds:
                    if (v < 10.0f)
                        return juce::String(v, 1) + " ms";
                    return juce::String(juce::roundToInt(v)) + " ms";
                case ValueFormat::ToneTilt:
                {
                    const int iv = juce::roundToInt(v);
                    if (iv < -2) return "W " + juce::String(std::abs(iv)) + "%";
                    if (iv >  2) return "B " + juce::String(iv) + "%";
                    return "0 %";
                }
            }
            return juce::String(v, 1);
        };

        auto valFromString = [fmt](const juce::String& text) -> float
        {
            const juce::String trimmed = text.trim();
            float num = trimmed.retainCharacters("0123456789.-+").getFloatValue();
            if (fmt == ValueFormat::Hertz && (trimmed.containsIgnoreCase("k")))
                num *= 1000.0f;
            if (fmt == ValueFormat::ToneTilt && trimmed.startsWithIgnoreCase("W"))
                num = -std::abs(num);
            return num;
        };

        return std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { id, 1 },
            name,
            range,
            defVal,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction(stringFromVal)
                .withValueFromStringFunction(valFromString));
    };

    auto makeBool = [](const char* id, const char* name, bool defVal)
    {
        return std::make_unique<juce::AudioParameterBool>(juce::ParameterID { id, 1 }, name, defVal);
    };

    // 1. Channel Input, Preamp Saturation, Filters & FILT/PRE TRIM
    params.push_back(makeCalibratedFloat(ParamIDs::inputGain,     "Input Line Trim", -24.0f, 24.0f, 0.0f, 0.0f, ValueFormat::Decibels));
    params.push_back(makeCalibratedFloat(ParamIDs::inputSatDrive, "Input Preamp Saturation", 0.0f, 100.0f, 0.0f, 35.0f, ValueFormat::Percent));
    params.push_back(makeBool           (ParamIDs::phaseInvert,   "Phase Invert", false));
    params.push_back(makeBool           (ParamIDs::hpfEnabled,    "HPF In", true));
    params.push_back(makeCalibratedFloat(ParamIDs::hpfFreq,       "HPF Freq", 20.0f, 500.0f, 40.0f, 95.0f, ValueFormat::Hertz));
    params.push_back(makeCalibratedFloat(ParamIDs::hpfQ,          "HPF Q", 0.50f, 4.00f, 0.71f, 1.25f, ValueFormat::QFactor));
    params.push_back(makeBool           (ParamIDs::lpfEnabled,    "LPF In", false));
    params.push_back(makeCalibratedFloat(ParamIDs::lpfFreq,       "LPF Freq", 2000.0f, 20000.0f, 18000.0f, 6500.0f, ValueFormat::Hertz));
    params.push_back(makeCalibratedFloat(ParamIDs::lpfQ,          "LPF Q", 0.50f, 4.00f, 0.71f, 1.25f, ValueFormat::QFactor));
    params.push_back(makeCalibratedFloat(ParamIDs::filtPreTrim,   "Filter / Preamp Trim", -12.0f, 12.0f, 0.0f, 0.0f, ValueFormat::Decibels));

    // 2. Dynamics
    params.push_back(makeBool           (ParamIDs::dynEnabled,      "Dynamics In", true));
    params.push_back(makeCalibratedFloat(ParamIDs::gateThresh,      "Gate Threshold", -70.0f, 0.0f, -55.0f, -28.0f, ValueFormat::Decibels));
    params.push_back(makeCalibratedFloat(ParamIDs::gateRange,       "Gate Range", 0.0f, 40.0f, 0.0f, 16.0f, ValueFormat::Decibels));
    params.push_back(makeCalibratedFloat(ParamIDs::compThresh,      "Comp Threshold", -40.0f, 0.0f, -14.0f, -14.0f, ValueFormat::Decibels));
    params.push_back(makeCalibratedFloat(ParamIDs::compRatio,       "Comp Ratio", 1.0f, 20.0f, 4.0f, 4.0f, ValueFormat::Ratio));
    params.push_back(makeCalibratedFloat(ParamIDs::compAttack,      "Comp Attack", 0.5f, 50.0f, 10.0f, 10.0f, ValueFormat::Milliseconds));
    params.push_back(makeCalibratedFloat(ParamIDs::compRelease,     "Comp Release", 20.0f, 1000.0f, 120.0f, 160.0f, ValueFormat::Milliseconds));
    params.push_back(makeBool           (ParamIDs::compAutoRelease, "Comp Auto-Release (Dual RC)", false));
    params.push_back(makeBool           (ParamIDs::limiterMode,     "Comp/Lim Mode (INF:1)", false));
    params.push_back(makeCalibratedFloat(ParamIDs::makeupGain,      "Dynamics Makeup", -12.0f, 24.0f, 0.0f, 3.0f, ValueFormat::Decibels));

    params.push_back(makeBool           (ParamIDs::extSidechain,    "External Sidechain", false));
    params.push_back(makeBool           (ParamIDs::filtersToSc,     "Filters to Sidechain", false));
    params.push_back(makeBool           (ParamIDs::scListen,        "Sidechain Listen", false));
    params.push_back(makeCalibratedFloat(ParamIDs::scHpfFreq,       "Dedicated Sidechain HPF", 20.0f, 300.0f, 20.0f, 85.0f, ValueFormat::Hertz));

    params.push_back(makeBool           (ParamIDs::deessIn,         "De-Esser In", false));
    params.push_back(makeCalibratedFloat(ParamIDs::deessFreq,       "De-Esser Freq", 4000.0f, 10000.0f, 6500.0f, 6500.0f, ValueFormat::Hertz));
    params.push_back(makeCalibratedFloat(ParamIDs::deessThresh,     "De-Esser Threshold", -40.0f, 0.0f, -18.0f, -16.0f, ValueFormat::Decibels));

    // 3. Console Color
    params.push_back(makeCalibratedFloat(ParamIDs::colorAmount,     "Color Intensity", 0.0f, 100.0f, 25.0f, 35.0f, ValueFormat::Percent));
    params.push_back(makeBool           (ParamIDs::colorLinkComp,   "Color Link to Comp/Lim", true));
    params.push_back(makeBool           (ParamIDs::circuitMode,     "Console Circuit (E/G)", true));
    params.push_back(makeCalibratedFloat(ParamIDs::satTone,         "Saturation Tone", -100.0f, 100.0f, 0.0f, 0.0f, ValueFormat::ToneTilt));
    params.push_back(makeCalibratedFloat(ParamIDs::crosstalk,       "Analog Crosstalk", 0.0f, 100.0f, 0.0f, 35.0f, ValueFormat::Percent));
    params.push_back(makeCalibratedFloat(ParamIDs::colorTrim,       "Color Stage Trim", -12.0f, 12.0f, 0.0f, 0.0f, ValueFormat::Decibels));

    // 4. Parametric Equaliser + 6 Permutations d'Ordre des Modules Centraux
    params.push_back(makeBool           (ParamIDs::eqEnabled,       "EQ In", true));
    params.push_back(makeBool           (ParamIDs::eqPreDyn,        "EQ Pre Dynamics", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamIDs::moduleOrder, 1 },
        "Central Module Routing Order",
        juce::StringArray {
            "DYN -> COLOR -> EQ",
            "DYN -> EQ -> COLOR",
            "COLOR -> DYN -> EQ",
            "COLOR -> EQ -> DYN",
            "EQ -> DYN -> COLOR",
            "EQ -> COLOR -> DYN"
        },
        0));
    params.push_back(makeBool           (ParamIDs::lfBell,          "LF Bell Mode", false));
    params.push_back(makeCalibratedFloat(ParamIDs::lfFreq,          "LF Freq", 30.0f, 450.0f, 80.0f, 110.0f, ValueFormat::Hertz));
    params.push_back(makeCalibratedFloat(ParamIDs::lfGain,          "LF Gain", -15.0f, 15.0f, 0.0f, 0.0f, ValueFormat::Decibels));
    params.push_back(makeCalibratedFloat(ParamIDs::lmfFreq,         "LMF Freq", 200.0f, 2500.0f, 450.0f, 650.0f, ValueFormat::Hertz));
    params.push_back(makeCalibratedFloat(ParamIDs::lmfGain,         "LMF Gain", -15.0f, 15.0f, 0.0f, 0.0f, ValueFormat::Decibels));
    params.push_back(makeCalibratedFloat(ParamIDs::lmfQ,            "LMF Q", 0.40f, 4.00f, 1.20f, 1.25f, ValueFormat::QFactor));
    params.push_back(makeCalibratedFloat(ParamIDs::hmfFreq,         "HMF Freq", 600.0f, 7000.0f, 2500.0f, 2200.0f, ValueFormat::Hertz));
    params.push_back(makeCalibratedFloat(ParamIDs::hmfGain,         "HMF Gain", -15.0f, 15.0f, 0.0f, 0.0f, ValueFormat::Decibels));
    params.push_back(makeCalibratedFloat(ParamIDs::hmfQ,            "HMF Q", 0.40f, 4.00f, 1.20f, 1.25f, ValueFormat::QFactor));
    params.push_back(makeBool           (ParamIDs::hfBell,          "HF Bell Mode", false));
    params.push_back(makeCalibratedFloat(ParamIDs::hfFreq,          "HF Freq", 1500.0f, 18000.0f, 10000.0f, 6500.0f, ValueFormat::Hertz));
    params.push_back(makeCalibratedFloat(ParamIDs::hfGain,          "HF Gain", -15.0f, 15.0f, 0.0f, 0.0f, ValueFormat::Decibels));
    params.push_back(makeCalibratedFloat(ParamIDs::eqTrim,          "EQ Stage Trim", -12.0f, 12.0f, 0.0f, 0.0f, ValueFormat::Decibels));

    // 5. Output, True Dynamic Auto-Gain, Mid/Side Mode, Analog Tolerance, Console Mix & Out Fader
    params.push_back(makeBool           (ParamIDs::autoGain,        "Auto-Gain", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamIDs::msMode, 1 },
        "Stereo / Mid-Side Routing",
        juce::StringArray { "L/R Stereo", "M/S (Mid+Side)", "M/S (Mid Only)", "M/S (Side Only)" },
        0));
    params.push_back(makeCalibratedFloat(ParamIDs::analogTolerance, "Analog L/R Tolerance", 0.0f, 100.0f, 25.0f, 35.0f, ValueFormat::Percent));
    params.push_back(makeCalibratedFloat(ParamIDs::outputGain,      "Output Fader", -24.0f, 12.0f, 0.0f, 0.0f, ValueFormat::Decibels));
    params.push_back(makeCalibratedFloat(ParamIDs::mix,             "Console Mix", 0.0f, 100.0f, 100.0f, 50.0f, ValueFormat::Percent));

    // 6. Global Settings
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamIDs::oversampling, 1 },
        "Oversampling",
        juce::StringArray { "1x (Off)", "2x", "4x" },
        0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamIDs::meterMode, 1 },
        "Meter Scale",
        juce::StringArray { "dBFS", "VU (dB)", "LUFS" },
        0));
    params.push_back(makeCalibratedFloat(ParamIDs::vuRefDb,         "0 VU Reference", -24.0f, -6.0f, -18.0f, -16.0f, ValueFormat::Decibels));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamIDs::fftMode, 1 },
        "FFT Spectrum Mode",
        juce::StringArray { "POST EQ", "PRE EQ", "OFF" },
        0));
    params.push_back(makeBool           (ParamIDs::dayMode,         "Day Mode Theme", false));

    return { params.begin(), params.end() };
}

void KalyxSSLConsoleProcessor::runZeroInputSilenceVerification(double sampleRate) noexcept
{
    // Test unitaire formel de silence au démarrage de prepareToPlay :
    // Vérifie qu'avec PREAMP SAT = 100%, COLOR DRIVE = 100% et ANALOG TOLERANCE = 100%,
    // une entrée de zéros (0.0f) produit strictement une sortie < 1e-6f (-120 dBFS).
    kalyx::dsp::SSLChannelStripDSP testDsp;
    testDsp.prepare(sampleRate, 64);

    kalyx::dsp::SSLParameters testParams {};
    testParams.inputSatDrive   = 1.0f;
    testParams.colorAmount     = 1.0f;
    testParams.analogTolerance = 1.0f;
    testParams.autoGain        = true;
    testParams.circuit4000G    = false; // 4000 E (hystérésis max)
    testDsp.updateFiltersAndCoefficients(testParams);

    float maxAbsOut = 0.0f;
    for (int i = 0; i < 128; ++i)
    {
        float l = 0.0f, r = 0.0f, refL = 0.0f, refR = 0.0f;
        float cGr = 0.0f, gGr = 0.0f, dGr = 0.0f, effCol = 0.0f;
        float cDet = -100.0f, gDet = -100.0f, dDet = -100.0f, ag = 0.0f;

        testDsp.processInputAndPreampStage(testParams, l, r, refL, refR);
        testDsp.processAllCentralModulesOrdered(testParams, l, r, 0.0f, 0.0f,
                                                cGr, gGr, dGr, effCol, cDet, gDet, dDet);
        testDsp.processOutputStage(testParams, 0.0f, 0.0f, refL, refR, l, r, ag);
        maxAbsOut = std::max(maxAbsOut, std::max(std::abs(l), std::abs(r)));
    }
    jassert(maxAbsOut < 1e-6f);
}

void KalyxSSLConsoleProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate_.store(sampleRate, std::memory_order_relaxed);
    const int safeBlockSize = std::max(samplesPerBlock, 64);

    runZeroInputSilenceVerification(sampleRate);

    dspBaseEngine_.prepare(sampleRate, safeBlockSize);
    dspNonlinearEngines_[0].prepare(sampleRate, safeBlockSize);
    dspNonlinearEngines_[1].prepare(sampleRate * 2.0, safeBlockSize * 2);
    dspNonlinearEngines_[2].prepare(sampleRate * 4.0, safeBlockSize * 4);

    if (oversampler2x_)
    {
        oversampler2x_->initProcessing(static_cast<std::size_t>(safeBlockSize));
        oversampler2x_->reset();
    }
    if (oversampler4x_)
    {
        oversampler4x_->initProcessing(static_cast<std::size_t>(safeBlockSize));
        oversampler4x_->reset();
    }

    preAllocatedWorkBuffer_.setSize(4, safeBlockSize * 4, false, true, false);
    preAllocatedDryBuffer_.setSize(2, safeBlockSize * 4, false, true, false);
    preAllocatedRefBuffer_.setSize(2, safeBlockSize * 4, false, true, false);
    activeOversamplingIndex_ = std::clamp(static_cast<int>(std::round(pOversampling_->load(std::memory_order_relaxed))), 0, 2);

    fftFifo_.fill(0.0f);
    fftTransferBlock_.fill(0.0f);
    fftFifoIndex_ = 0;
    nextFFTBlockReady_.store(false, std::memory_order_relaxed);

    constexpr double rampSeconds = 0.015;
    smoothInputGain_.reset(sampleRate, rampSeconds);
    smoothInputSatDrive_.reset(sampleRate, rampSeconds);
    smoothHpfFreq_.reset(sampleRate, rampSeconds);
    smoothHpfQ_.reset(sampleRate, rampSeconds);
    smoothLpfFreq_.reset(sampleRate, rampSeconds);
    smoothLpfQ_.reset(sampleRate, rampSeconds);
    smoothFiltPreTrim_.reset(sampleRate, rampSeconds);
    smoothGateThresh_.reset(sampleRate, rampSeconds);
    smoothGateRange_.reset(sampleRate, rampSeconds);
    smoothCompThresh_.reset(sampleRate, rampSeconds);
    smoothCompRatio_.reset(sampleRate, rampSeconds);
    smoothCompAttack_.reset(sampleRate, rampSeconds);
    smoothCompRelease_.reset(sampleRate, rampSeconds);
    smoothMakeupGain_.reset(sampleRate, rampSeconds);
    smoothScHpfFreq_.reset(sampleRate, rampSeconds);
    smoothDeessFreq_.reset(sampleRate, rampSeconds);
    smoothDeessThresh_.reset(sampleRate, rampSeconds);
    smoothColorAmount_.reset(sampleRate, rampSeconds);
    smoothSatTone_.reset(sampleRate, rampSeconds);
    smoothCrosstalk_.reset(sampleRate, rampSeconds);
    smoothColorTrim_.reset(sampleRate, rampSeconds);
    smoothLfFreq_.reset(sampleRate, rampSeconds);
    smoothLfGain_.reset(sampleRate, rampSeconds);
    smoothLmfFreq_.reset(sampleRate, rampSeconds);
    smoothLmfGain_.reset(sampleRate, rampSeconds);
    smoothLmfQ_.reset(sampleRate, rampSeconds);
    smoothHmfFreq_.reset(sampleRate, rampSeconds);
    smoothHmfGain_.reset(sampleRate, rampSeconds);
    smoothHmfQ_.reset(sampleRate, rampSeconds);
    smoothHfFreq_.reset(sampleRate, rampSeconds);
    smoothHfGain_.reset(sampleRate, rampSeconds);
    smoothEqTrim_.reset(sampleRate, rampSeconds);
    smoothAnalogTolerance_.reset(sampleRate, rampSeconds);
    smoothOutputGain_.reset(sampleRate, rampSeconds);
    smoothMix_.reset(sampleRate, rampSeconds);

    updateSmoothedTargetsFromAPVTS();

    smoothInputGain_.setCurrentAndTargetValue(smoothInputGain_.getTargetValue());
    smoothInputSatDrive_.setCurrentAndTargetValue(smoothInputSatDrive_.getTargetValue());
    smoothHpfFreq_.setCurrentAndTargetValue(smoothHpfFreq_.getTargetValue());
    smoothHpfQ_.setCurrentAndTargetValue(smoothHpfQ_.getTargetValue());
    smoothLpfFreq_.setCurrentAndTargetValue(smoothLpfFreq_.getTargetValue());
    smoothLpfQ_.setCurrentAndTargetValue(smoothLpfQ_.getTargetValue());
    smoothFiltPreTrim_.setCurrentAndTargetValue(smoothFiltPreTrim_.getTargetValue());
    smoothGateThresh_.setCurrentAndTargetValue(smoothGateThresh_.getTargetValue());
    smoothGateRange_.setCurrentAndTargetValue(smoothGateRange_.getTargetValue());
    smoothCompThresh_.setCurrentAndTargetValue(smoothCompThresh_.getTargetValue());
    smoothCompRatio_.setCurrentAndTargetValue(smoothCompRatio_.getTargetValue());
    smoothCompAttack_.setCurrentAndTargetValue(smoothCompAttack_.getTargetValue());
    smoothCompRelease_.setCurrentAndTargetValue(smoothCompRelease_.getTargetValue());
    smoothMakeupGain_.setCurrentAndTargetValue(smoothMakeupGain_.getTargetValue());
    smoothScHpfFreq_.setCurrentAndTargetValue(smoothScHpfFreq_.getTargetValue());
    smoothDeessFreq_.setCurrentAndTargetValue(smoothDeessFreq_.getTargetValue());
    smoothDeessThresh_.setCurrentAndTargetValue(smoothDeessThresh_.getTargetValue());
    smoothColorAmount_.setCurrentAndTargetValue(smoothColorAmount_.getTargetValue());
    smoothSatTone_.setCurrentAndTargetValue(smoothSatTone_.getTargetValue());
    smoothCrosstalk_.setCurrentAndTargetValue(smoothCrosstalk_.getTargetValue());
    smoothColorTrim_.setCurrentAndTargetValue(smoothColorTrim_.getTargetValue());
    smoothLfFreq_.setCurrentAndTargetValue(smoothLfFreq_.getTargetValue());
    smoothLfGain_.setCurrentAndTargetValue(smoothLfGain_.getTargetValue());
    smoothLmfFreq_.setCurrentAndTargetValue(smoothLmfFreq_.getTargetValue());
    smoothLmfGain_.setCurrentAndTargetValue(smoothLmfGain_.getTargetValue());
    smoothLmfQ_.setCurrentAndTargetValue(smoothLmfQ_.getTargetValue());
    smoothHmfFreq_.setCurrentAndTargetValue(smoothHmfFreq_.getTargetValue());
    smoothHmfGain_.setCurrentAndTargetValue(smoothHmfGain_.getTargetValue());
    smoothHmfQ_.setCurrentAndTargetValue(smoothHmfQ_.getTargetValue());
    smoothHfFreq_.setCurrentAndTargetValue(smoothHfFreq_.getTargetValue());
    smoothHfGain_.setCurrentAndTargetValue(smoothHfGain_.getTargetValue());
    smoothEqTrim_.setCurrentAndTargetValue(smoothEqTrim_.getTargetValue());
    smoothAnalogTolerance_.setCurrentAndTargetValue(smoothAnalogTolerance_.getTargetValue());
    smoothOutputGain_.setCurrentAndTargetValue(smoothOutputGain_.getTargetValue());
    smoothMix_.setCurrentAndTargetValue(smoothMix_.getTargetValue());
}

void KalyxSSLConsoleProcessor::releaseResources()
{
    dspBaseEngine_.reset();
    for (auto& eng : dspNonlinearEngines_)
        eng.reset();
    if (oversampler2x_)
        oversampler2x_->reset();
    if (oversampler4x_)
        oversampler4x_->reset();
}

bool KalyxSSLConsoleProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    if (mainIn != mainOut)
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto& scBus = layouts.getChannelSet(true, 1);
        if (!scBus.isDisabled()
            && scBus != juce::AudioChannelSet::mono()
            && scBus != juce::AudioChannelSet::stereo())
        {
            return false;
        }
    }

    return true;
}

void KalyxSSLConsoleProcessor::pushSampleToFFTFifo(float monoSample) noexcept
{
    if (fftFifoIndex_ == kFFTSize)
    {
        if (!nextFFTBlockReady_.load(std::memory_order_relaxed))
        {
            std::memcpy(fftTransferBlock_.data(), fftFifo_.data(), sizeof(float) * static_cast<std::size_t>(kFFTSize));
            nextFFTBlockReady_.store(true, std::memory_order_release);
        }
        fftFifoIndex_ = 0;
    }
    fftFifo_[static_cast<std::size_t>(fftFifoIndex_++)] = monoSample;
}

bool KalyxSSLConsoleProcessor::pullNextFFTBlock(float* destBuffer2048) noexcept
{
    if (!nextFFTBlockReady_.load(std::memory_order_acquire))
        return false;

    std::memcpy(destBuffer2048, fftTransferBlock_.data(), sizeof(float) * static_cast<std::size_t>(kFFTSize));
    nextFFTBlockReady_.store(false, std::memory_order_relaxed);
    return true;
}

void KalyxSSLConsoleProcessor::updateSmoothedTargetsFromAPVTS() noexcept
{
    smoothInputGain_.setTargetValue(pInputGain_->load(std::memory_order_relaxed));
    smoothInputSatDrive_.setTargetValue(pInputSatDrive_->load(std::memory_order_relaxed) * 0.01f);
    smoothHpfFreq_.setTargetValue(pHpfFreq_->load(std::memory_order_relaxed));
    smoothHpfQ_.setTargetValue(pHpfQ_->load(std::memory_order_relaxed));
    smoothLpfFreq_.setTargetValue(pLpfFreq_->load(std::memory_order_relaxed));
    smoothLpfQ_.setTargetValue(pLpfQ_->load(std::memory_order_relaxed));
    smoothFiltPreTrim_.setTargetValue(pFiltPreTrim_->load(std::memory_order_relaxed));
    smoothGateThresh_.setTargetValue(pGateThresh_->load(std::memory_order_relaxed));
    smoothGateRange_.setTargetValue(pGateRange_->load(std::memory_order_relaxed));
    smoothCompThresh_.setTargetValue(pCompThresh_->load(std::memory_order_relaxed));
    smoothCompRatio_.setTargetValue(pCompRatio_->load(std::memory_order_relaxed));
    smoothCompAttack_.setTargetValue(pCompAttack_->load(std::memory_order_relaxed));
    smoothCompRelease_.setTargetValue(pCompRelease_->load(std::memory_order_relaxed));
    smoothMakeupGain_.setTargetValue(pMakeupGain_->load(std::memory_order_relaxed));
    smoothScHpfFreq_.setTargetValue(pScHpfFreq_->load(std::memory_order_relaxed));
    smoothDeessFreq_.setTargetValue(pDeessFreq_->load(std::memory_order_relaxed));
    smoothDeessThresh_.setTargetValue(pDeessThresh_->load(std::memory_order_relaxed));
    smoothColorAmount_.setTargetValue(pColorAmount_->load(std::memory_order_relaxed) * 0.01f);
    smoothSatTone_.setTargetValue(pSatTone_->load(std::memory_order_relaxed) * 0.01f);
    smoothCrosstalk_.setTargetValue(pCrosstalk_->load(std::memory_order_relaxed) * 0.01f);
    smoothColorTrim_.setTargetValue(pColorTrim_->load(std::memory_order_relaxed));
    smoothLfFreq_.setTargetValue(pLfFreq_->load(std::memory_order_relaxed));
    smoothLfGain_.setTargetValue(pLfGain_->load(std::memory_order_relaxed));
    smoothLmfFreq_.setTargetValue(pLmfFreq_->load(std::memory_order_relaxed));
    smoothLmfGain_.setTargetValue(pLmfGain_->load(std::memory_order_relaxed));
    smoothLmfQ_.setTargetValue(pLmfQ_->load(std::memory_order_relaxed));
    smoothHmfFreq_.setTargetValue(pHmfFreq_->load(std::memory_order_relaxed));
    smoothHmfGain_.setTargetValue(pHmfGain_->load(std::memory_order_relaxed));
    smoothHmfQ_.setTargetValue(pHmfQ_->load(std::memory_order_relaxed));
    smoothHfFreq_.setTargetValue(pHfFreq_->load(std::memory_order_relaxed));
    smoothHfGain_.setTargetValue(pHfGain_->load(std::memory_order_relaxed));
    smoothEqTrim_.setTargetValue(pEqTrim_->load(std::memory_order_relaxed));
    smoothAnalogTolerance_.setTargetValue(pAnalogTolerance_->load(std::memory_order_relaxed) * 0.01f);
    smoothOutputGain_.setTargetValue(pOutputGain_->load(std::memory_order_relaxed));
    smoothMix_.setTargetValue(pMix_->load(std::memory_order_relaxed) * 0.01f);
}

void KalyxSSLConsoleProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    // 1. Isolation stricte des bus 4-in / 2-out
    for (int i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, numSamples);

    auto mainInput  = getBusBuffer(buffer, true,  0);
    auto mainOutput = getBusBuffer(buffer, false, 0);
    const int inChans  = mainInput.getNumChannels();
    const int outChans = mainOutput.getNumChannels();

    if (inChans <= 0 || outChans <= 0)
    {
        buffer.clear();
        return;
    }

    for (int i = inChans; i < outChans; ++i)
        mainOutput.clear(i, 0, numSamples);

    // Récupération isolée du bus Sidechain (ne sera JAMAIS copié ni mélangé dans mainOutput sauf si SC LISTEN est actif)
    const float* sc0 = nullptr;
    const float* sc1 = nullptr;
    if (getBusCount(true) > 1)
    {
        auto scInput = getBusBuffer(buffer, true, 1);
        if (scInput.getNumChannels() > 0)
        {
            sc0 = scInput.getReadPointer(0);
            sc1 = (scInput.getNumChannels() > 1) ? scInput.getReadPointer(1) : sc0;
        }
    }

    const float* in0 = mainInput.getReadPointer(0);
    const float* in1 = (inChans > 1) ? mainInput.getReadPointer(1) : in0;
    float* out0      = mainOutput.getWritePointer(0);
    float* out1      = (outChans > 1) ? mainOutput.getWritePointer(1) : nullptr;

    updateSmoothedTargetsFromAPVTS();

    kalyx::dsp::SSLParameters currentParams {};
    currentParams.phaseInvert     = (pPhaseInvert_->load(std::memory_order_relaxed) > 0.5f);
    currentParams.hpfEnabled      = (pHpfEnabled_->load(std::memory_order_relaxed) > 0.5f);
    currentParams.lpfEnabled      = (pLpfEnabled_->load(std::memory_order_relaxed) > 0.5f);
    currentParams.dynEnabled      = (pDynEnabled_->load(std::memory_order_relaxed) > 0.5f);
    currentParams.compAutoRelease = (pCompAutoRelease_->load(std::memory_order_relaxed) > 0.5f);
    currentParams.limiterMode     = (pLimiterMode_->load(std::memory_order_relaxed) > 0.5f);
    currentParams.extSidechain    = (pExtSidechain_->load(std::memory_order_relaxed) > 0.5f);
    currentParams.filtersToSc     = (pFiltersToSc_->load(std::memory_order_relaxed) > 0.5f);
    currentParams.scListen        = (pScListen_->load(std::memory_order_relaxed) > 0.5f);
    currentParams.deessEnabled    = (pDeessIn_->load(std::memory_order_relaxed) > 0.5f);
    currentParams.colorLinkComp   = (pColorLinkComp_->load(std::memory_order_relaxed) > 0.5f);
    currentParams.circuit4000G    = (pCircuitMode_->load(std::memory_order_relaxed) > 0.5f);
    currentParams.eqEnabled       = (pEqEnabled_->load(std::memory_order_relaxed) > 0.5f);
    currentParams.eqPreDyn        = (pEqPreDyn_->load(std::memory_order_relaxed) > 0.5f);
    currentParams.moduleOrder     = std::clamp(static_cast<int>(std::round(pModuleOrder_->load(std::memory_order_relaxed))), 0, 5);
    currentParams.lfBell          = (pLfBell_->load(std::memory_order_relaxed) > 0.5f);
    currentParams.hfBell          = (pHfBell_->load(std::memory_order_relaxed) > 0.5f);
    currentParams.autoGain        = (pAutoGain_->load(std::memory_order_relaxed) > 0.5f);
    currentParams.msMode          = std::clamp(static_cast<int>(std::round(pMsMode_->load(std::memory_order_relaxed))), 0, 3);

    const int fftMode = std::clamp(static_cast<int>(std::round(pFftMode_->load(std::memory_order_relaxed))), 0, 2);

    int requestedOsIdx = std::clamp(static_cast<int>(std::round(pOversampling_->load(std::memory_order_relaxed))), 0, 2);
    if (numSamples > preAllocatedWorkBuffer_.getNumSamples()
        || numSamples > preAllocatedDryBuffer_.getNumSamples()
        || numSamples > preAllocatedRefBuffer_.getNumSamples())
    {
        requestedOsIdx = 0;
    }

    if (requestedOsIdx != activeOversamplingIndex_)
    {
        activeOversamplingIndex_ = requestedOsIdx;
        dspNonlinearEngines_[static_cast<std::size_t>(activeOversamplingIndex_)].reset();
        if (oversampler2x_) oversampler2x_->reset();
        if (oversampler4x_) oversampler4x_->reset();
    }

    float maxInPeakL    = 0.0f;
    float maxInPeakR    = 0.0f;
    float maxOutPeakL   = 0.0f;
    float maxOutPeakR   = 0.0f;
    float maxCompGrDb   = 0.0f;
    float maxGateGrDb   = 0.0f;
    float maxDeessGrDb  = 0.0f;
    float lastColor     = 0.0f;
    float maxCompDetDb  = -100.0f;
    float maxGateDetDb  = -100.0f;
    float maxDeessDetDb = -100.0f;
    float lastInVuDbL   = -100.0f;
    float lastInVuDbR   = -100.0f;
    float lastOutVuDbL  = -100.0f;
    float lastOutVuDbR  = -100.0f;
    float lastInLufsL   = -100.0f;
    float lastInLufsR   = -100.0f;
    float lastOutLufsL  = -100.0f;
    float lastOutLufsR  = -100.0f;
    float lastAutoGain  = 0.0f;

    auto& nlEngine = dspNonlinearEngines_[static_cast<std::size_t>(activeOversamplingIndex_)];
    constexpr int coeffUpdateInterval = 16;
    constexpr float invSqrt2 = 0.70710678f;

    // =========================================================================
    // Chemin 1 : Sans Oversampling (1x)
    // =========================================================================
    if (activeOversamplingIndex_ == 0)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            currentParams.inputGainDb     = smoothInputGain_.getNextValue();
            currentParams.inputSatDrive   = smoothInputSatDrive_.getNextValue();
            currentParams.hpfFreqHz       = smoothHpfFreq_.getNextValue();
            currentParams.hpfQ            = smoothHpfQ_.getNextValue();
            currentParams.lpfFreqHz       = smoothLpfFreq_.getNextValue();
            currentParams.lpfQ            = smoothLpfQ_.getNextValue();
            currentParams.filtPreTrimDb   = smoothFiltPreTrim_.getNextValue();
            currentParams.gateThreshDb    = smoothGateThresh_.getNextValue();
            currentParams.gateRangeDb     = smoothGateRange_.getNextValue();
            currentParams.compThreshDb    = smoothCompThresh_.getNextValue();
            currentParams.compRatio       = smoothCompRatio_.getNextValue();
            currentParams.compAttackMs    = smoothCompAttack_.getNextValue();
            currentParams.compReleaseMs   = smoothCompRelease_.getNextValue();
            currentParams.makeupGainDb    = smoothMakeupGain_.getNextValue();
            currentParams.scHpfFreqHz     = smoothScHpfFreq_.getNextValue();
            currentParams.deessFreqHz     = smoothDeessFreq_.getNextValue();
            currentParams.deessThreshDb   = smoothDeessThresh_.getNextValue();
            currentParams.colorAmount     = smoothColorAmount_.getNextValue();
            currentParams.satTone         = smoothSatTone_.getNextValue();
            currentParams.crosstalk       = smoothCrosstalk_.getNextValue();
            currentParams.colorTrimDb     = smoothColorTrim_.getNextValue();
            currentParams.lfFreqHz        = smoothLfFreq_.getNextValue();
            currentParams.lfGainDb        = smoothLfGain_.getNextValue();
            currentParams.lmfFreqHz       = smoothLmfFreq_.getNextValue();
            currentParams.lmfGainDb       = smoothLmfGain_.getNextValue();
            currentParams.lmfQ            = smoothLmfQ_.getNextValue();
            currentParams.hmfFreqHz       = smoothHmfFreq_.getNextValue();
            currentParams.hmfGainDb       = smoothHmfGain_.getNextValue();
            currentParams.hmfQ            = smoothHmfQ_.getNextValue();
            currentParams.hfFreqHz        = smoothHfFreq_.getNextValue();
            currentParams.hfGainDb        = smoothHfGain_.getNextValue();
            currentParams.eqTrimDb        = smoothEqTrim_.getNextValue();
            currentParams.analogTolerance = smoothAnalogTolerance_.getNextValue();
            currentParams.outputGainDb    = smoothOutputGain_.getNextValue();
            currentParams.mix             = smoothMix_.getNextValue();

            if ((i & (coeffUpdateInterval - 1)) == 0)
            {
                dspBaseEngine_.updateFiltersAndCoefficients(currentParams);
                nlEngine.updateFiltersAndCoefficients(currentParams);
            }

            float left  = in0[i];
            float right = (in1 != nullptr) ? in1[i] : left;
            if (!std::isfinite(left))  left  = 0.0f;
            if (!std::isfinite(right)) right = 0.0f;

            const float dryLeft  = left;
            const float dryRight = right;

            float extL = (sc0 != nullptr) ? sc0[i] : left;
            float extR = (sc1 != nullptr) ? sc1[i] : right;
            if (!std::isfinite(extL)) extL = 0.0f;
            if (!std::isfinite(extR)) extR = 0.0f;

            // 1. Section 1 : Input Trim, Phase, Preamp Sat (ADAA 64-bit), Filters (ZDF) & FILT/PRE TRIM
            float postLineL = left;
            float postLineR = right;
            dspBaseEngine_.processInputAndPreampStage(currentParams, left, right, postLineL, postLineR);

            maxInPeakL = std::max(maxInPeakL, std::abs(postLineL));
            maxInPeakR = std::max(maxInPeakR, std::abs(postLineR));

            if (fftMode == 1)
                pushSampleToFFTFifo(0.5f * (left + right));

            // 2. Encodage Mid/Side à gain unitaire autour des 3 modules centraux
            if (currentParams.msMode > 0)
            {
                const float mid  = (left + right) * invSqrt2;
                const float side = (left - right) * invSqrt2;
                left  = mid;
                right = side;
            }

            // 3. Exécution des 3 modules centraux dans l'ordre exact choisi (kModulePermutations[moduleOrder])
            float sampleCompGrDb  = 0.0f;
            float sampleGateGrDb  = 0.0f;
            float sampleDeessGrDb = 0.0f;
            float sampleEffColor  = 0.0f;
            float sampleCompDet   = -100.0f;
            float sampleGateDet   = -100.0f;
            float sampleDeessDet  = -100.0f;

            nlEngine.processAllCentralModulesOrdered(currentParams, left, right, extL, extR,
                                                     sampleCompGrDb, sampleGateGrDb, sampleDeessGrDb,
                                                     sampleEffColor, sampleCompDet, sampleGateDet, sampleDeessDet);

            // 4. Décodage Mid/Side à gain unitaire
            if (currentParams.msMode > 0 && !currentParams.scListen)
            {
                const float mid  = left;
                const float side = right;
                left  = (mid + side) * invSqrt2;
                right = (mid - side) * invSqrt2;
            }

            // 5. Section 5 : Output Stage (Auto-Gain RMS gelé sous -60 dBFS, Console Mix, Out Fader)
            dspBaseEngine_.processOutputStage(currentParams, dryLeft, dryRight, postLineL, postLineR,
                                              left, right, lastAutoGain);

            if (!std::isfinite(left))  left  = 0.0f;
            if (!std::isfinite(right)) right = 0.0f;

            left  = std::clamp(left,  -4.0f, 4.0f);
            right = std::clamp(right, -4.0f, 4.0f);

            if (fftMode == 0)
                pushSampleToFFTFifo(0.5f * (left + right));

            out0[i] = left;
            if (out1 != nullptr)
                out1[i] = right;

            maxOutPeakL   = std::max(maxOutPeakL, std::abs(left));
            maxOutPeakR   = std::max(maxOutPeakR, std::abs(right));
            maxCompGrDb   = std::max(maxCompGrDb, sampleCompGrDb);
            maxGateGrDb   = std::max(maxGateGrDb, sampleGateGrDb);
            maxDeessGrDb  = std::max(maxDeessGrDb, sampleDeessGrDb);
            maxCompDetDb  = std::max(maxCompDetDb, sampleCompDet);
            maxGateDetDb  = std::max(maxGateDetDb, sampleGateDet);
            maxDeessDetDb = std::max(maxDeessDetDb, sampleDeessDet);
            lastColor     = sampleEffColor;

            dspBaseEngine_.updateLevelMetersSample(postLineL, postLineR, left, right,
                                                   lastInVuDbL, lastInVuDbR, lastOutVuDbL, lastOutVuDbR,
                                                   lastInLufsL, lastInLufsR, lastOutLufsL, lastOutLufsR);
        }
    }
    // =========================================================================
    // Chemin 2 : Oversampling 2x ou 4x sur les 3 modules centraux
    // =========================================================================
    else
    {
        float* workL   = preAllocatedWorkBuffer_.getWritePointer(0);
        float* workR   = preAllocatedWorkBuffer_.getWritePointer(1);
        float* workSL  = preAllocatedWorkBuffer_.getWritePointer(2);
        float* workSR  = preAllocatedWorkBuffer_.getWritePointer(3);
        float* dryBufL = preAllocatedDryBuffer_.getWritePointer(0);
        float* dryBufR = preAllocatedDryBuffer_.getWritePointer(1);
        float* refBufL = preAllocatedRefBuffer_.getWritePointer(0);
        float* refBufR = preAllocatedRefBuffer_.getWritePointer(1);

        for (int i = 0; i < numSamples; ++i)
        {
            currentParams.inputGainDb     = smoothInputGain_.getNextValue();
            currentParams.inputSatDrive   = smoothInputSatDrive_.getNextValue();
            currentParams.hpfFreqHz       = smoothHpfFreq_.getNextValue();
            currentParams.hpfQ            = smoothHpfQ_.getNextValue();
            currentParams.lpfFreqHz       = smoothLpfFreq_.getNextValue();
            currentParams.lpfQ            = smoothLpfQ_.getNextValue();
            currentParams.filtPreTrimDb   = smoothFiltPreTrim_.getNextValue();
            currentParams.gateThreshDb    = smoothGateThresh_.getNextValue();
            currentParams.gateRangeDb     = smoothGateRange_.getNextValue();
            currentParams.compThreshDb    = smoothCompThresh_.getNextValue();
            currentParams.compRatio       = smoothCompRatio_.getNextValue();
            currentParams.compAttackMs    = smoothCompAttack_.getNextValue();
            currentParams.compReleaseMs   = smoothCompRelease_.getNextValue();
            currentParams.makeupGainDb    = smoothMakeupGain_.getNextValue();
            currentParams.scHpfFreqHz     = smoothScHpfFreq_.getNextValue();
            currentParams.deessFreqHz     = smoothDeessFreq_.getNextValue();
            currentParams.deessThreshDb   = smoothDeessThresh_.getNextValue();
            currentParams.colorAmount     = smoothColorAmount_.getNextValue();
            currentParams.satTone         = smoothSatTone_.getNextValue();
            currentParams.crosstalk       = smoothCrosstalk_.getNextValue();
            currentParams.colorTrimDb     = smoothColorTrim_.getNextValue();
            currentParams.lfFreqHz        = smoothLfFreq_.getNextValue();
            currentParams.lfGainDb        = smoothLfGain_.getNextValue();
            currentParams.lmfFreqHz       = smoothLmfFreq_.getNextValue();
            currentParams.lmfGainDb       = smoothLmfGain_.getNextValue();
            currentParams.lmfQ            = smoothLmfQ_.getNextValue();
            currentParams.hmfFreqHz       = smoothHmfFreq_.getNextValue();
            currentParams.hmfGainDb       = smoothHmfGain_.getNextValue();
            currentParams.hmfQ            = smoothHmfQ_.getNextValue();
            currentParams.hfFreqHz        = smoothHfFreq_.getNextValue();
            currentParams.hfGainDb        = smoothHfGain_.getNextValue();
            currentParams.eqTrimDb        = smoothEqTrim_.getNextValue();
            currentParams.analogTolerance = smoothAnalogTolerance_.getNextValue();
            currentParams.outputGainDb    = smoothOutputGain_.getNextValue();
            currentParams.mix             = smoothMix_.getNextValue();

            if ((i & (coeffUpdateInterval - 1)) == 0)
                dspBaseEngine_.updateFiltersAndCoefficients(currentParams);

            float left  = in0[i];
            float right = (in1 != nullptr) ? in1[i] : left;
            if (!std::isfinite(left))  left  = 0.0f;
            if (!std::isfinite(right)) right = 0.0f;

            dryBufL[i] = left;
            dryBufR[i] = right;

            float extL = (sc0 != nullptr) ? sc0[i] : left;
            float extR = (sc1 != nullptr) ? sc1[i] : right;
            if (!std::isfinite(extL)) extL = 0.0f;
            if (!std::isfinite(extR)) extR = 0.0f;

            float postLineL = left;
            float postLineR = right;
            dspBaseEngine_.processInputAndPreampStage(currentParams, left, right, postLineL, postLineR);
            refBufL[i] = postLineL;
            refBufR[i] = postLineR;

            maxInPeakL = std::max(maxInPeakL, std::abs(postLineL));
            maxInPeakR = std::max(maxInPeakR, std::abs(postLineR));

            if (fftMode == 1)
                pushSampleToFFTFifo(0.5f * (left + right));

            if (currentParams.msMode > 0)
            {
                const float mid  = (left + right) * invSqrt2;
                const float side = (left - right) * invSqrt2;
                left  = mid;
                right = side;
            }

            workL[i]  = left;
            workR[i]  = right;
            workSL[i] = extL;
            workSR[i] = extR;
        }

        nlEngine.updateFiltersAndCoefficients(currentParams);
        auto* activeOs = (activeOversamplingIndex_ == 2) ? oversampler4x_.get() : oversampler2x_.get();

        juce::dsp::AudioBlock<float> baseBlock(preAllocatedWorkBuffer_);
        auto subBlock = baseBlock.getSubBlock(0, static_cast<std::size_t>(numSamples));
        auto osBlock  = activeOs->processSamplesUp(subBlock);

        const int osNumSamples = static_cast<int>(osBlock.getNumSamples());
        float* osL  = osBlock.getChannelPointer(0);
        float* osR  = osBlock.getChannelPointer(1);
        float* osSL = osBlock.getChannelPointer(2);
        float* osSR = osBlock.getChannelPointer(3);

        for (int i = 0; i < osNumSamples; ++i)
        {
            float sampleCompGrDb  = 0.0f;
            float sampleGateGrDb  = 0.0f;
            float sampleDeessGrDb = 0.0f;
            float sampleEffColor  = 0.0f;
            float sampleCompDet   = -100.0f;
            float sampleGateDet   = -100.0f;
            float sampleDeessDet  = -100.0f;

            nlEngine.processAllCentralModulesOrdered(currentParams, osL[i], osR[i], osSL[i], osSR[i],
                                                     sampleCompGrDb, sampleGateGrDb, sampleDeessGrDb,
                                                     sampleEffColor, sampleCompDet, sampleGateDet, sampleDeessDet);

            maxCompGrDb   = std::max(maxCompGrDb, sampleCompGrDb);
            maxGateGrDb   = std::max(maxGateGrDb, sampleGateGrDb);
            maxDeessGrDb  = std::max(maxDeessGrDb, sampleDeessGrDb);
            maxCompDetDb  = std::max(maxCompDetDb, sampleCompDet);
            maxGateDetDb  = std::max(maxGateDetDb, sampleGateDet);
            maxDeessDetDb = std::max(maxDeessDetDb, sampleDeessDet);
            lastColor     = sampleEffColor;
        }

        activeOs->processSamplesDown(subBlock);

        for (int i = 0; i < numSamples; ++i)
        {
            float left  = workL[i];
            float right = workR[i];

            if (currentParams.msMode > 0 && !currentParams.scListen)
            {
                const float mid  = left;
                const float side = right;
                left  = (mid + side) * invSqrt2;
                right = (mid - side) * invSqrt2;
            }

            dspBaseEngine_.processOutputStage(currentParams, dryBufL[i], dryBufR[i], refBufL[i], refBufR[i],
                                              left, right, lastAutoGain);

            if (!std::isfinite(left))  left  = 0.0f;
            if (!std::isfinite(right)) right = 0.0f;

            left  = std::clamp(left,  -4.0f, 4.0f);
            right = std::clamp(right, -4.0f, 4.0f);

            if (fftMode == 0)
                pushSampleToFFTFifo(0.5f * (left + right));

            out0[i] = left;
            if (out1 != nullptr)
                out1[i] = right;

            maxOutPeakL = std::max(maxOutPeakL, std::abs(left));
            maxOutPeakR = std::max(maxOutPeakR, std::abs(right));

            dspBaseEngine_.updateLevelMetersSample(refBufL[i], refBufR[i], left, right,
                                                   lastInVuDbL, lastInVuDbR, lastOutVuDbL, lastOutVuDbR,
                                                   lastInLufsL, lastInLufsR, lastOutLufsL, lastOutLufsR);
        }
    }

    const float inDbL  = kalyx::dsp::SSLChannelStripDSP::linearToDb(maxInPeakL,  -100.0f);
    const float inDbR  = kalyx::dsp::SSLChannelStripDSP::linearToDb(maxInPeakR,  -100.0f);
    const float outDbL = kalyx::dsp::SSLChannelStripDSP::linearToDb(maxOutPeakL, -100.0f);
    const float outDbR = kalyx::dsp::SSLChannelStripDSP::linearToDb(maxOutPeakR, -100.0f);

    meterInputPeakDbL_.store(std::max(inDbL,  meterInputPeakDbL_.load(std::memory_order_relaxed)  - 1.5f), std::memory_order_relaxed);
    meterInputPeakDbR_.store(std::max(inDbR,  meterInputPeakDbR_.load(std::memory_order_relaxed)  - 1.5f), std::memory_order_relaxed);
    meterOutputPeakDbL_.store(std::max(outDbL, meterOutputPeakDbL_.load(std::memory_order_relaxed) - 1.5f), std::memory_order_relaxed);
    meterOutputPeakDbR_.store(std::max(outDbR, meterOutputPeakDbR_.load(std::memory_order_relaxed) - 1.5f), std::memory_order_relaxed);

    meterInputVuDbL_.store(lastInVuDbL,   std::memory_order_relaxed);
    meterInputVuDbR_.store(lastInVuDbR,   std::memory_order_relaxed);
    meterOutputVuDbL_.store(lastOutVuDbL, std::memory_order_relaxed);
    meterOutputVuDbR_.store(lastOutVuDbR, std::memory_order_relaxed);

    meterInputLufsDbL_.store(lastInLufsL,   std::memory_order_relaxed);
    meterInputLufsDbR_.store(lastInLufsR,   std::memory_order_relaxed);
    meterOutputLufsDbL_.store(lastOutLufsL, std::memory_order_relaxed);
    meterOutputLufsDbR_.store(lastOutLufsR, std::memory_order_relaxed);

    meterCompDetectorDb_.store(std::max(maxCompDetDb,   meterCompDetectorDb_.load(std::memory_order_relaxed)  - 1.5f), std::memory_order_relaxed);
    meterGateDetectorDb_.store(std::max(maxGateDetDb,   meterGateDetectorDb_.load(std::memory_order_relaxed)  - 1.5f), std::memory_order_relaxed);
    meterDeessDetectorDb_.store(std::max(maxDeessDetDb, meterDeessDetectorDb_.load(std::memory_order_relaxed) - 1.5f), std::memory_order_relaxed);

    meterCompGrDb_.store(std::max(maxCompGrDb,   meterCompGrDb_.load(std::memory_order_relaxed)  * 0.85f), std::memory_order_relaxed);
    meterGateGrDb_.store(std::max(maxGateGrDb,   meterGateGrDb_.load(std::memory_order_relaxed)  * 0.85f), std::memory_order_relaxed);
    meterDeessGrDb_.store(std::max(maxDeessGrDb, meterDeessGrDb_.load(std::memory_order_relaxed) * 0.85f), std::memory_order_relaxed);

    meterDynamicColorDrive_.store(lastColor, std::memory_order_relaxed);
    meterAutoGainDb_.store(lastAutoGain, std::memory_order_relaxed);
}

kalyx::dsp::SSLMeterSnapshot KalyxSSLConsoleProcessor::getMeterSnapshot() const noexcept
{
    kalyx::dsp::SSLMeterSnapshot snap {};
    snap.inputPeakDbL      = meterInputPeakDbL_.load(std::memory_order_relaxed);
    snap.inputPeakDbR      = meterInputPeakDbR_.load(std::memory_order_relaxed);
    snap.outputPeakDbL     = meterOutputPeakDbL_.load(std::memory_order_relaxed);
    snap.outputPeakDbR     = meterOutputPeakDbR_.load(std::memory_order_relaxed);
    snap.inputVuDbL        = meterInputVuDbL_.load(std::memory_order_relaxed);
    snap.inputVuDbR        = meterInputVuDbR_.load(std::memory_order_relaxed);
    snap.outputVuDbL       = meterOutputVuDbL_.load(std::memory_order_relaxed);
    snap.outputVuDbR       = meterOutputVuDbR_.load(std::memory_order_relaxed);
    snap.inputLufsDbL      = meterInputLufsDbL_.load(std::memory_order_relaxed);
    snap.inputLufsDbR      = meterInputLufsDbR_.load(std::memory_order_relaxed);
    snap.outputLufsDbL     = meterOutputLufsDbL_.load(std::memory_order_relaxed);
    snap.outputLufsDbR     = meterOutputLufsDbR_.load(std::memory_order_relaxed);

    snap.compDetectorDb    = meterCompDetectorDb_.load(std::memory_order_relaxed);
    snap.gateDetectorDb    = meterGateDetectorDb_.load(std::memory_order_relaxed);
    snap.deessDetectorDb   = meterDeessDetectorDb_.load(std::memory_order_relaxed);

    snap.compGainRedDb     = meterCompGrDb_.load(std::memory_order_relaxed);
    snap.gateGainRedDb     = meterGateGrDb_.load(std::memory_order_relaxed);
    snap.deessGainRedDb    = meterDeessGrDb_.load(std::memory_order_relaxed);
    snap.dynamicColorDrive = meterDynamicColorDrive_.load(std::memory_order_relaxed);
    snap.autoGainAppliedDb = meterAutoGainDb_.load(std::memory_order_relaxed);
    return snap;
}

kalyx::dsp::SSLParameters KalyxSSLConsoleProcessor::getCurrentParametersForGUI() const noexcept
{
    kalyx::dsp::SSLParameters p {};
    p.inputGainDb     = pInputGain_->load(std::memory_order_relaxed);
    p.inputSatDrive   = pInputSatDrive_->load(std::memory_order_relaxed) * 0.01f;
    p.phaseInvert     = pPhaseInvert_->load(std::memory_order_relaxed) > 0.5f;
    p.hpfEnabled      = pHpfEnabled_->load(std::memory_order_relaxed) > 0.5f;
    p.hpfFreqHz       = pHpfFreq_->load(std::memory_order_relaxed);
    p.hpfQ            = pHpfQ_->load(std::memory_order_relaxed);
    p.lpfEnabled      = pLpfEnabled_->load(std::memory_order_relaxed) > 0.5f;
    p.lpfFreqHz       = pLpfFreq_->load(std::memory_order_relaxed);
    p.lpfQ            = pLpfQ_->load(std::memory_order_relaxed);
    p.filtPreTrimDb   = pFiltPreTrim_->load(std::memory_order_relaxed);
    p.dynEnabled      = pDynEnabled_->load(std::memory_order_relaxed) > 0.5f;
    p.gateThreshDb    = pGateThresh_->load(std::memory_order_relaxed);
    p.gateRangeDb     = pGateRange_->load(std::memory_order_relaxed);
    p.compThreshDb    = pCompThresh_->load(std::memory_order_relaxed);
    p.compRatio       = pCompRatio_->load(std::memory_order_relaxed);
    p.compAttackMs    = pCompAttack_->load(std::memory_order_relaxed);
    p.compReleaseMs   = pCompRelease_->load(std::memory_order_relaxed);
    p.compAutoRelease = pCompAutoRelease_->load(std::memory_order_relaxed) > 0.5f;
    p.limiterMode     = pLimiterMode_->load(std::memory_order_relaxed) > 0.5f;
    p.makeupGainDb    = pMakeupGain_->load(std::memory_order_relaxed);
    p.extSidechain    = pExtSidechain_->load(std::memory_order_relaxed) > 0.5f;
    p.filtersToSc     = pFiltersToSc_->load(std::memory_order_relaxed) > 0.5f;
    p.scListen        = pScListen_->load(std::memory_order_relaxed) > 0.5f;
    p.scHpfFreqHz     = pScHpfFreq_->load(std::memory_order_relaxed);
    p.deessEnabled    = pDeessIn_->load(std::memory_order_relaxed) > 0.5f;
    p.deessFreqHz     = pDeessFreq_->load(std::memory_order_relaxed);
    p.deessThreshDb   = pDeessThresh_->load(std::memory_order_relaxed);
    p.colorAmount     = pColorAmount_->load(std::memory_order_relaxed) * 0.01f;
    p.colorLinkComp   = pColorLinkComp_->load(std::memory_order_relaxed) > 0.5f;
    p.circuit4000G    = pCircuitMode_->load(std::memory_order_relaxed) > 0.5f;
    p.satTone         = pSatTone_->load(std::memory_order_relaxed) * 0.01f;
    p.crosstalk       = pCrosstalk_->load(std::memory_order_relaxed) * 0.01f;
    p.colorTrimDb     = pColorTrim_->load(std::memory_order_relaxed);
    p.eqEnabled       = pEqEnabled_->load(std::memory_order_relaxed) > 0.5f;
    p.eqPreDyn        = pEqPreDyn_->load(std::memory_order_relaxed) > 0.5f;
    p.moduleOrder     = std::clamp(static_cast<int>(std::round(pModuleOrder_->load(std::memory_order_relaxed))), 0, 5);
    p.lfBell          = pLfBell_->load(std::memory_order_relaxed) > 0.5f;
    p.lfFreqHz        = pLfFreq_->load(std::memory_order_relaxed);
    p.lfGainDb        = pLfGain_->load(std::memory_order_relaxed);
    p.lmfFreqHz       = pLmfFreq_->load(std::memory_order_relaxed);
    p.lmfGainDb       = pLmfGain_->load(std::memory_order_relaxed);
    p.lmfQ            = pLmfQ_->load(std::memory_order_relaxed);
    p.hmfFreqHz       = pHmfFreq_->load(std::memory_order_relaxed);
    p.hmfGainDb       = pHmfGain_->load(std::memory_order_relaxed);
    p.hmfQ            = pHmfQ_->load(std::memory_order_relaxed);
    p.hfBell          = pHfBell_->load(std::memory_order_relaxed) > 0.5f;
    p.hfFreqHz        = pHfFreq_->load(std::memory_order_relaxed);
    p.hfGainDb        = pHfGain_->load(std::memory_order_relaxed);
    p.eqTrimDb        = pEqTrim_->load(std::memory_order_relaxed);
    p.autoGain        = pAutoGain_->load(std::memory_order_relaxed) > 0.5f;
    p.msMode          = std::clamp(static_cast<int>(std::round(pMsMode_->load(std::memory_order_relaxed))), 0, 3);
    p.analogTolerance = pAnalogTolerance_->load(std::memory_order_relaxed) * 0.01f;
    p.outputGainDb    = pOutputGain_->load(std::memory_order_relaxed);
    p.mix             = pMix_->load(std::memory_order_relaxed) * 0.01f;
    return p;
}

int KalyxSSLConsoleProcessor::getModuleOrderIndex() const noexcept
{
    return std::clamp(static_cast<int>(std::round(pModuleOrder_->load(std::memory_order_relaxed))), 0, 5);
}

void KalyxSSLConsoleProcessor::setModuleOrderIndex(int newOrderIdx) noexcept
{
    const int clamped = std::clamp(newOrderIdx, 0, 5);
    if (auto* param = apvts_.getParameter(ParamIDs::moduleOrder))
        param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(clamped)));
}

void KalyxSSLConsoleProcessor::swapCentralModuleSlots(int slotA, int slotB) noexcept
{
    if (slotA < 0 || slotA > 2 || slotB < 0 || slotB > 2 || slotA == slotB)
        return;

    const int curIdx = getModuleOrderIndex();
    auto perm = kalyx::dsp::kModulePermutations[static_cast<std::size_t>(curIdx)];
    std::swap(perm[static_cast<std::size_t>(slotA)], perm[static_cast<std::size_t>(slotB)]);

    for (int i = 0; i < 6; ++i)
    {
        if (kalyx::dsp::kModulePermutations[static_cast<std::size_t>(i)] == perm)
        {
            setModuleOrderIndex(i);
            break;
        }
    }
}

void KalyxSSLConsoleProcessor::setSectionLocked(int sectionIndex, bool locked) noexcept
{
    if (sectionIndex >= 0 && sectionIndex < 5)
        sectionLocked_[static_cast<std::size_t>(sectionIndex)].store(locked, std::memory_order_relaxed);
}

bool KalyxSSLConsoleProcessor::isSectionLocked(int sectionIndex) const noexcept
{
    if (sectionIndex >= 0 && sectionIndex < 5)
        return sectionLocked_[static_cast<std::size_t>(sectionIndex)].load(std::memory_order_relaxed);
    return false;
}

int KalyxSSLConsoleProcessor::getParameterSectionIndex(const juce::String& id) noexcept
{
    if (id == ParamIDs::inputGain || id == ParamIDs::inputSatDrive || id == ParamIDs::phaseInvert
        || id == ParamIDs::hpfEnabled || id == ParamIDs::hpfFreq || id == ParamIDs::hpfQ
        || id == ParamIDs::lpfEnabled || id == ParamIDs::lpfFreq || id == ParamIDs::lpfQ
        || id == ParamIDs::filtPreTrim)
        return 0;

    if (id == ParamIDs::dynEnabled || id == ParamIDs::gateThresh || id == ParamIDs::gateRange
        || id == ParamIDs::compThresh || id == ParamIDs::compRatio || id == ParamIDs::compAttack
        || id == ParamIDs::compRelease || id == ParamIDs::compAutoRelease || id == ParamIDs::limiterMode
        || id == ParamIDs::makeupGain || id == ParamIDs::extSidechain || id == ParamIDs::filtersToSc
        || id == ParamIDs::scListen || id == ParamIDs::scHpfFreq
        || id == ParamIDs::deessIn || id == ParamIDs::deessFreq || id == ParamIDs::deessThresh)
        return 1;

    if (id == ParamIDs::colorAmount || id == ParamIDs::colorLinkComp || id == ParamIDs::circuitMode
        || id == ParamIDs::satTone || id == ParamIDs::crosstalk || id == ParamIDs::colorTrim)
        return 2;

    if (id == ParamIDs::eqEnabled || id == ParamIDs::eqPreDyn
        || id == ParamIDs::lfBell || id == ParamIDs::lfFreq || id == ParamIDs::lfGain
        || id == ParamIDs::lmfFreq || id == ParamIDs::lmfGain || id == ParamIDs::lmfQ
        || id == ParamIDs::hmfFreq || id == ParamIDs::hmfGain || id == ParamIDs::hmfQ
        || id == ParamIDs::hfBell || id == ParamIDs::hfFreq || id == ParamIDs::hfGain
        || id == ParamIDs::eqTrim)
        return 3;

    if (id == ParamIDs::autoGain || id == ParamIDs::msMode || id == ParamIDs::analogTolerance
        || id == ParamIDs::outputGain || id == ParamIDs::mix)
        return 4;

    return -1;
}

void KalyxSSLConsoleProcessor::setParamNormalizedValue(const char* paramID, float plainValue)
{
    const int sec = getParameterSectionIndex(paramID);
    if (sec >= 0 && isSectionLocked(sec))
        return;

    if (auto* param = apvts_.getParameter(paramID))
        param->setValueNotifyingHost(param->convertTo0to1(plainValue));
}

void KalyxSSLConsoleProcessor::applyValueTreeRespectingLocks(const juce::ValueTree& sourceState)
{
    if (!sourceState.isValid())
        return;

    for (int i = 0; i < sourceState.getNumChildren(); ++i)
    {
        const auto child = sourceState.getChild(i);
        if (child.hasType("PARAM"))
        {
            const juce::String id = child.getProperty("id").toString();
            const int sec = getParameterSectionIndex(id);
            if (sec >= 0 && isSectionLocked(sec))
                continue;

            if (child.hasProperty("value"))
            {
                const float plainVal = static_cast<float>(child.getProperty("value"));
                if (auto* param = apvts_.getParameter(id))
                    param->setValueNotifyingHost(param->convertTo0to1(plainVal));
            }
        }
    }
}

const juce::String KalyxSSLConsoleProcessor::getProgramName(int index)
{
    switch (index)
    {
        case 0: return "Init";
        case 1: return "Vocal Strip";
        case 2: return "Drum Bus Punch";
        case 3: return "Acoustic Glue";
        case 4: return "Bass Control";
        default: return "Init";
    }
}

void KalyxSSLConsoleProcessor::setCurrentProgram(int index)
{
    currentProgram_ = std::clamp(index, 0, getNumPrograms() - 1);
    applyFactoryPreset(currentProgram_);
}

void KalyxSSLConsoleProcessor::applyFactoryPreset(int presetIndex)
{
    currentProgram_ = std::clamp(presetIndex, 0, 4);

    setParamNormalizedValue(ParamIDs::inputGain,       0.0f);
    setParamNormalizedValue(ParamIDs::inputSatDrive,   0.0f);
    setParamNormalizedValue(ParamIDs::phaseInvert,     0.0f);
    setParamNormalizedValue(ParamIDs::hpfEnabled,      1.0f);
    setParamNormalizedValue(ParamIDs::hpfFreq,         40.0f);
    setParamNormalizedValue(ParamIDs::hpfQ,            0.71f);
    setParamNormalizedValue(ParamIDs::lpfEnabled,      0.0f);
    setParamNormalizedValue(ParamIDs::lpfFreq,         18000.0f);
    setParamNormalizedValue(ParamIDs::lpfQ,            0.71f);
    setParamNormalizedValue(ParamIDs::filtPreTrim,     0.0f);
    setParamNormalizedValue(ParamIDs::dynEnabled,      1.0f);
    setParamNormalizedValue(ParamIDs::gateThresh,      -55.0f);
    setParamNormalizedValue(ParamIDs::gateRange,       0.0f);
    setParamNormalizedValue(ParamIDs::compThresh,      -14.0f);
    setParamNormalizedValue(ParamIDs::compRatio,       4.0f);
    setParamNormalizedValue(ParamIDs::compAttack,      10.0f);
    setParamNormalizedValue(ParamIDs::compRelease,     120.0f);
    setParamNormalizedValue(ParamIDs::compAutoRelease, 0.0f);
    setParamNormalizedValue(ParamIDs::limiterMode,     0.0f);
    setParamNormalizedValue(ParamIDs::makeupGain,      0.0f);
    setParamNormalizedValue(ParamIDs::extSidechain,    0.0f);
    setParamNormalizedValue(ParamIDs::filtersToSc,     0.0f);
    setParamNormalizedValue(ParamIDs::scListen,        0.0f);
    setParamNormalizedValue(ParamIDs::scHpfFreq,       20.0f);
    setParamNormalizedValue(ParamIDs::deessIn,         0.0f);
    setParamNormalizedValue(ParamIDs::deessFreq,       6500.0f);
    setParamNormalizedValue(ParamIDs::deessThresh,     -18.0f);
    setParamNormalizedValue(ParamIDs::colorAmount,     25.0f);
    setParamNormalizedValue(ParamIDs::colorLinkComp,   1.0f);
    setParamNormalizedValue(ParamIDs::circuitMode,     1.0f);
    setParamNormalizedValue(ParamIDs::satTone,         0.0f);
    setParamNormalizedValue(ParamIDs::crosstalk,       0.0f);
    setParamNormalizedValue(ParamIDs::colorTrim,       0.0f);
    setParamNormalizedValue(ParamIDs::eqEnabled,       1.0f);
    setParamNormalizedValue(ParamIDs::eqPreDyn,        0.0f);
    setParamNormalizedValue(ParamIDs::moduleOrder,     0.0f);
    setParamNormalizedValue(ParamIDs::lfBell,          0.0f);
    setParamNormalizedValue(ParamIDs::lfFreq,          80.0f);
    setParamNormalizedValue(ParamIDs::lfGain,          0.0f);
    setParamNormalizedValue(ParamIDs::lmfFreq,         450.0f);
    setParamNormalizedValue(ParamIDs::lmfGain,         0.0f);
    setParamNormalizedValue(ParamIDs::lmfQ,            1.20f);
    setParamNormalizedValue(ParamIDs::hmfFreq,         2500.0f);
    setParamNormalizedValue(ParamIDs::hmfGain,         0.0f);
    setParamNormalizedValue(ParamIDs::hmfQ,            1.20f);
    setParamNormalizedValue(ParamIDs::hfBell,          0.0f);
    setParamNormalizedValue(ParamIDs::hfFreq,          10000.0f);
    setParamNormalizedValue(ParamIDs::hfGain,          0.0f);
    setParamNormalizedValue(ParamIDs::eqTrim,          0.0f);
    setParamNormalizedValue(ParamIDs::autoGain,        0.0f);
    setParamNormalizedValue(ParamIDs::msMode,          0.0f);
    setParamNormalizedValue(ParamIDs::analogTolerance, 25.0f);
    setParamNormalizedValue(ParamIDs::outputGain,      0.0f);
    setParamNormalizedValue(ParamIDs::mix,             100.0f);

    if (presetIndex == 1) // Vocal Strip
    {
        setParamNormalizedValue(ParamIDs::inputSatDrive,   20.0f);
        setParamNormalizedValue(ParamIDs::hpfFreq,         95.0f);
        setParamNormalizedValue(ParamIDs::hpfQ,            0.85f);
        setParamNormalizedValue(ParamIDs::compThresh,      -18.0f);
        setParamNormalizedValue(ParamIDs::compRatio,       4.5f);
        setParamNormalizedValue(ParamIDs::compAttack,      8.0f);
        setParamNormalizedValue(ParamIDs::compAutoRelease, 1.0f);
        setParamNormalizedValue(ParamIDs::scHpfFreq,       110.0f);
        setParamNormalizedValue(ParamIDs::deessIn,         1.0f);
        setParamNormalizedValue(ParamIDs::deessFreq,       6800.0f);
        setParamNormalizedValue(ParamIDs::deessThresh,     -20.0f);
        setParamNormalizedValue(ParamIDs::colorAmount,     38.0f);
        setParamNormalizedValue(ParamIDs::satTone,         20.0f);
        setParamNormalizedValue(ParamIDs::lmfFreq,         320.0f);
        setParamNormalizedValue(ParamIDs::lmfGain,         -2.5f);
        setParamNormalizedValue(ParamIDs::lmfQ,            1.6f);
        setParamNormalizedValue(ParamIDs::hmfFreq,         3400.0f);
        setParamNormalizedValue(ParamIDs::hmfGain,         2.8f);
        setParamNormalizedValue(ParamIDs::hfFreq,          12000.0f);
        setParamNormalizedValue(ParamIDs::hfGain,          3.5f);
        setParamNormalizedValue(ParamIDs::autoGain,        1.0f);
    }
    else if (presetIndex == 2) // Drum Bus Punch
    {
        setParamNormalizedValue(ParamIDs::inputSatDrive,   35.0f);
        setParamNormalizedValue(ParamIDs::hpfFreq,         45.0f);
        setParamNormalizedValue(ParamIDs::scHpfFreq,       95.0f);
        setParamNormalizedValue(ParamIDs::compThresh,      -15.0f);
        setParamNormalizedValue(ParamIDs::compRatio,       4.0f);
        setParamNormalizedValue(ParamIDs::compAttack,      28.0f);
        setParamNormalizedValue(ParamIDs::compAutoRelease, 1.0f);
        setParamNormalizedValue(ParamIDs::makeupGain,      3.0f);
        setParamNormalizedValue(ParamIDs::colorAmount,     55.0f);
        setParamNormalizedValue(ParamIDs::circuitMode,     0.0f); // 4000 E
        setParamNormalizedValue(ParamIDs::satTone,         -25.0f);
        setParamNormalizedValue(ParamIDs::crosstalk,       35.0f);
        setParamNormalizedValue(ParamIDs::lfBell,          1.0f);
        setParamNormalizedValue(ParamIDs::lfFreq,          65.0f);
        setParamNormalizedValue(ParamIDs::lfGain,          3.5f);
        setParamNormalizedValue(ParamIDs::lmfFreq,         420.0f);
        setParamNormalizedValue(ParamIDs::lmfGain,         -3.0f);
        setParamNormalizedValue(ParamIDs::hmfFreq,         4200.0f);
        setParamNormalizedValue(ParamIDs::hmfGain,         2.5f);
        setParamNormalizedValue(ParamIDs::hfFreq,          9500.0f);
        setParamNormalizedValue(ParamIDs::hfGain,          2.0f);
        setParamNormalizedValue(ParamIDs::mix,             85.0f);
    }
    else if (presetIndex == 3) // Acoustic Glue
    {
        setParamNormalizedValue(ParamIDs::inputSatDrive,   12.0f);
        setParamNormalizedValue(ParamIDs::hpfFreq,         75.0f);
        setParamNormalizedValue(ParamIDs::compThresh,      -16.0f);
        setParamNormalizedValue(ParamIDs::compRatio,       2.5f);
        setParamNormalizedValue(ParamIDs::compAttack,      18.0f);
        setParamNormalizedValue(ParamIDs::compAutoRelease, 1.0f);
        setParamNormalizedValue(ParamIDs::colorAmount,     22.0f);
        setParamNormalizedValue(ParamIDs::circuitMode,     1.0f); // 4000 G
        setParamNormalizedValue(ParamIDs::crosstalk,       20.0f);
        setParamNormalizedValue(ParamIDs::lmfFreq,         260.0f);
        setParamNormalizedValue(ParamIDs::lmfGain,         -2.2f);
        setParamNormalizedValue(ParamIDs::hmfFreq,         2800.0f);
        setParamNormalizedValue(ParamIDs::hmfGain,         1.8f);
        setParamNormalizedValue(ParamIDs::hfFreq,          11000.0f);
        setParamNormalizedValue(ParamIDs::hfGain,          2.5f);
        setParamNormalizedValue(ParamIDs::autoGain,        1.0f);
    }
    else if (presetIndex == 4) // Bass Control
    {
        setParamNormalizedValue(ParamIDs::inputSatDrive,   45.0f);
        setParamNormalizedValue(ParamIDs::hpfFreq,         32.0f);
        setParamNormalizedValue(ParamIDs::hpfQ,            1.25f);
        setParamNormalizedValue(ParamIDs::lpfEnabled,      1.0f);
        setParamNormalizedValue(ParamIDs::lpfFreq,         6500.0f);
        setParamNormalizedValue(ParamIDs::compThresh,      -20.0f);
        setParamNormalizedValue(ParamIDs::compRatio,       6.0f);
        setParamNormalizedValue(ParamIDs::compAttack,      12.0f);
        setParamNormalizedValue(ParamIDs::compRelease,     110.0f);
        setParamNormalizedValue(ParamIDs::scHpfFreq,       65.0f);
        setParamNormalizedValue(ParamIDs::colorAmount,     50.0f);
        setParamNormalizedValue(ParamIDs::circuitMode,     0.0f); // 4000 E
        setParamNormalizedValue(ParamIDs::satTone,         -35.0f);
        setParamNormalizedValue(ParamIDs::moduleOrder,     4.0f); // EQ -> DYN -> COLOR
        setParamNormalizedValue(ParamIDs::lfFreq,          90.0f);
        setParamNormalizedValue(ParamIDs::lfGain,          3.0f);
        setParamNormalizedValue(ParamIDs::lmfFreq,         750.0f);
        setParamNormalizedValue(ParamIDs::lmfGain,         2.5f);
        setParamNormalizedValue(ParamIDs::lmfQ,            1.4f);
        setParamNormalizedValue(ParamIDs::autoGain,        1.0f);
    }
}

void KalyxSSLConsoleProcessor::toggleABState()
{
    if (!isSlotB_)
    {
        slotAState_ = apvts_.copyState();
        isSlotB_ = true;
        applyValueTreeRespectingLocks(slotBState_);
    }
    else
    {
        slotBState_ = apvts_.copyState();
        isSlotB_ = false;
        applyValueTreeRespectingLocks(slotAState_);
    }
}

void KalyxSSLConsoleProcessor::copyActiveToInactiveAB()
{
    if (!isSlotB_)
        slotBState_ = apvts_.copyState();
    else
        slotAState_ = apvts_.copyState();
}

void KalyxSSLConsoleProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const auto state = apvts_.copyState();
    if (std::unique_ptr<juce::XmlElement> xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void KalyxSSLConsoleProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (std::unique_ptr<juce::XmlElement> xmlState = getXmlFromBinary(data, sizeInBytes))
    {
        if (xmlState->hasTagName(apvts_.state.getType()))
        {
            apvts_.replaceState(juce::ValueTree::fromXml(*xmlState));
            slotAState_ = apvts_.copyState();
            slotBState_ = apvts_.copyState();
        }
    }
}

juce::AudioProcessorEditor* KalyxSSLConsoleProcessor::createEditor()
{
    return new KalyxSSLConsoleEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KalyxSSLConsoleProcessor();
}
