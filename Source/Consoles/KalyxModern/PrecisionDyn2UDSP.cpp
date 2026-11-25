#include "PrecisionDyn2UDSP.h"
#include <algorithm>
#include <cmath>

namespace kalyx::consoles
{

void PrecisionDyn2UDSP::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(sampleRate, 8000.0);
    scopeDecimPeriod_ = std::max(64, static_cast<int>(sampleRate_ / 172.0));
    reset();
}

void PrecisionDyn2UDSP::reset() noexcept
{
    delayL_.fill(0.0);
    delayR_.fill(0.0);
    delayWriteIdx_ = 0;

    scHpfL_.reset();
    scHpfR_.reset();
    transLpL_.reset();
    transLpR_.reset();

    vcaEnvDbA_ = -120.0;
    vcaEnvDbB_ = -120.0;
    autoRelA_  = 0.1;
    autoRelB_  = 0.1;

    fastEnvHi_ = 0.0;
    slowEnvHi_ = 0.0;
    susEnvHi_  = 0.0;
    fastEnvLo_ = 0.0;
    slowEnvLo_ = 0.0;
    susEnvLo_  = 0.0;

    for (auto& s : scopeRing_)
        s = ScopeSample {};
    scopeWriteIdx_.store(0, std::memory_order_relaxed);
    scopeDecimCounter_ = 0;
    scopePeakWave_  = 0.0;
    scopePeakGr_    = 0.0;
    scopePeakTrans_ = 0.0;
}

double PrecisionDyn2UDSP::mapRatioFromNorm(float norm) noexcept
{
    const double t = std::clamp(static_cast<double>(norm), 0.0, 1.0);
    if (t < 0.15)
    {
        // Expansion vers le haut : 0.5:1 .. 1.0:1
        return 0.5 + (t / 0.15) * 0.5;
    }
    if (t > 0.96)
    {
        return 1000.0; // ∞:1
    }
    // Compression continue 1.05:1 .. 20.0:1 (log)
    const double u = (t - 0.15) / (0.96 - 0.15);
    return 1.05 * std::pow(20.0 / 1.05, u);
}

void PrecisionDyn2UDSP::processVcaChannel(double detSig, double& envDb, double& autoRelState,
                                          double threshDb, double ratio, double kneeDb,
                                          double attMs, double relMs, bool autoRel,
                                          double& outGainLin, double& outGrDb) noexcept
{
    const double absIn = std::abs(detSig);
    const double inDb = (absIn > 1.0e-12) ? (20.0 * std::log10(absIn)) : -120.0;

    const double attCoeff = std::exp(-1.0 / (std::max(0.01, attMs) * 0.001 * sampleRate_));
    double effectiveRelMs = std::max(5.0, relMs);
    if (autoRel)
    {
        // Programme-dépendant : plus de crête = release plus rapide au début puis maintien doux
        const double fastTrack = std::exp(-1.0 / (0.040 * sampleRate_));
        const double slowTrack = std::exp(-1.0 / (0.450 * sampleRate_));
        const double over = std::max(0.0, inDb - threshDb);
        autoRelState = (over > autoRelState)
            ? (fastTrack * autoRelState + (1.0 - fastTrack) * over)
            : (slowTrack * autoRelState + (1.0 - slowTrack) * over);
        effectiveRelMs = std::clamp(35.0 + autoRelState * 18.0, 25.0, 1400.0);
    }
    const double relCoeff = std::exp(-1.0 / (effectiveRelMs * 0.001 * sampleRate_));

    if (inDb > envDb)
        envDb = attCoeff * envDb + (1.0 - attCoeff) * inDb;
    else
        envDb = relCoeff * envDb + (1.0 - relCoeff) * inDb;

    double gainDb = 0.0;
    outGrDb = 0.0;

    if (ratio >= 1.0)
    {
        const double over = envDb - threshDb;
        const double slope = 1.0 - (1.0 / ratio);
        if (kneeDb > 0.05 && over > -0.5 * kneeDb && over < 0.5 * kneeDb)
        {
            const double x = over + 0.5 * kneeDb;
            outGrDb = slope * (x * x) / (2.0 * kneeDb);
        }
        else if (over >= 0.5 * kneeDb)
        {
            outGrDb = slope * over;
        }
        gainDb = -outGrDb;
    }
    else
    {
        // Mode expansion vers le haut (ratio < 1.0 -> ex. 0.5:1)
        const double over = std::max(0.0, envDb - threshDb);
        const double expSlope = std::clamp(1.0 - ratio, 0.0, 0.5);
        gainDb = std::min(12.0, expSlope * over);
        outGrDb = 0.0;
    }

    outGainLin = std::pow(10.0, gainDb / 20.0);
}

void PrecisionDyn2UDSP::processTransientBand(double xA, double xB, double punch, double sustain,
                                             double& fastEnv, double& slowEnv, double& susEnv,
                                             double& outScale, double& outTransMetric) noexcept
{
    const double level = std::max(std::abs(xA), std::abs(xB));

    // Enveloppes différentielles 64-bit sans allocation
    const double attFast = std::exp(-1.0 / (0.0012 * sampleRate_));
    const double relFast = std::exp(-1.0 / (0.0140 * sampleRate_));
    const double attSlow = std::exp(-1.0 / (0.0180 * sampleRate_));
    const double relSlow = std::exp(-1.0 / (0.0950 * sampleRate_));
    const double attSus  = std::exp(-1.0 / (0.0250 * sampleRate_));
    const double relSus  = std::exp(-1.0 / (0.2600 * sampleRate_));

    fastEnv = (level > fastEnv) ? (attFast * fastEnv + (1.0 - attFast) * level)
                                : (relFast * fastEnv + (1.0 - relFast) * level);
    slowEnv = (level > slowEnv) ? (attSlow * slowEnv + (1.0 - attSlow) * level)
                                : (relSlow * slowEnv + (1.0 - relSlow) * level);
    susEnv  = (level > susEnv)  ? (attSus  * susEnv  + (1.0 - attSus)  * level)
                                : (relSus  * susEnv  + (1.0 - relSus)  * level);

    const double denom = std::max(1.0e-7, slowEnv + 1.0e-5);
    const double attackRatio  = std::clamp((fastEnv - slowEnv) / denom, 0.0, 2.5);
    const double sustainRatio = std::clamp((susEnv - fastEnv) / std::max(1.0e-7, susEnv + 1.0e-5), 0.0, 1.5);

    const double attackGainDb  = punch * 12.0 * attackRatio;
    const double sustainGainDb = sustain * 12.0 * sustainRatio;
    const double totalDb = std::clamp(attackGainDb + sustainGainDb, -18.0, 18.0);

    outScale = std::pow(10.0, totalDb / 20.0);
    outTransMetric = std::clamp(totalDb / 12.0, -1.0, 1.0);
}

float PrecisionDyn2UDSP::processSample(float& l, float& r,
                                       const kalyx::core::SlotParamArray2U& p) noexcept
{
    const double dryL = static_cast<double>(l);
    const double dryR = static_cast<double>(r);

    if (p[0] <= 0.5f)
        return 0.0f;

    const bool   postTrans  = (p[1] > 0.5f);
    const double lookMs     = std::clamp(static_cast<double>(p[2]) * 5.0, 0.0, 5.0);
    const bool   autoRel    = (p[3] > 0.5f);
    const double threshDb   = kalyx::core::mapLin(p[4], -50.0f, 0.0f);
    const double ratio      = mapRatioFromNorm(p[5]);
    const double kneeDb     = kalyx::core::mapLin(p[6], 0.0f, 24.0f);
    const double attMs      = kalyx::core::mapLog(p[7], 0.01f, 250.0f);
    const double relMs      = kalyx::core::mapLog(p[8], 5.0f, 2500.0f);
    const double makeupDb   = kalyx::core::mapLin(p[9], -12.0f, 24.0f);
    const double punch      = kalyx::core::mapLin(p[10], -1.0f, 1.0f);
    const double sustain    = kalyx::core::mapLin(p[11], -1.0f, 1.0f);
    const double transFreq  = kalyx::core::mapLog(p[12], 100.0f, 8000.0f);
    const double scHpfFreq  = kalyx::core::mapLog(p[13], 20.0f, 500.0f);
    const double blend      = std::clamp(static_cast<double>(p[14]), 0.0, 1.0);
    const int    routing    = std::clamp(static_cast<int>(std::round(p[15] * 2.0f)), 0, 2); // 0=ST, 1=M/S, 2=Dual

    // Matrice M/S si nécessaire
    double workL = dryL;
    double workR = dryR;
    if (routing == 1)
    {
        const double mid  = (dryL + dryR) * 0.7071067811865475;
        const double side = (dryL - dryR) * 0.7071067811865475;
        workL = mid;
        workR = side;
    }

    // Lookahead buffer circulaire
    delayL_[static_cast<std::size_t>(delayWriteIdx_)] = workL;
    delayR_[static_cast<std::size_t>(delayWriteIdx_)] = workR;

    const int lookSamples = std::clamp(static_cast<int>(std::round(lookMs * 0.001 * sampleRate_)),
                                       0, kMaxLookaheadSamples - 1);
    const int readIdx = (delayWriteIdx_ - lookSamples + kMaxLookaheadSamples) % kMaxLookaheadSamples;
    delayWriteIdx_ = (delayWriteIdx_ + 1) % kMaxLookaheadSamples;

    double delL = delayL_[static_cast<std::size_t>(readIdx)];
    double delR = delayR_[static_cast<std::size_t>(readIdx)];

    // Sidechain HPF sur le signal présent (non retardé -> lookahead)
    scHpfL_.setHighPass(sampleRate_, scHpfFreq, 0.7071);
    scHpfR_.setHighPass(sampleRate_, scHpfFreq, 0.7071);
    const double scA = static_cast<double>(scHpfL_.processSample(static_cast<float>(workL)));
    const double scB = static_cast<double>(scHpfR_.processSample(static_cast<float>(workR)));

    auto runTransient = [&](double& a, double& b, double& outMetric) noexcept
    {
        if (std::abs(punch) < 1.0e-4 && std::abs(sustain) < 1.0e-4)
        {
            outMetric = 0.0;
            return;
        }
        transLpL_.setLowPass(sampleRate_, transFreq, 0.7071);
        transLpR_.setLowPass(sampleRate_, transFreq, 0.7071);
        const double loA = static_cast<double>(transLpL_.processSample(static_cast<float>(a)));
        const double loB = static_cast<double>(transLpR_.processSample(static_cast<float>(b)));
        const double hiA = a - loA;
        const double hiB = b - loB;

        double scaleHi = 1.0, scaleLo = 1.0;
        double mHi = 0.0, mLo = 0.0;
        processTransientBand(hiA, hiB, punch, sustain, fastEnvHi_, slowEnvHi_, susEnvHi_, scaleHi, mHi);
        processTransientBand(loA, loB, punch * 0.45, sustain * 0.65, fastEnvLo_, slowEnvLo_, susEnvLo_, scaleLo, mLo);

        a = loA * scaleLo + hiA * scaleHi;
        b = loB * scaleLo + hiB * scaleHi;
        outMetric = (std::abs(mHi) >= std::abs(mLo)) ? mHi : mLo;
    };

    double transMetric = 0.0;
    if (!postTrans)
        runTransient(delL, delR, transMetric);

    // Compresseur VCA
    double gainA = 1.0, gainB = 1.0;
    double grDbA = 0.0, grDbB = 0.0;

    if (routing == 0) // STEREO LINKED
    {
        const double linked = std::max(std::abs(scA), std::abs(scB));
        processVcaChannel(linked, vcaEnvDbA_, autoRelA_, threshDb, ratio, kneeDb, attMs, relMs, autoRel, gainA, grDbA);
        vcaEnvDbB_ = vcaEnvDbA_;
        autoRelB_  = autoRelA_;
        gainB = gainA;
        grDbB = grDbA;
    }
    else
    {
        processVcaChannel(scA, vcaEnvDbA_, autoRelA_, threshDb, ratio, kneeDb, attMs, relMs, autoRel, gainA, grDbA);
        processVcaChannel(scB, vcaEnvDbB_, autoRelB_, threshDb, ratio, kneeDb, attMs, relMs, autoRel, gainB, grDbB);
    }

    delL *= gainA;
    delR *= gainB;

    if (postTrans)
        runTransient(delL, delR, transMetric);

    const double makeupLin = std::pow(10.0, makeupDb / 20.0);
    delL *= makeupLin;
    delR *= makeupLin;

    // Retour M/S -> L/R si nécessaire
    double wetL = delL;
    double wetR = delR;
    if (routing == 1)
    {
        wetL = (delL + delR) * 0.7071067811865475;
        wetR = (delL - delR) * 0.7071067811865475;
    }

    const double outL = dryL * (1.0 - blend) + wetL * blend;
    const double outR = dryR * (1.0 - blend) + wetR * blend;

    l = std::isfinite(outL) ? static_cast<float>(outL) : 0.0f;
    r = std::isfinite(outR) ? static_cast<float>(outR) : 0.0f;

    const double avgGrDb = 0.5 * (grDbA + grDbB);

    // Télémétrie oscilloscope décimée (lock-free)
    const double absWave = std::max(std::abs(dryL), std::abs(dryR));
    scopePeakWave_ = std::max(scopePeakWave_, absWave);
    scopePeakGr_   = std::max(scopePeakGr_, avgGrDb);
    if (std::abs(transMetric) > std::abs(scopePeakTrans_))
        scopePeakTrans_ = transMetric;

    if (++scopeDecimCounter_ >= scopeDecimPeriod_)
    {
        const int idx = scopeWriteIdx_.load(std::memory_order_relaxed);
        scopeRing_[static_cast<std::size_t>(idx)] = ScopeSample {
            static_cast<float>(std::clamp(scopePeakWave_, 0.0, 1.0)),
            static_cast<float>(std::clamp(scopePeakGr_ / 24.0, 0.0, 1.0)),
            static_cast<float>(std::clamp(scopePeakTrans_, -1.0, 1.0))
        };
        scopeWriteIdx_.store((idx + 1) % kScopeSize, std::memory_order_release);
        scopeDecimCounter_ = 0;
        scopePeakWave_  = 0.0;
        scopePeakGr_    = 0.0;
        scopePeakTrans_ = 0.0;
    }

    return static_cast<float>(avgGrDb);
}

void PrecisionDyn2UDSP::copyScopeHistory(std::array<ScopeSample, kScopeSize>& dest) const noexcept
{
    const int head = scopeWriteIdx_.load(std::memory_order_acquire);
    for (int i = 0; i < kScopeSize; ++i)
        dest[static_cast<std::size_t>(i)] = scopeRing_[static_cast<std::size_t>((head + i) % kScopeSize)];
}

} // namespace kalyx::consoles
