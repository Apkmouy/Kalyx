#pragma once

#include "ADAASaturator.h"
#include "ZDFFilters.h"
#include "../Core/BassRigParams.h"
#include <array>
#include <atomic>

namespace kalyx::bass::dsp
{

struct DIControlState
{
    bool      enabled    { true };
    DICircuit circuit    { DICircuit::TubeDI };
    double    subMixPct  { 35.0 };  // 0..100 (for Sub-Harmonic Matrix)
    double    hpfFreqHz  { 30.0 };  // 20..120
    double    lpfFreqHz  { 800.0 }; // 60..5000
    double    compPct    { 25.0 };  // 0..100
    bool      autoPhase  { true };
};

class DIPathEngine
{
public:
    static constexpr int kMaxDelaySamples = 4096;

    DIPathEngine() noexcept = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setControlState(const DIControlState& state) noexcept;
    void setAutoPhaseDelaySamples(int delaySamples) noexcept;

    [[nodiscard]] double processSample(double x) noexcept;
    [[nodiscard]] float getOptoGrDb() const noexcept { return optoGrDb_.load(std::memory_order_relaxed); }

private:
    void updateFilters() noexcept;

    double sampleRate_ { 48000.0 };
    DIControlState state_ {};

    // 24 dB/oct ZDF HPF & LPF (2 cascaded 2nd-order SVFs)
    ZDFStateVariableFilter hpf1_, hpf2_;
    ZDFStateVariableFilter lpf1_, lpf2_;

    // Circuit shaping filters
    ZDFStateVariableFilter transfoPunch_; // 2503 low-mid punch
    ZDFStateVariableFilter neveWeight_;   // Class-A 73 low-end weight
    ZDFStateVariableFilter subLPF1_, subLPF2_;
    OnePoleTPT dcBlocker_;

    ADAASaturator satEven_;
    ADAASaturator satTransfo_;

    // Sub-Harmonic Matrix (zero-allocation octave-down flip-flop + envelope)
    double subPrevX_      { 0.0 };
    double subFlipFlop_   { 1.0 };
    double subEnv_        { 0.0 };

    // Opto Compressor
    double optoEnv_       { 0.0 };
    std::atomic<float> optoGrDb_ { 0.0f };

    // Pre-allocated delay line for DI Auto-Phase
    std::array<double, kMaxDelaySamples> delayBuf_ {};
    int writeIdx_         { 0 };
    int targetDelaySamp_  { 0 };
};

} // namespace kalyx::bass::dsp
