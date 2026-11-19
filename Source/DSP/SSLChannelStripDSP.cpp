#include "SSLChannelStripDSP.h"

namespace kalyx::dsp
{

void SSLChannelStripDSP::prepare(double sampleRate, int maxBlockSize) noexcept
{
    sampleRate_ = std::max(sampleRate, 8000.0);
    maxBlockSize_ = std::max(maxBlockSize, 1);

    inputDcBlockerFilter_.setCoefficients(BiquadFilter::makeHighPass(sampleRate_, 5.0f, 0.70710678f));
    dcBlocker5HzFilter_.setCoefficients(BiquadFilter::makeHighPass(sampleRate_, 5.0f, 0.70710678f));

    colorWarmthFilter_.setCoefficients(BiquadFilter::makeHighShelf(sampleRate_, 4500.0f, 1.2f, 0.70710678f));
    satTonePreFilter_.setCoefficients(BiquadFilter::makeHighShelf(sampleRate_, 1200.0f, 0.0f, 0.70710678f));
    satTonePostFilter_.setCoefficients(BiquadFilter::makeLowShelf(sampleRate_, 350.0f, 0.0f, 0.70710678f));
    crosstalkBandFilter_.setCoefficients(BiquadFilter::makeHighPass(sampleRate_, 180.0f, 0.70710678f));

    const auto kwStage1 = BiquadFilter::makeHighShelf(sampleRate_, 1681.97f, 4.0f, 0.70710678f);
    const auto kwStage2 = BiquadFilter::makeHighPass(sampleRate_, 38.135f, 0.5003f);
    kWeightInStage1_.setCoefficients(kwStage1);
    kWeightInStage2_.setCoefficients(kwStage2);
    kWeightOutStage1_.setCoefficients(kwStage1);
    kWeightOutStage2_.setCoefficients(kwStage2);

    gateAttackCoeff_  = std::exp(-1.0f / static_cast<float>(0.0015 * sampleRate_));
    gateReleaseCoeff_ = std::exp(-1.0f / static_cast<float>(0.0900 * sampleRate_));

    deessAttackCoeff_  = std::exp(-1.0f / static_cast<float>(0.0010 * sampleRate_));
    deessReleaseCoeff_ = std::exp(-1.0f / static_cast<float>(0.0350 * sampleRate_));

    compAutoFastRelCoeff_ = std::exp(-1.0f / static_cast<float>(0.0450 * sampleRate_));
    compAutoSlowAttCoeff_ = std::exp(-1.0f / static_cast<float>(0.1200 * sampleRate_));
    compAutoSlowRelCoeff_ = std::exp(-1.0f / static_cast<float>(0.7500 * sampleRate_));

    vuBallisticCoeff_ = std::exp(-1.0f / static_cast<float>(0.300 * sampleRate_));
    lufs400msCoeff_   = std::exp(-1.0f / static_cast<float>(0.400 * sampleRate_));

    reset();
}

void SSLChannelStripDSP::reset() noexcept
{
    preampAdaaSaturator_.reset(0.0);
    colorAdaaSaturator_.reset(0.0);
    inputDcBlockerFilter_.reset();
    dcBlocker5HzFilter_.reset();

    hpfZdfFilter_.reset();
    lpfZdfFilter_.reset();
    scHpfZdfFilter_.reset();
    scLpfZdfFilter_.reset();
    dedicatedScHpfFilter_.reset();

    deessSidechainFilter_.reset();
    deessDynamicZdfFilter_.reset();

    lfZdfFilter_.reset();
    lmfZdfFilter_.reset();
    hmfZdfFilter_.reset();
    hfZdfFilter_.reset();
    lmfGyratorDipFilter_.reset();
    hmfGyratorDipFilter_.reset();

    colorWarmthFilter_.reset();
    satTonePreFilter_.reset();
    satTonePostFilter_.reset();
    crosstalkBandFilter_.reset();

    transformerFluxState_ = { 0.0, 0.0 };
    transformerFluxPrev_  = { 0.0, 0.0 };

    kWeightInStage1_.reset();
    kWeightInStage2_.reset();
    kWeightOutStage1_.reset();
    kWeightOutStage2_.reset();

    compEnvelopeLin_    = 0.0f;
    compAutoFastCapLin_ = 0.0f;
    compAutoSlowCapLin_ = 0.0f;
    gateEnvelopeLin_    = 0.0f;
    deessEnvelopeLin_   = 0.0f;
    smoothedCompGrDb_   = 0.0f;
    lastDeessCutDb_     = 0.0f;
    lastDeessFreqHz_    = 6500.0f;

    autoGainRefMeanSq_  = 0.0f;
    autoGainProcMeanSq_ = 0.0f;
    smoothedAutoGainDb_ = 0.0f;

    vuInMeanSqL_    = 0.0f;
    vuInMeanSqR_    = 0.0f;
    vuOutMeanSqL_   = 0.0f;
    vuOutMeanSqR_   = 0.0f;
    lufsInMeanSqL_  = 0.0f;
    lufsInMeanSqR_  = 0.0f;
    lufsOutMeanSqL_ = 0.0f;
    lufsOutMeanSqR_ = 0.0f;
}

float SSLChannelStripDSP::computeEffectiveEqQ(float nominalQ, float gainDb, bool circuit4000G) noexcept
{
    if (!circuit4000G)
    {
        const float propFactor = 0.90f + 0.30f * (std::abs(gainDb) / 15.0f);
        return std::clamp(nominalQ * propFactor, 0.35f, 5.0f);
    }

    const float propFactor = 0.60f + 0.70f * (std::abs(gainDb) / 15.0f);
    return std::clamp(nominalQ * propFactor, 0.35f, 5.5f);
}

void SSLChannelStripDSP::updateFiltersAndCoefficients(const SSLParameters& params) noexcept
{
    const float tol = std::clamp(params.analogTolerance, 0.0f, 1.0f);
    const float fTolL = 1.0f - 0.012f * tol;
    const float fTolR = 1.0f + 0.015f * tol;
    const float gTolL = -0.18f * tol;
    const float gTolR = +0.20f * tol;

    hpfZdfFilter_.setCoefficients(
        ZDFStateVariableFilter::makeHighPass(sampleRate_, params.hpfFreqHz * fTolL, params.hpfQ),
        ZDFStateVariableFilter::makeHighPass(sampleRate_, params.hpfFreqHz * fTolR, params.hpfQ));

    lpfZdfFilter_.setCoefficients(
        ZDFStateVariableFilter::makeLowPass(sampleRate_, params.lpfFreqHz * fTolR, params.lpfQ),
        ZDFStateVariableFilter::makeLowPass(sampleRate_, params.lpfFreqHz * fTolL, params.lpfQ));

    scHpfZdfFilter_.setCoefficients(ZDFStateVariableFilter::makeHighPass(sampleRate_, params.hpfFreqHz, params.hpfQ));
    scLpfZdfFilter_.setCoefficients(ZDFStateVariableFilter::makeLowPass(sampleRate_, params.lpfFreqHz, params.lpfQ));
    dedicatedScHpfFilter_.setCoefficients(ZDFStateVariableFilter::makeHighPass(sampleRate_, params.scHpfFreqHz, 0.70710678f));

    deessSidechainFilter_.setCoefficients(BiquadFilter::makeBandPass(sampleRate_, params.deessFreqHz, 2.0f));
    deessDynamicZdfFilter_.setCoefficients(ZDFStateVariableFilter::makeBell(sampleRate_, params.deessFreqHz, -lastDeessCutDb_, 2.0f));
    lastDeessFreqHz_ = params.deessFreqHz;

    const double fcFlux = params.circuit4000G ? 110.0 : 165.0;
    transformerFluxAlpha_ = std::exp(-2.0 * 3.14159265358979323846 * fcFlux / sampleRate_);

    if (params.circuit4000G)
        colorWarmthFilter_.setCoefficients(BiquadFilter::makeHighShelf(sampleRate_, 4800.0f, 1.1f * params.colorAmount, 0.70710678f));
    else
        colorWarmthFilter_.setCoefficients(BiquadFilter::makePeakEQ(sampleRate_, 220.0f, 1.6f * params.colorAmount, 0.85f));

    const float toneVal = std::clamp(params.satTone, -1.0f, 1.0f);
    satTonePreFilter_.setCoefficients(BiquadFilter::makeHighShelf(sampleRate_, 1250.0f, 4.5f * toneVal, 0.70710678f));
    satTonePostFilter_.setCoefficients(BiquadFilter::makeLowShelf(sampleRate_, 320.0f, -3.5f * toneVal, 0.70710678f));

    const float bellRefQ = params.circuit4000G ? 1.15f : 0.90f;
    const float shelfQ   = params.circuit4000G ? 0.82f : 0.68f;

    const float lfG_L = (std::abs(params.lfGainDb) > 0.05f) ? (params.lfGainDb + gTolL) : 0.0f;
    const float lfG_R = (std::abs(params.lfGainDb) > 0.05f) ? (params.lfGainDb + gTolR) : 0.0f;
    if (params.lfBell)
    {
        lfZdfFilter_.setCoefficients(
            ZDFStateVariableFilter::makeBell(sampleRate_, params.lfFreqHz * fTolL, lfG_L, bellRefQ),
            ZDFStateVariableFilter::makeBell(sampleRate_, params.lfFreqHz * fTolR, lfG_R, bellRefQ));
    }
    else
    {
        lfZdfFilter_.setCoefficients(
            ZDFStateVariableFilter::makeLowShelf(sampleRate_, params.lfFreqHz * fTolL, lfG_L, shelfQ),
            ZDFStateVariableFilter::makeLowShelf(sampleRate_, params.lfFreqHz * fTolR, lfG_R, shelfQ));
    }

    const float lmfG_L  = (std::abs(params.lmfGainDb) > 0.05f) ? (params.lmfGainDb - gTolL) : 0.0f;
    const float lmfG_R  = (std::abs(params.lmfGainDb) > 0.05f) ? (params.lmfGainDb - gTolR) : 0.0f;
    const float effLmfQ = computeEffectiveEqQ(params.lmfQ, params.lmfGainDb, params.circuit4000G);
    lmfZdfFilter_.setCoefficients(
        ZDFStateVariableFilter::makeBell(sampleRate_, params.lmfFreqHz * fTolR, lmfG_L, effLmfQ),
        ZDFStateVariableFilter::makeBell(sampleRate_, params.lmfFreqHz * fTolL, lmfG_R, effLmfQ));

    const float hmfG_L  = (std::abs(params.hmfGainDb) > 0.05f) ? (params.hmfGainDb + gTolL) : 0.0f;
    const float hmfG_R  = (std::abs(params.hmfGainDb) > 0.05f) ? (params.hmfGainDb + gTolR) : 0.0f;
    const float effHmfQ = computeEffectiveEqQ(params.hmfQ, params.hmfGainDb, params.circuit4000G);
    hmfZdfFilter_.setCoefficients(
        ZDFStateVariableFilter::makeBell(sampleRate_, params.hmfFreqHz * fTolL, hmfG_L, effHmfQ),
        ZDFStateVariableFilter::makeBell(sampleRate_, params.hmfFreqHz * fTolR, hmfG_R, effHmfQ));

    if (params.circuit4000G)
    {
        lmfGyratorDipFilter_.setCoefficients(
            ZDFStateVariableFilter::makeBell(sampleRate_, params.lmfFreqHz, -0.12f * params.lmfGainDb, std::max(0.35f, effLmfQ * 0.42f)));
        hmfGyratorDipFilter_.setCoefficients(
            ZDFStateVariableFilter::makeBell(sampleRate_, params.hmfFreqHz, -0.12f * params.hmfGainDb, std::max(0.35f, effHmfQ * 0.42f)));
    }

    const float hfG_L = (std::abs(params.hfGainDb) > 0.05f) ? (params.hfGainDb - gTolL) : 0.0f;
    const float hfG_R = (std::abs(params.hfGainDb) > 0.05f) ? (params.hfGainDb - gTolR) : 0.0f;
    if (params.hfBell)
    {
        hfZdfFilter_.setCoefficients(
            ZDFStateVariableFilter::makeBell(sampleRate_, params.hfFreqHz * fTolR, hfG_L, bellRefQ),
            ZDFStateVariableFilter::makeBell(sampleRate_, params.hfFreqHz * fTolL, hfG_R, bellRefQ));
    }
    else
    {
        hfZdfFilter_.setCoefficients(
            ZDFStateVariableFilter::makeHighShelf(sampleRate_, params.hfFreqHz * fTolR, hfG_L, shelfQ),
            ZDFStateVariableFilter::makeHighShelf(sampleRate_, params.hfFreqHz * fTolL, hfG_R, shelfQ));
    }

    const float attSec = std::clamp(params.compAttackMs * 0.001f, 0.0002f, 0.250f);
    const float relSec = std::clamp(params.compReleaseMs * 0.001f, 0.005f, 2.500f);
    const float effectiveAttSec = params.limiterMode ? std::min(attSec, 0.001f) : attSec;

    compAttackCoeff_  = std::exp(-1.0f / (effectiveAttSec * static_cast<float>(sampleRate_)));
    compReleaseCoeff_ = std::exp(-1.0f / (relSec * static_cast<float>(sampleRate_)));
}

float SSLChannelStripDSP::computeCompGainReductionDb(float levelDb, float threshDb, float ratio, bool limiterMode) noexcept
{
    const float effRatio = limiterMode ? 20.0f : std::max(ratio, 1.0f);
    const float slope = 1.0f - (1.0f / effRatio);
    const float kneeDb = limiterMode ? 1.5f : 3.0f;
    const float overshoot = levelDb - threshDb;

    float grDb = 0.0f;
    if (kneeDb > 0.0f && std::abs(overshoot) < (kneeDb * 0.5f))
    {
        const float x = overshoot + kneeDb * 0.5f;
        grDb = slope * (x * x) / (2.0f * kneeDb);
    }
    else if (overshoot >= (kneeDb * 0.5f))
    {
        grDb = overshoot * slope;
    }

    return std::max(0.0f, grDb);
}

float SSLChannelStripDSP::computeGateAttenuationDb(float levelDb, float gateThreshDb, float gateRangeDb) noexcept
{
    if (gateRangeDb <= 0.05f || levelDb >= gateThreshDb)
        return 0.0f;

    const float undershoot = gateThreshDb - levelDb;
    constexpr float expanderRatio = 3.0f;
    const float attenDb = undershoot * (expanderRatio - 1.0f);
    return std::clamp(attenDb, 0.0f, gateRangeDb);
}

float SSLChannelStripDSP::computeDeessGainReductionDb(float scLevelDb, float deessThreshDb) noexcept
{
    const float overshoot = scLevelDb - deessThreshDb;
    if (overshoot <= -1.0f)
        return 0.0f;

    float grDb = 0.0f;
    if (overshoot < 1.0f)
    {
        const float x = overshoot + 1.0f;
        grDb = 0.85f * (x * x) * 0.25f;
    }
    else
    {
        grDb = 0.85f * overshoot;
    }

    return std::clamp(grDb, 0.0f, 12.0f);
}

void SSLChannelStripDSP::processInputAndPreampStage(const SSLParameters& params,
                                                    float& left,
                                                    float& right,
                                                    float& outPostLineTrimL,
                                                    float& outPostLineTrimR) noexcept
{
    // 1. CHANNEL INPUT (LINE TRIM) & POLARITY INVERT
    const float inGainLin = dbToLinear(params.inputGainDb) * (params.phaseInvert ? -1.0f : 1.0f);
    float sL = left * inGainLin;
    float sR = right * inGainLin;

    outPostLineTrimL = sL;
    outPostLineTrimR = sR;

    // 2. PREAMP INPUT SATURATION (ADAA 64-bit avec garantie saturate(0.0) == 0.0)
    if (params.inputSatDrive > 1e-4f)
    {
        const float satAmt  = std::clamp(params.inputSatDrive, 0.0f, 1.0f);
        const float tol     = std::clamp(params.analogTolerance, 0.0f, 1.0f);
        const float driveL  = (1.0f + 2.4f * satAmt) * (1.0f - 0.015f * tol);
        const float driveR  = (1.0f + 2.4f * satAmt) * (1.0f + 0.015f * tol);
        const float biasL   = 0.065f * satAmt * (1.0f + 0.04f * tol);
        const float biasR   = 0.065f * satAmt * (1.0f - 0.04f * tol);
        const float rmsComp = 1.0f / static_cast<float>(0.68 * (1.0 + 2.4 * satAmt) + 0.32 * ADAASaturator::f(1.0 + 2.4 * satAmt));

        const float wL = preampAdaaSaturator_.processSampleOrder2(sL, driveL, biasL, 0) * rmsComp;
        const float wR = preampAdaaSaturator_.processSampleOrder2(sR, driveR, biasR, 1) * rmsComp;

        const float blend = 0.80f * satAmt;
        sL = (1.0f - blend) * sL + blend * wL;
        sR = (1.0f - blend) * sR + blend * wR;

        sL = inputDcBlockerFilter_.processSample(sL, 0);
        sR = inputDcBlockerFilter_.processSample(sR, 1);
    }

    // 3. INPUT FILTERS ZDF/TPT (HPF / LPF)
    if (!params.filtersToSc)
    {
        if (params.hpfEnabled)
        {
            sL = hpfZdfFilter_.processSample(sL, 0);
            sR = hpfZdfFilter_.processSample(sR, 1);
        }

        if (params.lpfEnabled)
        {
            sL = lpfZdfFilter_.processSample(sL, 0);
            sR = lpfZdfFilter_.processSample(sR, 1);
        }
    }

    // 4. Trim de sortie de la Section 1 (FILT/PRE TRIM)
    const float preTrimLin = dbToLinear(params.filtPreTrimDb);
    left  = sL * preTrimLin;
    right = sR * preTrimLin;
}

void SSLChannelStripDSP::processDynamicsStage(const SSLParameters& params,
                                              float& left,
                                              float& right,
                                              float extScLeft,
                                              float extScRight,
                                              float& outCompGrDb,
                                              float& outGateGrDb,
                                              float& outDeessGrDb,
                                              float& outCompDetDb,
                                              float& outGateDetDb,
                                              float& outDeessDetDb) noexcept
{
    float sL = left;
    float sR = right;

    // 1. SIDECHAIN ROUTING (INT vs EXT SC + FILTERS TO SC + DEDICATED SC HPF + SC LISTEN)
    float detL = params.extSidechain ? extScLeft  : sL;
    float detR = params.extSidechain ? extScRight : sR;

    if (params.filtersToSc)
    {
        if (params.hpfEnabled)
        {
            detL = scHpfZdfFilter_.processSample(detL, 0);
            detR = scHpfZdfFilter_.processSample(detR, 1);
        }
        if (params.lpfEnabled)
        {
            detL = scLpfZdfFilter_.processSample(detL, 0);
            detR = scLpfZdfFilter_.processSample(detR, 1);
        }
    }

    if (params.scHpfFreqHz > 21.0f)
    {
        detL = dedicatedScHpfFilter_.processSample(detL, 0);
        detR = dedicatedScHpfFilter_.processSample(detR, 1);
    }

    float compGrDb  = 0.0f;
    float gateGrDb  = 0.0f;
    float deessGrDb = 0.0f;

    const float stereoDetector = std::max(std::abs(detL), std::abs(detR));

    if (params.compAutoRelease)
    {
        if (stereoDetector > compAutoFastCapLin_)
            compAutoFastCapLin_ = compAttackCoeff_ * compAutoFastCapLin_ + (1.0f - compAttackCoeff_) * stereoDetector;
        else
            compAutoFastCapLin_ = compAutoFastRelCoeff_ * compAutoFastCapLin_ + (1.0f - compAutoFastRelCoeff_) * std::max(stereoDetector, compAutoSlowCapLin_);

        if (stereoDetector > compAutoSlowCapLin_)
            compAutoSlowCapLin_ = compAutoSlowAttCoeff_ * compAutoSlowCapLin_ + (1.0f - compAutoSlowAttCoeff_) * stereoDetector;
        else
            compAutoSlowCapLin_ = compAutoSlowRelCoeff_ * compAutoSlowCapLin_ + (1.0f - compAutoSlowRelCoeff_) * stereoDetector;

        compEnvelopeLin_ = std::max(compAutoFastCapLin_, compAutoSlowCapLin_);
    }
    else
    {
        if (stereoDetector > compEnvelopeLin_)
            compEnvelopeLin_ = compAttackCoeff_ * compEnvelopeLin_ + (1.0f - compAttackCoeff_) * stereoDetector;
        else
            compEnvelopeLin_ = compReleaseCoeff_ * compEnvelopeLin_ + (1.0f - compReleaseCoeff_) * stereoDetector;

        compAutoFastCapLin_ = compEnvelopeLin_;
        compAutoSlowCapLin_ = compEnvelopeLin_;
    }

    if (stereoDetector > gateEnvelopeLin_)
        gateEnvelopeLin_ = gateAttackCoeff_ * gateEnvelopeLin_ + (1.0f - gateAttackCoeff_) * stereoDetector;
    else
        gateEnvelopeLin_ = gateReleaseCoeff_ * gateEnvelopeLin_ + (1.0f - gateReleaseCoeff_) * stereoDetector;

    const float compLevelDb = linearToDb(compEnvelopeLin_);
    const float gateLevelDb = linearToDb(gateEnvelopeLin_);
    outCompDetDb = compLevelDb;
    outGateDetDb = gateLevelDb;

    if (params.scListen)
    {
        left  = detL;
        right = detR;
        outCompGrDb   = 0.0f;
        outGateGrDb   = 0.0f;
        outDeessGrDb  = 0.0f;
        outDeessDetDb = -100.0f;
        return;
    }

    const bool processCh0 = (params.msMode != 3);
    const bool processCh1 = (params.msMode != 2);

    if (params.dynEnabled)
    {
        compGrDb = computeCompGainReductionDb(compLevelDb, params.compThreshDb, params.compRatio, params.limiterMode);
        gateGrDb = computeGateAttenuationDb(gateLevelDb, params.gateThreshDb, params.gateRangeDb);

        const float totalDynGainLin = dbToLinear(-(compGrDb + gateGrDb));
        if (processCh0) sL *= totalDynGainLin;
        if (processCh1) sR *= totalDynGainLin;
    }

    // 2. DE-ESSER INTÉGRÉ
    const float scL = deessSidechainFilter_.processSample(sL, 0);
    const float scR = deessSidechainFilter_.processSample(sR, 1);
    const float scPeak = std::max(std::abs(scL), std::abs(scR));

    if (scPeak > deessEnvelopeLin_)
        deessEnvelopeLin_ = deessAttackCoeff_ * deessEnvelopeLin_ + (1.0f - deessAttackCoeff_) * scPeak;
    else
        deessEnvelopeLin_ = deessReleaseCoeff_ * deessEnvelopeLin_ + (1.0f - deessReleaseCoeff_) * scPeak;

    const float scLevelDb = linearToDb(deessEnvelopeLin_);
    outDeessDetDb = scLevelDb;

    if (params.deessEnabled)
    {
        deessGrDb = computeDeessGainReductionDb(scLevelDb, params.deessThreshDb);

        if (std::abs(deessGrDb - lastDeessCutDb_) > 0.12f || std::abs(params.deessFreqHz - lastDeessFreqHz_) > 1.0f)
        {
            lastDeessCutDb_  = deessGrDb;
            lastDeessFreqHz_ = params.deessFreqHz;
            deessDynamicZdfFilter_.setCoefficients(
                ZDFStateVariableFilter::makeBell(sampleRate_, params.deessFreqHz, -lastDeessCutDb_, 2.0f));
        }

        if (processCh0) sL = deessDynamicZdfFilter_.processSample(sL, 0);
        if (processCh1) sR = deessDynamicZdfFilter_.processSample(sR, 1);
    }
    else
    {
        lastDeessCutDb_ = 0.0f;
    }

    if (params.dynEnabled || params.deessEnabled)
    {
        const float dynMakeupLin = dbToLinear(params.makeupGainDb);
        if (processCh0) sL *= dynMakeupLin;
        if (processCh1) sR *= dynMakeupLin;
    }

    smoothedCompGrDb_ = 0.992f * smoothedCompGrDb_ + 0.008f * compGrDb;

    left  = sL;
    right = sR;

    outCompGrDb  = compGrDb;
    outGateGrDb  = gateGrDb;
    outDeessGrDb = deessGrDb;
}

void SSLChannelStripDSP::processColorStage(const SSLParameters& params,
                                           float& left,
                                           float& right,
                                           float& outEffectiveColor) noexcept
{
    if (params.scListen)
    {
        outEffectiveColor = 0.0f;
        return;
    }

    float sL = left;
    float sR = right;

    float effectiveColor = params.colorAmount;
    if (params.colorAmount > 1e-4f)
    {
        if (params.colorLinkComp)
        {
            const float grMod = std::clamp(smoothedCompGrDb_ / 12.0f, 0.0f, 1.5f);
            effectiveColor = std::clamp(params.colorAmount * (0.25f + 0.95f * grMod), 0.0f, 1.25f);
        }

        if (std::abs(params.satTone) > 0.01f)
        {
            sL = satTonePreFilter_.processSample(sL, 0);
            sR = satTonePreFilter_.processSample(sR, 1);
        }

        const float tol        = std::clamp(params.analogTolerance, 0.0f, 1.0f);
        const float driveScale = params.circuit4000G ? 2.8f : 3.4f;
        const float biasScale  = params.circuit4000G ? 0.06f : 0.115f;

        const float drive    = 1.0f + driveScale * effectiveColor;
        const float asymBias = biasScale * effectiveColor;
        const float normGain = 1.0f / std::max(static_cast<float>(ADAASaturator::f(static_cast<double>(drive))), 0.45f);
        const float blend    = std::clamp(effectiveColor, 0.0f, 1.0f) * 0.75f;

        // Saturation ADAA 64-bit d'ordre 2 (garantissant saturate(0.0) == 0.0 même avec tolérance L/R 100%)
        const float satL = colorAdaaSaturator_.processSampleOrder2(sL, drive * (1.0f + 0.015f * tol), asymBias * (1.0f + 0.05f * tol), 0) * normGain;
        const float satR = colorAdaaSaturator_.processSampleOrder2(sR, drive * (1.0f - 0.015f * tol), asymBias * (1.0f - 0.05f * tol), 1) * normGain;

        sL = (1.0f - blend) * sL + blend * satL;
        sR = (1.0f - blend) * sR + blend * satR;

        // Hystérésis magnétique de transformateur en double précision (64 bits)
        const double hystWeight = static_cast<double>(params.circuit4000G ? 0.16f : 0.34f)
                                * static_cast<double>(std::clamp(effectiveColor, 0.0f, 1.25f));

        auto applyTransformerHysteresis64 = [&](float xIn, std::size_t ch) noexcept -> float
        {
            const double x = static_cast<double>(xIn);
            if (std::abs(x) < 1e-6 && std::abs(transformerFluxState_[ch]) < 1e-6 && std::abs(transformerFluxPrev_[ch]) < 1e-6)
            {
                transformerFluxState_[ch] = 0.0;
                transformerFluxPrev_[ch]  = 0.0;
                return 0.0f;
            }
            transformerFluxState_[ch] = transformerFluxAlpha_ * transformerFluxState_[ch] + (1.0 - transformerFluxAlpha_) * x;
            const double hFlux = transformerFluxState_[ch] + 0.35 * (transformerFluxState_[ch] - transformerFluxPrev_[ch]);
            transformerFluxPrev_[ch] = transformerFluxState_[ch];
            const double fluxNl = std::tanh(2.4 * hFlux) - 2.4 * hFlux;
            return static_cast<float>(x + hystWeight * fluxNl);
        };

        sL = applyTransformerHysteresis64(sL, 0);
        sR = applyTransformerHysteresis64(sR, 1);

        if (std::abs(params.satTone) > 0.01f)
        {
            sL = satTonePostFilter_.processSample(sL, 0);
            sR = satTonePostFilter_.processSample(sR, 1);
        }

        sL = colorWarmthFilter_.processSample(sL, 0);
        sR = colorWarmthFilter_.processSample(sR, 1);

        sL = dcBlocker5HzFilter_.processSample(sL, 0);
        sR = dcBlocker5HzFilter_.processSample(sR, 1);
    }
    else
    {
        effectiveColor = 0.0f;
    }

    if (params.crosstalk > 1e-4f)
    {
        const float xtalkGain = std::clamp(params.crosstalk, 0.0f, 1.0f) * 0.038f;
        const float bleedFromL = crosstalkBandFilter_.processSample(sL, 0);
        const float bleedFromR = crosstalkBandFilter_.processSample(sR, 1);
        sL += xtalkGain * bleedFromR;
        sR += xtalkGain * bleedFromL;
    }

    const float colorTrimLin = dbToLinear(params.colorTrimDb);
    left  = sL * colorTrimLin;
    right = sR * colorTrimLin;

    outEffectiveColor = effectiveColor;
}

void SSLChannelStripDSP::processEqStage(const SSLParameters& params,
                                        float& left,
                                        float& right) noexcept
{
    if (!params.eqEnabled || params.scListen)
        return;

    const bool processCh0 = (params.msMode != 3);
    const bool processCh1 = (params.msMode != 2);

    float sL = left;
    float sR = right;

    if (processCh0)
    {
        sL = lfZdfFilter_.processSample(sL, 0);
        sL = lmfZdfFilter_.processSample(sL, 0);
        sL = hmfZdfFilter_.processSample(sL, 0);
        if (params.circuit4000G)
        {
            sL = lmfGyratorDipFilter_.processSample(sL, 0);
            sL = hmfGyratorDipFilter_.processSample(sL, 0);
        }
        sL = hfZdfFilter_.processSample(sL, 0);
    }

    if (processCh1)
    {
        sR = lfZdfFilter_.processSample(sR, 1);
        sR = lmfZdfFilter_.processSample(sR, 1);
        sR = hmfZdfFilter_.processSample(sR, 1);
        if (params.circuit4000G)
        {
            sR = lmfGyratorDipFilter_.processSample(sR, 1);
            sR = hmfGyratorDipFilter_.processSample(sR, 1);
        }
        sR = hfZdfFilter_.processSample(sR, 1);
    }

    const float eqTrimLin = dbToLinear(params.eqTrimDb);
    if (processCh0) sL *= eqTrimLin;
    if (processCh1) sR *= eqTrimLin;

    left  = sL;
    right = sR;
}

void SSLChannelStripDSP::processCentralModuleById(int moduleId,
                                                  const SSLParameters& params,
                                                  float& left,
                                                  float& right,
                                                  float extScLeft,
                                                  float extScRight,
                                                  float& outCompGrDb,
                                                  float& outGateGrDb,
                                                  float& outDeessGrDb,
                                                  float& outEffectiveColor,
                                                  float& outCompDetDb,
                                                  float& outGateDetDb,
                                                  float& outDeessDetDb) noexcept
{
    if (moduleId == 0)
    {
        processDynamicsStage(params, left, right, extScLeft, extScRight,
                             outCompGrDb, outGateGrDb, outDeessGrDb,
                             outCompDetDb, outGateDetDb, outDeessDetDb);
    }
    else if (moduleId == 1)
    {
        processColorStage(params, left, right, outEffectiveColor);
    }
    else if (moduleId == 2)
    {
        processEqStage(params, left, right);
    }
}

void SSLChannelStripDSP::processAllCentralModulesOrdered(const SSLParameters& params,
                                                         float& left,
                                                         float& right,
                                                         float extScLeft,
                                                         float extScRight,
                                                         float& outCompGrDb,
                                                         float& outGateGrDb,
                                                         float& outDeessGrDb,
                                                         float& outEffectiveColor,
                                                         float& outCompDetDb,
                                                         float& outGateDetDb,
                                                         float& outDeessDetDb) noexcept
{
    const int orderIdx = std::clamp(params.moduleOrder, 0, 5);
    const auto& perm = kModulePermutations[static_cast<std::size_t>(orderIdx)];

    for (int step = 0; step < 3; ++step)
    {
        processCentralModuleById(perm[static_cast<std::size_t>(step)],
                                 params, left, right, extScLeft, extScRight,
                                 outCompGrDb, outGateGrDb, outDeessGrDb,
                                 outEffectiveColor, outCompDetDb, outGateDetDb, outDeessDetDb);
    }
}

void SSLChannelStripDSP::processOutputStage(const SSLParameters& params,
                                            float dryLeft,
                                            float dryRight,
                                            float postLineTrimL,
                                            float postLineTrimR,
                                            float& left,
                                            float& right,
                                            float& outAppliedAutoGainDb) noexcept
{
    if (params.scListen)
    {
        outAppliedAutoGainDb = 0.0f;
        return;
    }

    // Gel strict de l'AUTO-GAIN sous -60 dBFS (amplitude < 1e-3 -> puissance < 1e-6)
    const float refSq  = 0.5f * (postLineTrimL * postLineTrimL + postLineTrimR * postLineTrimR);
    const float procSq = 0.5f * (left * left + right * right);

    if (refSq > 1e-6f)
    {
        autoGainRefMeanSq_  = vuBallisticCoeff_ * autoGainRefMeanSq_  + (1.0f - vuBallisticCoeff_) * refSq;
        autoGainProcMeanSq_ = vuBallisticCoeff_ * autoGainProcMeanSq_ + (1.0f - vuBallisticCoeff_) * procSq;

        if (autoGainRefMeanSq_ > 1e-6f && autoGainProcMeanSq_ > 1e-6f)
        {
            const float refDb  = 10.0f * std::log10(autoGainRefMeanSq_);
            const float procDb = 10.0f * std::log10(autoGainProcMeanSq_);
            const float targetAutoGainDb = std::clamp(refDb - procDb, -12.0f, 12.0f);
            smoothedAutoGainDb_ = vuBallisticCoeff_ * smoothedAutoGainDb_ + (1.0f - vuBallisticCoeff_) * targetAutoGainDb;
        }
    }
    // Si refSq <= 1e-6f (-60 dBFS), smoothedAutoGainDb_ est strictement gelé (aucune amplification du silence)

    const float appliedAutoGainDb = params.autoGain ? std::clamp(smoothedAutoGainDb_, -12.0f, 12.0f) : 0.0f;
    outAppliedAutoGainDb = appliedAutoGainDb;

    const float wetL = left  * dbToLinear(appliedAutoGainDb);
    const float wetR = right * dbToLinear(appliedAutoGainDb);

    const float wetMix = std::clamp(params.mix, 0.0f, 1.0f);
    const float mixedL = (1.0f - wetMix) * dryLeft  + wetMix * wetL;
    const float mixedR = (1.0f - wetMix) * dryRight + wetMix * wetR;

    const float outFaderLin = dbToLinear(params.outputGainDb);
    left  = mixedL * outFaderLin;
    right = mixedR * outFaderLin;
}

void SSLChannelStripDSP::updateLevelMetersSample(float inL, float inR,
                                                 float outL, float outR,
                                                 float& outInVuDbL, float& outInVuDbR,
                                                 float& outOutVuDbL, float& outOutVuDbR,
                                                 float& outInLufsDbL, float& outInLufsDbR,
                                                 float& outOutLufsDbL, float& outOutLufsDbR) noexcept
{
    vuInMeanSqL_  = vuBallisticCoeff_ * vuInMeanSqL_  + (1.0f - vuBallisticCoeff_) * (inL * inL);
    vuInMeanSqR_  = vuBallisticCoeff_ * vuInMeanSqR_  + (1.0f - vuBallisticCoeff_) * (inR * inR);
    vuOutMeanSqL_ = vuBallisticCoeff_ * vuOutMeanSqL_ + (1.0f - vuBallisticCoeff_) * (outL * outL);
    vuOutMeanSqR_ = vuBallisticCoeff_ * vuOutMeanSqR_ + (1.0f - vuBallisticCoeff_) * (outR * outR);

    outInVuDbL  = 10.0f * std::log10(std::max(vuInMeanSqL_,  1e-10f));
    outInVuDbR  = 10.0f * std::log10(std::max(vuInMeanSqR_,  1e-10f));
    outOutVuDbL = 10.0f * std::log10(std::max(vuOutMeanSqL_, 1e-10f));
    outOutVuDbR = 10.0f * std::log10(std::max(vuOutMeanSqR_, 1e-10f));

    const float kwInL = kWeightInStage2_.processSample(kWeightInStage1_.processSample(inL, 0), 0);
    const float kwInR = kWeightInStage2_.processSample(kWeightInStage1_.processSample(inR, 1), 1);

    const float kwOutL = kWeightOutStage2_.processSample(kWeightOutStage1_.processSample(outL, 0), 0);
    const float kwOutR = kWeightOutStage2_.processSample(kWeightOutStage1_.processSample(outR, 1), 1);

    lufsInMeanSqL_  = lufs400msCoeff_ * lufsInMeanSqL_  + (1.0f - lufs400msCoeff_) * (kwInL * kwInL);
    lufsInMeanSqR_  = lufs400msCoeff_ * lufsInMeanSqR_  + (1.0f - lufs400msCoeff_) * (kwInR * kwInR);
    lufsOutMeanSqL_ = lufs400msCoeff_ * lufsOutMeanSqL_ + (1.0f - lufs400msCoeff_) * (kwOutL * kwOutL);
    lufsOutMeanSqR_ = lufs400msCoeff_ * lufsOutMeanSqR_ + (1.0f - lufs400msCoeff_) * (kwOutR * kwOutR);

    outInLufsDbL  = -0.691f + 10.0f * std::log10(std::max(lufsInMeanSqL_,  1e-10f));
    outInLufsDbR  = -0.691f + 10.0f * std::log10(std::max(lufsInMeanSqR_,  1e-10f));
    outOutLufsDbL = -0.691f + 10.0f * std::log10(std::max(lufsOutMeanSqL_, 1e-10f));
    outOutLufsDbR = -0.691f + 10.0f * std::log10(std::max(lufsOutMeanSqR_, 1e-10f));
}

} // namespace kalyx::dsp
