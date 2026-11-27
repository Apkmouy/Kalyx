#include "TubeAmpModels.h"
#include <cmath>
#include <algorithm>

namespace kalyx::bass::dsp
{

namespace
{
constexpr double kFACCapacitorsF[6] = {
    100.0e-9, // Pos 1: 100 nF
    47.0e-9,  // Pos 2: 47 nF
    10.0e-9,  // Pos 3: 10 nF
    4.7e-9,   // Pos 4: 4.7 nF
    2.2e-9,   // Pos 5: 2.2 nF
    1.0e-9    // Pos 6: 1.0 nF
};

constexpr double kMidFreqs5[5] = { 220.0, 450.0, 800.0, 1600.0, 3000.0 };
} // namespace

void TubeAmpEngine::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(sampleRate, 8000.0);
    reset();
    forceUpdate_ = true;
    updateFilters();
}

void TubeAmpEngine::reset() noexcept
{
    inputCouplingHP_.reset();
    brightShelfHP_.reset();
    dcBlocker_.reset();
    ultraLoShelf_.reset();
    ultraLoMidCut_.reset();
    ultraHiShelf_.reset();
    tsBass_.reset();
    tsMid_.reset();
    tsTreble_.reset();
    boostMid_.reset();
    presenceShelf_.reset();
    nfbRolloff_.reset();
    for (auto& b : eq9Bands_)
        b.reset();

    stageV1_.reset(0.0);
    stageV2_.reset(0.0);
    stageV3_.reset(0.0);
    stagePower_.reset(0.0);

    optoEnv_ = 0.0;
    sagEnv_  = 0.0;
    thdHeatmap_.store(0.0f, std::memory_order_relaxed);
    sagDrop_.store(0.0f, std::memory_order_relaxed);
}

void TubeAmpEngine::setControlState(const AmpControlState& state) noexcept
{
    state_ = state;
    updateFilters();
}

void TubeAmpEngine::updateFilters() noexcept
{
    dcBlocker_.setCutoffHz(sampleRate_, 12.0);

    // 1. Input Coupling & Voicing per schematic
    switch (state_.model)
    {
        case AmpModel::Heritage300:
        {
            // CH1 (12DW7): 47k / Bright C1=4.7nF||100k; CH2 (12AX7): Bright C8=4.7nF
            if (state_.channel == 0)
            {
                // Bass Select: 0=OFF, 1=BASS CUT (C4=C5=2nF -> ~80Hz HPF), 2/3/4=ULTRA LO (C3=2nF + 150k/820k)
                if (state_.ultraLoMode == 1 || state_.ultraLoMode == 2)
                    inputCouplingHP_.setFromRC(sampleRate_, 1.0e6, 2.0e-9); // ~79.6 Hz
                else
                    inputCouplingHP_.setCutoffHz(sampleRate_, 22.0);
            }
            else
            {
                inputCouplingHP_.setCutoffHz(sampleRate_, 22.0);
            }

            // Bright (C1=4.7nF || 100k -> fc ~ 338 Hz shelf)
            brightShelfHP_.setFromRC(sampleRate_, 100.0e3, 4.7e-9);

            // Ultra Lo (boost @ 45 Hz, scoop @ 450 Hz)
            const bool uLo = (state_.ultraLoMode >= 3);
            ultraLoShelf_.setLowShelf(sampleRate_, 48.0, 0.707, uLo ? 4.5 : 0.0);
            ultraLoMidCut_.setBell(sampleRate_, 480.0, 0.85, uLo ? -8.5 : 0.0);

            // Ultra Hi (C6=500pF / C12=500pF -> +7.5 dB @ 6.5 kHz)
            ultraHiShelf_.setHighShelf(sampleRate_, 6500.0, 0.707, state_.ultraHi ? 7.5 : 0.0);

            // P.E.C. 6470000 Tonestack + Toroidal Inductor 8910000
            tsBass_.setLowShelf(sampleRate_, 55.0, 0.65, state_.bassDb);
            const int mIdx = std::clamp(state_.midFreqIdx, 0, 4);
            tsMid_.setBell(sampleRate_, kMidFreqs5[mIdx], 1.15, state_.midDb);
            tsTreble_.setHighShelf(sampleRate_, 3200.0, 0.65, state_.trebleDb);

            // NFB loop 47k || 120pF
            nfbRolloff_.setLowPass(sampleRate_, 14000.0, 0.65);
            presenceShelf_.setHighShelf(sampleRate_, 3800.0, 0.707, (state_.presencePct - 50.0) * 0.06);
            boostMid_.setBell(sampleRate_, 500.0, 0.8, 0.0);
            break;
        }

        case AmpModel::ClassicTube:
        {
            inputCouplingHP_.setCutoffHz(sampleRate_, 24.0);
            brightShelfHP_.setCutoffHz(sampleRate_, 1200.0);

            // SVT-2PRO / CL: ULTRA LO (+2 dB @ 40 Hz, -10 dB @ 500 Hz), ULTRA HI (+9 dB @ 8 kHz)
            const bool uLo = (state_.ultraLoMode >= 3);
            ultraLoShelf_.setLowShelf(sampleRate_, 40.0, 0.707, uLo ? 2.5 : 0.0);
            ultraLoMidCut_.setBell(sampleRate_, 500.0, 0.90, uLo ? -10.0 : 0.0);
            ultraHiShelf_.setHighShelf(sampleRate_, 8000.0, 0.707, state_.ultraHi ? 9.0 : 0.0);

            tsBass_.setLowShelf(sampleRate_, 40.0, 0.707, state_.bassDb);
            const int mIdx = std::clamp(state_.midFreqIdx, 0, 4);
            tsMid_.setBell(sampleRate_, kMidFreqs5[mIdx], 1.25, state_.midDb);
            tsTreble_.setHighShelf(sampleRate_, 4000.0, 0.707, state_.trebleDb);

            nfbRolloff_.setLowPass(sampleRate_, 13500.0, 0.65);
            presenceShelf_.setHighShelf(sampleRate_, 4200.0, 0.707, (state_.presencePct - 50.0) * 0.05);
            boostMid_.setBell(sampleRate_, 500.0, 0.8, 0.0);
            break;
        }

        case AmpModel::Hybrid4Pro:
        {
            // 4-pos ULTRA LOW: 0=OFF, 1=SUB CUT (45Hz HP), 2=LOW CUT (2.2nF -> 95Hz), 3=ULTRA LO (22nF), 4=MODERN
            if (state_.ultraLoMode == 1)
                inputCouplingHP_.setCutoffHz(sampleRate_, 45.0);
            else if (state_.ultraLoMode == 2)
                inputCouplingHP_.setFromRC(sampleRate_, 750.0e3, 2.2e-9); // ~96 Hz
            else
                inputCouplingHP_.setCutoffHz(sampleRate_, 20.0);

            brightShelfHP_.setFromRC(sampleRate_, 100.0e3, 2.2e-9); // C28 = 2.2nF

            if (state_.ultraLoMode == 3)
            {
                ultraLoShelf_.setLowShelf(sampleRate_, 45.0, 0.707, 4.0);
                ultraLoMidCut_.setBell(sampleRate_, 450.0, 0.9, -7.0);
            }
            else if (state_.ultraLoMode == 4)
            {
                ultraLoShelf_.setLowShelf(sampleRate_, 50.0, 0.75, 6.0);
                ultraLoMidCut_.setBell(sampleRate_, 520.0, 1.0, -9.5);
            }
            else
            {
                ultraLoShelf_.setLowShelf(sampleRate_, 45.0, 0.707, 0.0);
                ultraLoMidCut_.setBell(sampleRate_, 450.0, 0.9, 0.0);
            }

            ultraHiShelf_.setHighShelf(sampleRate_, 7200.0, 0.707, state_.ultraHi ? 8.5 : 0.0);

            tsBass_.setLowShelf(sampleRate_, 45.0, 0.707, state_.bassDb);
            const int mIdx = std::clamp(state_.midFreqIdx, 0, 4);
            tsMid_.setBell(sampleRate_, kMidFreqs5[mIdx], 1.20, state_.midDb);
            tsTreble_.setHighShelf(sampleRate_, 3800.0, 0.707, state_.trebleDb);

            // Exact 9-Band ZDF Graphic EQ (07S728-02): Q = 2.2
            for (size_t i = 0; i < 9; ++i)
            {
                const double gDb = state_.eq9Enabled ? state_.eq9GainsDb[i] : 0.0;
                eq9Bands_[i].setBell(sampleRate_, kEQ9FreqsHz[i], 2.2, gDb);
            }

            nfbRolloff_.setLowPass(sampleRate_, 18000.0, 0.707);
            presenceShelf_.setHighShelf(sampleRate_, 4500.0, 0.707, (state_.presencePct - 50.0) * 0.04);
            boostMid_.setBell(sampleRate_, 500.0, 0.8, 0.0);
            break;
        }

        case AmpModel::Tangerine200:
        {
            // F.A.C. 6-pos SW1: 100nF..1nF into 1M grid resistor
            const int dIdx = std::clamp(state_.depthIdx, 0, 5);
            inputCouplingHP_.setFromRC(sampleRate_, 220.0e3, kFACCapacitorsF[dIdx]);
            brightShelfHP_.setFromRC(sampleRate_, 1.0e6, 150.0e-12); // C1 = 150pF

            ultraLoShelf_.setLowShelf(sampleRate_, 60.0, 0.707, 0.0);
            ultraLoMidCut_.setBell(sampleRate_, 500.0, 0.8, 0.0);
            ultraHiShelf_.setHighShelf(sampleRate_, 6000.0, 0.707, state_.ultraHi ? 5.5 : 0.0);

            // Baxandall Bass & Treble (C9=22nF, C10=2.2nF, C4=330pF)
            tsBass_.setLowShelf(sampleRate_, 110.0, 0.55, state_.bassDb);
            tsMid_.setBell(sampleRate_, 650.0, 0.85, state_.midDb * 0.5);
            tsTreble_.setHighShelf(sampleRate_, 2200.0, 0.55, state_.trebleDb);

            // BOOST (50k, C13=0.1uF, 1.5k, C17=2.2nF) -> thickens low-mids & gain
            const double boostDb = (state_.boostPct / 100.0) * 8.0;
            boostMid_.setBell(sampleRate_, 380.0, 0.75, boostDb);

            // PRESENCE (C20=100pF in NFB loop)
            presenceShelf_.setHighShelf(sampleRate_, 3100.0, 0.75, (state_.presencePct - 25.0) * 0.11);
            nfbRolloff_.setLowPass(sampleRate_, 11500.0, 0.70);
            break;
        }
    }
}

double TubeAmpEngine::processSample(double x) noexcept
{
    // Strict silence gate for zero input
    if (std::abs(x) < 1.0e-7)
    {
        thdHeatmap_.store(0.0f, std::memory_order_relaxed);
        sagDrop_.store(0.0f, std::memory_order_relaxed);
        return 0.0;
    }

    const double gainNorm   = std::clamp(state_.gainPct / 100.0, 0.0, 1.0);
    const double driveNorm  = std::clamp(state_.drivePct / 100.0, 0.0, 1.0);
    const double biasNorm   = std::clamp(state_.biasPct / 50.0, -1.0, 1.0); // -1..+1
    const double sagAmt     = std::clamp(state_.sagPct / 100.0, 0.0, 1.0);
    const double masterGain = 0.25 + 1.15 * std::clamp(state_.masterPct / 100.0, 0.0, 1.0);

    // 1. Input Coupling + Bright
    double sig = inputCouplingHP_.processHP(x);
    if (state_.bright)
        sig += 0.35 * brightShelfHP_.processHP(sig);

    // 2. Preamp stages (model-specific topology)
    double preDrive = 0.8 + gainNorm * 4.5;
    double biasV1   = 0.06 + biasNorm * 0.08;
    double biasV2   = -0.05 + biasNorm * 0.06;

    if (state_.model == AmpModel::Heritage300)
    {
        // CH1 (12DW7) slightly warmer/rounder; CH2 (12AX7) higher gain & bite
        const double chBoost = (state_.channel == 1) ? 1.35 : 1.0;
        sig = stageV1_.processADAA1(sig, preDrive * chBoost, biasV1);
        sig = ultraLoShelf_.processSample(sig);
        sig = ultraLoMidCut_.processSample(sig);
        sig = ultraHiShelf_.processSample(sig);
        sig = stageV2_.processADAA1(sig, 1.1 + driveNorm * 2.2, biasV2);
    }
    else if (state_.model == AmpModel::ClassicTube)
    {
        // 12AX7 + dual 12AU7 hotter bias saturating earlier above 50% gain
        const double hotThreshold = std::max(0.0, gainNorm - 0.5) * 2.0;
        const double hotBias = 0.10 + 0.12 * hotThreshold + biasNorm * 0.08;
        sig = stageV1_.processADAA1(sig, preDrive * 1.15, biasV1);
        sig = ultraLoShelf_.processSample(sig);
        sig = ultraLoMidCut_.processSample(sig);
        sig = ultraHiShelf_.processSample(sig);
        sig = stageV2_.processADAA2(sig, 1.2 + hotThreshold * 2.6 + driveNorm * 1.8, hotBias);
    }
    else if (state_.model == AmpModel::Hybrid4Pro)
    {
        // 3x 12AX7 + VTL5C7 Opto Comp + Zener 1N754 / J175 asymmetric Drive
        sig = stageV1_.processADAA1(sig, preDrive * 1.1, 0.04 + biasNorm * 0.05);
        sig = ultraLoShelf_.processSample(sig);
        sig = ultraLoMidCut_.processSample(sig);
        sig = ultraHiShelf_.processSample(sig);

        // VTL5C7 Opto Compressor (soft-knee optical envelope)
        const double compAmt = std::clamp(state_.compPct / 100.0, 0.0, 1.0);
        if (compAmt > 0.01)
        {
            const double absS = std::abs(sig);
            const double coeff = (absS > optoEnv_) ? std::exp(-1.0 / (0.008 * sampleRate_))
                                                   : std::exp(-1.0 / (0.140 * sampleRate_));
            optoEnv_ = coeff * optoEnv_ + (1.0 - coeff) * absS;
            const double thresh = 0.35 - 0.25 * compAmt;
            if (optoEnv_ > thresh)
            {
                const double gr = thresh / std::max(optoEnv_, 1.0e-6);
                sig *= (1.0 - compAmt * 0.65) + compAmt * 0.65 * gr;
            }
        }

        // Zener 6.8V + JFET asymmetric drive
        const double zenerDrive = 1.0 + driveNorm * 4.5;
        sig = stageV2_.processADAA2(sig, zenerDrive, 0.14 + biasNorm * 0.06);
    }
    else // Tangerine200
    {
        // Dual ECC83 without local NFB + BOOST stage
        const double boostFactor = 1.0 + (state_.boostPct / 100.0) * 1.6;
        sig = stageV1_.processADAA1(sig, preDrive * 1.25 * boostFactor, 0.08 + biasNorm * 0.07);
        sig = boostMid_.processSample(sig);
        sig = ultraHiShelf_.processSample(sig);
        sig = stageV2_.processADAA2(sig, 1.3 + driveNorm * 3.2, -0.09 + biasNorm * 0.07);
    }

    // 3. Tonestack & 9-Band EQ
    sig = tsBass_.processSample(sig);
    sig = tsMid_.processSample(sig);
    sig = tsTreble_.processSample(sig);

    if (state_.model == AmpModel::Hybrid4Pro && state_.eq9Enabled)
    {
        for (auto& band : eq9Bands_)
            sig = band.processSample(sig);
    }

    // 4. Phase Inverter + Power Sag (660V rail sag on bass transients) + Push-Pull Power Stage
    const double absSig = std::abs(sig);
    const double sagAtt = std::exp(-1.0 / (0.012 * sampleRate_));
    const double sagRel = std::exp(-1.0 / (0.180 * sampleRate_));
    sagEnv_ = (absSig > sagEnv_) ? (sagAtt * sagEnv_ + (1.0 - sagAtt) * absSig)
                                 : (sagRel * sagEnv_ + (1.0 - sagRel) * absSig);

    const double sagReduction = 1.0 - sagAmt * 0.32 * std::min(sagEnv_, 1.2);
    const double powerDrive   = (state_.model == AmpModel::Hybrid4Pro)
                                    ? (1.1 + masterGain * 0.7)   // 1200W high headroom
                                    : (1.4 + masterGain * 1.6);  // Tube push-pull saturation

    sig = stageV3_.processADAA1(sig * sagReduction, 1.15, biasNorm * 0.04);
    const double prePower = sig;
    sig = stagePower_.processADAA2(sig, powerDrive, biasNorm * 0.03);
    sig = presenceShelf_.processSample(sig);
    sig = nfbRolloff_.processSample(sig);
    sig = dcBlocker_.processHP(sig);

    const double out = sig * masterGain;

    // Telemetry for UI Heatmap ("Aura Réactive") & Sag meter
    const double delta = std::min(1.0, std::abs(out - prePower) * 2.5 + gainNorm * 0.25 + driveNorm * 0.25);
    thdHeatmap_.store(static_cast<float>(delta), std::memory_order_relaxed);
    sagDrop_.store(static_cast<float>((1.0 - sagReduction) * 100.0), std::memory_order_relaxed);

    return std::isfinite(out) ? std::clamp(out, -4.0, 4.0) : 0.0;
}

float TubeAmpEngine::evaluateCurveDb(const AmpControlState& st, double freqHz, double sampleRate) noexcept
{
    double totalDb = 0.0;

    // Approximate voicing + tonestack + 9-band EQ analytically using BiquadFilter::getMagnitudeDbAtFrequency
    if (st.ultraLoMode >= 3)
    {
        totalDb += BiquadFilter::getMagnitudeDbAtFrequency(BiquadFilter::makeLowShelf(sampleRate, 45.0, 4.0, 0.707), freqHz, sampleRate);
        totalDb += BiquadFilter::getMagnitudeDbAtFrequency(BiquadFilter::makePeakEQ(sampleRate, 480.0, -8.5, 0.9), freqHz, sampleRate);
    }
    if (st.ultraHi)
        totalDb += BiquadFilter::getMagnitudeDbAtFrequency(BiquadFilter::makeHighShelf(sampleRate, 7000.0, 8.0, 0.707), freqHz, sampleRate);

    totalDb += BiquadFilter::getMagnitudeDbAtFrequency(BiquadFilter::makeLowShelf(sampleRate, 55.0, st.bassDb, 0.65), freqHz, sampleRate);
    const double midF = kMidFreqs5[std::clamp(st.midFreqIdx, 0, 4)];
    totalDb += BiquadFilter::getMagnitudeDbAtFrequency(BiquadFilter::makePeakEQ(sampleRate, midF, st.midDb, 1.2), freqHz, sampleRate);
    totalDb += BiquadFilter::getMagnitudeDbAtFrequency(BiquadFilter::makeHighShelf(sampleRate, 3400.0, st.trebleDb, 0.65), freqHz, sampleRate);

    if (st.model == AmpModel::Tangerine200)
    {
        const double boostDb = (st.boostPct / 100.0) * 8.0;
        totalDb += BiquadFilter::getMagnitudeDbAtFrequency(BiquadFilter::makePeakEQ(sampleRate, 380.0, boostDb, 0.75), freqHz, sampleRate);
        totalDb += BiquadFilter::getMagnitudeDbAtFrequency(BiquadFilter::makeHighShelf(sampleRate, 3100.0, (st.presencePct - 25.0) * 0.11, 0.75), freqHz, sampleRate);
    }

    if (st.model == AmpModel::Hybrid4Pro && st.eq9Enabled)
    {
        for (size_t i = 0; i < 9; ++i)
            totalDb += BiquadFilter::getMagnitudeDbAtFrequency(
                BiquadFilter::makePeakEQ(sampleRate, kEQ9FreqsHz[i], st.eq9GainsDb[i], 2.2), freqHz, sampleRate);
    }

    return static_cast<float>(std::clamp(totalDb, -24.0, 24.0));
}

} // namespace kalyx::bass::dsp
