#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "ADAASaturator.h"
#include "ZDFFilters.h"
#include "../Core/BassRigParams.h"
#include <array>
#include <atomic>
#include <vector>

namespace kalyx::bass::dsp
{

struct IRPresetEntry
{
    juce::String displayName;
    const void*  data     { nullptr };
    size_t       dataSize { 0 };
};

struct CabControlState
{
    bool    enabled      { true };
    int     cabAIndex    { 0 };
    int     cabBIndex    { 8 };
    bool    phaseA       { false };
    bool    phaseB       { false };
    double  mixPct       { 35.0 }; // 0=100% A, 100=100% B
    bool    autoPhase    { true };
    double  breakupPct   { 18.0 }; // Speaker Excursion / Breakup
    MicType micType      { MicType::RE20 };
    double  micPosXPct   { 25.0 }; // 0=Cap, 100=Edge
    double  micPosYPct   { 75.0 }; // 0=Far, 100=Close Proximity
    double  roomPct      { 15.0 }; // 0..100
};

class DualCabEngine
{
public:
    static constexpr int kMaxDelaySamples = 4096;

    DualCabEngine() = default;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;

    [[nodiscard]] const std::vector<IRPresetEntry>& getBuiltinIRs() const noexcept { return builtinIRs_; }

    void loadBuiltinIR(int cabSlot, int index);
    void loadCustomIRFile(int cabSlot, const juce::File& wavFile);
    [[nodiscard]] juce::String getActiveIRName(int cabSlot) const;

    void setControlState(const CabControlState& state) noexcept;

    // Processes mono input into stereo outL / outR (zero-allocation)
    void processBlock(const float* monoIn, float* outL, float* outR, int numSamples) noexcept;

    [[nodiscard]] int getCabAPeakSample() const noexcept { return cabAPeakSample_.load(std::memory_order_relaxed); }

private:
    void scanBuiltinIRs();
    void loadBufferIntoConvolution(int cabSlot, juce::AudioBuffer<float>&& irBuffer, double sourceSampleRate, const juce::String& name);
    void updateMicAndRoomFilters() noexcept;

    double sampleRate_ { 48000.0 };
    juce::uint32 maxBlockSize_ { 512 };

    std::vector<IRPresetEntry> builtinIRs_;
    juce::String customNameA_ { "Ampeg 8x10" };
    juce::String customNameB_ { "Ampeg SVT" };
    bool usingCustomA_ { false };
    bool usingCustomB_ { false };
    int lastBuiltinA_ { -1 };
    int lastBuiltinB_ { -1 };

    juce::dsp::Convolution convA_;
    juce::dsp::Convolution convB_;
    juce::AudioBuffer<float> workBufA_;
    juce::AudioBuffer<float> workBufB_;

    std::atomic<int> cabAPeakSample_ { 0 };
    std::atomic<int> cabBPeakSample_ { 0 };

    // Pre-allocated delay line to align Cab B to Cab A
    std::array<float, kMaxDelaySamples> alignDelayA_ {};
    std::array<float, kMaxDelaySamples> alignDelayB_ {};
    int alignWriteIdx_ { 0 };

    CabControlState state_ {};

    // Speaker excursion / breakup (64-bit)
    ADAASaturator coneBreakup_;
    ZDFStateVariableFilter coneResonance_;

    // 2D Mic Placement & Voicing filters (64-bit)
    ZDFStateVariableFilter micProximityShelf_;
    ZDFStateVariableFilter micCapEdgeLPF_;
    ZDFStateVariableFilter micVoicingBell_;
    ZDFStateVariableFilter micVoicingAir_;

    // Zero-allocation stereo Room early-reflections
    static constexpr int kRoomBufSize = 8192;
    std::array<float, kRoomBufSize> roomBufL_ {};
    std::array<float, kRoomBufSize> roomBufR_ {};
    int roomWriteIdx_ { 0 };
    ZDFStateVariableFilter roomLPF_L_, roomLPF_R_;
};

} // namespace kalyx::bass::dsp
