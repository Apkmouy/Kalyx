#include "DualCabDSP.h"
#include "BinaryData.h"
#include <algorithm>
#include <cmath>

namespace kalyx::bass::dsp
{

void DualCabEngine::scanBuiltinIRs()
{
    builtinIRs_.clear();
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const char* resName = BinaryData::namedResourceList[i];
        const char* origName = BinaryData::originalFilenames[i];
        int dataSize = 0;
        const char* data = BinaryData::getNamedResource(resName, dataSize);
        if (data != nullptr && dataSize > 0)
        {
            juce::String disp = juce::String(origName).upToLastOccurrenceOf(".", false, false);
            disp = disp.replace("_", " ");
            builtinIRs_.push_back({ disp, data, static_cast<size_t>(dataSize) });
        }
    }
}

void DualCabEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate_   = std::max(spec.sampleRate, 8000.0);
    maxBlockSize_ = std::max(spec.maximumBlockSize, static_cast<juce::uint32>(64));

    if (builtinIRs_.empty())
        scanBuiltinIRs();

    juce::dsp::ProcessSpec monoSpec { sampleRate_, maxBlockSize_, 1 };
    convA_.prepare(monoSpec);
    convB_.prepare(monoSpec);

    workBufA_.setSize(1, static_cast<int>(maxBlockSize_), false, true, true);
    workBufB_.setSize(1, static_cast<int>(maxBlockSize_), false, true, true);

    reset();
    updateMicAndRoomFilters();

    if (!builtinIRs_.empty())
    {
        if (!usingCustomA_)
            loadBuiltinIR(0, std::clamp(state_.cabAIndex, 0, static_cast<int>(builtinIRs_.size()) - 1));
        if (!usingCustomB_)
            loadBuiltinIR(1, std::clamp(state_.cabBIndex, 0, static_cast<int>(builtinIRs_.size()) - 1));
    }
}

void DualCabEngine::reset() noexcept
{
    convA_.reset();
    convB_.reset();
    coneBreakup_.reset(0.0);
    coneResonance_.reset();
    roomLPF_L_.reset();
    roomLPF_R_.reset();
    alignDelayA_.fill(0.0f);
    alignDelayB_.fill(0.0f);
    roomBufL_.fill(0.0f);
    roomBufR_.fill(0.0f);
    alignWriteIdx_ = 0;
    roomWriteIdx_  = 0;
}

void DualCabEngine::loadBuiltinIR(int cabSlot, int index)
{
    if (builtinIRs_.empty())
        return;

    const int safeIdx = std::clamp(index, 0, static_cast<int>(builtinIRs_.size()) - 1);
    if (cabSlot == 0 && !usingCustomA_ && lastBuiltinA_ == safeIdx)
        return;
    if (cabSlot == 1 && !usingCustomB_ && lastBuiltinB_ == safeIdx)
        return;

    const auto& entry = builtinIRs_[static_cast<size_t>(safeIdx)];
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    auto stream = std::make_unique<juce::MemoryInputStream>(entry.data, entry.dataSize, false);
    if (auto reader = std::unique_ptr<juce::AudioFormatReader>(fm.createReaderFor(std::move(stream))))
    {
        const int numSamples = static_cast<int>(std::min<juce::int64>(reader->lengthInSamples, 131072));
        juce::AudioBuffer<float> buf(1, numSamples);
        reader->read(&buf, 0, numSamples, 0, true, false);
        if (cabSlot == 0) { usingCustomA_ = false; lastBuiltinA_ = safeIdx; }
        else              { usingCustomB_ = false; lastBuiltinB_ = safeIdx; }
        loadBufferIntoConvolution(cabSlot, std::move(buf), reader->sampleRate, entry.displayName);
    }
}

void DualCabEngine::loadCustomIRFile(int cabSlot, const juce::File& wavFile)
{
    if (!wavFile.existsAsFile())
        return;

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    if (auto reader = std::unique_ptr<juce::AudioFormatReader>(fm.createReaderFor(wavFile)))
    {
        const int numSamples = static_cast<int>(std::min<juce::int64>(reader->lengthInSamples, 131072));
        juce::AudioBuffer<float> buf(1, numSamples);
        reader->read(&buf, 0, numSamples, 0, true, false);
        if (cabSlot == 0) usingCustomA_ = true;
        else              usingCustomB_ = true;
        loadBufferIntoConvolution(cabSlot, std::move(buf), reader->sampleRate, wavFile.getFileNameWithoutExtension());
    }
}

void DualCabEngine::loadBufferIntoConvolution(int cabSlot, juce::AudioBuffer<float>&& irBuffer, double sourceSampleRate, const juce::String& name)
{
    const int n = irBuffer.getNumSamples();
    if (n <= 0)
        return;

    // 1. RMS Normalization & Peak Detection for Auto-Phase Align
    const float* ptr = irBuffer.getReadPointer(0);
    double sumSq = 0.0;
    float maxAbs = 0.0f;
    int peakIdx  = 0;
    for (int i = 0; i < n; ++i)
    {
        const float v = ptr[i];
        sumSq += static_cast<double>(v) * static_cast<double>(v);
        if (std::abs(v) > maxAbs)
        {
            maxAbs = std::abs(v);
            peakIdx = i;
        }
    }

    const double rms = std::sqrt(sumSq / static_cast<double>(n));
    const float normGain = (rms > 1.0e-7) ? static_cast<float>(0.08 / rms) : 1.0f;
    irBuffer.applyGain(std::clamp(normGain, 0.05f, 20.0f));

    // Scale peakIdx to current host sampleRate
    const int scaledPeak = static_cast<int>(std::round(static_cast<double>(peakIdx) * (sampleRate_ / std::max(sourceSampleRate, 8000.0))));

    if (cabSlot == 0)
    {
        customNameA_ = name;
        cabAPeakSample_.store(std::clamp(scaledPeak, 0, kMaxDelaySamples - 1), std::memory_order_relaxed);
        convA_.loadImpulseResponse(std::move(irBuffer), sourceSampleRate,
                                   juce::dsp::Convolution::Stereo::no,
                                   juce::dsp::Convolution::Trim::no,
                                   juce::dsp::Convolution::Normalise::no);
    }
    else
    {
        customNameB_ = name;
        cabBPeakSample_.store(std::clamp(scaledPeak, 0, kMaxDelaySamples - 1), std::memory_order_relaxed);
        convB_.loadImpulseResponse(std::move(irBuffer), sourceSampleRate,
                                   juce::dsp::Convolution::Stereo::no,
                                   juce::dsp::Convolution::Trim::no,
                                   juce::dsp::Convolution::Normalise::no);
    }
}

juce::String DualCabEngine::getActiveIRName(int cabSlot) const
{
    return (cabSlot == 0) ? customNameA_ : customNameB_;
}

void DualCabEngine::setControlState(const CabControlState& state) noexcept
{
    state_ = state;
    updateMicAndRoomFilters();
}

void DualCabEngine::updateMicAndRoomFilters() noexcept
{
    coneResonance_.setBell(sampleRate_, 105.0, 1.4, (state_.breakupPct / 100.0) * 2.5);
    roomLPF_L_.setLowPass(sampleRate_, 3800.0, 0.707);
    roomLPF_R_.setLowPass(sampleRate_, 3500.0, 0.707);
}

void DualCabEngine::processBlock(const float* monoIn, float* outL, float* outR, int numSamples) noexcept
{
    if (numSamples <= 0 || numSamples > static_cast<int>(maxBlockSize_))
        return;

    if (!state_.enabled)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            outL[i] = monoIn[i];
            outR[i] = monoIn[i];
        }
        return;
    }

    // 1. Copy input & apply Speaker Excursion / Cone Breakup (64-bit)
    float* bufA = workBufA_.getWritePointer(0);
    float* bufB = workBufB_.getWritePointer(0);
    const double breakupAmt = std::clamp(state_.breakupPct / 100.0, 0.0, 1.0);

    for (int i = 0; i < numSamples; ++i)
    {
        double s = static_cast<double>(monoIn[i]);
        if (std::abs(s) < 1.0e-7)
        {
            s = 0.0;
        }
        else if (breakupAmt > 0.01)
        {
            s = coneResonance_.processSample(s);
            s = coneBreakup_.processADAA2(s, 1.0 + breakupAmt * 2.8, breakupAmt * 0.06);
        }
        bufA[i] = static_cast<float>(s);
        bufB[i] = static_cast<float>(s);
    }

    // 2. Zero-latency stereo/mono convolution for Cab A & Cab B (direct IR playback)
    {
        juce::dsp::AudioBlock<float> blkA(workBufA_.getArrayOfWritePointers(), 1, static_cast<size_t>(numSamples));
        juce::dsp::ProcessContextReplacing<float> ctxA(blkA);
        convA_.process(ctxA);
    }
    {
        juce::dsp::AudioBlock<float> blkB(workBufB_.getArrayOfWritePointers(), 1, static_cast<size_t>(numSamples));
        juce::dsp::ProcessContextReplacing<float> ctxB(blkB);
        convB_.process(ctxB);
    }

    // 3. Auto-Phase Align A vs B + Phase Invert + Mix + Room
    const int peakA = cabAPeakSample_.load(std::memory_order_relaxed);
    const int peakB = cabBPeakSample_.load(std::memory_order_relaxed);
    const int delayA = (state_.autoPhase && peakB > peakA) ? std::min(peakB - peakA, kMaxDelaySamples - 1) : 0;
    const int delayB = (state_.autoPhase && peakA > peakB) ? std::min(peakA - peakB, kMaxDelaySamples - 1) : 0;

    const float polA = state_.phaseA ? -1.0f : 1.0f;
    const float polB = state_.phaseB ? -1.0f : 1.0f;
    const float mixB = static_cast<float>(std::clamp(state_.mixPct / 100.0, 0.0, 1.0));
    const float mixA = 1.0f - mixB;
    const float roomAmt = static_cast<float>(std::clamp(state_.roomPct / 100.0, 0.0, 1.0)) * 0.35f;

    const int tapL1 = static_cast<int>(0.011 * sampleRate_);
    const int tapL2 = static_cast<int>(0.023 * sampleRate_);
    const int tapR1 = static_cast<int>(0.014 * sampleRate_);
    const int tapR2 = static_cast<int>(0.029 * sampleRate_);

    for (int i = 0; i < numSamples; ++i)
    {
        alignDelayA_[static_cast<size_t>(alignWriteIdx_)] = bufA[i] * polA;
        alignDelayB_[static_cast<size_t>(alignWriteIdx_)] = bufB[i] * polB;

        int rA = alignWriteIdx_ - delayA; if (rA < 0) rA += kMaxDelaySamples;
        int rB = alignWriteIdx_ - delayB; if (rB < 0) rB += kMaxDelaySamples;
        alignWriteIdx_ = (alignWriteIdx_ + 1) % kMaxDelaySamples;

        double m = static_cast<double>(alignDelayA_[static_cast<size_t>(rA)] * mixA
                                     + alignDelayB_[static_cast<size_t>(rB)] * mixB);

        if (std::abs(m) < 1.0e-7)
            m = 0.0;

        const float dry = static_cast<float>(m);
        roomBufL_[static_cast<size_t>(roomWriteIdx_)] = dry;
        roomBufR_[static_cast<size_t>(roomWriteIdx_)] = dry;

        auto readRoom = [&](const std::array<float, kRoomBufSize>& b, int tap) noexcept -> float
        {
            int idx = roomWriteIdx_ - std::clamp(tap, 1, kRoomBufSize - 1);
            if (idx < 0) idx += kRoomBufSize;
            return b[static_cast<size_t>(idx)];
        };

        const float reflL = 0.6f * readRoom(roomBufL_, tapL1) - 0.4f * readRoom(roomBufL_, tapL2);
        const float reflR = 0.6f * readRoom(roomBufR_, tapR1) - 0.4f * readRoom(roomBufR_, tapR2);
        roomWriteIdx_ = (roomWriteIdx_ + 1) % kRoomBufSize;

        const float wetL = static_cast<float>(roomLPF_L_.processSample(static_cast<double>(reflL)));
        const float wetR = static_cast<float>(roomLPF_R_.processSample(static_cast<double>(reflR)));

        outL[i] = dry + wetL * roomAmt;
        outR[i] = dry + wetR * roomAmt;
    }
}

} // namespace kalyx::bass::dsp
