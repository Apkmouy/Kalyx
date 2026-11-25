#include "HarmonicSculptorDSP.h"
#include <algorithm>
#include <cmath>

namespace kalyx::consoles
{

namespace
{

// ln(cosh(x)) numériquement stable pour tout x réel
inline double stableLogCosh(double x) noexcept
{
    const double ax = std::abs(x);
    return ax + std::log1p(std::exp(-2.0 * ax)) - 0.6931471805599453;
}

// ADAA 1er ordre pour f(x) = tanh(x + bias) - tanh(bias) (harmoniques paires asymétriques)
inline double adaaAsymTanh(double x, double& prevX, double bias) noexcept
{
    const double tb = std::tanh(bias);
    auto f = [bias, tb](double v) noexcept { return std::tanh(v + bias) - tb; };
    auto F1 = [bias, tb](double v) noexcept { return stableLogCosh(v + bias) - v * tb; };

    const double dx = x - prevX;
    double y = 0.0;
    if (std::abs(dx) < 1.0e-4)
        y = f(0.5 * (x + prevX));
    else
        y = (F1(x) - F1(prevX)) / dx;

    prevX = x;
    return y;
}

// ADAA 1er ordre pour f(x) = tanh(x) (harmoniques impaires symétriques)
inline double adaaSymTanh(double x, double& prevX) noexcept
{
    const double dx = x - prevX;
    double y = 0.0;
    if (std::abs(dx) < 1.0e-4)
        y = std::tanh(0.5 * (x + prevX));
    else
        y = (stableLogCosh(x) - stableLogCosh(prevX)) / dx;

    prevX = x;
    return y;
}

} // namespace

void HarmonicSculptorDSP::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(sampleRate, 8000.0);
    reset();
}

void HarmonicSculptorDSP::reset() noexcept
{
    auto resetCh = [](ChannelState& c) noexcept
    {
        c.prevEvenIn = 0.0;
        c.prevOddIn  = 0.0;
        c.dcX1       = 0.0;
        c.dcY1       = 0.0;
        c.fastEnv    = 0.0;
        c.slowEnv    = 0.0;
        c.bandLp.reset();
        c.bandHp.reset();
        c.preTiltLo.reset();
        c.preTiltHi.reset();
        c.postTiltLo.reset();
        c.postTiltHi.reset();
    };
    resetCh(chA_);
    resetCh(chB_);
    thdSmooth_ = 0.0;
    instantThdPct_.store(0.0f, std::memory_order_relaxed);
}

double HarmonicSculptorDSP::processChannel(double x, ChannelState& st,
                                           double evenAmt, double oddAmt,
                                           double tiltDb, bool tiltLink,
                                           double dynAmt, double driveLin,
                                           int bandMode, double& outThdSample) noexcept
{
    // Séparation de bande (0=FULL, 1=LOW <250Hz, 2=MID, 3=HI >3kHz)
    double active = x;
    double cleanRest = 0.0;
    if (bandMode == 1) // LOW < 250 Hz
    {
        st.bandLp.setLowPass(sampleRate_, 250.0, 0.7071);
        active = static_cast<double>(st.bandLp.processSample(static_cast<float>(x)));
        cleanRest = x - active;
    }
    else if (bandMode == 2) // MID (250 Hz .. 3 kHz)
    {
        st.bandHp.setHighPass(sampleRate_, 250.0, 0.7071);
        st.bandLp.setLowPass(sampleRate_, 3000.0, 0.7071);
        const double hp = static_cast<double>(st.bandHp.processSample(static_cast<float>(x)));
        active = static_cast<double>(st.bandLp.processSample(static_cast<float>(hp)));
        cleanRest = x - active;
    }
    else if (bandMode == 3) // HI > 3 kHz
    {
        st.bandHp.setHighPass(sampleRate_, 3000.0, 0.7071);
        active = static_cast<double>(st.bandHp.processSample(static_cast<float>(x)));
        cleanRest = x - active;
    }

    // DYNAMICS / PUNCH (-50 % .. +50 %) : modulation de dynamique pré-saturation
    double dynScale = 1.0;
    if (std::abs(dynAmt) > 1.0e-4)
    {
        const double ax = std::abs(active);
        const double attF = std::exp(-1.0 / (0.0015 * sampleRate_));
        const double relF = std::exp(-1.0 / (0.0160 * sampleRate_));
        const double attS = std::exp(-1.0 / (0.0220 * sampleRate_));
        const double relS = std::exp(-1.0 / (0.1100 * sampleRate_));

        st.fastEnv = (ax > st.fastEnv) ? (attF * st.fastEnv + (1.0 - attF) * ax)
                                       : (relF * st.fastEnv + (1.0 - relF) * ax);
        st.slowEnv = (ax > st.slowEnv) ? (attS * st.slowEnv + (1.0 - attS) * ax)
                                       : (relS * st.slowEnv + (1.0 - relS) * ax);

        const double trans = std::clamp((st.fastEnv - st.slowEnv) / std::max(1.0e-6, st.slowEnv + 1.0e-4), 0.0, 2.0);
        dynScale = std::pow(10.0, (dynAmt * 10.0 * trans) / 20.0);
    }

    // TILT EQ pré-saturation
    double sig = active * dynScale;
    if (std::abs(tiltDb) > 0.02)
    {
        const double halfG = tiltDb * 0.5;
        st.preTiltLo.setLowShelf(sampleRate_, 800.0, 0.48, -halfG);
        st.preTiltHi.setHighShelf(sampleRate_, 800.0, 0.48, halfG);
        sig = static_cast<double>(st.preTiltHi.processSample(st.preTiltLo.processSample(static_cast<float>(sig))));
    }

    const double driven = sig * driveLin;

    // Générateurs harmoniques dissociés EVEN (2nd) & ODD (3rd) via ADAA 64-bit
    double satOut = driven;
    if (evenAmt > 1.0e-4 || oddAmt > 1.0e-4)
    {
        const double evenDrive = 0.5 + evenAmt * 2.2;
        const double evenRaw = adaaAsymTanh(driven * evenDrive, st.prevEvenIn, 0.42) / std::max(0.5, evenDrive * 0.85);

        const double oddDrive = 0.5 + oddAmt * 2.4;
        const double oddRaw = adaaSymTanh(driven * oddDrive, st.prevOddIn) / std::max(0.5, oddDrive * 0.90);

        const double totalMix = std::clamp(evenAmt + oddAmt, 0.0, 1.0);
        const double wSum = std::max(1.0e-6, evenAmt + oddAmt);
        const double blendedHarm = (evenRaw * evenAmt + oddRaw * oddAmt) / wSum;
        satOut = driven * (1.0 - totalMix) + blendedHarm * totalMix;
    }

    satOut /= std::max(0.25, driveLin * 0.72 + 0.28);

    // Filtre DC-block 1er ordre (fc ~ 12 Hz) pour éliminer tout offset asymétrique Even
    const double dcR = 1.0 - (2.0 * 3.14159265358979323846 * 12.0 / sampleRate_);
    const double dcOut = satOut - st.dcX1 + dcR * st.dcY1;
    st.dcX1 = satOut;
    st.dcY1 = dcOut;
    satOut = dcOut;

    // Compensation TILT LNK en sortie (filtre Tilt inverse)
    if (tiltLink && std::abs(tiltDb) > 0.02)
    {
        const double halfG = -tiltDb * 0.5;
        st.postTiltLo.setLowShelf(sampleRate_, 800.0, 0.48, -halfG);
        st.postTiltHi.setHighShelf(sampleRate_, 800.0, 0.48, halfG);
        satOut = static_cast<double>(st.postTiltHi.processSample(st.postTiltLo.processSample(static_cast<float>(satOut))));
    }

    const double diff = std::abs(satOut - active);
    const double ref  = std::max(1.0e-5, std::abs(active));
    outThdSample = std::clamp((diff / ref) * 100.0, 0.0, 25.0);

    return cleanRest + satOut;
}

void HarmonicSculptorDSP::processSample(float& l, float& r,
                                        const kalyx::core::SlotParamArray& p) noexcept
{
    const double dryL = static_cast<double>(l);
    const double dryR = static_cast<double>(r);

    if (p[0] <= 0.5f)
    {
        thdSmooth_ *= 0.999;
        instantThdPct_.store(static_cast<float>(thdSmooth_), std::memory_order_relaxed);
        return;
    }

    const bool   tiltLink = (p[1] > 0.5f);
    const double evenAmt  = std::clamp(static_cast<double>(p[2]), 0.0, 1.0);
    const double oddAmt   = std::clamp(static_cast<double>(p[3]), 0.0, 1.0);
    const double tiltDb   = kalyx::core::mapLin(p[4], -6.0f, 6.0f);
    const double dynAmt   = kalyx::core::mapLin(p[5], -0.5f, 0.5f);
    const double driveDb  = kalyx::core::mapLin(p[6], -12.0f, 24.0f);
    const double driveLin = std::pow(10.0, driveDb / 20.0);
    const double blend    = std::clamp(static_cast<double>(p[7]), 0.0, 1.0);
    const int    bandMode = std::clamp(static_cast<int>(std::round(p[8] * 3.0f)), 0, 3);
    const int    routing  = std::clamp(static_cast<int>(std::round(p[9] * 3.0f)), 0, 3); // 0=ST, 1=M/S, 2=MID, 3=SIDE

    double inA = dryL, inB = dryR;
    if (routing >= 1)
    {
        inA = (dryL + dryR) * 0.7071067811865475; // Mid
        inB = (dryL - dryR) * 0.7071067811865475; // Side
    }

    double thdA = 0.0, thdB = 0.0;
    double outA = inA, outB = inB;
    if (routing == 2) // MID ONLY
    {
        outA = processChannel(inA, chA_, evenAmt, oddAmt, tiltDb, tiltLink, dynAmt, driveLin, bandMode, thdA);
        thdB = thdA;
    }
    else if (routing == 3) // SIDE ONLY
    {
        outB = processChannel(inB, chB_, evenAmt, oddAmt, tiltDb, tiltLink, dynAmt, driveLin, bandMode, thdB);
        thdA = thdB;
    }
    else
    {
        outA = processChannel(inA, chA_, evenAmt, oddAmt, tiltDb, tiltLink, dynAmt, driveLin, bandMode, thdA);
        outB = processChannel(inB, chB_, evenAmt, oddAmt, tiltDb, tiltLink, dynAmt, driveLin, bandMode, thdB);
    }

    double wetL = outA, wetR = outB;
    if (routing >= 1)
    {
        wetL = (outA + outB) * 0.7071067811865475;
        wetR = (outA - outB) * 0.7071067811865475;
    }

    const double outL = dryL * (1.0 - blend) + wetL * blend;
    const double outR = dryR * (1.0 - blend) + wetR * blend;

    l = std::isfinite(outL) ? static_cast<float>(outL) : 0.0f;
    r = std::isfinite(outR) ? static_cast<float>(outR) : 0.0f;

    const double avgThd = (std::abs(dryL) + std::abs(dryR) > 1.0e-6) ? 0.5 * (thdA + thdB) : 0.0;
    const double coeff = std::exp(-1.0 / (0.045 * sampleRate_));
    thdSmooth_ = coeff * thdSmooth_ + (1.0 - coeff) * avgThd;
    instantThdPct_.store(static_cast<float>(thdSmooth_), std::memory_order_relaxed);
}

} // namespace kalyx::consoles
