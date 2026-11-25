#pragma once

#include "../../Core/ConsoleModuleBase.h"
#include "../../DSPShared/ZDFFilters.h"
#include <array>
#include <cstdint>

namespace kalyx::consoles
{

/**
 * Kalyx Surgical EQ 2U — Moteur DSP 64 bits (SplitEQ Transients/Tonal + 6 Bandes ZDF + Dynamic EQ).
 *
 * Mapping sur le double réservoir APVTS 2U (slotHost_p1..p18 + slotPartner_p1..p18 = 36 paramètres) :
 * - p[0]  : EQ IN (Bypass / Engage)
 * - p[1]  : PHASE MODE (0 = ZERO LAT, 1 = NATURAL)
 * - p[2]  : FFT MODE (0 = PRE, 1 = POST)
 * - p[3]  : SOLO BAND (0 = Off, 1..6 = Band 1..6)
 * - p[4]  : ACTIVE BAND (0..5)
 * - p[5]  : OUT TRIM (-12 .. +12 dB)
 * - Pour b = 0..5 (base = 6 + b * 5) :
 *     * p[base + 0] : FREQ (20 Hz .. 20 kHz log)
 *     * p[base + 1] : TONAL GAIN (-24 .. +24 dB)
 *     * p[base + 2] : TRANSIENT GAIN (-24 .. +24 dB)
 *     * p[base + 3] : Q (0.2 .. 18.0 log)
 *     * p[base + 4] : DYN RANGE (-18 .. +18 dB)
 *
 * Métadonnées lock-free par bande (uint32_t packed) :
 * - bit 0     : enabled (0/1)
 * - bits 1..4 : shape (0=Bell, 1=LowShelf, 2=HighShelf, 3=LowCut, 4=HighCut, 5=Notch, 6=BandPass, 7=TiltShelf, 8=FlatTilt)
 * - bits 5..7 : slope (0=6, 1=12, 2=18, 3=24, 4=36, 5=48, 6=96 dB/oct)
 * - bits 8..10: stereo (0=Stereo, 1=Mid, 2=Side, 3=Left, 4=Right)
 */
class SurgicalEQ2UDSP
{
public:
    enum class BandShape : int
    {
        Bell = 0, LowShelf = 1, HighShelf = 2, LowCut = 3, HighCut = 4,
        Notch = 5, BandPass = 6, TiltShelf = 7, FlatTilt = 8
    };

    enum class BandStereo : int
    {
        Stereo = 0, Mid = 1, Side = 2, Left = 3, Right = 4
    };

    struct BandMeta
    {
        bool enabled { true };
        int  shape   { 0 }; // 0..8
        int  slope   { 3 }; // 0..6 (default 24 dB/oct)
        int  stereo  { 0 }; // 0..4 (Stereo)
    };

    static constexpr uint32_t packMeta(const BandMeta& m) noexcept
    {
        return (m.enabled ? 1u : 0u)
             | ((static_cast<uint32_t>(m.shape)  & 0xFu) << 1u)
             | ((static_cast<uint32_t>(m.slope)  & 0x7u) << 5u)
             | ((static_cast<uint32_t>(m.stereo) & 0x7u) << 8u);
    }

    static constexpr BandMeta unpackMeta(uint32_t packed) noexcept
    {
        BandMeta m;
        m.enabled = (packed & 1u) != 0u;
        m.shape   = static_cast<int>((packed >> 1u) & 0xFu);
        m.slope   = static_cast<int>((packed >> 5u) & 0x7u);
        m.stereo  = static_cast<int>((packed >> 8u) & 0x7u);
        return m;
    }

    static constexpr std::array<uint32_t, 6> getDefaultBandMeta() noexcept
    {
        return {{
            packMeta({ true, static_cast<int>(BandShape::LowShelf),  3, 0 }),
            packMeta({ true, static_cast<int>(BandShape::Bell),      3, 0 }),
            packMeta({ true, static_cast<int>(BandShape::Bell),      3, 0 }),
            packMeta({ true, static_cast<int>(BandShape::Bell),      3, 0 }),
            packMeta({ true, static_cast<int>(BandShape::Bell),      3, 0 }),
            packMeta({ true, static_cast<int>(BandShape::HighShelf), 3, 0 })
        }};
    }

    SurgicalEQ2UDSP() noexcept = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    [[nodiscard]] float processSample(float& l, float& r,
                                      const kalyx::core::SlotParamArray2U& p,
                                      const std::array<uint32_t, 6>& metaPacked) noexcept;

    [[nodiscard]] static double computeBandMagnitudeDb(double freqHz, double sampleRate,
                                                       double centerHz, double gainDb, double q,
                                                       const BandMeta& meta) noexcept;

    [[nodiscard]] static double computeCombinedMagnitudeDb(double freqHz, double sampleRate,
                                                           const kalyx::core::SlotParamArray2U& p,
                                                           const std::array<uint32_t, 6>& metaPacked,
                                                           bool transientCurve) noexcept;

private:
    static constexpr int kMaxStages = 8; // Jusqu'à 96 dB/oct (8 x 12 dB/oct)

    struct BandFilters
    {
        std::array<kalyx::dsp::ZDFStateVariableFilter, kMaxStages> tonalA {};
        std::array<kalyx::dsp::ZDFStateVariableFilter, kMaxStages> tonalB {};
        std::array<kalyx::dsp::ZDFStateVariableFilter, kMaxStages> transA {};
        std::array<kalyx::dsp::ZDFStateVariableFilter, kMaxStages> transB {};
        kalyx::dsp::ZDFStateVariableFilter dynDetA, dynDetB;
        kalyx::dsp::ZDFStateVariableFilter tiltLowA, tiltLowB;
        double dynEnvDb { 0.0 };

        void reset() noexcept
        {
            for (auto& f : tonalA) f.reset();
            for (auto& f : tonalB) f.reset();
            for (auto& f : transA) f.reset();
            for (auto& f : transB) f.reset();
            dynDetA.reset(); dynDetB.reset();
            tiltLowA.reset(); tiltLowB.reset();
            dynEnvDb = 0.0;
        }
    };

    double processFilterChain(std::array<kalyx::dsp::ZDFStateVariableFilter, kMaxStages>& chain,
                              kalyx::dsp::ZDFStateVariableFilter& tiltLow,
                              double x, double freqHz, double gainDb, double q,
                              const BandMeta& meta, bool naturalPhase) noexcept;

    double sampleRate_ { 44100.0 };

    // Détecteur SplitEQ Transients / Tonal 64-bit
    double fastEnv_   { 0.0 };
    double slowEnv_   { 0.0 };
    double prevMono_  { 0.0 };
    double fluxEnv_   { 0.0 };
    double transMix_  { 0.0 };

    std::array<BandFilters, 6> bands_ {};
    kalyx::dsp::ZDFStateVariableFilter soloBpL_, soloBpR_;
};

} // namespace kalyx::consoles
