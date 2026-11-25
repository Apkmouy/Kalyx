#include "SurgicalEQ2UDSP.h"
#include <algorithm>
#include <cmath>

namespace kalyx::consoles
{

void SurgicalEQ2UDSP::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(sampleRate, 44100.0);
    reset();
}

void SurgicalEQ2UDSP::reset() noexcept
{
    fastEnv_ = slowEnv_ = prevMono_ = fluxEnv_ = transMix_ = 0.0;
    for (auto& b : bands_)
        b.reset();
    soloBpL_.reset();
    soloBpR_.reset();
}

double SurgicalEQ2UDSP::processFilterChain(std::array<kalyx::dsp::ZDFStateVariableFilter, kMaxStages>& chain,
                                           kalyx::dsp::ZDFStateVariableFilter& tiltLow,
                                           double x, double freqHz, double gainDb, double q,
                                           const BandMeta& meta, bool naturalPhase) noexcept
{
    const double maxNyq = sampleRate_ * 0.46;
    double fc = std::clamp(freqHz, 18.0, maxNyq);
    if (naturalPhase)
    {
        // Analog-matched high-frequency de-cramping near Nyquist
        const double ratio = fc / sampleRate_;
        if (ratio > 0.05)
            fc = std::clamp(fc * (1.0 - 0.14 * ratio * ratio), 18.0, maxNyq);
    }

    const int shape = std::clamp(meta.shape, 0, 8);
    const int slopeIdx = std::clamp(meta.slope, 0, 6); // 6,12,18,24,36,48,96 dB/oct
    static constexpr int kStagesBySlope[7] = { 1, 1, 2, 2, 3, 4, 8 };
    const int nStages = (shape == 3 || shape == 4 || shape == 6) ? kStagesBySlope[slopeIdx] : 1;

    if (shape == 0) // Bell
    {
        if (std::abs(gainDb) < 0.01) return x;
        chain[0].setBell(sampleRate_, fc, q, gainDb);
        return chain[0].processSample(x);
    }
    if (shape == 1) // Low Shelf
    {
        if (std::abs(gainDb) < 0.01) return x;
        chain[0].setLowShelf(sampleRate_, fc, std::clamp(q, 0.35, 2.0), gainDb);
        return chain[0].processSample(x);
    }
    if (shape == 2) // High Shelf
    {
        if (std::abs(gainDb) < 0.01) return x;
        chain[0].setHighShelf(sampleRate_, fc, std::clamp(q, 0.35, 2.0), gainDb);
        return chain[0].processSample(x);
    }
    if (shape == 3) // Low Cut (HPF)
    {
        double y = x;
        for (int s = 0; s < nStages; ++s)
        {
            chain[static_cast<std::size_t>(s)].setHighPass(sampleRate_, fc, (s == 0 && slopeIdx == 0) ? 0.54 : std::clamp(q, 0.5, 1.4));
            y = chain[static_cast<std::size_t>(s)].processSample(y);
        }
        return y;
    }
    if (shape == 4) // High Cut (LPF)
    {
        double y = x;
        for (int s = 0; s < nStages; ++s)
        {
            chain[static_cast<std::size_t>(s)].setLowPass(sampleRate_, fc, (s == 0 && slopeIdx == 0) ? 0.54 : std::clamp(q, 0.5, 1.4));
            y = chain[static_cast<std::size_t>(s)].processSample(y);
        }
        return y;
    }
    if (shape == 5) // Notch
    {
        chain[0].setNotch(sampleRate_, fc, std::clamp(q, 0.5, 18.0));
        return chain[0].processSample(x);
    }
    if (shape == 6) // Band Pass
    {
        double y = x;
        for (int s = 0; s < std::min(nStages, 2); ++s)
        {
            chain[static_cast<std::size_t>(s)].setBandPass(sampleRate_, fc, q);
            y = chain[static_cast<std::size_t>(s)].processSample(y);
        }
        return y;
    }
    if (shape == 7 || shape == 8) // Tilt Shelf / Flat Tilt
    {
        if (std::abs(gainDb) < 0.01) return x;
        const double halfG = gainDb * 0.5;
        const double tiltQ = (shape == 8) ? 0.45 : std::clamp(q, 0.35, 1.2);
        tiltLow.setLowShelf(sampleRate_, fc, tiltQ, -halfG);
        chain[0].setHighShelf(sampleRate_, fc, tiltQ, halfG);
        return chain[0].processSample(tiltLow.processSample(x));
    }
    return x;
}

float SurgicalEQ2UDSP::processSample(float& l, float& r,
                                     const kalyx::core::SlotParamArray2U& p,
                                     const std::array<uint32_t, 6>& metaPacked) noexcept
{
    if (p[0] <= 0.5f)
        return 0.0f;

    const double dL = static_cast<double>(l);
    const double dR = static_cast<double>(r);
    if (std::abs(dL) < 1.0e-7 && std::abs(dR) < 1.0e-7 && fastEnv_ < 1.0e-7 && slowEnv_ < 1.0e-7)
        return 0.0f;

    const bool naturalPhase = (p[1] > 0.5f);
    const int  soloBand     = kalyx::core::mapChoice(p[3], 7); // 0=Off, 1..6=Band 1..6
    const double outTrimDb  = static_cast<double>(kalyx::core::mapLin(p[5], -12.0f, 12.0f));

    // 1. Décomposition Transients / Tonal (x[n] = x_transient[n] + x_tonal[n])
    const double mono    = 0.5 * (dL + dR);
    const double absMono = std::abs(mono);
    const double flux    = std::abs(mono - prevMono_);
    prevMono_ = mono;

    const double fastA = std::exp(-1.0 / (0.0005 * sampleRate_));
    const double fastR = std::exp(-1.0 / (0.0080 * sampleRate_));
    const double slowA = std::exp(-1.0 / (0.0220 * sampleRate_));
    const double slowR = std::exp(-1.0 / (0.1400 * sampleRate_));
    const double fluxC = std::exp(-1.0 / (0.0030 * sampleRate_));

    fastEnv_ = (absMono > fastEnv_) ? (fastA * fastEnv_ + (1.0 - fastA) * absMono)
                                    : (fastR * fastEnv_ + (1.0 - fastR) * absMono);
    slowEnv_ = (absMono > slowEnv_) ? (slowA * slowEnv_ + (1.0 - slowA) * absMono)
                                    : (slowR * slowEnv_ + (1.0 - slowR) * absMono);
    fluxEnv_ = fluxC * fluxEnv_ + (1.0 - fluxC) * flux;

    const double ratio = (fastEnv_ + 1.8 * fluxEnv_) / std::max(slowEnv_ * 1.15, 1.0e-7);
    const double targetTrans = std::clamp((ratio - 1.05) * 0.75, 0.0, 1.0);
    const double mixC = std::exp(-1.0 / (0.0025 * sampleRate_));
    transMix_ = mixC * transMix_ + (1.0 - mixC) * targetTrans;

    double transL = transMix_ * dL;
    double transR = transMix_ * dR;
    double tonalL = dL - transL;
    double tonalR = dR - transR;

    // 2. Band Solo Monitor (🎧)
    if (soloBand >= 1 && soloBand <= 6)
    {
        const int b = soloBand - 1;
        const std::size_t base = static_cast<std::size_t>(6 + b * 5);
        const double fc = static_cast<double>(kalyx::core::mapLog(p[base + 0], 20.0f, 20000.0f));
        const double q  = static_cast<double>(kalyx::core::mapLog(p[base + 3], 0.2f, 18.0f));
        soloBpL_.setBandPass(sampleRate_, fc, std::clamp(q, 0.5, 8.0));
        soloBpR_.setBandPass(sampleRate_, fc, std::clamp(q, 0.5, 8.0));
        l = static_cast<float>(soloBpL_.processSample(dL));
        r = static_cast<float>(soloBpR_.processSample(dR));
        return 0.0f;
    }

    // 3. Traitement des 6 Bandes Chirurgicales (Tonal + Transient + Dynamic EQ)
    double totalDynGr = 0.0;
    for (int b = 0; b < 6; ++b)
    {
        const auto meta = unpackMeta(metaPacked[static_cast<std::size_t>(b)]);
        if (!meta.enabled) continue;

        const std::size_t base = static_cast<std::size_t>(6 + b * 5);
        const double fc      = static_cast<double>(kalyx::core::mapLog(p[base + 0], 20.0f, 20000.0f));
        const double tonalDb = static_cast<double>(kalyx::core::mapLin(p[base + 1], -24.0f, 24.0f));
        const double transDb = static_cast<double>(kalyx::core::mapLin(p[base + 2], -24.0f, 24.0f));
        const double q       = static_cast<double>(kalyx::core::mapLog(p[base + 3], 0.2f, 18.0f));
        const double dynDb   = static_cast<double>(kalyx::core::mapLin(p[base + 4], -18.0f, 18.0f));

        auto& bf = bands_[static_cast<std::size_t>(b)];

        // Dynamic EQ par bande (enveloppe auto-attaque/relâchement)
        double effDynDb = 0.0;
        if (std::abs(dynDb) > 0.05)
        {
            bf.dynDetA.setBandPass(sampleRate_, fc, std::clamp(q, 0.5, 6.0));
            bf.dynDetB.setBandPass(sampleRate_, fc, std::clamp(q, 0.5, 6.0));
            const double detSig = std::max(std::abs(bf.dynDetA.processSample(dL)),
                                           std::abs(bf.dynDetB.processSample(dR)));
            const double detDb = 20.0 * std::log10(std::max(detSig, 1.0e-7));
            const double over  = std::clamp((detDb - (-24.0)) / 18.0, 0.0, 1.0);
            const double targetDyn = over * dynDb;
            const double dAtt = std::exp(-1.0 / (0.008 * sampleRate_));
            const double dRel = std::exp(-1.0 / (0.095 * sampleRate_));
            bf.dynEnvDb = (std::abs(targetDyn) > std::abs(bf.dynEnvDb))
                        ? (dAtt * bf.dynEnvDb + (1.0 - dAtt) * targetDyn)
                        : (dRel * bf.dynEnvDb + (1.0 - dRel) * targetDyn);
            effDynDb = bf.dynEnvDb;
            if (effDynDb < 0.0)
                totalDynGr += (-effDynDb);
        }

        const double effTonalDb = tonalDb + effDynDb;
        const double effTransDb = transDb + effDynDb;

        // Placement stéréo : 0=Stereo, 1=Mid, 2=Side, 3=Left, 4=Right
        if (meta.stereo == 0)
        {
            tonalL = processFilterChain(bf.tonalA, bf.tiltLowA, tonalL, fc, effTonalDb, q, meta, naturalPhase);
            tonalR = processFilterChain(bf.tonalB, bf.tiltLowB, tonalR, fc, effTonalDb, q, meta, naturalPhase);
            transL = processFilterChain(bf.transA, bf.tiltLowA, transL, fc, effTransDb, q, meta, naturalPhase);
            transR = processFilterChain(bf.transB, bf.tiltLowB, transR, fc, effTransDb, q, meta, naturalPhase);
        }
        else if (meta.stereo == 1 || meta.stereo == 2) // Mid / Side
        {
            double tonM = 0.5 * (tonalL + tonalR), tonS = 0.5 * (tonalL - tonalR);
            double trnM = 0.5 * (transL + transR), trnS = 0.5 * (transL - transR);
            if (meta.stereo == 1)
            {
                tonM = processFilterChain(bf.tonalA, bf.tiltLowA, tonM, fc, effTonalDb, q, meta, naturalPhase);
                trnM = processFilterChain(bf.transA, bf.tiltLowA, trnM, fc, effTransDb, q, meta, naturalPhase);
            }
            else
            {
                tonS = processFilterChain(bf.tonalB, bf.tiltLowB, tonS, fc, effTonalDb, q, meta, naturalPhase);
                trnS = processFilterChain(bf.transB, bf.tiltLowB, trnS, fc, effTransDb, q, meta, naturalPhase);
            }
            tonalL = tonM + tonS; tonalR = tonM - tonS;
            transL = trnM + trnS; transR = trnM - trnS;
        }
        else if (meta.stereo == 3) // Left only
        {
            tonalL = processFilterChain(bf.tonalA, bf.tiltLowA, tonalL, fc, effTonalDb, q, meta, naturalPhase);
            transL = processFilterChain(bf.transA, bf.tiltLowA, transL, fc, effTransDb, q, meta, naturalPhase);
        }
        else // Right only
        {
            tonalR = processFilterChain(bf.tonalB, bf.tiltLowB, tonalR, fc, effTonalDb, q, meta, naturalPhase);
            transR = processFilterChain(bf.transB, bf.tiltLowB, transR, fc, effTransDb, q, meta, naturalPhase);
        }
    }

    const double outGain = std::pow(10.0, outTrimDb / 20.0);
    l = static_cast<float>((tonalL + transL) * outGain);
    r = static_cast<float>((tonalR + transR) * outGain);
    return static_cast<float>(totalDynGr);
}

double SurgicalEQ2UDSP::computeBandMagnitudeDb(double freqHz, double /*sampleRate*/,
                                               double centerHz, double gainDb, double q,
                                               const BandMeta& meta) noexcept
{
    if (!meta.enabled) return 0.0;
    const double r = std::max(freqHz, 1.0) / std::max(centerHz, 1.0);
    const double logR = std::log2(r);
    static constexpr double kSlopeDb[7] = { 6.0, 12.0, 18.0, 24.0, 36.0, 48.0, 96.0 };
    const double slope = kSlopeDb[std::clamp(meta.slope, 0, 6)];

    switch (meta.shape)
    {
        case 0: // Bell
        {
            const double bw = 1.0 / std::max(q, 0.2);
            const double x = logR / (0.65 * bw);
            return gainDb / (1.0 + x * x);
        }
        case 1: // Low Shelf
            return gainDb / (1.0 + std::exp(3.2 * logR * std::clamp(q, 0.4, 2.0)));
        case 2: // High Shelf
            return gainDb / (1.0 + std::exp(-3.2 * logR * std::clamp(q, 0.4, 2.0)));
        case 3: // Low Cut
            return (r < 1.0) ? std::max(-48.0, slope * logR) : 0.0;
        case 4: // High Cut
            return (r > 1.0) ? std::max(-48.0, -slope * logR) : 0.0;
        case 5: // Notch
        {
            const double bw = 0.35 / std::max(q, 0.5);
            const double x = logR / bw;
            return -28.0 / (1.0 + x * x);
        }
        case 6: // Band Pass
            return std::max(-36.0, -12.0 * std::abs(logR) * std::clamp(q, 0.4, 4.0));
        case 7: // Tilt Shelf
        case 8: // Flat Tilt
            return gainDb * 0.5 * std::tanh(1.4 * logR);
        default:
            return 0.0;
    }
}

double SurgicalEQ2UDSP::computeCombinedMagnitudeDb(double freqHz, double sampleRate,
                                                   const kalyx::core::SlotParamArray2U& p,
                                                   const std::array<uint32_t, 6>& metaPacked,
                                                   bool transientCurve) noexcept
{
    double sum = 0.0;
    for (int b = 0; b < 6; ++b)
    {
        const auto meta = unpackMeta(metaPacked[static_cast<std::size_t>(b)]);
        if (!meta.enabled) continue;
        const std::size_t base = static_cast<std::size_t>(6 + b * 5);
        const double fc = static_cast<double>(kalyx::core::mapLog(p[base + 0], 20.0f, 20000.0f));
        const double g  = static_cast<double>(kalyx::core::mapLin(p[base + (transientCurve ? 2 : 1)], -24.0f, 24.0f));
        const double q  = static_cast<double>(kalyx::core::mapLog(p[base + 3], 0.2f, 18.0f));
        sum += computeBandMagnitudeDb(freqHz, sampleRate, fc, g, q, meta);
    }
    return sum;
}

} // namespace kalyx::consoles
