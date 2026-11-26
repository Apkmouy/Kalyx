#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

KalyxBassRigProcessor::KalyxBassRigProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain (Kick)", juce::AudioChannelSet::stereo(), false)),
      apvts_(*this, nullptr, "KALYX_BASS_RIG_STATE", kalyx::bass::createParameterLayout()),
      presetManager_(apvts_)
{
}

bool KalyxBassRigProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainIn  = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != juce::AudioChannelSet::stereo())
        return false;
    if (mainIn != juce::AudioChannelSet::mono() && mainIn != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void KalyxBassRigProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    hostSampleRate_ = std::max(sampleRate, 8000.0);
    const size_t maxN = static_cast<size_t>(std::max(samplesPerBlock, 64));

    ampMonoBuf_.assign(maxN, 0.0f);
    cabOutL_.assign(maxN, 0.0f);
    cabOutR_.assign(maxN, 0.0f);
    diMonoBuf_.assign(maxN, 0.0f);

    gate_.prepare(hostSampleRate_);
    tuner_.prepare(hostSampleRate_);
    ampEngine_.prepare(hostSampleRate_);
    diEngine_.prepare(hostSampleRate_);
    kickRelief_.prepare(hostSampleRate_);
    limiter_.prepare(hostSampleRate_);

    juce::dsp::ProcessSpec spec { hostSampleRate_, static_cast<juce::uint32>(maxN), 1 };
    dualCab_.prepare(spec);

    vuEnvIn_  = 0.0;
    vuEnvOut_ = 0.0;
    corrNum_  = 0.0;
    corrDenA_ = 1.0e-12;
    corrDenB_ = 1.0e-12;
}

void KalyxBassRigProcessor::releaseResources()
{
    gate_.reset();
    tuner_.reset();
    ampEngine_.reset();
    diEngine_.reset();
    dualCab_.reset();
    kickRelief_.reset();
    limiter_.reset();
}

void KalyxBassRigProcessor::triggerIRReload(int cabSlot, int irIndex)
{
    if (cabSlot == 0) pendingIrA_.store(irIndex, std::memory_order_relaxed);
    else              pendingIrB_.store(irIndex, std::memory_order_relaxed);
}

void KalyxBassRigProcessor::loadCustomIR(int cabSlot, const juce::File& file)
{
    dualCab_.loadCustomIRFile(cabSlot, file);
}

kalyx::bass::dsp::AmpControlState KalyxBassRigProcessor::getAmpSnapshot() const noexcept
{
    auto getF = [this](const char* id) -> float
    {
        if (auto* p = apvts_.getRawParameterValue(id))
            return p->load(std::memory_order_relaxed);
        return 0.0f;
    };

    kalyx::bass::dsp::AmpControlState st;
    st.model       = static_cast<kalyx::bass::AmpModel>(std::clamp(static_cast<int>(getF("amp_model")), 0, 3));
    st.channel     = std::clamp(static_cast<int>(getF("amp_channel")), 0, 1);
    st.gainPct     = getF("amp_gain");
    st.bright      = getF("amp_bright") > 0.5f;
    st.ultraLoMode = std::clamp(static_cast<int>(getF("amp_ultra_lo")), 0, 4);
    st.ultraHi     = getF("amp_ultra_hi") > 0.5f;
    st.bassDb      = getF("amp_bass");
    st.midDb       = getF("amp_mid");
    st.midFreqIdx  = std::clamp(static_cast<int>(getF("amp_mid_freq")), 0, 4);
    st.trebleDb    = getF("amp_treble");
    st.drivePct    = getF("amp_drive");
    st.compPct     = getF("amp_comp");
    st.depthIdx    = std::clamp(static_cast<int>(getF("amp_depth")), 0, 5);
    st.boostPct    = getF("amp_boost");
    st.presencePct = getF("amp_presence");
    st.sagPct      = getF("amp_sag");
    st.biasPct     = getF("amp_bias");
    st.masterPct   = getF("amp_master");
    st.eq9Enabled  = getF("eq9_en") > 0.5f;
    const char* eqIds[9] = { "eq9_40", "eq9_90", "eq9_180", "eq9_300", "eq9_500", "eq9_1k", "eq9_2k", "eq9_4k", "eq9_10k" };
    for (size_t i = 0; i < 9; ++i)
        st.eq9GainsDb[i] = getF(eqIds[i]);
    return st;
}

void KalyxBassRigProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainBus = getBusBuffer(buffer, true, 0);
    auto scBus   = (getBusCount(true) > 1) ? getBusBuffer(buffer, true, 1) : juce::AudioBuffer<float>();

    const int numSamples = mainBus.getNumSamples();
    const int numInCh    = mainBus.getNumChannels();
    if (numSamples <= 0 || numInCh <= 0)
        return;

    // Ensure pre-allocated buffers are large enough (safety guard; normally sized in prepareToPlay)
    if (static_cast<size_t>(numSamples) > ampMonoBuf_.size())
        return;

    auto getF = [this](const char* id) -> float
    {
        if (auto* p = apvts_.getRawParameterValue(id))
            return p->load(std::memory_order_relaxed);
        return 0.0f;
    };

    // Check IR index changes
    const int irA = static_cast<int>(getF("cab_a_ir"));
    const int irB = static_cast<int>(getF("cab_b_ir"));
    if (pendingIrA_.exchange(-1, std::memory_order_relaxed) != -1 || irA >= 0)
        dualCab_.loadBuiltinIR(0, irA);
    if (pendingIrB_.exchange(-1, std::memory_order_relaxed) != -1 || irB >= 0)
        dualCab_.loadBuiltinIR(1, irB);

    // 1. Update DSP states
    const double inTrimGain = std::pow(10.0, static_cast<double>(getF("in_trim")) / 20.0)
                            * ((getF("in_pad") > 0.5f) ? std::pow(10.0, -15.0 / 20.0) : 1.0);
    const bool gateEn       = getF("gate_en") > 0.5f;
    const double gateThresh = static_cast<double>(getF("gate_thresh"));
    const bool tunerMute    = getF("tuner_mute") > 0.5f;

    const bool ampEn        = getF("amp_en") > 0.5f;
    ampEngine_.setControlState(getAmpSnapshot());

    kalyx::bass::dsp::DIControlState diSt;
    diSt.enabled   = getF("di_en") > 0.5f;
    diSt.circuit   = static_cast<kalyx::bass::DICircuit>(std::clamp(static_cast<int>(getF("di_circuit")), 0, 4));
    diSt.subMixPct = getF("di_sub_mix");
    diSt.hpfFreqHz = getF("di_hpf_freq");
    diSt.lpfFreqHz = getF("di_lpf_freq");
    diSt.compPct   = getF("di_comp");
    diSt.autoPhase = getF("di_auto_phase") > 0.5f;
    diEngine_.setControlState(diSt);
    diEngine_.setAutoPhaseDelaySamples(dualCab_.getCabAPeakSample());

    kalyx::bass::dsp::CabControlState cabSt;
    cabSt.enabled    = getF("cab_en") > 0.5f;
    cabSt.cabAIndex  = irA;
    cabSt.cabBIndex  = irB;
    cabSt.phaseA     = getF("cab_a_phase") > 0.5f;
    cabSt.phaseB     = getF("cab_b_phase") > 0.5f;
    cabSt.mixPct     = getF("cab_mix");
    cabSt.autoPhase  = getF("cab_auto_phase") > 0.5f;
    cabSt.breakupPct = getF("cab_breakup");
    cabSt.micType    = static_cast<kalyx::bass::MicType>(std::clamp(static_cast<int>(getF("mic_type")), 0, 5));
    cabSt.micPosXPct = getF("mic_pos_x");
    cabSt.micPosYPct = getF("mic_pos_y");
    cabSt.roomPct    = getF("cab_room");
    dualCab_.setControlState(cabSt);

    const double ampLevelGain = std::pow(10.0, static_cast<double>(getF("amp_level")) / 20.0);
    const double diLevelGain  = std::pow(10.0, static_cast<double>(getF("di_level")) / 20.0);
    const bool kickEn         = getF("kick_relief_en") > 0.5f;
    const double kickAmt      = getF("kick_relief_amt");
    const bool limEn          = getF("lim_en") > 0.5f;
    const double limCeil      = getF("lim_ceiling");
    const double outFaderGain = std::pow(10.0, static_cast<double>(getF("out_fader")) / 20.0);

    const float* inL = mainBus.getReadPointer(0);
    const float* inR = (numInCh > 1) ? mainBus.getReadPointer(1) : inL;
    const float* scL = (scBus.getNumChannels() > 0 && scBus.getNumSamples() >= numSamples)
                           ? scBus.getReadPointer(0) : nullptr;

    // 300 ms VU ballistics
    const double vuCoeff = std::exp(-1.0 / (0.300 * hostSampleRate_));
    const double corrCoeff = std::exp(-1.0 / (0.120 * hostSampleRate_));

    // 2. Input -> Gate -> Tuner -> Parallel Amp & DI
    for (int i = 0; i < numSamples; ++i)
    {
        double monoIn = 0.5 * (static_cast<double>(inL[i]) + static_cast<double>(inR[i])) * inTrimGain;
        vuEnvIn_ = vuCoeff * vuEnvIn_ + (1.0 - vuCoeff) * std::abs(monoIn);

        tuner_.pushSample(monoIn);
        if (tunerMute)
            monoIn = 0.0;

        monoIn = gate_.processSample(monoIn, gateEn, gateThresh);

        const double ampS = ampEn ? ampEngine_.processSample(monoIn) : monoIn;
        const double diS  = diEngine_.processSample(monoIn);

        ampMonoBuf_[static_cast<size_t>(i)] = static_cast<float>(ampS);
        diMonoBuf_[static_cast<size_t>(i)]  = static_cast<float>(diS);
    }

    // 3. Dual Cabinet IR + 2D Mic + Room
    dualCab_.processBlock(ampMonoBuf_.data(), cabOutL_.data(), cabOutR_.data(), numSamples);

    // 4. Sum Amp+Cab & DI -> Kick-Relief -> Brickwall Limiter -> Out Fader
    auto outBus = getBusBuffer(buffer, false, 0);
    float* outL = outBus.getWritePointer(0);
    float* outR = (outBus.getNumChannels() > 1) ? outBus.getWritePointer(1) : outL;

    for (int i = 0; i < numSamples; ++i)
    {
        const double aL = static_cast<double>(cabOutL_[static_cast<size_t>(i)]) * ampLevelGain;
        const double aR = static_cast<double>(cabOutR_[static_cast<size_t>(i)]) * ampLevelGain;
        const double d  = static_cast<double>(diMonoBuf_[static_cast<size_t>(i)]) * diLevelGain;

        const double aMono = 0.5 * (aL + aR);
        corrNum_  = corrCoeff * corrNum_  + (1.0 - corrCoeff) * (aMono * d);
        corrDenA_ = corrCoeff * corrDenA_ + (1.0 - corrCoeff) * (aMono * aMono);
        corrDenB_ = corrCoeff * corrDenB_ + (1.0 - corrCoeff) * (d * d);

        double sumL = aL + d;
        double sumR = aR + d;

        const double scS = (scL != nullptr) ? static_cast<double>(scL[i]) : 0.0;
        kickRelief_.processStereo(sumL, sumR, scS, kickEn, kickAmt);
        limiter_.processStereo(sumL, sumR, limEn, limCeil);

        sumL *= outFaderGain;
        sumR *= outFaderGain;

        if (!std::isfinite(sumL)) sumL = 0.0;
        if (!std::isfinite(sumR)) sumR = 0.0;

        sumL = std::clamp(sumL, -4.0, 4.0);
        sumR = std::clamp(sumR, -4.0, 4.0);

        vuEnvOut_ = vuCoeff * vuEnvOut_ + (1.0 - vuCoeff) * std::max(std::abs(sumL), std::abs(sumR));

        outL[i] = static_cast<float>(sumL);
        if (outR != outL)
            outR[i] = static_cast<float>(sumR);
    }

    auto toDb = [](double lin) -> float
    {
        if (lin <= 1.0e-6) return -120.0f;
        return static_cast<float>(20.0 * std::log10(lin));
    };

    vuInDb_.store(toDb(vuEnvIn_), std::memory_order_relaxed);
    vuOutDb_.store(toDb(vuEnvOut_), std::memory_order_relaxed);
    vuGrDb_.store(std::min(limiter_.getGrDb(), diEngine_.getOptoGrDb()), std::memory_order_relaxed);

    const double denom = std::sqrt(std::max(corrDenA_ * corrDenB_, 1.0e-18));
    phaseCorr_.store(static_cast<float>(std::clamp(corrNum_ / denom, -1.0, 1.0)), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* KalyxBassRigProcessor::createEditor()
{
    return new KalyxBassRigEditor(*this);
}

void KalyxBassRigProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts_.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void KalyxBassRigProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts_.state.getType()))
            apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KalyxBassRigProcessor();
}
