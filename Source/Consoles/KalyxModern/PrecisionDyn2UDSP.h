#pragma once

#include "../../Core/ConsoleModuleBase.h"
#include "../../DSPShared/ZDFFilters.h"
#include <array>
#include <atomic>

namespace kalyx::consoles
{

/**
 * Kalyx Precision VCA & Transient 2U — Moteur DSP 64 bits.
 *
 * Alliance d'un compresseur VCA de mastering à Lookahead (0..5 ms) avec courbe continue
 * (expansion 0.5:1 à compression ∞:1, Knee 0..24 dB, Attack 0.01..250 ms, Release 5..2500 ms + Auto)
 * et d'un sculpteur de transitoires Dual-Band (Punch & Sustain avec crossover 100 Hz..8 kHz).
 *
 * Mapping sur SlotParamArray2U (p[0..15]) :
 * - p[0]  : IN (Bypass / Engage)
 * - p[1]  : ORDER (0 = PRE-TRANS, 1 = POST-TRANS)
 * - p[2]  : LOOKAHEAD (0.0 .. 5.0 ms)
 * - p[3]  : AUTO REL (0 = Manual, 1 = Auto)
 * - p[4]  : THRESH (-50.0 .. 0.0 dB)
 * - p[5]  : RATIO (0.5:1 expansion .. 1.05:1 .. ∞:1)
 * - p[6]  : KNEE (0.0 .. 24.0 dB)
 * - p[7]  : ATTACK (0.01 .. 250.0 ms log)
 * - p[8]  : RELEASE (5.0 .. 2500.0 ms log)
 * - p[9]  : MAKEUP (-12.0 .. +24.0 dB)
 * - p[10] : PUNCH (-100 .. +100 %)
 * - p[11] : SUSTAIN (-100 .. +100 %)
 * - p[12] : TRANS FRQ (100 .. 8000 Hz log)
 * - p[13] : SC HPF (20 .. 500 Hz log)
 * - p[14] : BLEND (0 .. 100 %)
 * - p[15] : ROUTING (0 = STEREO, 1 = M/S, 2 = DUAL MONO)
 */
class PrecisionDyn2UDSP
{
public:
    static constexpr int kScopeSize = 128;

    struct ScopeSample
    {
        float wave  { 0.0f }; // 0..1 amplitude entrante
        float gr    { 0.0f }; // 0..1 réduction de gain (24 dB = 1.0)
        float trans { 0.0f }; // -1..+1 action transitoire normalisée
    };

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    /**
     * Traite un échantillon stéréo en 64-bit et retourne la réduction de gain (dB >= 0).
     */
    [[nodiscard]] float processSample(float& l, float& r,
                                      const kalyx::core::SlotParamArray2U& p) noexcept;

    void copyScopeHistory(std::array<ScopeSample, kScopeSize>& dest) const noexcept;

    [[nodiscard]] static double mapRatioFromNorm(float norm) noexcept;

private:
    static constexpr int kMaxLookaheadSamples = 1024; // > 5 ms @ 192 kHz (960)

    void processVcaChannel(double detSig, double& envDb, double& autoRelState,
                           double threshDb, double ratio, double kneeDb,
                           double attMs, double relMs, bool autoRel,
                           double& outGainLin, double& outGrDb) noexcept;

    void processTransientBand(double xA, double xB, double punch, double sustain,
                              double& fastEnv, double& slowEnv, double& susEnv,
                              double& outScale, double& outTransMetric) noexcept;

    double sampleRate_ { 44100.0 };

    // Buffers Lookahead pré-alloués (zéro allocation dans processBlock)
    std::array<double, kMaxLookaheadSamples> delayL_ {};
    std::array<double, kMaxLookaheadSamples> delayR_ {};
    int delayWriteIdx_ { 0 };

    // Filtres Sidechain HPF & Crossover Transient Dual-Band (LP + HP complémentaires)
    kalyx::dsp::ZDFStateVariableFilter scHpfL_, scHpfR_;
    kalyx::dsp::ZDFStateVariableFilter transLpL_, transLpR_;

    // États VCA (canal A = L ou Mid, canal B = R ou Side)
    double vcaEnvDbA_    { -120.0 };
    double vcaEnvDbB_    { -120.0 };
    double autoRelA_     { 0.1 };
    double autoRelB_     { 0.1 };

    // États Transient Shaper (bande haute & bande basse)
    double fastEnvHi_    { 0.0 };
    double slowEnvHi_    { 0.0 };
    double susEnvHi_     { 0.0 };
    double fastEnvLo_    { 0.0 };
    double slowEnvLo_    { 0.0 };
    double susEnvLo_     { 0.0 };

    // Télémétrie oscilloscope lock-free
    std::array<ScopeSample, kScopeSize> scopeRing_ {};
    std::atomic<int> scopeWriteIdx_ { 0 };
    int scopeDecimCounter_ { 0 };
    int scopeDecimPeriod_  { 256 };
    double scopePeakWave_  { 0.0 };
    double scopePeakGr_    { 0.0 };
    double scopePeakTrans_ { 0.0 };
};

} // namespace kalyx::consoles
