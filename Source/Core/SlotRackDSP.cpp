#include "SlotRackDSP.h"
#include <cmath>
#include <algorithm>
#include <array>

namespace kalyx::core
{

// =============================================================================
// InputStripDSP (Tranche Fixe 1 : INPUT)
// =============================================================================
void InputStripDSP::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(sampleRate, 44100.0);
    reset();
}

void InputStripDSP::reset() noexcept
{
    satL_.reset(); satR_.reset();
    hystL_.reset(); hystR_.reset();
    midFocusL_.reset(); midFocusR_.reset();
    hpfL_.reset(); hpfR_.reset();
    lpfL_.reset(); lpfR_.reset();
}

void InputStripDSP::processSample(float& l, float& r,
                                  float& scL, float& scR,
                                  const InputStripParameters& p,
                                  float tolBiasL, float tolBiasR) noexcept
{
    const float gainLin = std::pow(10.0f, p.trimDb / 20.0f) * (p.phaseInvert ? -1.0f : 1.0f);
    l *= gainLin;
    r *= gainLin;

    const float drvNorm = std::clamp(p.drivePct * 0.01f, 0.0f, 1.0f);
    if (drvNorm > 1.0e-4f && p.preampModel != static_cast<int>(PreampModel::KalyxClean))
    {
        float drive = 0.5f + drvNorm * 3.2f;
        float bias  = 0.025f;
        int hystMod = 0;

        switch (static_cast<PreampModel>(p.preampModel))
        {
            case PreampModel::SSL4000E: drive = 0.50f + drvNorm * 3.2f; bias = 0.025f; hystMod = 0; break;
            case PreampModel::SSL4000G: drive = 0.40f + drvNorm * 2.4f; bias = 0.008f; hystMod = 1; break;
            case PreampModel::API1608:  drive = 0.65f + drvNorm * 3.8f; bias = 0.140f; hystMod = 2; break;
            case PreampModel::Neve1073: drive = 0.70f + drvNorm * 3.6f; bias = 0.090f; hystMod = 3; break;
            case PreampModel::KalyxClean:
            default: break;
        }

        l = (1.0f - drvNorm) * l + drvNorm * satL_.processADAA1(l, drive, bias + tolBiasL);
        r = (1.0f - drvNorm) * r + drvNorm * satR_.processADAA1(r, drive, bias + tolBiasR);

        if (p.preampModel == static_cast<int>(PreampModel::Neve1073))
        {
            l = hystL_.processSample(l, drvNorm * 0.85f, sampleRate_, hystMod);
            r = hystR_.processSample(r, drvNorm * 0.85f, sampleRate_, hystMod);
        }
        else if (p.preampModel == static_cast<int>(PreampModel::API1608))
        {
            midFocusL_.setBell(sampleRate_, 780.0, 0.90, 1.5 * drvNorm);
            midFocusR_.setBell(sampleRate_, 780.0, 0.90, 1.5 * drvNorm);
            l = midFocusL_.processSample(l);
            r = midFocusR_.processSample(r);
        }
    }
    else
    {
        (void)satL_.processADAA1(0.0f, 0.0f, 0.0f);
        (void)satR_.processADAA1(0.0f, 0.0f, 0.0f);
        (void)hystL_.processSample(0.0f, 0.0f, sampleRate_, 0);
        (void)hystR_.processSample(0.0f, 0.0f, sampleRate_, 0);
    }

    if (p.hpfEnabled)
    {
        hpfL_.setHighPass(sampleRate_, p.hpfFreqHz, 0.7071);
        hpfR_.setHighPass(sampleRate_, p.hpfFreqHz, 0.7071);
        if (p.filtersToSc) { scL = hpfL_.processSample(scL); scR = hpfR_.processSample(scR); }
        else               { l   = hpfL_.processSample(l);   r   = hpfR_.processSample(r);   }
    }

    if (p.lpfEnabled)
    {
        lpfL_.setLowPass(sampleRate_, p.lpfFreqHz, 0.7071);
        lpfR_.setLowPass(sampleRate_, p.lpfFreqHz, 0.7071);
        if (p.filtersToSc) { scL = lpfL_.processSample(scL); scR = lpfR_.processSample(scR); }
        else               { l   = lpfL_.processSample(l);   r   = lpfR_.processSample(r);   }
    }
}

// =============================================================================
// SlotUnitDSP (Unité modulaire Série 500 avec Crossfade 15 ms sans clic)
// =============================================================================
void SlotUnitDSP::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(sampleRate, 44100.0);
    fadeTotal_  = std::max(1, static_cast<int>(std::round(0.015 * sampleRate_)));
    slotPreampDsp_.prepare(sampleRate_);
    ua1176Dsp_.prepare(sampleRate_);
    la2aDsp_.prepare(sampleRate_);
    distressorDsp_.prepare(sampleRate_);
    fairchildDsp_.prepare(sampleRate_);
    surgicalEq2uDsp_.prepare(sampleRate_);
    precisionDyn2uDsp_.prepare(sampleRate_);
    harmonicSculptorDsp_.prepare(sampleRate_);
    stereoPhaseDsp_.prepare(sampleRate_);
    slotAutoGain_.prepare(sampleRate_);
    reset();
}

void SlotUnitDSP::reset() noexcept
{
    fadeRemaining_ = 0;
    slotAutoGain_.reset();
    lastAutoGainDb_ = 0.0f;
    slotPreampDsp_.reset();
    ua1176Dsp_.reset();
    la2aDsp_.reset();
    distressorDsp_.reset();
    fairchildDsp_.reset();
    surgicalEq2uDsp_.reset();
    precisionDyn2uDsp_.reset();
    harmonicSculptorDsp_.reset();
    stereoPhaseDsp_.reset();
    gateEnv_ = 1.0; gateHoldCount_ = 0; gateOpen_ = true;
    deessEnvDb_ = 0.0;
    deessScL_.reset(); deessScR_.reset(); deessNotchL_.reset(); deessNotchR_.reset();

    scHpfL_.reset(); scHpfR_.reset();
    compEnvADb_ = 0.0; compEnvBDb_ = 0.0; compFastRc_ = 0.0; compSlowRc_ = 0.0;
    apiPrevGainL_ = 1.0; apiPrevGainR_ = 1.0;
    apiThrustLpL_ = 0.0; apiThrustLpR_ = 0.0;
    diodeSatL_.reset(); diodeSatR_.reset();

    eqHpfL_.reset(); eqHpfR_.reset();
    lfL_.reset(); lfR_.reset(); lmfL_.reset(); lmfR_.reset(); lmfDipL_.reset(); lmfDipR_.reset();
    hmfL_.reset(); hmfR_.reset(); hmfDipL_.reset(); hmfDipR_.reset(); hfL_.reset(); hfR_.reset();

    colSatL_.reset(); colSatR_.reset(); colHystL_.reset(); colHystR_.reset();
    colToneLowL_.reset(); colToneLowR_.reset(); colToneHighL_.reset(); colToneHighR_.reset();
    colSpecL_.reset(); colSpecR_.reset(); xtalkHpfL_.reset(); xtalkHpfR_.reset();

    limEnvDb_ = 0.0; limFastRc_ = 0.0; limSlowRc_ = 0.0;
}

void SlotUnitDSP::notifyModuleSelection(int newModuleId) noexcept
{
    const int clamped = std::clamp(newModuleId, 0, static_cast<int>(SlotModuleId::Count) - 1);
    if (clamped != activeModuleId_)
    {
        previousModuleId_ = activeModuleId_;
        activeModuleId_   = clamped;
        fadeTotal_        = std::max(1, static_cast<int>(std::round(0.015 * sampleRate_)));
        fadeRemaining_    = fadeTotal_;
    }
}

float SlotUnitDSP::computeSslEqQ(float baseQ, float gainDb, bool is4000G) noexcept
{
    const float absG = std::abs(gainDb);
    return is4000G ? std::clamp(baseQ * 0.85f * (1.0f + 0.095f * absG), 0.30f, 5.50f)
                   : std::clamp(baseQ * (1.0f + 0.048f * absG), 0.35f, 6.00f);
}

float SlotUnitDSP::computeApi550BQ(float baseQ, float gainDb) noexcept
{
    return std::clamp(0.55f * (1.0f + 0.22f * std::abs(gainDb)) * (baseQ / 1.2f), 0.40f, 6.50f);
}

float SlotUnitDSP::quantizeApi550BGain(float gainDb, bool stepped) noexcept
{
    if (!stepped) return gainDb;
    static constexpr std::array<float, 11> kSteps {{ -12.f, -9.f, -6.f, -4.f, -2.f, 0.f, 2.f, 4.f, 6.f, 9.f, 12.f }};
    float best = 0.0f, bestDiff = 999.0f;
    for (float s : kSteps)
    {
        const float d = std::abs(gainDb - s);
        if (d < bestDiff) { bestDiff = d; best = s; }
    }
    return best;
}

float SlotUnitDSP::quantizeApi550BFreq(int bandIdx, float freqHz, bool stepped) noexcept
{
    if (!stepped) return freqHz;
    static constexpr std::array<std::array<float, 7>, 4> k550BFreqs {{
        {{   30.f,   40.f,   50.f,  100.f,  200.f,  300.f,   400.f }}, // LF
        {{   75.f,  150.f,  180.f,  240.f,  500.f,  700.f,  1000.f }}, // LMF
        {{  800.f, 1500.f, 3000.f, 5000.f, 8000.f,10000.f, 12500.f }}, // HMF
        {{ 2500.f, 5000.f, 7000.f,10000.f,12500.f,15000.f, 20000.f }}  // HF
    }};
    const auto& tbl = k550BFreqs[static_cast<std::size_t>(std::clamp(bandIdx, 0, 3))];
    float best = tbl[0], bestDiff = 1.0e9f;
    for (float f : tbl)
    {
        const float d = std::abs(freqHz - f);
        if (d < bestDiff) { bestDiff = d; best = f; }
    }
    return best;
}

float SlotUnitDSP::quantizeApi2500RelMs(float relMs, bool isFixed) noexcept
{
    if (!isFixed) return relMs;
    static constexpr float kFixedRels[6] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f };
    float best = kFixedRels[0], bestDiff = 1.0e9f;
    for (float r : kFixedRels)
    {
        const float d = std::abs(relMs - r);
        if (d < bestDiff) { bestDiff = d; best = r; }
    }
    return best;
}

float SlotUnitDSP::quantizeNeve1073LowFreq(float freqHz, bool stepped) noexcept
{
    if (!stepped) return freqHz;
    static constexpr float kLow[4] = { 35.0f, 60.0f, 110.0f, 220.0f };
    float best = kLow[0], bestDiff = 1.0e9f;
    for (float f : kLow) { const float d = std::abs(freqHz - f); if (d < bestDiff) { bestDiff = d; best = f; } }
    return best;
}

float SlotUnitDSP::quantizeNeve1073MidFreq(float freqHz, bool stepped) noexcept
{
    if (!stepped) return freqHz;
    static constexpr float kMid[6] = { 360.0f, 700.0f, 1600.0f, 3200.0f, 4800.0f, 7200.0f };
    float best = kMid[0], bestDiff = 1.0e9f;
    for (float f : kMid) { const float d = std::abs(freqHz - f); if (d < bestDiff) { bestDiff = d; best = f; } }
    return best;
}

float SlotUnitDSP::quantizeNeve1073HpfFreq(float freqHz, bool stepped) noexcept
{
    if (!stepped) return freqHz;
    static constexpr float kHpf[5] = { 35.0f, 50.0f, 80.0f, 160.0f, 300.0f };
    float best = kHpf[0], bestDiff = 1.0e9f;
    for (float f : kHpf) { const float d = std::abs(freqHz - f); if (d < bestDiff) { bestDiff = d; best = f; } }
    return best;
}

float SlotUnitDSP::processSample(float& l, float& r,
                                 float scL, float scR,
                                 const SlotParamArray& params,
                                 bool extSidechain,
                                 float tolBiasL, float tolBiasR,
                                 float tolFreqL, float tolFreqR,
                                 bool autoGainOn) noexcept
{
    const float refL = l;
    const float refR = r;
    float outGr = 0.0f;

    if (fadeRemaining_ > 0)
    {
        float oldL = l, oldR = r;
        const float grOld = processModuleById(previousModuleId_, oldL, oldR, scL, scR, params, extSidechain,
                                              tolBiasL, tolBiasR, tolFreqL, tolFreqR);
        const float grNew = processModuleById(activeModuleId_, l, r, scL, scR, params, extSidechain,
                                              tolBiasL, tolBiasR, tolFreqL, tolFreqR);
        const float alpha = 1.0f - static_cast<float>(fadeRemaining_) / static_cast<float>(fadeTotal_);
        l = (1.0f - alpha) * oldL + alpha * l;
        r = (1.0f - alpha) * oldR + alpha * r;
        --fadeRemaining_;
        outGr = (1.0f - alpha) * grOld + alpha * grNew;
    }
    else
    {
        outGr = processModuleById(activeModuleId_, l, r, scL, scR, params, extSidechain,
                                  tolBiasL, tolBiasR, tolFreqL, tolFreqR);
    }

    const float agLin = slotAutoGain_.computeAutoGainSample(refL, refR, l, r, autoGainOn, lastAutoGainDb_);
    l *= agLin;
    r *= agLin;
    return outGr;
}

float SlotUnitDSP::processSample2U(float& l, float& r,
                                   const SlotParamArray2U& params2U,
                                   const std::array<uint32_t, 6>& bandMeta,
                                   bool autoGainOn) noexcept
{
    const float refL = l;
    const float refR = r;
    float outGr = 0.0f;
    if (activeModuleId_ == static_cast<int>(SlotModuleId::PrecisionDyn2U))
        outGr = precisionDyn2uDsp_.processSample(l, r, params2U);
    else
        outGr = surgicalEq2uDsp_.processSample(l, r, params2U, bandMeta);
    const float agLin = slotAutoGain_.computeAutoGainSample(refL, refR, l, r, autoGainOn, lastAutoGainDb_);
    l *= agLin;
    r *= agLin;
    return outGr;
}

float SlotUnitDSP::processModuleById(int modId,
                                     float& l, float& r,
                                     float scL, float scR,
                                     const SlotParamArray& p,
                                     bool extSidechain,
                                     float tolBiasL, float tolBiasR,
                                     float tolFreqL, float tolFreqR) noexcept
{
    const float detL = extSidechain ? scL : l;
    const float detR = extSidechain ? scR : r;

    switch (static_cast<SlotModuleId>(modId))
    {
        case SlotModuleId::SSL4000Preamp:      procSlotPreamp(l, r, p, (p[8] > 0.5f) ? 1 : 0, tolBiasL, tolBiasR); return 0.0f;
        case SlotModuleId::API2520Preamp:      procSlotPreamp(l, r, p, 2, tolBiasL, tolBiasR); return 0.0f;
        case SlotModuleId::Neve1073Preamp:     procSlotPreamp(l, r, p, 3, tolBiasL, tolBiasR); return 0.0f;
        case SlotModuleId::KalyxCleanPreamp:   procSlotPreamp(l, r, p, 4, tolBiasL, tolBiasR); return 0.0f;
        case SlotModuleId::SSL4000Comp:        return procSslComp(l, r, detL, detR, p);
        case SlotModuleId::API2500Comp:        return procApi2500Comp(l, r, detL, detR, p);
        case SlotModuleId::Neve5254Comp:       return procNeve5254Comp(l, r, detL, detR, p);
        case SlotModuleId::UA1176Comp:         return ua1176Dsp_.processSample(l, r, detL, detR, p, extSidechain, tolBiasL, tolBiasR);
        case SlotModuleId::LA2AComp:           return la2aDsp_.processSample(l, r, detL, detR, p, extSidechain, tolBiasL, tolBiasR);
        case SlotModuleId::DistressorComp:     return distressorDsp_.processSample(l, r, detL, detR, p, extSidechain, tolBiasL, tolBiasR);
        case SlotModuleId::FairchildComp:      return fairchildDsp_.processSample(l, r, detL, detR, p, extSidechain, tolBiasL, tolBiasR);
        case SlotModuleId::SSL4000EQ:          proc4BandEq(l, r, p, false, tolFreqL, tolFreqR); return 0.0f;
        case SlotModuleId::API550BEQ:          proc4BandEq(l, r, p, true,  tolFreqL, tolFreqR); return 0.0f;
        case SlotModuleId::Neve1073EQ:         procNeve1073Eq(l, r, p, tolFreqL, tolFreqR); return 0.0f;
        case SlotModuleId::ConsoleColor:       procConsoleColor(l, r, p, tolBiasL, tolBiasR); return 0.0f;
        case SlotModuleId::HarmonicSculptor:   harmonicSculptorDsp_.processSample(l, r, p); return 0.0f;
        case SlotModuleId::StereoPhase:        stereoPhaseDsp_.processSample(l, r, p); return 0.0f;
        case SlotModuleId::UtilityGateDeEsser: return procUtilityGateDeEsser(l, r, detL, detR, p);
        case SlotModuleId::PrecisionLimiter:   return procPrecisionLimiter(l, r, p);
        case SlotModuleId::SurgicalEQ2U:
        case SlotModuleId::PrecisionDyn2U:
        case SlotModuleId::EmptyBypass:
        case SlotModuleId::Count:
        default:                               return 0.0f;
    }
}

void SlotUnitDSP::procSlotPreamp(float& l, float& r, const SlotParamArray& p, int preampStyle, float tolBiasL, float tolBiasR) noexcept
{
    if (p[0] <= 0.5f) return;
    InputStripParameters ip;
    ip.preampModel = preampStyle;
    ip.trimDb      = mapLin(p[1], -24.0f, 24.0f);
    ip.drivePct    = mapLin(p[2], 0.0f, 100.0f);
    ip.phaseInvert = p[3] > 0.5f;
    ip.hpfEnabled  = p[4] > 0.5f;
    ip.hpfFreqHz   = mapLog(p[5], 20.0f, 500.0f);
    ip.lpfEnabled  = p[6] > 0.5f;
    ip.lpfFreqHz   = mapLog(p[7], 2000.0f, 20000.0f);
    float dummyScL = l, dummyScR = r;
    slotPreampDsp_.processSample(l, r, dummyScL, dummyScR, ip, tolBiasL, tolBiasR);
}

float SlotUnitDSP::procUtilityGateDeEsser(float& l, float& r, float detL, float detR, const SlotParamArray& p) noexcept
{
    float totalGr = 0.0f;
    const bool gateOn  = p[0] > 0.5f;
    const float gThr   = mapLin(p[1], -70.0f, 0.0f);
    const float gRange = mapLin(p[2], 0.0f, 40.0f);
    const float gHold  = mapLin(p[3], 5.0f, 300.0f);
    const float gRel   = mapLin(p[4], 20.0f, 800.0f);

    if (gateOn && gRange > 0.2f)
    {
        const double peak = std::max({ static_cast<double>(std::abs(detL)), static_cast<double>(std::abs(detR)), 1.0e-7 });
        const double db   = 20.0 * std::log10(peak);
        if (db >= static_cast<double>(gThr))
        {
            gateOpen_ = true;
            gateHoldCount_ = static_cast<int>(gHold * 0.001 * sampleRate_);
        }
        else if (db < static_cast<double>(gThr) - 2.5)
        {
            if (gateHoldCount_ > 0) --gateHoldCount_;
            else gateOpen_ = false;
        }
        const double minG   = std::pow(10.0, -static_cast<double>(gRange) / 20.0);
        const double target = gateOpen_ ? 1.0 : minG;
        const double coeff  = gateOpen_ ? std::exp(-1.0 / (0.0015 * sampleRate_))
                                        : std::exp(-1.0 / (static_cast<double>(gRel) * 0.001 * sampleRate_));
        gateEnv_ = coeff * gateEnv_ + (1.0 - coeff) * target;
        l *= static_cast<float>(gateEnv_);
        r *= static_cast<float>(gateEnv_);
        totalGr += static_cast<float>(-20.0 * std::log10(std::max(gateEnv_, 1.0e-4)));
    }
    else
    {
        gateEnv_ = 1.0;
    }

    const bool dsOn    = p[5] > 0.5f;
    const float dsFreq = mapLog(p[6], 3000.0f, 12000.0f);
    const float dsThr  = mapLin(p[7], -40.0f, 0.0f);
    const float dsRng  = mapLin(p[8], 0.0f, 18.0f);

    if (dsOn && dsRng > 0.2f)
    {
        deessScL_.setBell(sampleRate_, dsFreq, 2.2, 12.0);
        deessScR_.setBell(sampleRate_, dsFreq, 2.2, 12.0);
        const double dsPeak = std::max({ static_cast<double>(std::abs(deessScL_.processSample(detL))),
                                         static_cast<double>(std::abs(deessScR_.processSample(detR))), 1.0e-7 });
        const double dsDb   = 20.0 * std::log10(dsPeak);
        const double target = std::min(static_cast<double>(dsRng), std::max(0.0, dsDb - static_cast<double>(dsThr)) * 0.80);
        const double att    = std::exp(-1.0 / (0.0012 * sampleRate_));
        const double rel    = std::exp(-1.0 / (0.0450 * sampleRate_));
        deessEnvDb_ = (target > deessEnvDb_) ? (att * deessEnvDb_ + (1.0 - att) * target)
                                             : (rel * deessEnvDb_ + (1.0 - rel) * target);
        if (deessEnvDb_ > 0.05)
        {
            deessNotchL_.setBell(sampleRate_, dsFreq, 2.0, -deessEnvDb_);
            deessNotchR_.setBell(sampleRate_, dsFreq, 2.0, -deessEnvDb_);
            l = deessNotchL_.processSample(l);
            r = deessNotchR_.processSample(r);
            totalGr += static_cast<float>(deessEnvDb_);
        }
    }
    else
    {
        deessEnvDb_ = 0.0;
    }

    return totalGr;
}

float SlotUnitDSP::procSslComp(float& l, float& r, float detL, float detR, const SlotParamArray& p) noexcept
{
    if (p[0] <= 0.5f) { compEnvADb_ = compEnvBDb_ = 0.0; return 0.0f; }

    const float thrDb   = mapLin(p[1], -40.0f, 0.0f);
    const float ratio   = mapLog(p[2], 1.0f, 20.0f);
    const float attMs   = mapLog(p[3], 0.5f, 50.0f);
    const float relMs   = mapLog(p[4], 20.0f, 1000.0f);
    const bool  autoRel = p[5] > 0.5f;
    const float mkupDb  = mapLin(p[6], -12.0f, 24.0f);
    const float blend   = std::clamp(p[7], 0.0f, 1.0f);
    const int   routing = mapChoice(p[8], 5);
    const float scHpf   = mapLog(p[9], 20.0f, 300.0f);
    const bool  is4000G = p[10] > 0.5f;

    float dL = detL, dR = detR;
    if (scHpf > 21.0f)
    {
        scHpfL_.setHighPass(sampleRate_, scHpf, 0.7071);
        scHpfR_.setHighPass(sampleRate_, scHpf, 0.7071);
        dL = scHpfL_.processSample(dL);
        dR = scHpfR_.processSample(dR);
    }

    const auto det   = dsp::computeUniversalDetectorLevels(dL, dR, routing);
    const bool isMS  = (routing == static_cast<int>(dsp::DynamicsRoutingMode::MidSide));
    const bool isInv = (routing == static_cast<int>(dsp::DynamicsRoutingMode::InverseDucking));
    const double knee = is4000G ? 4.5 : 3.0;

    auto calcGr = [&](double db) noexcept -> double
    {
        const double over = db - static_cast<double>(thrDb);
        if (isInv) return (over > 0.0) ? std::min(30.0, over * 1.45) : 0.0;
        if (over <= -0.5 * knee) return 0.0;
        if (over >=  0.5 * knee) return over * (1.0 - 1.0 / static_cast<double>(ratio));
        const double x = over + 0.5 * knee;
        return ((1.0 - 1.0 / static_cast<double>(ratio)) * x * x) / (2.0 * knee);
    };

    const double tA = calcGr(det.detADb), tB = calcGr(det.detBDb), tMax = std::max(tA, tB);
    const double attC = std::exp(-1.0 / std::max(1.0, static_cast<double>(attMs) * 0.001 * sampleRate_));
    double relC = 0.999;
    if (autoRel)
    {
        const double cF = std::exp(-1.0 / (0.045 * sampleRate_));
        const double cS = std::exp(-1.0 / (0.650 * sampleRate_));
        compFastRc_ = std::max(tMax, cF * compFastRc_ + (1.0 - cF) * tMax);
        compSlowRc_ = std::max(tMax * 0.55, cS * compSlowRc_ + (1.0 - cS) * tMax * 0.55);
        relC = (compSlowRc_ > 0.62 * std::max(compFastRc_, 0.01)) ? cS : cF;
    }
    else
    {
        relC = std::exp(-1.0 / std::max(1.0, static_cast<double>(relMs) * 0.001 * sampleRate_));
    }

    compEnvADb_ = (tA > compEnvADb_) ? (attC * compEnvADb_ + (1.0 - attC) * tA) : (relC * compEnvADb_ + (1.0 - relC) * tA);
    compEnvBDb_ = (tB > compEnvBDb_) ? (attC * compEnvBDb_ + (1.0 - attC) * tB) : (relC * compEnvBDb_ + (1.0 - relC) * tB);
    if (compEnvADb_ < 1.0e-5) compEnvADb_ = 0.0;
    if (compEnvBDb_ < 1.0e-5) compEnvBDb_ = 0.0;

    const float dryL = l, dryR = r;
    const float gA = static_cast<float>(std::pow(10.0, -compEnvADb_ / 20.0));
    const float gB = static_cast<float>(std::pow(10.0, -compEnvBDb_ / 20.0));
    float wetL = dryL * gA, wetR = dryR * gB;
    if (isMS)
    {
        const float mid = 0.5f * (dryL + dryR) * gA, side = 0.5f * (dryL - dryR) * gB;
        wetL = mid + side; wetR = mid - side;
    }

    const float mkLin = std::pow(10.0f, mkupDb / 20.0f);
    l = ((1.0f - blend) * dryL + blend * wetL) * mkLin;
    r = ((1.0f - blend) * dryR + blend * wetR) * mkLin;
    return static_cast<float>(std::max(compEnvADb_, compEnvBDb_));
}

float SlotUnitDSP::procApi2500Comp(float& l, float& r, float detL, float detR, const SlotParamArray& p) noexcept
{
    if (p[0] <= 0.5f) { compEnvADb_ = compEnvBDb_ = 0.0; apiPrevGainL_ = apiPrevGainR_ = 1.0; return 0.0f; }

    const float thrDb      = mapLin(p[1], -40.0f, 0.0f);
    const float ratio      = mapLog(p[2], 1.5f, 20.0f);
    const float attMs      = mapLog(p[3], 0.3f, 30.0f);
    const bool  relFixed   = (p[13] <= 0.5f);    // 0=FIXED (crans), 1=VAR (continu)
    const float relMs      = quantizeApi2500RelMs(mapLog(p[4], 50.0f, 1500.0f), relFixed);
    const int   kneeMode   = mapChoice(p[5], 3); // 0=HARD, 1=MED, 2=SOFT
    const int   thrustMode = mapChoice(p[6], 3); // 0=LOUD, 1=MED, 2=NORM (ou ordre LED)
    const bool  oldFb      = p[7] > 0.5f;        // 0=NEW (FF), 1=OLD (FB)
    const float lrLink     = mapLin(p[8], 0.5f, 1.0f);
    const int   linkFilt   = mapChoice(p[9], 3); // 0=HP, 1=BP, 2=LP
    const float mkupDb     = mapLin(p[10], -12.0f, 24.0f);
    const float blend      = std::clamp(p[11], 0.0f, 1.0f);
    const int   routing    = mapChoice(p[12], 5);

    double dL = static_cast<double>(detL) * (oldFb ? apiPrevGainL_ : 1.0);
    double dR = static_cast<double>(detR) * (oldFb ? apiPrevGainR_ : 1.0);

    const double alphaHp = std::exp(-2.0 * 3.14159265358979323846 * 220.0 / sampleRate_);
    apiThrustLpL_ = alphaHp * apiThrustLpL_ + (1.0 - alphaHp) * dL;
    apiThrustLpR_ = alphaHp * apiThrustLpR_ + (1.0 - alphaHp) * dR;
    if (thrustMode == 0)      { dL -= 0.78 * apiThrustLpL_; dR -= 0.78 * apiThrustLpR_; } // LOUD
    else if (thrustMode == 1) { dL -= 0.42 * apiThrustLpL_; dR -= 0.42 * apiThrustLpR_; } // MED

    if (linkFilt == 0)      { dL -= 0.55 * apiThrustLpL_; dR -= 0.55 * apiThrustLpR_; } // HP
    else if (linkFilt == 2) { dL = 0.5 * dL + 0.5 * apiThrustLpL_; dR = 0.5 * dR + 0.5 * apiThrustLpR_; } // LP

    const auto univ = dsp::computeUniversalDetectorLevels(static_cast<float>(dL), static_cast<float>(dR), routing);
    const double linkedMax = std::max(univ.detADb, univ.detBDb);
    const double detADb = (1.0 - static_cast<double>(lrLink)) * univ.detADb + static_cast<double>(lrLink) * linkedMax;
    const double detBDb = (1.0 - static_cast<double>(lrLink)) * univ.detBDb + static_cast<double>(lrLink) * linkedMax;

    const double knee = (kneeMode == 0) ? 0.5 : (kneeMode == 1 ? 4.0 : 10.0);
    const bool isMS   = (routing == static_cast<int>(dsp::DynamicsRoutingMode::MidSide));
    const bool isInv  = (routing == static_cast<int>(dsp::DynamicsRoutingMode::InverseDucking));

    auto calcApiGr = [&](double db) noexcept -> double
    {
        const double over = db - static_cast<double>(thrDb);
        if (isInv) return (over > 0.0) ? std::min(30.0, over * 1.45) : 0.0;
        if (over <= -0.5 * knee) return 0.0;
        if (over >=  0.5 * knee) return over * (1.0 - 1.0 / static_cast<double>(ratio));
        const double x = over + 0.5 * knee;
        return ((1.0 - 1.0 / static_cast<double>(ratio)) * x * x) / (2.0 * knee);
    };

    const double tA = calcApiGr(detADb), tB = calcApiGr(detBDb);
    const double attC = std::exp(-1.0 / std::max(1.0, static_cast<double>(attMs) * 0.001 * sampleRate_));
    const double relC = std::exp(-1.0 / std::max(1.0, static_cast<double>(relMs) * 0.001 * sampleRate_));

    compEnvADb_ = (tA > compEnvADb_) ? (attC * compEnvADb_ + (1.0 - attC) * tA) : (relC * compEnvADb_ + (1.0 - relC) * tA);
    compEnvBDb_ = (tB > compEnvBDb_) ? (attC * compEnvBDb_ + (1.0 - attC) * tB) : (relC * compEnvBDb_ + (1.0 - relC) * tB);
    if (compEnvADb_ < 1.0e-5) compEnvADb_ = 0.0;
    if (compEnvBDb_ < 1.0e-5) compEnvBDb_ = 0.0;

    apiPrevGainL_ = std::pow(10.0, -compEnvADb_ / 20.0);
    apiPrevGainR_ = std::pow(10.0, -compEnvBDb_ / 20.0);

    const float dryL = l, dryR = r;
    float wetL = dryL * static_cast<float>(apiPrevGainL_);
    float wetR = dryR * static_cast<float>(apiPrevGainR_);
    if (isMS)
    {
        const float mid  = 0.5f * (dryL + dryR) * static_cast<float>(apiPrevGainL_);
        const float side = 0.5f * (dryL - dryR) * static_cast<float>(apiPrevGainR_);
        wetL = mid + side; wetR = mid - side;
    }

    const float mkLin = std::pow(10.0f, mkupDb / 20.0f);
    l = ((1.0f - blend) * dryL + blend * wetL) * mkLin;
    r = ((1.0f - blend) * dryR + blend * wetR) * mkLin;
    return static_cast<float>(std::max(compEnvADb_, compEnvBDb_));
}

float SlotUnitDSP::procNeve5254Comp(float& l, float& r, float detL, float detR, const SlotParamArray& p) noexcept
{
    if (p[0] <= 0.5f) { compEnvADb_ = compEnvBDb_ = 0.0; return 0.0f; }

    static constexpr double kNeveRatios[6] = { 1.5, 2.0, 3.0, 4.0, 6.0, 8.0 };
    static constexpr double kAttMs[6]      = { 1.5,  4.0,   8.0,  14.0,  22.0,   6.0 };
    static constexpr double kRelMs[6]      = { 45.0, 90.0, 180.0, 350.0, 750.0, 220.0 };

    const bool  isVintage = (p[10] > 0.5f);
    float thrDb = mapLin(p[1], -40.0f, 0.0f);
    if (isVintage)
        thrDb = std::round(thrDb * 0.5f) * 2.0f;
    double ratio = 3.0;
    double attMs = 8.0;
    double relMs = 180.0;
    int    timing = 2;
    if (isVintage)
    {
        ratio  = kNeveRatios[mapChoice(p[2], 6)];
        timing = mapChoice(p[3], 6);
        attMs  = kAttMs[timing];
        relMs  = kRelMs[timing];
    }
    else
    {
        ratio = static_cast<double>(mapLog(p[2], 1.5f, 8.0f));
        const double pos = std::clamp(static_cast<double>(p[3]), 0.0, 1.0) * 5.0;
        const int i0 = std::clamp(static_cast<int>(std::floor(pos)), 0, 4);
        const double f = pos - static_cast<double>(i0);
        attMs = kAttMs[i0] + f * (kAttMs[i0 + 1] - kAttMs[i0]);
        relMs = kRelMs[i0] + f * (kRelMs[i0 + 1] - kRelMs[i0]);
        timing = (pos >= 4.5) ? 5 : i0;
    }
    const bool  fastBtn  = p[4] > 0.5f;
    const float scHpf    = mapLog(p[5], 20.0f, 250.0f);
    float gainDb         = mapLin(p[6], -12.0f, 24.0f);
    if (isVintage)
        gainDb = std::round(gainDb * 0.5f) * 2.0f;
    const float blend    = std::clamp(p[7], 0.0f, 1.0f);
    const int   routing  = mapChoice(p[8], 5);
    const bool  linkOn   = p[9] > 0.5f;

    const double speedMul = fastBtn ? 0.70 : 1.0;

    float dL = detL, dR = detR;
    if (scHpf > 21.0f)
    {
        scHpfL_.setHighPass(sampleRate_, scHpf, 0.7071);
        scHpfR_.setHighPass(sampleRate_, scHpf, 0.7071);
        dL = scHpfL_.processSample(dL);
        dR = scHpfR_.processSample(dR);
    }

    const auto det   = dsp::computeUniversalDetectorLevels(dL, dR, routing);
    const bool isMS  = (routing == static_cast<int>(dsp::DynamicsRoutingMode::MidSide));
    const bool isInv = (routing == static_cast<int>(dsp::DynamicsRoutingMode::InverseDucking));
    constexpr double diodeKnee = 9.5;

    const double detA = (linkOn && !isMS) ? std::max(det.detADb, det.detBDb) : det.detADb;
    const double detB = (linkOn && !isMS) ? std::max(det.detADb, det.detBDb) : det.detBDb;

    auto calcDiodeGr = [&](double db) noexcept -> double
    {
        const double over = db - static_cast<double>(thrDb);
        if (isInv) return (over > 0.0) ? std::min(30.0, over * 1.45) : 0.0;
        if (over <= -0.5 * diodeKnee) return 0.0;
        if (over >=  0.5 * diodeKnee) return over * (1.0 - 1.0 / ratio);
        const double x = over + 0.5 * diodeKnee;
        return ((1.0 - 1.0 / ratio) * x * x) / (2.0 * diodeKnee);
    };

    const double tA = calcDiodeGr(detA), tB = calcDiodeGr(detB), tMax = std::max(tA, tB);
    const double attC = std::exp(-1.0 / std::max(1.0, attMs * speedMul * 0.001 * sampleRate_));
    double relC = 0.999;
    if (timing == 5)
    {
        const double cF = std::exp(-1.0 / (0.055 * speedMul * sampleRate_));
        const double cS = std::exp(-1.0 / (0.800 * speedMul * sampleRate_));
        compFastRc_ = std::max(tMax, cF * compFastRc_ + (1.0 - cF) * tMax);
        compSlowRc_ = std::max(tMax * 0.55, cS * compSlowRc_ + (1.0 - cS) * tMax * 0.55);
        relC = (compSlowRc_ > 0.58 * std::max(compFastRc_, 0.01)) ? cS : cF;
    }
    else
    {
        relC = std::exp(-1.0 / std::max(1.0, relMs * speedMul * 0.001 * sampleRate_));
    }

    compEnvADb_ = (tA > compEnvADb_) ? (attC * compEnvADb_ + (1.0 - attC) * tA) : (relC * compEnvADb_ + (1.0 - relC) * tA);
    compEnvBDb_ = (tB > compEnvBDb_) ? (attC * compEnvBDb_ + (1.0 - attC) * tB) : (relC * compEnvBDb_ + (1.0 - relC) * tB);
    if (compEnvADb_ < 1.0e-5) compEnvADb_ = 0.0;
    if (compEnvBDb_ < 1.0e-5) compEnvBDb_ = 0.0;

    const float dryL = l, dryR = r;
    const float gA = static_cast<float>(std::pow(10.0, -compEnvADb_ / 20.0));
    const float gB = static_cast<float>(std::pow(10.0, -compEnvBDb_ / 20.0));
    float wetL = dryL * gA, wetR = dryR * gB;
    if (isMS)
    {
        const float mid = 0.5f * (dryL + dryR) * gA, side = 0.5f * (dryL - dryR) * gB;
        wetL = mid + side; wetR = mid - side;
    }

    const float grMax = static_cast<float>(std::max(compEnvADb_, compEnvBDb_));
    if (grMax > 0.25f)
    {
        const float diodeDrive = std::clamp(0.35f + grMax * 0.08f, 0.35f, 1.8f);
        wetL = 0.82f * wetL + 0.18f * diodeSatL_.processADAA1(wetL, diodeDrive, 0.06f);
        wetR = 0.82f * wetR + 0.18f * diodeSatR_.processADAA1(wetR, diodeDrive, 0.06f);
    }

    const float mkLin = std::pow(10.0f, gainDb / 20.0f);
    l = ((1.0f - blend) * dryL + blend * wetL) * mkLin;
    r = ((1.0f - blend) * dryR + blend * wetR) * mkLin;
    return grMax;
}

void SlotUnitDSP::proc4BandEq(float& l, float& r, const SlotParamArray& p, bool isApi550B, float tolFreqL, float tolFreqR) noexcept
{
    if (p[0] <= 0.5f) return;

    const bool is4000G = (!isApi550B) && (p[14] > 0.5f);
    const bool stepped = isApi550B ? (p[14] > 0.5f) : (p[15] > 0.5f);

    const float lfFreq  = quantizeApi550BFreq(0, mapLog(p[1], 30.0f, 450.0f), stepped);
    const float lfGain  = quantizeApi550BGain(mapLin(p[2], -15.0f, 15.0f), stepped);
    const float lmfFreq = quantizeApi550BFreq(1, mapLog(p[3], 200.0f, 2500.0f), stepped);
    const float lmfGain = quantizeApi550BGain(mapLin(p[4], -15.0f, 15.0f), stepped);
    const float lmfQ    = mapLog(p[5], 0.40f, 4.00f);
    const float hmfFreq = quantizeApi550BFreq(2, mapLog(p[6], 600.0f, 7000.0f), stepped);
    const float hmfGain = quantizeApi550BGain(mapLin(p[7], -15.0f, 15.0f), stepped);
    const float hmfQ    = mapLog(p[8], 0.40f, 4.00f);
    const float hfFreq  = quantizeApi550BFreq(3, mapLog(p[9], 1500.0f, 18000.0f), stepped);
    const float hfGain  = quantizeApi550BGain(mapLin(p[10], -15.0f, 15.0f), stepped);
    const float trimDb  = mapLin(p[11], -12.0f, 12.0f);
    const bool  lfBell  = p[12] > 0.5f;
    const bool  hfBell  = p[13] > 0.5f;

    if (std::abs(lfGain) > 0.02f)
    {
        const float q = isApi550B ? computeApi550BQ(1.0f, lfGain) : (is4000G ? 1.15f : 0.90f);
        if (lfBell) { lfL_.setBell(sampleRate_, lfFreq * tolFreqL, q, lfGain); lfR_.setBell(sampleRate_, lfFreq * tolFreqR, q, lfGain); }
        else        { lfL_.setLowShelf(sampleRate_, lfFreq * tolFreqL, is4000G ? 0.82 : 0.70, lfGain); lfR_.setLowShelf(sampleRate_, lfFreq * tolFreqR, is4000G ? 0.82 : 0.70, lfGain); }
        l = lfL_.processSample(l); r = lfR_.processSample(r);
    }

    if (std::abs(lmfGain) > 0.02f)
    {
        const float effQ = isApi550B ? computeApi550BQ(lmfQ, lmfGain) : computeSslEqQ(lmfQ, lmfGain, is4000G);
        lmfL_.setBell(sampleRate_, lmfFreq * tolFreqL, effQ, lmfGain);
        lmfR_.setBell(sampleRate_, lmfFreq * tolFreqR, effQ, lmfGain);
        l = lmfL_.processSample(l); r = lmfR_.processSample(r);
        if (is4000G)
        {
            const float dipQ = std::max(0.35f, effQ * 0.42f);
            lmfDipL_.setBell(sampleRate_, lmfFreq * tolFreqL, dipQ, -0.12f * lmfGain);
            lmfDipR_.setBell(sampleRate_, lmfFreq * tolFreqR, dipQ, -0.12f * lmfGain);
            l = lmfDipL_.processSample(l); r = lmfDipR_.processSample(r);
        }
    }

    // API 550B historique : 3 bandes (LF, MF, HF) — la 4e bande (HMF) est réservée au SSL 4000 EQ
    if (!isApi550B && std::abs(hmfGain) > 0.02f)
    {
        const float effQ = computeSslEqQ(hmfQ, hmfGain, is4000G);
        hmfL_.setBell(sampleRate_, hmfFreq * tolFreqL, effQ, hmfGain);
        hmfR_.setBell(sampleRate_, hmfFreq * tolFreqR, effQ, hmfGain);
        l = hmfL_.processSample(l); r = hmfR_.processSample(r);
        if (is4000G)
        {
            const float dipQ = std::max(0.35f, effQ * 0.42f);
            hmfDipL_.setBell(sampleRate_, hmfFreq * tolFreqL, dipQ, -0.12f * hmfGain);
            hmfDipR_.setBell(sampleRate_, hmfFreq * tolFreqR, dipQ, -0.12f * hmfGain);
            l = hmfDipL_.processSample(l); r = hmfDipR_.processSample(r);
        }
    }

    if (std::abs(hfGain) > 0.02f)
    {
        const float q = isApi550B ? computeApi550BQ(1.0f, hfGain) : (is4000G ? 1.15f : 0.90f);
        if (hfBell) { hfL_.setBell(sampleRate_, hfFreq * tolFreqL, q, hfGain); hfR_.setBell(sampleRate_, hfFreq * tolFreqR, q, hfGain); }
        else        { hfL_.setHighShelf(sampleRate_, hfFreq * tolFreqL, is4000G ? 0.82 : 0.70, hfGain); hfR_.setHighShelf(sampleRate_, hfFreq * tolFreqR, is4000G ? 0.82 : 0.70, hfGain); }
        l = hfL_.processSample(l); r = hfR_.processSample(r);
    }

    const float trimG = std::pow(10.0f, trimDb / 20.0f);
    l *= trimG; r *= trimG;
}

void SlotUnitDSP::procNeve1073Eq(float& l, float& r, const SlotParamArray& p, float tolFreqL, float tolFreqR) noexcept
{
    if (p[0] <= 0.5f) return;

    const bool  stepped = (p[10] > 0.5f);
    const float lfFreq  = quantizeNeve1073LowFreq(mapLog(p[1], 35.0f, 220.0f), stepped);
    const float lfGain  = mapLin(p[2], -15.0f, 15.0f);
    const float midFreq = quantizeNeve1073MidFreq(mapLog(p[3], 360.0f, 7200.0f), stepped);
    const float midGain = mapLin(p[4], -15.0f, 15.0f);
    const float midQ    = mapLog(p[5], 0.40f, 3.50f);
    const float hf12kG  = mapLin(p[6], -15.0f, 15.0f);
    const bool  hpfOn   = p[7] > 0.5f;
    const float hpfFreq = quantizeNeve1073HpfFreq(mapLog(p[8], 35.0f, 360.0f), stepped);
    const float trimDb  = mapLin(p[9], -12.0f, 12.0f);

    if (hpfOn)
    {
        eqHpfL_.setHighPass(sampleRate_, hpfFreq, 0.78);
        eqHpfR_.setHighPass(sampleRate_, hpfFreq, 0.78);
        l = eqHpfL_.processSample(l); r = eqHpfR_.processSample(r);
    }
    if (std::abs(lfGain) > 0.02f)
    {
        lfL_.setLowShelf(sampleRate_, lfFreq * tolFreqL, 0.65, lfGain);
        lfR_.setLowShelf(sampleRate_, lfFreq * tolFreqR, 0.65, lfGain);
        l = lfL_.processSample(l); r = lfR_.processSample(r);
    }
    if (std::abs(midGain) > 0.02f)
    {
        const float effQ = std::clamp(midQ * 0.72f * (1.0f + 0.032f * std::abs(midGain)), 0.35f, 3.8f);
        lmfL_.setBell(sampleRate_, midFreq * tolFreqL, effQ, midGain);
        lmfR_.setBell(sampleRate_, midFreq * tolFreqR, effQ, midGain);
        l = lmfL_.processSample(l); r = lmfR_.processSample(r);
    }
    if (std::abs(hf12kG) > 0.02f)
    {
        hfL_.setHighShelf(sampleRate_, 12000.0 * tolFreqL, 0.62, hf12kG);
        hfR_.setHighShelf(sampleRate_, 12000.0 * tolFreqR, 0.62, hf12kG);
        l = hfL_.processSample(l); r = hfR_.processSample(r);
    }

    const float trimG = std::pow(10.0f, trimDb / 20.0f);
    l *= trimG; r *= trimG;
}

void SlotUnitDSP::procConsoleColor(float& l, float& r, const SlotParamArray& p, float tolBiasL, float tolBiasR) noexcept
{
    if (p[0] <= 0.5f) return;

    const int   model  = mapChoice(p[1], 5); // 0=SSL E, 1=SSL G, 2=API 2503, 3=Neve Marinair/Silk, 4=Kalyx Harmonic
    const float drive  = std::clamp(p[2], 0.0f, 1.0f);
    const float tone   = mapLin(p[3], -1.0f, 1.0f);
    const float xtalk  = std::clamp(p[4], 0.0f, 1.0f);
    const float trimDb = mapLin(p[5], -12.0f, 12.0f);

    if (drive > 1.0e-4f)
    {
        static constexpr float kDrvScale[5] = { 3.4f, 2.5f, 3.7f, 3.8f, 3.1f };
        static constexpr float kBias[5]     = { 0.032f, 0.008f, 0.140f, 0.090f, 0.065f };
        const float effDrv = 0.55f + drive * kDrvScale[model];
        const float wetMix = std::clamp(drive * 0.88f, 0.0f, 1.0f);

        const float peakSig = std::max(std::abs(l), std::abs(r));
        const float drvAttack = peakSig * (0.35f + effDrv * 0.65f);
        lastDriveLevelDb_ = (drvAttack > 1.0e-6f) ? (20.0f * std::log10(drvAttack)) : -100.0f;

        l = (1.0f - wetMix) * l + wetMix * colSatL_.processADAA2(l, effDrv, kBias[model] + tolBiasL);
        r = (1.0f - wetMix) * r + wetMix * colSatR_.processADAA2(r, effDrv, kBias[model] + tolBiasR);

        l = colHystL_.processSample(l, drive, sampleRate_, std::min(model, 3));
        r = colHystR_.processSample(r, drive, sampleRate_, std::min(model, 3));

        if (model == 2) // API 2503 Mid-Punch
        {
            colSpecL_.setBell(sampleRate_, 850.0, 0.85, 1.6 * drive);
            colSpecR_.setBell(sampleRate_, 850.0, 0.85, 1.6 * drive);
            l = colSpecL_.processSample(l); r = colSpecR_.processSample(r);
        }
        else if (model == 3 || model == 4) // Neve Silk / Kalyx Air Harmonic
        {
            colSpecL_.setHighShelf(sampleRate_, 11500.0, 0.65, 1.6 * drive);
            colSpecR_.setHighShelf(sampleRate_, 11500.0, 0.65, 1.6 * drive);
            l = colSpecL_.processSample(l); r = colSpecR_.processSample(r);
        }

        const float tiltDb = tone * 3.5f * std::min(drive * 1.4f, 1.0f);
        if (std::abs(tiltDb) > 0.05f)
        {
            colToneLowL_.setLowShelf(sampleRate_, 220.0, 0.7071, -tiltDb);
            colToneLowR_.setLowShelf(sampleRate_, 220.0, 0.7071, -tiltDb);
            colToneHighL_.setHighShelf(sampleRate_, 4200.0, 0.7071, tiltDb);
            colToneHighR_.setHighShelf(sampleRate_, 4200.0, 0.7071, tiltDb);
            l = colToneHighL_.processSample(colToneLowL_.processSample(l));
            r = colToneHighR_.processSample(colToneLowR_.processSample(r));
        }
    }
    else
    {
        lastDriveLevelDb_ = -100.0f;
        (void)colSatL_.processADAA2(0.0f, 0.0f, 0.0f);
        (void)colSatR_.processADAA2(0.0f, 0.0f, 0.0f);
        (void)colHystL_.processSample(0.0f, 0.0f, sampleRate_, 0);
        (void)colHystR_.processSample(0.0f, 0.0f, sampleRate_, 0);
    }

    if (xtalk > 0.005f)
    {
        xtalkHpfL_.setHighPass(sampleRate_, 170.0, 0.7071);
        xtalkHpfR_.setHighPass(sampleRate_, 170.0, 0.7071);
        const float bG = xtalk * 0.018f;
        const float bL = xtalkHpfL_.processSample(l) * bG;
        const float bR = xtalkHpfR_.processSample(r) * bG;
        l += bR; r += bL;
    }

    const float trimG = std::pow(10.0f, trimDb / 20.0f);
    l *= trimG; r *= trimG;
}

float SlotUnitDSP::procPrecisionLimiter(float& l, float& r, const SlotParamArray& p) noexcept
{
    if (p[0] <= 0.5f) { limEnvDb_ = limFastRc_ = limSlowRc_ = 0.0; return 0.0f; }

    const float thrDb     = mapLin(p[1], -30.0f, 0.0f);
    const float relMs     = mapLog(p[2], 10.0f, 500.0f);
    const bool  autoRel   = p[3] > 0.5f;
    const float ceilDb    = mapLin(p[4], -24.0f, 0.0f);
    const float blend     = std::clamp(p[5], 0.0f, 1.0f);

    const double peak = std::max({ static_cast<double>(std::abs(l)), static_cast<double>(std::abs(r)), 1.0e-7 });
    const double db   = 20.0 * std::log10(peak);
    const double over = std::max(0.0, db - static_cast<double>(thrDb));

    const double attC = std::exp(-1.0 / std::max(1.0, 0.0002 * sampleRate_)); // < 0.25 ms zero-latency attack
    double relC = 0.999;
    if (autoRel)
    {
        const double cF = std::exp(-1.0 / (0.025 * sampleRate_));
        const double cS = std::exp(-1.0 / (0.350 * sampleRate_));
        limFastRc_ = std::max(over, cF * limFastRc_ + (1.0 - cF) * over);
        limSlowRc_ = std::max(over * 0.55, cS * limSlowRc_ + (1.0 - cS) * over * 0.55);
        relC = (limSlowRc_ > 0.60 * std::max(limFastRc_, 0.01)) ? cS : cF;
    }
    else
    {
        relC = std::exp(-1.0 / std::max(1.0, static_cast<double>(relMs) * 0.001 * sampleRate_));
    }

    limEnvDb_ = (over > limEnvDb_) ? (attC * limEnvDb_ + (1.0 - attC) * over) : (relC * limEnvDb_ + (1.0 - relC) * over);
    if (limEnvDb_ < 1.0e-5) limEnvDb_ = 0.0;

    const float g       = static_cast<float>(std::pow(10.0, -limEnvDb_ / 20.0));
    const float ceilLin = std::pow(10.0f, ceilDb / 20.0f);
    const float wetL    = std::clamp(l * g, -ceilLin, ceilLin);
    const float wetR    = std::clamp(r * g, -ceilLin, ceilLin);

    l = (1.0f - blend) * l + blend * wetL;
    r = (1.0f - blend) * r + blend * wetR;
    return static_cast<float>(limEnvDb_);
}

} // namespace kalyx::core
