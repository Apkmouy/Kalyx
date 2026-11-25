#pragma once

#include "../../Core/ConsoleModuleBase.h"
#include "../../DSPShared/ZDFFilters.h"
#include <atomic>

namespace kalyx::consoles
{

/**
 * Kalyx Harmonic Sculptor (1U) — Moteur DSP 64 bits.
 *
 * Générateurs harmoniques dissociés EVEN (2nd, asymétrique Tube/Class-A) et ODD (3rd, Tape/Transistor)
 * via ADAA 64-bit avec repli Taylor (1e-4), filtre Tilt Pré/Post (TILT LNK),
 * contrôle DYNAMICS / PUNCH (-50..+50 %) et sélecteur de bande active (FULL / LOW / MID / HI).
 *
 * Mapping sur SlotParamArray (p[0..9]) :
 * - p[0] : IN (0/1)
 * - p[1] : TILT LNK (0/1)
 * - p[2] : EVEN 2nd (0..100 %)
 * - p[3] : ODD 3rd (0..100 %)
 * - p[4] : TILT EQ (-6..+6 dB)
 * - p[5] : DYNAMICS (-50..+50 %)
 * - p[6] : DRIVE (-12..+24 dB)
 * - p[7] : BLEND (0..100 %)
 * - p[8] : BAND (0=FULL, 1=LOW<250Hz, 2=MID, 3=HI>3kHz)
 * - p[9] : ROUTING (0=STEREO, 1=M/S, 2=MID, 3=SIDE)
 */
class HarmonicSculptorDSP
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void processSample(float& l, float& r,
                       const kalyx::core::SlotParamArray& p) noexcept;

    [[nodiscard]] float getInstantThdPct() const noexcept
    {
        return instantThdPct_.load(std::memory_order_relaxed);
    }

private:
    struct ChannelState
    {
        double prevEvenIn { 0.0 };
        double prevOddIn  { 0.0 };
        double dcX1       { 0.0 };
        double dcY1       { 0.0 };
        double fastEnv    { 0.0 };
        double slowEnv    { 0.0 };
        kalyx::dsp::ZDFStateVariableFilter bandLp;
        kalyx::dsp::ZDFStateVariableFilter bandHp;
        kalyx::dsp::ZDFStateVariableFilter preTiltLo;
        kalyx::dsp::ZDFStateVariableFilter preTiltHi;
        kalyx::dsp::ZDFStateVariableFilter postTiltLo;
        kalyx::dsp::ZDFStateVariableFilter postTiltHi;
    };

    double processChannel(double x, ChannelState& st,
                          double evenAmt, double oddAmt,
                          double tiltDb, bool tiltLink,
                          double dynAmt, double driveLin,
                          int bandMode, double& outThdSample) noexcept;

    double sampleRate_ { 44100.0 };
    ChannelState chA_, chB_;
    double thdSmooth_ { 0.0 };
    std::atomic<float> instantThdPct_ { 0.0f };
};

} // namespace kalyx::consoles
