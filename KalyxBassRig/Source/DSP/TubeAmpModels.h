#pragma once

#include "ADAASaturator.h"
#include "ZDFFilters.h"
#include "../Core/BassRigParams.h"
#include <array>
#include <atomic>

namespace kalyx::bass::dsp
{

struct AmpControlState
{
    AmpModel model       { AmpModel::Heritage300 };
    int      channel     { 0 };     // 0 = CH1 (12DW7), 1 = CH2 (12AX7)
    double   gainPct     { 42.0 };  // 0..100
    bool     bright      { false };
    int      ultraLoMode { 0 };     // 0=OFF, 1=SUB CUT, 2=LOW CUT, 3=ULTRA LO, 4=MODERN
    bool     ultraHi     { false };
    double   bassDb      { 0.0 };   // -15..+15
    double   midDb       { 0.0 };   // -15..+15
    int      midFreqIdx  { 2 };     // 0=220, 1=450, 2=800, 3=1600, 4=3000
    double   trebleDb    { 0.0 };   // -15..+15
    double   drivePct    { 25.0 };  // 0..100
    double   compPct     { 20.0 };  // 0..100 (Hybrid 4-PRO VTL5C7)
    int      depthIdx    { 1 };     // 0..5 (Tangerine 200 F.A.C. 100nF..1nF)
    double   boostPct    { 25.0 };  // 0..100 (Tangerine 200)
    double   presencePct { 35.0 };  // 0..100
    double   sagPct      { 30.0 };  // 0..100
    double   biasPct     { 0.0 };   // -50..+50
    double   masterPct   { 65.0 };  // 0..100
    double   headOutDb   { 0.0 };   // -24..+24 dB post-amp clean trim
    bool     eq9Enabled  { true };
    std::array<double, 9> eq9GainsDb { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
};

class TubeAmpEngine
{
public:
    static constexpr std::array<double, 9> kEQ9FreqsHz {
        40.0, 90.0, 180.0, 300.0, 500.0, 1000.0, 2000.0, 4000.0, 10000.0
    };

    TubeAmpEngine() noexcept = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setControlState(const AmpControlState& state) noexcept;
    [[nodiscard]] double processSample(double x) noexcept;

    // Thread-safe / lock-free UI telemetry
    [[nodiscard]] float getThdHeatmap() const noexcept { return thdHeatmap_.load(std::memory_order_relaxed); }
    [[nodiscard]] float getSagVoltsDrop() const noexcept { return sagDrop_.load(std::memory_order_relaxed); }

    // Analytical frequency response for UI curve (called on GUI thread with snapshot)
    [[nodiscard]] static float evaluateCurveDb(const AmpControlState& st, double freqHz, double sampleRate) noexcept;

private:
    void updateFilters() noexcept;

    double sampleRate_ { 48000.0 };
    AmpControlState state_ {};
    AmpControlState lastUpdated_ {};
    bool forceUpdate_ { true };

    // Preamp & Voicing filters (64-bit)
    OnePoleTPT inputCouplingHP_;
    OnePoleTPT brightShelfHP_;
    OnePoleTPT dcBlocker_;
    ZDFStateVariableFilter ultraLoShelf_;
    ZDFStateVariableFilter ultraLoMidCut_;
    ZDFStateVariableFilter ultraHiShelf_;

    // Tonestack & Inductor
    ZDFStateVariableFilter tsBass_;
    ZDFStateVariableFilter tsMid_;
    ZDFStateVariableFilter tsTreble_;
    ZDFStateVariableFilter boostMid_;
    ZDFStateVariableFilter presenceShelf_;
    ZDFStateVariableFilter nfbRolloff_;

    // 9-band ZDF Graphic EQ (07S728-02)
    std::array<ZDFStateVariableFilter, 9> eq9Bands_;

    // Non-linear ADAA stages (preamp V1, V2, V3/PI, Power/OT)
    ADAASaturator stageV1_;
    ADAASaturator stageV2_;
    ADAASaturator stageV3_;
    ADAASaturator stagePower_;

    // VTL5C7 opto compressor envelope (Hybrid 4-PRO)
    double optoEnv_ { 0.0 };
    // 660V Power Sag envelope
    double sagEnv_  { 0.0 };

    std::atomic<float> thdHeatmap_ { 0.0f };
    std::atomic<float> sagDrop_    { 0.0f };
};

} // namespace kalyx::bass::dsp
