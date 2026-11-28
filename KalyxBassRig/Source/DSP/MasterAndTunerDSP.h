#pragma once

#include "ZDFFilters.h"
#include <array>
#include <atomic>
#include <cmath>

namespace kalyx::bass::dsp
{

class NoiseGate64
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    [[nodiscard]] double processSample(double x, bool enabled, double threshDb) noexcept;
    [[nodiscard]] float getGateGain() const noexcept { return gateGain_.load(std::memory_order_relaxed); }

private:
    double sampleRate_ { 48000.0 };
    double env_        { 0.0 };
    double gain_       { 1.0 };
    std::atomic<float> gateGain_ { 1.0f };
};

class StrobeTuner64
{
public:
    static constexpr int kBufSize = 2048;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void pushSample(double x) noexcept;

    [[nodiscard]] float getFrequencyHz() const noexcept { return freqHz_.load(std::memory_order_relaxed); }
    [[nodiscard]] int   getMidiNote()    const noexcept { return midiNote_.load(std::memory_order_relaxed); }
    [[nodiscard]] float getCents()       const noexcept { return cents_.load(std::memory_order_relaxed); }
    [[nodiscard]] float getConfidence()  const noexcept { return confidence_.load(std::memory_order_relaxed); }

private:
    void analyzePitch() noexcept;

    double sampleRate_ { 48000.0 };
    std::array<double, kBufSize> ring_ {};
    int writeIdx_ { 0 };
    int hopCounter_ { 0 };

    std::atomic<float> freqHz_     { 0.0f };
    std::atomic<int>   midiNote_   { -1 };
    std::atomic<float> cents_      { 0.0f };
    std::atomic<float> confidence_ { 0.0f };
};

class BrickwallLimiter64
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void processStereo(double& l, double& r, bool enabled, double ceilingDb) noexcept;
    [[nodiscard]] float getGrDb() const noexcept { return grDb_.load(std::memory_order_relaxed); }

private:
    double sampleRate_ { 48000.0 };
    double envGain_    { 1.0 };
    std::atomic<float> grDb_ { 0.0f };
};

class SmartKickRelief64
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void processStereo(double& l, double& r, double sidechainSample,
                       bool enabled, double scInDb, double scHpfHz,
                       double duckDepthDb, bool scListen) noexcept;
    [[nodiscard]] float getDuckDb() const noexcept { return duckDb_.load(std::memory_order_relaxed); }

private:
    double sampleRate_ { 48000.0 };
    double scEnv_      { 0.0 };
    double lastHpfHz_  { -1.0 };
    ZDFStateVariableFilter scHpf_;
    ZDFStateVariableFilter dynBellL_, dynBellR_;
    std::atomic<float> duckDb_ { 0.0f };
};

} // namespace kalyx::bass::dsp
