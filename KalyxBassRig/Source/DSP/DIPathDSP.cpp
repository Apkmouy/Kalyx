#include "DIPathDSP.h"
#include <algorithm>
#include <cmath>

namespace kalyx::bass::dsp
{

void DIPathEngine::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(sampleRate, 8000.0);
    reset();
    updateFilters();
}

void DIPathEngine::reset() noexcept
{
    hpf1_.reset(); hpf2_.reset();
    lpf1_.reset(); lpf2_.reset();
    transfoPunch_.reset();
    neveWeight_.reset();
    subLPF1_.reset(); subLPF2_.reset();
    dcBlocker_.reset();
    satEven_.reset(0.0);
    satTransfo_.reset(0.0);
    subPrevX_    = 0.0;
    subFlipFlop_ = 1.0;
    subEnv_      = 0.0;
    optoEnv_     = 0.0;
    optoGrDb_.store(0.0f, std::memory_order_relaxed);
    delayBuf_.fill(0.0);
    writeIdx_ = 0;
}

void DIPathEngine::setControlState(const DIControlState& state) noexcept
{
    state_ = state;
    updateFilters();
}

void DIPathEngine::setAutoPhaseDelaySamples(int delaySamples) noexcept
{
    targetDelaySamp_ = std::clamp(delaySamples, 0, kMaxDelaySamples - 1);
}

void DIPathEngine::updateFilters() noexcept
{
    const double hp = std::clamp(state_.hpfFreqHz, 20.0, 120.0);
    const double lp = std::clamp(state_.lpfFreqHz, std::max(60.0, hp + 20.0), 5000.0);

    // 24 dB/oct Linkwitz-Riley / Butterworth cascade
    hpf1_.setHighPass(sampleRate_, hp, 0.5411961);
    hpf2_.setHighPass(sampleRate_, hp, 1.3065630);
    lpf1_.setLowPass(sampleRate_, lp, 0.5411961);
    lpf2_.setLowPass(sampleRate_, lp, 1.3065630);

    transfoPunch_.setBell(sampleRate_, 115.0, 1.1, 2.8);
    neveWeight_.setLowShelf(sampleRate_, 85.0, 0.707, 3.2);
    subLPF1_.setLowPass(sampleRate_, 110.0, 0.707);
    subLPF2_.setLowPass(sampleRate_, 110.0, 0.707);
    dcBlocker_.setCutoffHz(sampleRate_, 15.0);
}

double DIPathEngine::processSample(double x) noexcept
{
    // Push into delay line first (or process then delay)
    double sig = x;

    if (std::abs(sig) < 1.0e-7)
    {
        optoGrDb_.store(0.0f, std::memory_order_relaxed);
        sig = 0.0;
    }
    else if (state_.enabled)
    {
        // 1. DI Circuit Coloration
        switch (state_.circuit)
        {
            case DICircuit::Transparent:
                break;

            case DICircuit::TubeDI:
                // Soft even-harmonic tube DI via 64-bit ADAA1 with gentle asymmetric bias
                sig = satEven_.processADAA1(sig, 1.65, 0.10);
                break;

            case DICircuit::American2503:
                // Punchy American 2503 transformer
                sig = transfoPunch_.processSample(sig);
                sig = satTransfo_.processADAA2(sig, 1.85, 0.02);
                break;

            case DICircuit::ClassA73:
                // Class-A 73 Preamp (analog thickness + transformer weight)
                sig = neveWeight_.processSample(sig);
                sig = satEven_.processADAA2(sig, 2.05, 0.08);
                break;

            case DICircuit::SubHarmonic:
            {
                // Dynamic octave-down sub-harmonic synthesizer (zero-allocation)
                const double absS = std::abs(sig);
                const double envC = std::exp(-1.0 / (0.015 * sampleRate_));
                subEnv_ = envC * subEnv_ + (1.0 - envC) * absS;

                if (subPrevX_ <= 0.0 && sig > 0.0)
                    subFlipFlop_ = -subFlipFlop_;
                subPrevX_ = sig;

                double sub = subFlipFlop_ * subEnv_ * 1.35;
                sub = subLPF1_.processSample(sub);
                sub = subLPF2_.processSample(sub);
                const double mix = std::clamp(state_.subMixPct / 100.0, 0.0, 1.0);
                sig = sig * (1.0 - 0.35 * mix) + sub * mix;
                sig = satEven_.processADAA1(sig, 1.25, 0.04);
                break;
            }
        }

        // 2. 24 dB/oct HPF & LPF
        sig = hpf1_.processSample(sig);
        sig = hpf2_.processSample(sig);
        sig = lpf1_.processSample(sig);
        sig = lpf2_.processSample(sig);
        sig = dcBlocker_.processHP(sig);

        // 3. DI Opto Compressor
        const double compAmt = std::clamp(state_.compPct / 100.0, 0.0, 1.0);
        if (compAmt > 0.01)
        {
            const double absS = std::abs(sig);
            const double att = std::exp(-1.0 / (0.010 * sampleRate_));
            const double rel = std::exp(-1.0 / (0.160 * sampleRate_));
            optoEnv_ = (absS > optoEnv_) ? (att * optoEnv_ + (1.0 - att) * absS)
                                         : (rel * optoEnv_ + (1.0 - rel) * absS);

            const double thresh = 0.45 - 0.35 * compAmt; // ~ -7 to -20 dB
            if (optoEnv_ > thresh)
            {
                const double ratioInv = 1.0 / (1.0 + compAmt * 5.0);
                const double gain = std::pow(thresh / std::max(optoEnv_, 1.0e-7), 1.0 - ratioInv);
                sig *= gain * (1.0 + 0.45 * compAmt); // mild makeup
                const float grDb = static_cast<float>(20.0 * std::log10(std::max(gain, 1.0e-4)));
                optoGrDb_.store(grDb, std::memory_order_relaxed);
            }
            else
            {
                optoGrDb_.store(0.0f, std::memory_order_relaxed);
            }
        }
        else
        {
            optoGrDb_.store(0.0f, std::memory_order_relaxed);
        }
    }
    else
    {
        sig = 0.0;
        optoGrDb_.store(0.0f, std::memory_order_relaxed);
    }

    // 4. Write to delay ring & read at Auto-Phase offset
    delayBuf_[static_cast<size_t>(writeIdx_)] = std::isfinite(sig) ? sig : 0.0;
    const int d = state_.autoPhase ? targetDelaySamp_ : 0;
    int readIdx = writeIdx_ - d;
    if (readIdx < 0)
        readIdx += kMaxDelaySamples;
    writeIdx_ = (writeIdx_ + 1) % kMaxDelaySamples;

    return delayBuf_[static_cast<size_t>(readIdx)];
}

} // namespace kalyx::bass::dsp
