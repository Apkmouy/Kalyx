#include "PedalboardEngine.h"
#include <algorithm>
#include <cmath>

namespace kalyx::bass::fx
{

namespace
{
constexpr double kTwoPi = 6.28318530717958647692;
}

double PedalboardEngine::divisionToMs(int divIdx, double bpm) noexcept
{
    const double safeBpm = std::clamp(bpm, 30.0, 300.0);
    const double quarterMs = 60000.0 / safeBpm;
    switch (std::clamp(divIdx, 0, 3))
    {
        case 0: return quarterMs;         // 1/4
        case 1: return quarterMs * 0.5;   // 1/8
        case 2: return quarterMs * 0.75;  // 1/8D (dotted eighth)
        case 3: return quarterMs * 0.25;  // 1/16
        default: return quarterMs;
    }
}

void PedalboardEngine::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(sampleRate, 8000.0);
    reset();
}

void PedalboardEngine::reset() noexcept
{
    compEnv_ = 0.0;
    compGrDb_.store(0.0f, std::memory_order_relaxed);

    octTrackLPF_.reset(); octSub1LPF_.reset(); octSub2LPF_.reset(); octUpHPF_.reset();
    octPrev_ = 0.0; octFF1_ = 1.0; octFF2_ = 1.0; octEnv_ = 0.0;

    envFollow_ = 0.0;
    envBP_L_.reset(); envBP_R_.reset();

    tdrvSatL_.reset(0.0); tdrvSatR_.reset(0.0);
    tdrvToneL_.reset(); tdrvToneR_.reset();

    ratHpfL_.reset(); ratHpfR_.reset();
    ratFilterL_.reset(); ratFilterR_.reset();
    ratSatL_.reset(0.0); ratSatR_.reset(0.0);

    fuzzSatL_.reset(0.0); fuzzSatR_.reset(0.0);
    fuzzScoopL_.reset(); fuzzScoopR_.reset();
    fuzzLowL_.reset(); fuzzLowR_.reset();
    fuzzTiltL_.reset(); fuzzTiltR_.reset();

    choXoverLP_L_.reset(); choXoverLP_R_.reset();
    choXoverHP_L_.reset(); choXoverHP_R_.reset();
    choBufL_.fill(0.0); choBufR_.fill(0.0);
    choWrite_ = 0; choPhase_ = 0.0;

    phaPhase_ = 0.0;
    phaFbStateL_ = 0.0; phaFbStateR_ = 0.0;
    phaApL_.fill(0.0); phaApR_.fill(0.0);

    tapeBufL_.fill(0.0); tapeBufR_.fill(0.0);
    tapeWrite_ = 0; tapeWowPhase_ = 0.0;
    tapeSatL_.reset(0.0); tapeSatR_.reset(0.0);
    tapeWarmL_.reset(); tapeWarmR_.reset();

    for (int i = 0; i < 4; ++i) { eq4L_[i].reset(); eq4R_[i].reset(); }
    monoLP_L_.reset(); monoLP_R_.reset(); monoHP_L_.reset(); monoHP_R_.reset();
}

void PedalboardEngine::setState(const PedalboardState& st, const HostSyncInfo& sync) noexcept
{
    state_ = st;
    sync_  = sync;

    // Build sorted PRE and POST chains (zero allocation)
    std::array<int, NumPedals> order { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    std::sort(order.begin(), order.end(), [&](int a, int b)
    {
        return state_.pos[static_cast<size_t>(a)] < state_.pos[static_cast<size_t>(b)];
    });

    preCount_  = 0;
    postCount_ = 0;
    for (int id : order)
    {
        if (state_.isPost[static_cast<size_t>(id)])
            postChain_[static_cast<size_t>(postCount_++)] = id;
        else
            preChain_[static_cast<size_t>(preCount_++)] = id;
    }

    // Update filter coefficients
    octTrackLPF_.setLowPass(sampleRate_, 260.0, 0.707);
    octSub1LPF_.setLowPass(sampleRate_, 180.0, 0.707);
    octSub2LPF_.setLowPass(sampleRate_, 110.0, 0.707);
    octUpHPF_.setHighPass(sampleRate_, 180.0, 0.707);

    const double tdrvHz = 1200.0 + (state_.tdrvTone / 100.0) * 6500.0;
    tdrvToneL_.setCutoffHz(sampleRate_, tdrvHz);
    tdrvToneR_.setCutoffHz(sampleRate_, tdrvHz);

    ratHpfL_.setHighPass(sampleRate_, 140.0, 0.707);
    ratHpfR_.setHighPass(sampleRate_, 140.0, 0.707);
    // ProCo Rat inverted FILTER: higher value = darker
    const double ratFltHz = 9000.0 * std::pow(750.0 / 9000.0, std::clamp(state_.ratFlt / 100.0, 0.0, 1.0));
    ratFilterL_.setCutoffHz(sampleRate_, ratFltHz);
    ratFilterR_.setCutoffHz(sampleRate_, ratFltHz);

    fuzzScoopL_.setBell(sampleRate_, 750.0, 0.9, -7.5);
    fuzzScoopR_.setBell(sampleRate_, 750.0, 0.9, -7.5);
    fuzzLowL_.setLowShelf(sampleRate_, 85.0, 0.707, (state_.fuzzLow / 100.0) * 6.0);
    fuzzLowR_.setLowShelf(sampleRate_, 85.0, 0.707, (state_.fuzzLow / 100.0) * 6.0);
    const double tiltDb = (state_.fuzzTilt / 100.0) * 6.0;
    fuzzTiltL_.setHighShelf(sampleRate_, 1200.0, 0.55, tiltDb);
    fuzzTiltR_.setHighShelf(sampleRate_, 1200.0, 0.55, tiltDb);

    // Dimension Chorus 200 Hz crossover (LOW RETAIN)
    choXoverLP_L_.setLowPass(sampleRate_, 200.0, 0.707);
    choXoverLP_R_.setLowPass(sampleRate_, 200.0, 0.707);
    choXoverHP_L_.setHighPass(sampleRate_, 200.0, 0.707);
    choXoverHP_R_.setHighPass(sampleRate_, 200.0, 0.707);

    tapeWarmL_.setCutoffHz(sampleRate_, 3400.0);
    tapeWarmR_.setCutoffHz(sampleRate_, 3400.0);

    // Studio Bass EQ 4 bands + Mono Maker
    constexpr double kEqFreqs[4] = { 60.0, 250.0, 1200.0, 4500.0 };
    const double eqGains[4] = { state_.eq4B1, state_.eq4B2, state_.eq4B3, state_.eq4B4 };
    for (int i = 0; i < 4; ++i)
    {
        if (i == 0)
        {
            eq4L_[i].setLowShelf(sampleRate_, kEqFreqs[i], 0.707, eqGains[i]);
            eq4R_[i].setLowShelf(sampleRate_, kEqFreqs[i], 0.707, eqGains[i]);
        }
        else if (i == 3)
        {
            eq4L_[i].setHighShelf(sampleRate_, kEqFreqs[i], 0.707, eqGains[i]);
            eq4R_[i].setHighShelf(sampleRate_, kEqFreqs[i], 0.707, eqGains[i]);
        }
        else
        {
            eq4L_[i].setBell(sampleRate_, kEqFreqs[i], 1.1, eqGains[i]);
            eq4R_[i].setBell(sampleRate_, kEqFreqs[i], 1.1, eqGains[i]);
        }
    }
    const double monoHz = std::clamp(state_.eq4MonoHz, 20.0, 250.0);
    monoLP_L_.setLowPass(sampleRate_, monoHz, 0.707);
    monoLP_R_.setLowPass(sampleRate_, monoHz, 0.707);
    monoHP_L_.setHighPass(sampleRate_, monoHz, 0.707);
    monoHP_R_.setHighPass(sampleRate_, monoHz, 0.707);
}

void PedalboardEngine::processPre(double* monoBuf, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        double l = monoBuf[i];
        double r = l;
        for (int p = 0; p < preCount_; ++p)
            processPedalStereo(preChain_[static_cast<size_t>(p)], l, r);
        monoBuf[i] = 0.5 * (l + r);
    }
}

void PedalboardEngine::processPost(double* bufL, double* bufR, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        double l = bufL[i];
        double r = bufR[i];
        for (int p = 0; p < postCount_; ++p)
            processPedalStereo(postChain_[static_cast<size_t>(p)], l, r);
        bufL[i] = l;
        bufR[i] = r;
    }
}

void PedalboardEngine::processPedalStereo(int pedalIdx, double& l, double& r) noexcept
{
    switch (pedalIdx)
    {
        case Pedal_OptoFetComp:  runComp(l, r); break;
        case Pedal_Octaver:      runOctaver(l, r); break;
        case Pedal_EnvFilter:    runEnvFilter(l, r); break;
        case Pedal_TransDrive:   runTransDrive(l, r); break;
        case Pedal_RodentBass:   runRodentBass(l, r); break;
        case Pedal_BigWallFuzz:  runBigWallFuzz(l, r); break;
        case Pedal_DimChorus:    runDimChorus(l, r); break;
        case Pedal_AnalogPhaser: runAnalogPhaser(l, r); break;
        case Pedal_TapeEcho:     runTapeEcho(l, r); break;
        case Pedal_StudioEQ:     runStudioEQ(l, r); break;
        default: break;
    }
}

void PedalboardEngine::runComp(double& l, double& r) noexcept
{
    if (!state_.compEn || (std::abs(l) < 1.0e-7 && std::abs(r) < 1.0e-7))
    {
        compGrDb_.store(0.0f, std::memory_order_relaxed);
        return;
    }
    const bool isFet = (state_.compMode == 1);
    const double attMs = isFet ? 0.4 : 12.0;
    const double relMs = isFet ? 45.0 : 180.0;
    const double att = std::exp(-1.0 / (attMs * 0.001 * sampleRate_));
    const double rel = std::exp(-1.0 / (relMs * 0.001 * sampleRate_));

    const double det = std::max(std::abs(l), std::abs(r));
    compEnv_ = (det > compEnv_) ? (att * compEnv_ + (1.0 - att) * det)
                                : (rel * compEnv_ + (1.0 - rel) * det);

    const double amt = std::clamp(state_.compAmt / 100.0, 0.0, 1.0);
    const double thresh = 0.50 - 0.42 * amt;
    double gain = 1.0;
    if (compEnv_ > thresh)
    {
        const double ratio = isFet ? 8.0 : 3.5;
        gain = std::pow(thresh / std::max(compEnv_, 1.0e-7), 1.0 - 1.0 / ratio);
        gain *= (1.0 + (isFet ? 0.55 : 0.40) * amt);
    }
    const double blend = std::clamp(state_.compBlend / 100.0, 0.0, 1.0);
    l = l * (1.0 - blend) + (l * gain) * blend;
    r = r * (1.0 - blend) + (r * gain) * blend;
    compGrDb_.store(static_cast<float>(20.0 * std::log10(std::clamp(gain, 1.0e-3, 1.0))), std::memory_order_relaxed);
}

void PedalboardEngine::runOctaver(double& l, double& r) noexcept
{
    if (!state_.octEn || (std::abs(l) < 1.0e-7 && std::abs(r) < 1.0e-7))
        return;

    const double mono = 0.5 * (l + r);
    const double tracked = octTrackLPF_.processSample(mono);
    const double envC = std::exp(-1.0 / (0.012 * sampleRate_));
    octEnv_ = envC * octEnv_ + (1.0 - envC) * std::abs(tracked);

    if (octPrev_ <= 0.0 && tracked > 0.0)
    {
        octFF1_ = -octFF1_;
        if (octFF1_ > 0.0)
            octFF2_ = -octFF2_;
    }
    octPrev_ = tracked;

    const double sub1 = octSub1LPF_.processSample(octFF1_ * octEnv_ * 1.3);
    const double sub2 = octSub2LPF_.processSample(octFF2_ * octEnv_ * 1.4);
    const double octU = octUpHPF_.processSample(std::abs(mono) - octEnv_ * 0.5);

    const double cMix  = state_.octClean / 100.0;
    const double s1Mix = state_.octSub1 / 100.0;
    const double s2Mix = state_.octSub2 / 100.0;
    const double uMix  = state_.octUp / 100.0;

    const double wet = sub1 * s1Mix + sub2 * s2Mix + octU * uMix;
    l = l * cMix + wet;
    r = r * cMix + wet;
}

void PedalboardEngine::runEnvFilter(double& l, double& r) noexcept
{
    if (!state_.envEn || (std::abs(l) < 1.0e-7 && std::abs(r) < 1.0e-7))
        return;

    const double det = std::max(std::abs(l), std::abs(r));
    const double att = std::exp(-1.0 / (std::max(1.0, state_.envAtt) * 0.001 * sampleRate_));
    const double dec = std::exp(-1.0 / (std::max(10.0, state_.envDec) * 0.001 * sampleRate_));
    envFollow_ = (det > envFollow_) ? (att * envFollow_ + (1.0 - att) * det)
                                    : (dec * envFollow_ + (1.0 - dec) * det);

    const double sens = (state_.envSens / 100.0) * 6.0;
    const double mod  = std::clamp(envFollow_ * sens, 0.0, 1.0);
    const double fc   = 110.0 * std::pow(2600.0 / 110.0, mod);
    const double q    = std::clamp(state_.envQ, 0.8, 10.0);

    envBP_L_.setBell(sampleRate_, fc, q, 11.0);
    envBP_R_.setBell(sampleRate_, fc, q, 11.0);
    l = envBP_L_.processSample(l);
    r = envBP_R_.processSample(r);
}

void PedalboardEngine::runTransDrive(double& l, double& r) noexcept
{
    if (!state_.tdrvEn || (std::abs(l) < 1.0e-7 && std::abs(r) < 1.0e-7))
        return;

    const double drv = 1.0 + (state_.tdrvDrv / 100.0) * 5.5;
    const double dryBlend = std::clamp(state_.tdrvBlend / 100.0, 0.0, 1.0);

    double wl = tdrvSatL_.processADAA2(l, drv, 0.14);
    double wr = tdrvSatR_.processADAA2(r, drv, 0.14);
    wl = tdrvToneL_.processLP(wl);
    wr = tdrvToneR_.processLP(wr);

    l = l * dryBlend + wl * (1.0 - dryBlend * 0.5);
    r = r * dryBlend + wr * (1.0 - dryBlend * 0.5);
}

void PedalboardEngine::runRodentBass(double& l, double& r) noexcept
{
    if (!state_.ratEn || (std::abs(l) < 1.0e-7 && std::abs(r) < 1.0e-7))
        return;

    const double drv = 1.5 + (state_.ratDist / 100.0) * 9.0;
    const double cleanBlend = std::clamp(state_.ratBlend / 100.0, 0.0, 1.0);

    // HPF on distortion path so sub-bass isn't crushed
    double wl = ratHpfL_.processSample(l);
    double wr = ratHpfR_.processSample(r);
    wl = ratSatL_.processADAA2(wl, drv, 0.06);
    wr = ratSatR_.processADAA2(wr, drv, 0.06);
    wl = ratFilterL_.processLP(wl);
    wr = ratFilterR_.processLP(wr);

    l = l * cleanBlend + wl * (1.0 - cleanBlend * 0.35);
    r = r * cleanBlend + wr * (1.0 - cleanBlend * 0.35);
}

void PedalboardEngine::runBigWallFuzz(double& l, double& r) noexcept
{
    if (!state_.fuzzEn || (std::abs(l) < 1.0e-7 && std::abs(r) < 1.0e-7))
        return;

    const double drv = 2.5 + (state_.fuzzSust / 100.0) * 14.0;
    double wl = fuzzSatL_.processADAA2(l, drv, 0.0);
    double wr = fuzzSatR_.processADAA2(r, drv, 0.0);

    wl = fuzzScoopL_.processSample(wl);
    wr = fuzzScoopR_.processSample(wr);
    wl = fuzzTiltL_.processSample(wl);
    wr = fuzzTiltR_.processSample(wr);
    wl = fuzzLowL_.processSample(wl);
    wr = fuzzLowR_.processSample(wr);

    l = wl * 0.85;
    r = wr * 0.85;
}

void PedalboardEngine::runDimChorus(double& l, double& r) noexcept
{
    if (!state_.choEn)
        return;

    const double lowL = choXoverLP_L_.processSample(l);
    const double lowR = choXoverLP_R_.processSample(r);
    const double hiL  = choXoverHP_L_.processSample(l);
    const double hiR  = choXoverHP_R_.processSample(r);

    choBufL_[static_cast<size_t>(choWrite_)] = hiL;
    choBufR_[static_cast<size_t>(choWrite_)] = hiR;

    choPhase_ += kTwoPi * std::clamp(state_.choRate, 0.05, 6.0) / sampleRate_;
    if (choPhase_ >= kTwoPi) choPhase_ -= kTwoPi;

    const double depthMs = (state_.choDepth / 100.0) * 6.5;
    const double dL = (8.0 + depthMs * std::sin(choPhase_)) * 0.001 * sampleRate_;
    const double dR = (8.0 + depthMs * std::cos(choPhase_)) * 0.001 * sampleRate_;

    auto readInterp = [&](const std::array<double, kMaxChorusDelay>& buf, double delaySamp) noexcept -> double
    {
        const double d = std::clamp(delaySamp, 1.0, static_cast<double>(kMaxChorusDelay - 2));
        const int i0 = static_cast<int>(d);
        const double frac = d - static_cast<double>(i0);
        int idx0 = choWrite_ - i0; if (idx0 < 0) idx0 += kMaxChorusDelay;
        int idx1 = idx0 - 1;       if (idx1 < 0) idx1 += kMaxChorusDelay;
        return buf[static_cast<size_t>(idx0)] * (1.0 - frac) + buf[static_cast<size_t>(idx1)] * frac;
    };

    const double wetL = readInterp(choBufL_, dL);
    const double wetR = readInterp(choBufR_, dR);
    choWrite_ = (choWrite_ + 1) % kMaxChorusDelay;

    const double mix = std::clamp(state_.choMix / 100.0, 0.0, 1.0);
    // Low spectrum (< 200 Hz) remains rock-solid mono!
    const double lowMono = 0.5 * (lowL + lowR);
    l = lowMono + hiL * (1.0 - 0.5 * mix) + wetL * mix;
    r = lowMono + hiR * (1.0 - 0.5 * mix) + wetR * mix;
}

void PedalboardEngine::runAnalogPhaser(double& l, double& r) noexcept
{
    if (!state_.phaEn || (std::abs(l) < 1.0e-7 && std::abs(r) < 1.0e-7))
        return;

    const double rateHz = state_.phaSync
                              ? (1000.0 / std::max(10.0, divisionToMs(state_.phaDiv, sync_.bpm)))
                              : std::clamp(state_.phaRate, 0.05, 8.0);
    phaPhase_ += kTwoPi * rateHz / sampleRate_;
    if (phaPhase_ >= kTwoPi) phaPhase_ -= kTwoPi;

    const double lfo = 0.5 + 0.5 * std::sin(phaPhase_);
    const double depth = std::clamp(state_.phaDepth / 100.0, 0.0, 1.0);
    const double minF = 180.0;
    const double maxF = 180.0 + depth * 3200.0;
    const double fc   = minF * std::pow(maxF / minF, lfo);
    const double tanW = std::tan(3.14159265358979323846 * std::clamp(fc, 40.0, 0.45 * sampleRate_) / sampleRate_);
    const double a1   = (tanW - 1.0) / (tanW + 1.0);

    const int stages = (state_.phaStages == 1) ? 8 : 4;
    const double fb  = std::clamp(state_.phaFb / 100.0, 0.0, 0.90);

    double yl = l + phaFbStateL_ * fb;
    double yr = r + phaFbStateR_ * fb;
    for (int s = 0; s < stages; ++s)
    {
        const double outL = a1 * yl + phaApL_[static_cast<size_t>(s)];
        phaApL_[static_cast<size_t>(s)] = yl - a1 * outL;
        yl = outL;

        const double outR = a1 * yr + phaApR_[static_cast<size_t>(s)];
        phaApR_[static_cast<size_t>(s)] = yr - a1 * outR;
        yr = outR;
    }
    phaFbStateL_ = std::isfinite(yl) ? yl : 0.0;
    phaFbStateR_ = std::isfinite(yr) ? yr : 0.0;

    l = 0.5 * (l + phaFbStateL_);
    r = 0.5 * (r + phaFbStateR_);
}

void PedalboardEngine::runTapeEcho(double& l, double& r) noexcept
{
    if (!state_.tapeEn)
        return;

    const double baseMs = state_.tapeSync ? divisionToMs(state_.tapeDiv, sync_.bpm)
                                          : std::clamp(state_.tapeTimeMs, 40.0, 1200.0);
    // Wow & Flutter slow LFO
    tapeWowPhase_ += kTwoPi * 0.42 / sampleRate_;
    if (tapeWowPhase_ >= kTwoPi) tapeWowPhase_ -= kTwoPi;
    const double wowMs = 1.8 * std::sin(tapeWowPhase_);

    const double dSamp = std::clamp((baseMs + wowMs) * 0.001 * sampleRate_, 1.0, static_cast<double>(kMaxTapeDelay - 2));
    const int i0 = static_cast<int>(dSamp);
    const double frac = dSamp - static_cast<double>(i0);
    int idx0 = tapeWrite_ - i0; if (idx0 < 0) idx0 += kMaxTapeDelay;
    int idx1 = idx0 - 1;       if (idx1 < 0) idx1 += kMaxTapeDelay;

    const double wetL = tapeBufL_[static_cast<size_t>(idx0)] * (1.0 - frac) + tapeBufL_[static_cast<size_t>(idx1)] * frac;
    const double wetR = tapeBufR_[static_cast<size_t>(idx0)] * (1.0 - frac) + tapeBufR_[static_cast<size_t>(idx1)] * frac;

    const double rep = std::clamp(state_.tapeRep / 100.0, 0.0, 0.92);
    double fbL = tapeWarmL_.processLP(tapeSatL_.processADAA1(l + wetL * rep, 1.35, 0.0));
    double fbR = tapeWarmR_.processLP(tapeSatR_.processADAA1(r + wetR * rep, 1.35, 0.0));

    tapeBufL_[static_cast<size_t>(tapeWrite_)] = std::isfinite(fbL) ? fbL : 0.0;
    tapeBufR_[static_cast<size_t>(tapeWrite_)] = std::isfinite(fbR) ? fbR : 0.0;
    tapeWrite_ = (tapeWrite_ + 1) % kMaxTapeDelay;

    const double mix = std::clamp(state_.tapeMix / 100.0, 0.0, 1.0);
    l = l + wetL * mix;
    r = r + wetR * mix;
}

void PedalboardEngine::runStudioEQ(double& l, double& r) noexcept
{
    if (!state_.eq4En || (std::abs(l) < 1.0e-7 && std::abs(r) < 1.0e-7))
        return;

    for (int i = 0; i < 4; ++i)
    {
        l = eq4L_[i].processSample(l);
        r = eq4R_[i].processSample(r);
    }

    // Mono Maker below monoHz
    const double lowMono = 0.5 * (monoLP_L_.processSample(l) + monoLP_R_.processSample(r));
    l = lowMono + monoHP_L_.processSample(l);
    r = lowMono + monoHP_R_.processSample(r);
}

} // namespace kalyx::bass::fx
