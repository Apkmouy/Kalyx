#pragma once

#include "../../Core/ConsoleModuleBase.h"
#include "../../DSPShared/ZDFFilters.h"
#include <atomic>

namespace kalyx::consoles
{

/**
 * Kalyx Stereo & Phase Studio (1U) — Moteur DSP 64 bits.
 *
 * Outil de bus stéréo & mastering :
 * - MONO MAKER (20..500 Hz, filtre Linkwitz-Riley 24 dB/oct sur le canal Side)
 * - STEREO WIDTH (0..200 % avec compensation automatique d'énergie)
 * - PHASE ROTATE (-180°..+180°, filtre All-Pass continu)
 * - SIDE TILT EQ (-6..+6 dB sur le canal Side)
 * - MID GAIN / SIDE GAIN (-12..+12 dB)
 * - Inversion de phase (Ø L, Ø R), permutation L<>R, écoute isolée (STEREO / MID / SIDE)
 * - Corrélomètre de phase temps réel (-1.0 .. +1.0)
 *
 * Mapping sur SlotParamArray (p[0..11]) :
 * - p[0]  : IN (0/1)
 * - p[1]  : MODE (0 = L/R, 1 = M/S)
 * - p[2]  : STEREO WIDTH (0..200 %)
 * - p[3]  : MONO MAKER (20..500 Hz log)
 * - p[4]  : PHASE ROTATE (-180..+180 deg)
 * - p[5]  : SIDE TILT (-6..+6 dB)
 * - p[6]  : MID GAIN (-12..+12 dB)
 * - p[7]  : SIDE GAIN (-12..+12 dB)
 * - p[8]  : PHASE INV L (0/1)
 * - p[9]  : PHASE INV R (0/1)
 * - p[10] : SWAP L<>R (0/1)
 * - p[11] : MONITOR (0=STEREO, 1=MID, 2=SIDE)
 */
class StereoPhaseDSP
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void processSample(float& l, float& r,
                       const kalyx::core::SlotParamArray& p) noexcept;

    [[nodiscard]] float getCorrelation() const noexcept
    {
        return correlation_.load(std::memory_order_relaxed);
    }

private:
    struct AllPass1
    {
        double x1 { 0.0 };
        double y1 { 0.0 };
        void reset() noexcept { x1 = 0.0; y1 = 0.0; }
        double process(double x, double coeff) noexcept
        {
            const double y = coeff * x + x1 - coeff * y1;
            x1 = x;
            y1 = y;
            return y;
        }
    };

    double sampleRate_ { 44100.0 };

    // Linkwitz-Riley 24 dB/oct (2 HPF Butterworth 12 dB/oct en cascade sur Side pour Mono Maker)
    kalyx::dsp::ZDFStateVariableFilter monoMakerHp1_;
    kalyx::dsp::ZDFStateVariableFilter monoMakerHp2_;

    // Side Tilt EQ
    kalyx::dsp::ZDFStateVariableFilter sideTiltLo_;
    kalyx::dsp::ZDFStateVariableFilter sideTiltHi_;

    // All-Pass 2 étages pour rotation de phase continue (-180°..+180°)
    AllPass1 apL1_, apL2_, apR1_, apR2_;

    // Corrélomètre temps réel
    double corrLR_   { 0.0 };
    double corrLL_   { 0.0 };
    double corrRR_   { 0.0 };
    std::atomic<float> correlation_ { 1.0f };
};

} // namespace kalyx::consoles
