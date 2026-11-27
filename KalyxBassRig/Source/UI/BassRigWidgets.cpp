#include "BassRigWidgets.h"
#include <algorithm>
#include <cmath>

namespace kalyx::bass::ui
{

// ==============================================================================
// StrobeTunerBar
// ==============================================================================
StrobeTunerBar::StrobeTunerBar(KalyxBassRigProcessor& proc)
    : proc_(proc)
{
}

void StrobeTunerBar::updateFromProcessor()
{
    midi_   = proc_.getTunerMidi();
    cents_  = proc_.getTunerCents();
    conf_   = proc_.getTunerConf();
    freqHz_ = proc_.getTunerFreq();
    if (conf_ > 0.25f)
        strobePhase_ = std::fmod(strobePhase_ + cents_ * 0.04f, 6.283185f);
    repaint();
}

void StrobeTunerBar::paint(juce::Graphics& g)
{
    const float sc = uiScale_;
    const auto r = getLocalBounds().toFloat().reduced(1.0f * sc);
    const auto bg = darkText_ ? KalyxTheme::dayControlIdleBg : KalyxTheme::nightChassisBg;
    const auto bd = darkText_ ? KalyxTheme::dayGridBorder : KalyxTheme::nightGridBorder;
    const auto tx = darkText_ ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary;

    g.setColour(bg);
    g.fillRoundedRectangle(r, 4.0f * sc);
    g.setColour(bd);
    g.drawRoundedRectangle(r, 4.0f * sc, 1.2f * sc);

    static const char* kNotes[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const bool active = (conf_ > 0.25f && midi_ >= 0);
    juce::String noteStr = "—";
    if (active)
    {
        const int noteIdx = ((midi_ % 12) + 12) % 12;
        const int oct = (midi_ / 12) - 1;
        noteStr = juce::String(kNotes[noteIdx]) + juce::String(oct);
    }

    const bool inTune = (active && std::abs(cents_) <= 3.0f);

    // Large bold Note badge on left
    auto noteR = r.withWidth(46.0f * sc).reduced(2.0f * sc);
    g.setColour(inTune ? KalyxTheme::jadeGreen.withAlpha(0.22f)
                       : (darkText_ ? KalyxTheme::dayStripBg : KalyxTheme::nightStripBg));
    g.fillRoundedRectangle(noteR, 3.0f * sc);
    g.setColour(inTune ? KalyxTheme::jadeGreen : (active ? tx : tx.withAlpha(0.45f)));
    g.setFont(KalyxTheme::makeMonoFont(15.0f * sc, juce::Font::bold));
    g.drawFittedText(noteStr, noteR.toNearestInt(), juce::Justification::centred, 1);

    // Precision horizontal cents gauge (-50 to +50)
    auto gaugeR = r.withTrimmedLeft(50.0f * sc).withTrimmedRight(56.0f * sc).reduced(2.0f * sc, 4.0f * sc);
    const float cx = gaugeR.getCentreX();
    const float trackY = gaugeR.getCentreY() + 2.0f * sc;

    // Recessed track line
    g.setColour(darkText_ ? KalyxTheme::dayKnobTrackBg : KalyxTheme::knobTrackBg);
    g.fillRoundedRectangle(gaugeR.getX(), trackY - 2.0f * sc, gaugeR.getWidth(), 4.0f * sc, 2.0f * sc);

    // Scale ticks (-50, -25, 0, +25, +50)
    for (int t = -2; t <= 2; ++t)
    {
        const float txPos = cx + (static_cast<float>(t) / 2.0f) * (gaugeR.getWidth() * 0.46f);
        const float tickH = (t == 0) ? 10.0f * sc : 5.0f * sc;
        g.setColour((t == 0 && inTune) ? KalyxTheme::jadeGreen : tx.withAlpha(t == 0 ? 0.65f : 0.28f));
        g.drawLine(txPos, trackY - tickH, txPos, trackY + tickH * 0.6f, (t == 0) ? 1.8f * sc : 1.0f * sc);
    }

    // Center lock LED when +-3 cents
    if (inTune)
    {
        g.setColour(KalyxTheme::jadeGreen.withAlpha(0.35f));
        g.fillEllipse(cx - 8.0f * sc, trackY - 8.0f * sc, 16.0f * sc, 16.0f * sc);
        g.setColour(KalyxTheme::kalyxRoyalBlue);
        g.fillEllipse(cx - 4.0f * sc, trackY - 4.0f * sc, 8.0f * sc, 8.0f * sc);
    }

    // Visual cursor (-50..+50 cents)
    if (active)
    {
        const float normC = std::clamp(cents_ / 50.0f, -1.0f, 1.0f);
        const float nx = cx + normC * (gaugeR.getWidth() * 0.46f);
        g.setColour(inTune ? KalyxTheme::jadeGreen : (std::abs(cents_) < 12.0f ? KalyxTheme::kalyxRoyalBlueGlow : KalyxTheme::classicAmber));
        g.fillRoundedRectangle(nx - 2.5f * sc, gaugeR.getY() + 1.0f * sc, 5.0f * sc, gaugeR.getHeight() - 2.0f * sc, 2.5f * sc);
    }
    else
    {
        g.setColour(tx.withAlpha(0.35f));
        g.setFont(KalyxTheme::makeFont(8.5f * sc, juce::Font::bold));
        g.drawFittedText("TUNER READY", gaugeR.withHeight(11.0f * sc).toNearestInt(), juce::Justification::centredTop, 1);
    }

    // Cents / Hz readout on right
    auto centsR = r.withLeft(r.getRight() - 54.0f * sc).reduced(2.0f * sc);
    g.setColour(active ? (inTune ? KalyxTheme::jadeGreen : tx.withAlpha(0.88f)) : tx.withAlpha(0.45f));
    g.setFont(KalyxTheme::makeMonoFont(10.0f * sc, juce::Font::bold));
    const juce::String cStr = active
                                  ? ((cents_ >= 0.0f ? "+" : "") + juce::String(cents_, 0) + "c")
                                  : "440Hz";
    g.drawFittedText(cStr, centsR.toNearestInt(), juce::Justification::centred, 1);
}

// ==============================================================================
// NoiseGateCurveWidget
// ==============================================================================
void NoiseGateCurveWidget::init(juce::AudioProcessorValueTreeState& apvts)
{
    threshParam_ = apvts.getParameter("gate_thresh");
    enParam_     = apvts.getParameter("gate_en");
    if (threshParam_ != nullptr)
    {
        threshAtt_ = std::make_unique<juce::ParameterAttachment>(*threshParam_, [this](float v)
        {
            threshDb_ = v;
            repaint();
        });
        threshAtt_->sendInitialUpdate();
    }
    if (enParam_ != nullptr)
    {
        enAtt_ = std::make_unique<juce::ParameterAttachment>(*enParam_, [this](float v)
        {
            enabled_ = v > 0.5f;
            repaint();
        });
        enAtt_->sendInitialUpdate();
    }
}

void NoiseGateCurveWidget::paint(juce::Graphics& g)
{
    const float sc = uiScale_;
    const auto r = getLocalBounds().toFloat().reduced(1.0f * sc);
    const auto bg = darkText_ ? KalyxTheme::dayControlIdleBg : KalyxTheme::nightChassisBg;
    const auto bd = darkText_ ? KalyxTheme::dayGridBorder : KalyxTheme::nightGridBorder;
    const auto tx = darkText_ ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary;

    g.setColour(bg);
    g.fillRoundedRectangle(r, 5.0f * sc);
    g.setColour(bd);
    g.drawRoundedRectangle(r, 5.0f * sc, 1.0f * sc);

    const float alpha = enabled_ ? 1.0f : 0.38f;
    const auto plot = r.reduced(8.0f * sc, 6.0f * sc);
    const auto headerArea = plot.withHeight(14.0f * sc);
    const auto graphArea  = plot.withTrimmedTop(16.0f * sc);

    const float txNorm = std::clamp((threshDb_ + 80.0f) / 70.0f, 0.0f, 1.0f);
    const float txX = graphArea.getX() + txNorm * graphArea.getWidth();
    const float txY = graphArea.getBottom() - txNorm * graphArea.getHeight();

    // Recessed gate attenuation region
    g.setColour(KalyxTheme::kalyxAubergine.withAlpha(0.22f * alpha));
    g.fillRect(graphArea.getX(), graphArea.getY(), std::max(0.0f, txX - graphArea.getX()), graphArea.getHeight());

    juce::Path p;
    p.startNewSubPath(graphArea.getX(), graphArea.getBottom());
    p.lineTo(txX, graphArea.getBottom());
    p.lineTo(txX, txY);
    p.lineTo(graphArea.getRight(), graphArea.getY());

    g.setColour(KalyxTheme::kalyxRoyalBlueGlow.withAlpha(alpha));
    g.strokePath(p, juce::PathStrokeType(2.0f * sc));

    // Threshold vertical handle + node
    g.setColour(KalyxTheme::classicAmber.withAlpha(0.75f * alpha));
    g.drawLine(txX, graphArea.getY(), txX, graphArea.getBottom(), 1.2f * sc);
    g.fillEllipse(txX - 3.5f * sc, txY - 3.5f * sc, 7.0f * sc, 7.0f * sc);

    g.setColour(tx.withAlpha(0.80f));
    g.setFont(KalyxTheme::makeMonoFont(9.5f * sc, juce::Font::bold));
    g.drawFittedText("GATE THRESH", headerArea.toNearestInt(), juce::Justification::centredLeft, 1);
    g.drawFittedText(juce::String(threshDb_, 0) + " dB", headerArea.toNearestInt(), juce::Justification::centredRight, 1);
}

void NoiseGateCurveWidget::mouseDown(const juce::MouseEvent& e) { mouseDrag(e); }
void NoiseGateCurveWidget::mouseDrag(const juce::MouseEvent& e)
{
    if (threshAtt_ == nullptr) return;
    const auto plot = getLocalBounds().toFloat().reduced(8.0f * uiScale_, 6.0f * uiScale_);
    const float norm = std::clamp((static_cast<float>(e.x) - plot.getX()) / std::max(10.0f, plot.getWidth()), 0.0f, 1.0f);
    threshAtt_->setValueAsCompleteGesture(-80.0f + norm * 70.0f);
}

// ==============================================================================
// DIFilterGraphWidget
// ==============================================================================
void DIFilterGraphWidget::init(juce::AudioProcessorValueTreeState& apvts)
{
    hpfParam_ = apvts.getParameter("di_hpf_freq");
    lpfParam_ = apvts.getParameter("di_lpf_freq");
    if (hpfParam_ != nullptr)
    {
        hpfAtt_ = std::make_unique<juce::ParameterAttachment>(*hpfParam_, [this](float v)
        {
            hpfHz_ = v;
            repaint();
        });
        hpfAtt_->sendInitialUpdate();
    }
    if (lpfParam_ != nullptr)
    {
        lpfAtt_ = std::make_unique<juce::ParameterAttachment>(*lpfParam_, [this](float v)
        {
            lpfHz_ = v;
            repaint();
        });
        lpfAtt_->sendInitialUpdate();
    }
}

float DIFilterGraphWidget::freqToX(float hz) const noexcept
{
    const auto plot = getLocalBounds().toFloat().reduced(8.0f * uiScale_, 6.0f * uiScale_);
    const float t = std::log(std::clamp(hz, 20.0f, 5000.0f) / 20.0f) / std::log(5000.0f / 20.0f);
    return plot.getX() + t * plot.getWidth();
}

float DIFilterGraphWidget::xToFreq(float x) const noexcept
{
    const auto plot = getLocalBounds().toFloat().reduced(8.0f * uiScale_, 6.0f * uiScale_);
    const float t = std::clamp((x - plot.getX()) / std::max(10.0f, plot.getWidth()), 0.0f, 1.0f);
    return 20.0f * std::pow(5000.0f / 20.0f, t);
}

void DIFilterGraphWidget::paint(juce::Graphics& g)
{
    const float sc = uiScale_;
    const auto r = getLocalBounds().toFloat().reduced(1.0f * sc);
    const auto bg = darkText_ ? KalyxTheme::dayControlIdleBg : KalyxTheme::nightChassisBg;
    const auto bd = darkText_ ? KalyxTheme::dayGridBorder : KalyxTheme::nightGridBorder;
    const auto tx = darkText_ ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary;

    g.setColour(bg);
    g.fillRoundedRectangle(r, 5.0f * sc);
    g.setColour(bd);
    g.drawRoundedRectangle(r, 5.0f * sc, 1.0f * sc);

    const auto plot = r.reduced(8.0f * sc, 6.0f * sc);
    const auto graphArea = plot.withTrimmedBottom(15.0f * sc);
    const auto labelArea = plot.withTop(graphArea.getBottom() + 2.0f * sc);
    const float hx = freqToX(hpfHz_);
    const float lx = freqToX(lpfHz_);

    juce::Path band;
    band.startNewSubPath(graphArea.getX(), graphArea.getBottom());
    band.lineTo(hx, graphArea.getY() + 6.0f * sc);
    band.lineTo(std::max(hx + 2.0f, lx), graphArea.getY() + 6.0f * sc);
    band.lineTo(graphArea.getRight(), graphArea.getBottom());

    g.setColour(KalyxTheme::kalyxAubergine.withAlpha(0.32f));
    g.fillPath(band);
    g.setColour(KalyxTheme::kalyxRoyalBlueGlow);
    g.strokePath(band, juce::PathStrokeType(1.8f * sc));

    // Handles
    g.setColour(KalyxTheme::classicAmber);
    g.fillEllipse(hx - 4.0f * sc, graphArea.getY() + 2.0f * sc, 8.0f * sc, 8.0f * sc);
    g.fillEllipse(lx - 4.0f * sc, graphArea.getY() + 2.0f * sc, 8.0f * sc, 8.0f * sc);

    g.setColour(tx.withAlpha(0.80f));
    g.setFont(KalyxTheme::makeMonoFont(9.5f * sc, juce::Font::plain));
    g.drawFittedText("HPF " + juce::String(hpfHz_, 0) + " Hz", labelArea.toNearestInt(), juce::Justification::centredLeft, 1);
    g.drawFittedText("LPF " + juce::String(lpfHz_, 0) + " Hz", labelArea.toNearestInt(), juce::Justification::centredRight, 1);
}

void DIFilterGraphWidget::mouseDown(const juce::MouseEvent& e)
{
    const float hx = freqToX(hpfHz_);
    const float lx = freqToX(lpfHz_);
    dragHandle_ = (std::abs(static_cast<float>(e.x) - hx) < std::abs(static_cast<float>(e.x) - lx)) ? 1 : 2;
    if (dragHandle_ == 1 && hpfAtt_) hpfAtt_->beginGesture();
    if (dragHandle_ == 2 && lpfAtt_) lpfAtt_->beginGesture();
}

void DIFilterGraphWidget::mouseDrag(const juce::MouseEvent& e)
{
    const float f = xToFreq(static_cast<float>(e.x));
    if (dragHandle_ == 1 && hpfAtt_)
        hpfAtt_->setValueAsPartOfGesture(std::clamp(f, 20.0f, 120.0f));
    else if (dragHandle_ == 2 && lpfAtt_)
        lpfAtt_->setValueAsPartOfGesture(std::clamp(f, 60.0f, 5000.0f));
}

void DIFilterGraphWidget::mouseUp(const juce::MouseEvent&)
{
    if (dragHandle_ == 1 && hpfAtt_) hpfAtt_->endGesture();
    if (dragHandle_ == 2 && lpfAtt_) lpfAtt_->endGesture();
    dragHandle_ = 0;
}

// ==============================================================================
// AmpToneEQGraphWidget (with THD/Bias Heatmap "Aura Réactive")
// ==============================================================================
namespace
{
constexpr const char* kAmpEqListenIds[] = {
    "amp_mid_freq", "amp_bass", "amp_mid", "amp_treble",
    "amp_ultra_lo", "amp_ultra_hi", "amp_bright",
    "amp_model", "amp_boost", "amp_presence"
};
}

AmpToneEQGraphWidget::AmpToneEQGraphWidget(KalyxBassRigProcessor& proc)
    : proc_(proc)
{
    smoothedBinsDb_.fill(-100.0f);
    for (const char* id : kAmpEqListenIds)
        proc_.getAPVTS().addParameterListener(id, this);
}

AmpToneEQGraphWidget::~AmpToneEQGraphWidget()
{
    cancelPendingUpdate();
    for (const char* id : kAmpEqListenIds)
        proc_.getAPVTS().removeParameterListener(id, this);
}

void AmpToneEQGraphWidget::updateFFTFromProcessor() noexcept
{
    std::array<float, 2048> raw {};
    proc_.copyPostAmpFFT(raw.data());
    fftData_.fill(0.0f);
    for (size_t i = 0; i < 2048; ++i)
        fftData_[i] = raw[i];
    window_.multiplyWithWindowingTable(fftData_.data(), 2048);
    forwardFFT_.performFrequencyOnlyForwardTransform(fftData_.data());

    for (size_t b = 1; b < 1024; ++b)
    {
        const float mag = fftData_[b] / 512.0f;
        const float db  = (mag > 1.0e-5f) ? (20.0f * std::log10(mag)) : -100.0f;
        smoothedBinsDb_[b] = 0.72f * smoothedBinsDb_[b] + 0.28f * std::clamp(db, -100.0f, 6.0f);
    }
}

float AmpToneEQGraphWidget::freqToX(float hz, const juce::Rectangle<float>& area) const noexcept
{
    const float t = std::log(std::clamp(hz, 20.0f, 16000.0f) / 20.0f) / std::log(16000.0f / 20.0f);
    return area.getX() + t * area.getWidth();
}

float AmpToneEQGraphWidget::gainToY(float db, const juce::Rectangle<float>& area) const noexcept
{
    const float t = std::clamp((db + 18.0f) / 36.0f, 0.0f, 1.0f);
    return area.getBottom() - t * area.getHeight();
}

float AmpToneEQGraphWidget::yToGain(float y, const juce::Rectangle<float>& area) const noexcept
{
    const float t = std::clamp((area.getBottom() - y) / std::max(10.0f, area.getHeight()), 0.0f, 1.0f);
    return -18.0f + t * 36.0f;
}

void AmpToneEQGraphWidget::paint(juce::Graphics& g)
{
    const float sc = uiScale_;
    const auto r = getLocalBounds().toFloat().reduced(1.0f * sc);
    const auto bg = darkText_ ? KalyxTheme::dayControlIdleBg : KalyxTheme::nightChassisBg;
    const auto bd = darkText_ ? KalyxTheme::dayGridBorder : KalyxTheme::nightGridBorder;
    const auto tx = darkText_ ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary;

    g.setColour(bg);
    g.fillRoundedRectangle(r, 5.0f * sc);

    // Aura Réactive (Heatmap THD) glowing amber -> crimson around border
    const float thd = std::clamp(thdLevel_, 0.0f, 1.0f);
    if (thd > 0.02f)
    {
        const auto heatCol = KalyxTheme::classicAmber.interpolatedWith(KalyxTheme::alertRed, thd);
        g.setColour(heatCol.withAlpha(0.12f + 0.35f * thd));
        g.drawRoundedRectangle(r.reduced(1.0f), 5.0f * sc, 3.5f * sc);
    }

    g.setColour(bd);
    g.drawRoundedRectangle(r, 5.0f * sc, 1.2f * sc);

    const auto plot = r.reduced(10.0f * sc, 8.0f * sc);
    const double fs = proc_.getCurrentSampleRate();

    // Watermark Grid: Horizontal dB lines (+12, +6, 0, -6, -12)
    g.setFont(KalyxTheme::makeMonoFont(8.5f * sc, juce::Font::plain));
    for (float db : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
    {
        const float y = gainToY(db, plot);
        const bool isZero = (std::abs(db) < 0.1f);
        g.setColour(tx.withAlpha(isZero ? 0.25f : 0.10f));
        g.drawLine(plot.getX(), y, plot.getRight(), y, isZero ? 1.1f * sc : 0.7f * sc);
        g.setColour(tx.withAlpha(0.35f));
        g.drawFittedText((db > 0 ? "+" : "") + juce::String(static_cast<int>(db)),
                         juce::Rectangle<int>(static_cast<int>(plot.getX() + 2.0f), static_cast<int>(y - 10.0f * sc),
                                              static_cast<int>(24.0f * sc), static_cast<int>(10.0f * sc)),
                         juce::Justification::centredLeft, 1);
    }

    // Watermark Grid: Vertical Hz lines
    const struct { float f; const char* lbl; } kGridFreqs[] = {
        { 40.0f, "40" }, { 100.0f, "100" }, { 250.0f, "250" }, { 500.0f, "500" },
        { 1000.0f, "1k" }, { 4000.0f, "4k" }, { 10000.0f, "10k" }
    };
    for (const auto& gf : kGridFreqs)
    {
        const float x = freqToX(gf.f, plot);
        g.setColour(tx.withAlpha(0.10f));
        g.drawLine(x, plot.getY(), x, plot.getBottom(), 0.7f * sc);
        g.setColour(tx.withAlpha(0.35f));
        g.drawFittedText(gf.lbl,
                         juce::Rectangle<int>(static_cast<int>(x - 14.0f * sc), static_cast<int>(plot.getBottom() - 11.0f * sc),
                                              static_cast<int>(28.0f * sc), static_cast<int>(11.0f * sc)),
                         juce::Justification::centred, 1);
    }

    // Post-Amp FFT Spectrum in low-opacity background
    {
        juce::Path fftPath;
        bool started = false;
        const float binHz = static_cast<float>(fs) / 2048.0f;
        for (int i = 0; i <= 96; ++i)
        {
            const float t = static_cast<float>(i) / 96.0f;
            const float f = 20.0f * std::pow(16000.0f / 20.0f, t);
            const int bin = std::clamp(static_cast<int>(std::round(f / std::max(1.0f, binHz))), 1, 1023);
            const float db = smoothedBinsDb_[static_cast<size_t>(bin)];
            const float normY = std::clamp((db + 84.0f) / 84.0f, 0.0f, 1.0f);
            const float px = freqToX(f, plot);
            const float py = plot.getBottom() - normY * plot.getHeight();
            if (!started) { fftPath.startNewSubPath(plot.getX(), plot.getBottom()); fftPath.lineTo(px, py); started = true; }
            else          { fftPath.lineTo(px, py); }
        }
        fftPath.lineTo(plot.getRight(), plot.getBottom());
        fftPath.closeSubPath();
        g.setColour(accent_.withAlpha(0.16f));
        g.fillPath(fftPath);
        g.setColour(accent_.withAlpha(0.32f));
        g.strokePath(fftPath, juce::PathStrokeType(1.0f * sc));
    }

    const auto st = proc_.getAmpSnapshot();

    juce::Path curve;
    const int nPts = 96;
    for (int i = 0; i <= nPts; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(nPts);
        const float f = 20.0f * std::pow(16000.0f / 20.0f, t);
        const float db = kalyx::bass::dsp::TubeAmpEngine::evaluateCurveDb(st, static_cast<double>(f), fs);
        const float px = freqToX(f, plot);
        const float py = gainToY(db, plot);
        if (i == 0) curve.startNewSubPath(px, py);
        else        curve.lineTo(px, py);
    }

    g.setColour(accent_);
    g.strokePath(curve, juce::PathStrokeType(2.2f * sc));

    // If Hybrid 4-PRO, draw draggable 9-band EQ nodes
    if (st.model == kalyx::bass::AmpModel::Hybrid4Pro)
    {
        for (size_t i = 0; i < 9; ++i)
        {
            const float f  = static_cast<float>(kalyx::bass::dsp::TubeAmpEngine::kEQ9FreqsHz[i]);
            const float db = static_cast<float>(st.eq9GainsDb[i]);
            const float nx = freqToX(f, plot);
            const float ny = gainToY(db, plot);
            g.setColour(st.eq9Enabled ? KalyxTheme::hybridCyan : juce::Colours::grey);
            g.fillEllipse(nx - 4.0f * sc, ny - 4.0f * sc, 8.0f * sc, 8.0f * sc);
        }
    }

    g.setColour(tx.withAlpha(0.65f));
    g.setFont(KalyxTheme::makeMonoFont(9.5f * sc, juce::Font::plain));
    g.drawFittedText("THD AURA " + juce::String(static_cast<int>(thd * 100.0f)) + "%",
                     plot.toNearestInt(), juce::Justification::topRight, 1);
}

void AmpToneEQGraphWidget::mouseDown(const juce::MouseEvent& e)
{
    const auto st = proc_.getAmpSnapshot();
    if (st.model != kalyx::bass::AmpModel::Hybrid4Pro) return;
    const auto plot = getLocalBounds().toFloat().reduced(8.0f * uiScale_);
    float bestDist = 18.0f * uiScale_;
    dragEqIdx_ = -1;
    for (size_t i = 0; i < 9; ++i)
    {
        const float nx = freqToX(static_cast<float>(kalyx::bass::dsp::TubeAmpEngine::kEQ9FreqsHz[i]), plot);
        const float d = std::abs(static_cast<float>(e.x) - nx);
        if (d < bestDist)
        {
            bestDist = d;
            dragEqIdx_ = static_cast<int>(i);
        }
    }
}

void AmpToneEQGraphWidget::mouseDrag(const juce::MouseEvent& e)
{
    if (dragEqIdx_ < 0 || dragEqIdx_ >= 9) return;
    static const char* kEqIds[9] = { "eq9_40", "eq9_90", "eq9_180", "eq9_300", "eq9_500", "eq9_1k", "eq9_2k", "eq9_4k", "eq9_10k" };
    const auto plot = getLocalBounds().toFloat().reduced(8.0f * uiScale_);
    const float db = std::clamp(yToGain(static_cast<float>(e.y), plot), -15.0f, 15.0f);
    if (auto* p = proc_.getAPVTS().getParameter(kEqIds[dragEqIdx_]))
        p->setValueNotifyingHost(p->convertTo0to1(db));
}

void AmpToneEQGraphWidget::mouseUp(const juce::MouseEvent&)
{
    dragEqIdx_ = -1;
}

void AmpToneEQGraphWidget::mouseDoubleClick(const juce::MouseEvent&)
{
    const auto st = proc_.getAmpSnapshot();
    if (st.model != kalyx::bass::AmpModel::Hybrid4Pro) return;
    static const char* kEqIds[9] = { "eq9_40", "eq9_90", "eq9_180", "eq9_300", "eq9_500", "eq9_1k", "eq9_2k", "eq9_4k", "eq9_10k" };
    for (auto* id : kEqIds)
        if (auto* p = proc_.getAPVTS().getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(0.0f));
}

// ==============================================================================
// PhaseCorrelometerBar
// ==============================================================================
void PhaseCorrelometerBar::paint(juce::Graphics& g)
{
    const float sc = uiScale_;
    const auto r = getLocalBounds().toFloat().reduced(1.0f * sc);
    const auto bg = darkText_ ? KalyxTheme::dayControlIdleBg : KalyxTheme::nightControlIdleBg;
    const auto tx = darkText_ ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary;

    g.setColour(bg);
    g.fillRoundedRectangle(r, 3.0f * sc);

    const float cx = r.getCentreX();
    const float norm = std::clamp(corr_, -1.0f, 1.0f);
    const float vx = cx + norm * (r.getWidth() * 0.46f);
    g.setColour(norm >= 0.0f ? KalyxTheme::jadeGreen : KalyxTheme::alertRed);
    g.fillRoundedRectangle(std::min(cx, vx), r.getY() + 2.0f * sc, std::max(2.0f, std::abs(vx - cx)), r.getHeight() - 4.0f * sc, 2.0f * sc);

    g.setColour(tx.withAlpha(0.75f));
    g.setFont(KalyxTheme::makeMonoFont(9.0f * sc, juce::Font::bold));
    g.drawFittedText("PHASE AMP/DI", r.reduced(4.0f * sc, 0.0f).toNearestInt(), juce::Justification::centredLeft, 1);
    g.drawFittedText(juce::String(corr_, 2), r.reduced(4.0f * sc, 0.0f).toNearestInt(), juce::Justification::centredRight, 1);
}

// ==============================================================================
// SettingsOverlay
// ==============================================================================
SettingsOverlay::SettingsOverlay(KalyxBassRigProcessor& proc)
    : proc_(proc)
{
    closeBtn_.onClick = [this]() { setVisible(false); };
    addAndMakeVisible(closeBtn_);

    const float refs[4] = { -20.0f, -18.0f, -14.0f, 0.0f };
    for (size_t i = 0; i < 4; ++i)
    {
        vuBtns_[i].onClick = [this, val = refs[i]]()
        {
            if (auto* p = proc_.getAPVTS().getParameter("vu_ref"))
                p->setValueNotifyingHost(p->convertTo0to1(val));
        };
        addAndMakeVisible(vuBtns_[i]);
    }
    for (size_t i = 0; i < 3; ++i)
    {
        osBtns_[i].onClick = [this, idx = static_cast<float>(i)]()
        {
            if (auto* p = proc_.getAPVTS().getParameter("oversampling"))
                p->setValueNotifyingHost(p->convertTo0to1(idx));
        };
        addAndMakeVisible(osBtns_[i]);
    }
}

void SettingsOverlay::setUiScale(float s) noexcept
{
    uiScale_ = std::clamp(s, 0.75f, 2.20f);
    resized();
    repaint();
}

void SettingsOverlay::paint(juce::Graphics& g)
{
    const float sc = uiScale_;
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillAll();

    const auto card = getLocalBounds().toFloat().withSizeKeepingCentre(420.0f * sc, 240.0f * sc);
    g.setColour(KalyxTheme::getStripBg(theme_));
    g.fillRoundedRectangle(card, 8.0f * sc);
    g.setColour(KalyxTheme::getAccent(theme_));
    g.drawRoundedRectangle(card, 8.0f * sc, 1.5f * sc);

    g.setColour(KalyxTheme::getTextPrimary(theme_));
    g.setFont(KalyxTheme::makeFont(14.0f * sc, juce::Font::bold));
    g.drawFittedText(juce::String::fromUTF8("KALYX // BASS RIG v" JucePlugin_VersionString " — SETTINGS"),
                     card.reduced(14.0f * sc).withHeight(24.0f * sc).toNearestInt(),
                     juce::Justification::centredLeft, 1);

    g.setFont(KalyxTheme::makeFont(11.0f * sc, juce::Font::bold));
    g.drawFittedText("0 VU CALIBRATION",
                     card.withTrimmedTop(56.0f * sc).withTrimmedLeft(16.0f * sc).withHeight(18.0f * sc).toNearestInt(),
                     juce::Justification::centredLeft, 1);
    g.drawFittedText("OVERSAMPLING",
                     card.withTrimmedTop(126.0f * sc).withTrimmedLeft(16.0f * sc).withHeight(18.0f * sc).toNearestInt(),
                     juce::Justification::centredLeft, 1);
}

void SettingsOverlay::resized()
{
    const float sc = uiScale_;
    const auto card = getLocalBounds().withSizeKeepingCentre(static_cast<int>(420.0f * sc), static_cast<int>(240.0f * sc));
    closeBtn_.setBounds(card.getRight() - static_cast<int>(34.0f * sc), card.getY() + static_cast<int>(10.0f * sc),
                        static_cast<int>(24.0f * sc), static_cast<int>(24.0f * sc));

    const int bw = static_cast<int>(92.0f * sc);
    const int bh = static_cast<int>(26.0f * sc);
    for (int i = 0; i < 4; ++i)
        vuBtns_[static_cast<size_t>(i)].setBounds(card.getX() + static_cast<int>(16.0f * sc) + i * (bw + 6),
                                                   card.getY() + static_cast<int>(80.0f * sc), bw, bh);

    const int ow = static_cast<int>(124.0f * sc);
    for (int i = 0; i < 3; ++i)
        osBtns_[static_cast<size_t>(i)].setBounds(card.getX() + static_cast<int>(16.0f * sc) + i * (ow + 6),
                                                   card.getY() + static_cast<int>(150.0f * sc), ow, bh);
}

void SettingsOverlay::mouseDown(const juce::MouseEvent& e)
{
    const auto card = getLocalBounds().toFloat().withSizeKeepingCentre(420.0f * uiScale_, 240.0f * uiScale_);
    if (!card.contains(e.position))
        setVisible(false);
}

// ==============================================================================
// TunerOverlay
// ==============================================================================
TunerOverlay::TunerOverlay(KalyxBassRigProcessor& proc)
    : proc_(proc), tunerBar_(proc)
{
    addAndMakeVisible(tunerBar_);
    muteBtn_.setClickingTogglesState(true);
    muteBtn_.setWantsKeyboardFocus(false);
    muteAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc_.getAPVTS(), "tuner_mute", muteBtn_);
    addAndMakeVisible(muteBtn_);

    closeBtn_.setWantsKeyboardFocus(false);
    closeBtn_.onClick = [this]() { setVisible(false); };
    addAndMakeVisible(closeBtn_);
}

void TunerOverlay::setUiScale(float s) noexcept
{
    uiScale_ = std::clamp(s, 0.75f, 2.20f);
    tunerBar_.setUiScale(uiScale_ * 1.25f);
    resized();
    repaint();
}

void TunerOverlay::setTheme(kalyx::bass::ChassisTheme t) noexcept
{
    theme_ = t;
    tunerBar_.setDarkText(t == kalyx::bass::ChassisTheme::Day);
    repaint();
}

void TunerOverlay::updateFromProcessor()
{
    if (isVisible())
    {
        tunerBar_.updateFromProcessor();
        repaint();
    }
}

void TunerOverlay::paint(juce::Graphics& g)
{
    const float sc = uiScale_;
    g.setColour(juce::Colours::black.withAlpha(0.62f));
    g.fillAll();

    const auto card = getLocalBounds().toFloat().withSizeKeepingCentre(520.0f * sc, 185.0f * sc);
    g.setColour(KalyxTheme::getStripBg(theme_));
    g.fillRoundedRectangle(card, 8.0f * sc);
    g.setColour(KalyxTheme::getAccent(theme_));
    g.drawRoundedRectangle(card, 8.0f * sc, 1.8f * sc);

    g.setColour(KalyxTheme::getTextPrimary(theme_));
    g.setFont(KalyxTheme::makeFont(14.0f * sc, juce::Font::bold));
    g.drawFittedText("CHROMATIC PRECISION TUNER",
                     card.reduced(16.0f * sc).withHeight(24.0f * sc).toNearestInt(),
                     juce::Justification::centredLeft, 1);

    const float hz = proc_.getTunerFreq();
    const float conf = proc_.getTunerConf();
    const juce::String hzStr = (conf > 0.25f && hz > 10.0f) ? (juce::String(hz, 1) + " Hz") : "--- Hz";
    g.setColour(KalyxTheme::getTextSecondary(theme_));
    g.setFont(KalyxTheme::makeMonoFont(11.5f * sc, juce::Font::plain));
    g.drawFittedText("PITCH: " + hzStr,
                     card.reduced(16.0f * sc).withTrimmedTop(118.0f * sc).withHeight(24.0f * sc).toNearestInt(),
                     juce::Justification::centredLeft, 1);
}

void TunerOverlay::resized()
{
    const float sc = uiScale_;
    const auto card = getLocalBounds().withSizeKeepingCentre(static_cast<int>(520.0f * sc), static_cast<int>(185.0f * sc));
    closeBtn_.setBounds(card.getRight() - static_cast<int>(36.0f * sc), card.getY() + static_cast<int>(10.0f * sc),
                        static_cast<int>(26.0f * sc), static_cast<int>(26.0f * sc));
    tunerBar_.setBounds(card.getX() + static_cast<int>(16.0f * sc), card.getY() + static_cast<int>(48.0f * sc),
                        card.getWidth() - static_cast<int>(32.0f * sc), static_cast<int>(64.0f * sc));
    muteBtn_.setBounds(card.getRight() - static_cast<int>(126.0f * sc), card.getBottom() - static_cast<int>(42.0f * sc),
                       static_cast<int>(110.0f * sc), static_cast<int>(28.0f * sc));
}

void TunerOverlay::mouseDown(const juce::MouseEvent& e)
{
    const auto card = getLocalBounds().toFloat().withSizeKeepingCentre(520.0f * uiScale_, 185.0f * uiScale_);
    if (!card.contains(e.position))
        setVisible(false);
}

} // namespace kalyx::bass::ui
