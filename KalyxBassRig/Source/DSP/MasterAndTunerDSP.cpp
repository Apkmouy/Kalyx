#include "MasterAndTunerDSP.h"
#include <algorithm>
#include <cmath>

namespace kalyx::bass::dsp
{

// ==============================================================================
// NoiseGate64
// ==============================================================================
void NoiseGate64::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(sampleRate, 8000.0);
    reset();
}

void NoiseGate64::reset() noexcept
{
    env_  = 0.0;
    gain_ = 1.0;
    gateGain_.store(1.0f, std::memory_order_relaxed);
}

double NoiseGate64::processSample(double x, bool enabled, double threshDb) noexcept
{
    if (!enabled)
    {
        gain_ = 1.0;
        gateGain_.store(1.0f, std::memory_order_relaxed);
        return x;
    }

    const double absX = std::abs(x);
    const double att  = std::exp(-1.0 / (0.001 * sampleRate_));
    const double rel  = std::exp(-1.0 / (0.065 * sampleRate_));
    env_ = (absX > env_) ? (att * env_ + (1.0 - att) * absX)
                         : (rel * env_ + (1.0 - rel) * absX);

    const double threshLin = std::pow(10.0, threshDb / 20.0);
    const double target = (env_ >= threshLin) ? 1.0 : 0.0;
    const double gCoeff = (target > gain_) ? std::exp(-1.0 / (0.002 * sampleRate_))
                                           : std::exp(-1.0 / (0.045 * sampleRate_));
    gain_ = gCoeff * gain_ + (1.0 - gCoeff) * target;
    gateGain_.store(static_cast<float>(gain_), std::memory_order_relaxed);
    return x * gain_;
}

// ==============================================================================
// StrobeTuner64 (Zero-allocation YIN-inspired autocorrelation)
// ==============================================================================
void StrobeTuner64::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(sampleRate, 8000.0);
    reset();
}

void StrobeTuner64::reset() noexcept
{
    ring_.fill(0.0);
    writeIdx_   = 0;
    hopCounter_ = 0;
    freqHz_.store(0.0f, std::memory_order_relaxed);
    midiNote_.store(-1, std::memory_order_relaxed);
    cents_.store(0.0f, std::memory_order_relaxed);
    confidence_.store(0.0f, std::memory_order_relaxed);
}

void StrobeTuner64::pushSample(double x) noexcept
{
    ring_[static_cast<size_t>(writeIdx_)] = x;
    writeIdx_ = (writeIdx_ + 1) % kBufSize;
    if (++hopCounter_ >= 1024)
    {
        hopCounter_ = 0;
        analyzePitch();
    }
}

void StrobeTuner64::analyzePitch() noexcept
{
    // Compute signal RMS first
    double rmsSq = 0.0;
    for (int i = 0; i < kBufSize; ++i)
        rmsSq += ring_[static_cast<size_t>(i)] * ring_[static_cast<size_t>(i)];
    const double rms = std::sqrt(rmsSq / static_cast<double>(kBufSize));
    if (rms < 1.0e-4)
    {
        confidence_.store(0.0f, std::memory_order_relaxed);
        return;
    }

    // Bass range: 27 Hz (A0) to 400 Hz (G4)
    const int minLag = std::max(16, static_cast<int>(sampleRate_ / 400.0));
    const int maxLag = std::min(kBufSize / 2 - 2, static_cast<int>(sampleRate_ / 27.0));
    if (maxLag <= minLag)
        return;

    const int halfN = kBufSize / 2;
    double bestTau = -1.0;
    double bestDiff = 1.0e9;

    // Difference function d(tau) normalized by running mean (YIN core)
    double runningSum = 0.0;
    double prevCmndf  = 1.0;
    double currCmndf  = 1.0;

    for (int tau = 1; tau <= maxLag; ++tau)
    {
        double diff = 0.0;
        for (int j = 0; j < halfN; ++j)
        {
            const int i1 = (writeIdx_ + j) % kBufSize;
            const int i2 = (writeIdx_ + j + tau) % kBufSize;
            const double d = ring_[static_cast<size_t>(i1)] - ring_[static_cast<size_t>(i2)];
            diff += d * d;
        }
        runningSum += diff;
        const double cmndf = (runningSum > 1.0e-12) ? (diff * tau / runningSum) : 1.0;

        if (tau >= minLag)
        {
            if (cmndf < bestDiff)
            {
                bestDiff = cmndf;
                bestTau  = static_cast<double>(tau);
            }
            if (cmndf < 0.15 && prevCmndf < cmndf && bestTau > 0.0)
                break; // First strong local minimum
        }
        prevCmndf = currCmndf;
        currCmndf = cmndf;
    }

    if (bestTau <= 0.0 || bestDiff > 0.28)
    {
        confidence_.store(0.0f, std::memory_order_relaxed);
        return;
    }

    const double hz = sampleRate_ / bestTau;
    const double midiFloat = 69.0 + 12.0 * std::log2(hz / 440.0);
    const int nearestMidi  = static_cast<int>(std::round(midiFloat));
    const float cents      = static_cast<float>((midiFloat - static_cast<double>(nearestMidi)) * 100.0);

    freqHz_.store(static_cast<float>(hz), std::memory_order_relaxed);
    midiNote_.store(nearestMidi, std::memory_order_relaxed);
    cents_.store(cents, std::memory_order_relaxed);
    confidence_.store(static_cast<float>(std::clamp(1.0 - bestDiff * 2.5, 0.0, 1.0)), std::memory_order_relaxed);
}

// ==============================================================================
// BrickwallLimiter64
// ==============================================================================
void BrickwallLimiter64::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(sampleRate, 8000.0);
    reset();
}

void BrickwallLimiter64::reset() noexcept
{
    envGain_ = 1.0;
    grDb_.store(0.0f, std::memory_order_relaxed);
}

void BrickwallLimiter64::processStereo(double& l, double& r, bool enabled, double ceilingDb) noexcept
{
    if (!enabled)
    {
        envGain_ = 1.0;
        grDb_.store(0.0f, std::memory_order_relaxed);
        return;
    }

    const double ceilLin = std::pow(10.0, std::clamp(ceilingDb, -24.0, 0.0) / 20.0);
    const double peak    = std::max(std::abs(l), std::abs(r));
    const double reqGain = (peak > ceilLin && peak > 1.0e-9) ? (ceilLin / peak) : 1.0;

    if (reqGain < envGain_)
    {
        envGain_ = reqGain; // Zero-latency instant clamp
    }
    else
    {
        const double rel = std::exp(-1.0 / (0.080 * sampleRate_));
        envGain_ = rel * envGain_ + (1.0 - rel) * reqGain;
    }

    l *= envGain_;
    r *= envGain_;

    // Hard safety clamp at ceiling
    l = std::clamp(l, -ceilLin, ceilLin);
    r = std::clamp(r, -ceilLin, ceilLin);

    const float gr = static_cast<float>(20.0 * std::log10(std::max(envGain_, 1.0e-4)));
    grDb_.store(gr, std::memory_order_relaxed);
}

// ==============================================================================
// SmartKickRelief64
// ==============================================================================
void SmartKickRelief64::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(sampleRate, 8000.0);
    reset();
}

void SmartKickRelief64::reset() noexcept
{
    scEnv_ = 0.0;
    dynBellL_.reset();
    dynBellR_.reset();
    duckDb_.store(0.0f, std::memory_order_relaxed);
}

void SmartKickRelief64::processStereo(double& l, double& r, double sidechainSample, bool enabled, double amountPct) noexcept
{
    if (!enabled || amountPct <= 0.5)
    {
        duckDb_.store(0.0f, std::memory_order_relaxed);
        return;
    }

    const double absSc = std::abs(sidechainSample);
    const double att   = std::exp(-1.0 / (0.002 * sampleRate_));
    const double rel   = std::exp(-1.0 / (0.090 * sampleRate_));
    scEnv_ = (absSc > scEnv_) ? (att * scEnv_ + (1.0 - att) * absSc)
                              : (rel * scEnv_ + (1.0 - rel) * absSc);

    const double amt = std::clamp(amountPct / 100.0, 0.0, 1.0);
    const double duckDb = -std::min(12.0, scEnv_ * 24.0 * amt);
    duckDb_.store(static_cast<float>(duckDb), std::memory_order_relaxed);

    if (duckDb < -0.05)
    {
        dynBellL_.setBell(sampleRate_, 65.0, 1.1, duckDb);
        dynBellR_.setBell(sampleRate_, 65.0, 1.1, duckDb);
        l = dynBellL_.processSample(l);
        r = dynBellR_.processSample(r);
    }
}

} // namespace kalyx::bass::dsp
