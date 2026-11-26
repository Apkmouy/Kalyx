#pragma once

#include "../DSP/ADAASaturator.h"
#include "../DSP/ZDFFilters.h"
#include <array>
#include <atomic>
#include <cmath>

namespace kalyx::bass::fx
{

enum PedalId
{
    Pedal_OptoFetComp = 0,
    Pedal_Octaver     = 1,
    Pedal_EnvFilter   = 2,
    Pedal_TransDrive  = 3,
    Pedal_RodentBass  = 4,
    Pedal_BigWallFuzz = 5,
    Pedal_DimChorus   = 6,
    Pedal_AnalogPhaser= 7,
    Pedal_TapeEcho    = 8,
    Pedal_StudioEQ    = 9,
    NumPedals         = 10
};

struct HostSyncInfo
{
    double bpm       { 120.0 };
    bool   isPlaying { false };
};

struct PedalboardState
{
    // 0: Opto/FET Comp
    bool   compEn { false }; int compMode { 0 }; double compAmt { 40.0 }; double compBlend { 75.0 };
    // 1: Octaver
    bool   octEn { false }; double octClean { 70.0 }; double octSub1 { 50.0 }; double octSub2 { 20.0 }; double octUp { 0.0 };
    // 2: Env Filter
    bool   envEn { false }; double envSens { 55.0 }; double envAtt { 8.0 }; double envDec { 180.0 }; double envQ { 3.5 };
    // 3: Transparent Drive
    bool   tdrvEn { false }; double tdrvDrv { 45.0 }; double tdrvTone { 55.0 }; double tdrvBlend { 50.0 };
    // 4: Rodent Bass
    bool   ratEn { false }; double ratDist { 50.0 }; double ratFlt { 40.0 }; double ratBlend { 35.0 };
    // 5: Big Wall Fuzz
    bool   fuzzEn { false }; double fuzzSust { 65.0 }; double fuzzTilt { 0.0 }; double fuzzLow { 45.0 };
    // 6: Dimension Chorus
    bool   choEn { false }; double choRate { 0.8 }; double choDepth { 45.0 }; double choMix { 40.0 };
    // 7: Analog Phaser
    bool   phaEn { false }; int phaStages { 0 }; bool phaSync { false }; int phaDiv { 0 };
    double phaRate { 0.6 }; double phaDepth { 60.0 }; double phaFb { 35.0 };
    // 8: Tape Echo
    bool   tapeEn { false }; bool tapeSync { true }; int tapeDiv { 2 };
    double tapeTimeMs { 320.0 }; double tapeRep { 38.0 }; double tapeMix { 28.0 };
    // 9: Studio Bass EQ + Mono Maker
    bool   eq4En { false }; double eq4MonoHz { 120.0 };
    double eq4B1 { 0.0 }; double eq4B2 { 0.0 }; double eq4B3 { 0.0 }; double eq4B4 { 0.0 };

    // Routing: pos[i] (0..9), isPost[i] (false=PRE, true=POST)
    std::array<int, NumPedals>  pos    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    std::array<bool, NumPedals> isPost { false, false, false, false, false, false, true, true, true, true };
};

class PedalboardEngine
{
public:
    static constexpr int kMaxChorusDelay = 4096;
    static constexpr int kMaxTapeDelay   = 262144; // ~1.36s @ 192 kHz, > 5s @ 48 kHz

    PedalboardEngine() = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setState(const PedalboardState& st, const HostSyncInfo& sync) noexcept;

    void processPre(double* monoBuf, int numSamples) noexcept;
    void processPost(double* bufL, double* bufR, int numSamples) noexcept;

    [[nodiscard]] float getCompGrDb() const noexcept { return compGrDb_.load(std::memory_order_relaxed); }

    [[nodiscard]] static double divisionToMs(int divIdx, double bpm) noexcept;

private:
    void processPedalStereo(int pedalIdx, double& l, double& r) noexcept;

    // Individual 64-bit pedals
    void runComp(double& l, double& r) noexcept;
    void runOctaver(double& l, double& r) noexcept;
    void runEnvFilter(double& l, double& r) noexcept;
    void runTransDrive(double& l, double& r) noexcept;
    void runRodentBass(double& l, double& r) noexcept;
    void runBigWallFuzz(double& l, double& r) noexcept;
    void runDimChorus(double& l, double& r) noexcept;
    void runAnalogPhaser(double& l, double& r) noexcept;
    void runTapeEcho(double& l, double& r) noexcept;
    void runStudioEQ(double& l, double& r) noexcept;

    double sampleRate_ { 48000.0 };
    PedalboardState state_ {};
    HostSyncInfo sync_ {};

    std::array<int, NumPedals> preChain_ {};
    int preCount_ { 0 };
    std::array<int, NumPedals> postChain_ {};
    int postCount_ { 0 };

    // 0: Comp
    double compEnv_ { 0.0 };
    std::atomic<float> compGrDb_ { 0.0f };

    // 1: Octaver (Zero-crossing + LPF)
    dsp::ZDFStateVariableFilter octTrackLPF_;
    dsp::ZDFStateVariableFilter octSub1LPF_, octSub2LPF_, octUpHPF_;
    double octPrev_ { 0.0 }, octFF1_ { 1.0 }, octFF2_ { 1.0 }, octEnv_ { 0.0 };

    // 2: Envelope Filter
    double envFollow_ { 0.0 };
    dsp::ZDFStateVariableFilter envBP_L_, envBP_R_;

    // 3: Transparent Drive
    dsp::ADAASaturator tdrvSatL_, tdrvSatR_;
    dsp::OnePoleTPT tdrvToneL_, tdrvToneR_;

    // 4: Rodent Bass
    dsp::ZDFStateVariableFilter ratHpfL_, ratHpfR_;
    dsp::OnePoleTPT ratFilterL_, ratFilterR_;
    dsp::ADAASaturator ratSatL_, ratSatR_;

    // 5: Big Wall Fuzz
    dsp::ADAASaturator fuzzSatL_, fuzzSatR_;
    dsp::ZDFStateVariableFilter fuzzScoopL_, fuzzScoopR_, fuzzLowL_, fuzzLowR_, fuzzTiltL_, fuzzTiltR_;

    // 6: Dimension Chorus (Crossover 200Hz + Dual LFO delay)
    dsp::ZDFStateVariableFilter choXoverLP_L_, choXoverLP_R_, choXoverHP_L_, choXoverHP_R_;
    std::array<double, kMaxChorusDelay> choBufL_ {}, choBufR_ {};
    int choWrite_ { 0 };
    double choPhase_ { 0.0 };

    // 7: Analog Phaser (up to 8 all-pass stages per channel)
    double phaPhase_ { 0.0 };
    double phaFbStateL_ { 0.0 }, phaFbStateR_ { 0.0 };
    std::array<double, 8> phaApL_ {}, phaApR_ {};

    // 8: Tape Echo (pre-allocated ring + ADAA saturator + wow/flutter)
    std::array<double, kMaxTapeDelay> tapeBufL_ {}, tapeBufR_ {};
    int tapeWrite_ { 0 };
    double tapeWowPhase_ { 0.0 };
    dsp::ADAASaturator tapeSatL_, tapeSatR_;
    dsp::OnePoleTPT tapeWarmL_, tapeWarmR_;

    // 9: Studio Bass EQ + Mono Maker
    dsp::ZDFStateVariableFilter eq4L_[4], eq4R_[4];
    dsp::ZDFStateVariableFilter monoLP_L_, monoLP_R_, monoHP_L_, monoHP_R_;
};

} // namespace kalyx::bass::fx
