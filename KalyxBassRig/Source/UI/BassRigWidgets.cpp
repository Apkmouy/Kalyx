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
    const auto bg = darkText_ ? KalyxTheme::dayControlIdleBg : KalyxTheme::nightControlIdleBg;
    const auto bd = darkText_ ? KalyxTheme::dayGridBorder : KalyxTheme::nightGridBorder;
    const auto tx = darkText_ ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary;

    g.setColour(bg);
    g.fillRoundedRectangle(r, 4.0f * sc);
    g.setColour(bd);
    g.drawRoundedRectangle(r, 4.0f * sc, 1.0f * sc);

    static const char* kNotes[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    juce::String noteStr = "--";
    if (conf_ > 0.25f && midi_ >= 0)
    {
        const int noteIdx = ((midi_ % 12) + 12) % 12;
        const int oct = (midi_ / 12) - 1;
        noteStr = juce::String(kNotes[noteIdx]) + juce::String(oct);
    }

    const bool inTune = (conf_ > 0.25f && std::abs(cents_) < 4.0f);
    auto noteR = r.withWidth(44.0f * sc);
    g.setColour(inTune ? KalyxTheme::jadeGreen : tx);
    g.setFont(KalyxTheme::makeMonoFont(13.0f * sc, juce::Font::bold));
    g.drawFittedText(noteStr, noteR.toNearestInt(), juce::Justification::centred, 1);

    auto barR = r.withTrimmedLeft(46.0f * sc).withTrimmedRight(52.0f * sc).reduced(2.0f * sc, 6.0f * sc);
    g.setColour(darkText_ ? KalyxTheme::dayKnobTrackBg : KalyxTheme::knobTrackBg);
    g.fillRoundedRectangle(barR, 3.0f * sc);

    // Strobe ticks
    const int nTicks = 12;
    const float tickW = barR.getWidth() / static_cast<float>(nTicks);
    for (int i = 0; i < nTicks; ++i)
    {
        const float wave = 0.5f + 0.5f * std::sin(strobePhase_ + static_cast<float>(i) * 1.05f);
        if (conf_ > 0.25f)
        {
            g.setColour((inTune ? KalyxTheme::jadeGreen : KalyxTheme::kalyxRoyalBlueGlow).withAlpha(0.18f + 0.35f * wave));
            g.fillRect(barR.getX() + i * tickW + 1.0f, barR.getY() + 1.0f, std::max(1.0f, tickW - 2.0f), barR.getHeight() - 2.0f);
        }
    }

    // Center target line + needle
    const float cx = barR.getCentreX();
    g.setColour(tx.withAlpha(0.45f));
    g.drawLine(cx, barR.getY() - 2.0f * sc, cx, barR.getBottom() + 2.0f * sc, 1.2f * sc);

    if (conf_ > 0.25f)
    {
        const float normC = std::clamp(cents_ / 50.0f, -1.0f, 1.0f);
        const float nx = cx + normC * (barR.getWidth() * 0.46f);
        g.setColour(inTune ? KalyxTheme::jadeGreen : KalyxTheme::classicAmber);
        g.fillRoundedRectangle(nx - 2.5f * sc, barR.getY() - 1.0f * sc, 5.0f * sc, barR.getHeight() + 2.0f * sc, 2.0f * sc);
    }

    auto centsR = r.withLeft(r.getRight() - 50.0f * sc);
    g.setColour(tx.withAlpha(0.75f));
    g.setFont(KalyxTheme::makeMonoFont(10.0f * sc, juce::Font::plain));
    const juce::String cStr = (conf_ > 0.25f)
                                  ? ((cents_ >= 0.0f ? "+" : "") + juce::String(cents_, 0) + "c")
                                  : "---";
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
    const auto bg = darkText_ ? KalyxTheme::dayControlIdleBg : KalyxTheme::nightControlIdleBg;
    const auto bd = darkText_ ? KalyxTheme::dayGridBorder : KalyxTheme::nightGridBorder;
    const auto tx = darkText_ ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary;

    g.setColour(bg);
    g.fillRoundedRectangle(r, 4.0f * sc);
    g.setColour(bd);
    g.drawRoundedRectangle(r, 4.0f * sc, 1.0f * sc);

    const float alpha = enabled_ ? 1.0f : 0.35f;
    const auto plot = r.reduced(6.0f * sc);
    const float txNorm = std::clamp((threshDb_ + 80.0f) / 70.0f, 0.0f, 1.0f);
    const float txX = plot.getX() + txNorm * plot.getWidth();
    const float txY = plot.getBottom() - txNorm * plot.getHeight();

    juce::Path p;
    p.startNewSubPath(plot.getX(), plot.getBottom());
    p.lineTo(txX, plot.getBottom());
    p.lineTo(txX, txY);
    p.lineTo(plot.getRight(), plot.getY());

    g.setColour(KalyxTheme::kalyxRoyalBlueGlow.withAlpha(alpha));
    g.strokePath(p, juce::PathStrokeType(2.0f * sc));

    g.setColour(tx.withAlpha(0.78f));
    g.setFont(KalyxTheme::makeMonoFont(9.5f * sc, juce::Font::plain));
    g.drawFittedText("GATE " + juce::String(threshDb_, 0) + " dB",
                     plot.toNearestInt(), juce::Justification::topLeft, 1);
}

void NoiseGateCurveWidget::mouseDown(const juce::MouseEvent& e) { mouseDrag(e); }
void NoiseGateCurveWidget::mouseDrag(const juce::MouseEvent& e)
{
    if (threshAtt_ == nullptr) return;
    const auto plot = getLocalBounds().toFloat().reduced(6.0f * uiScale_);
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
    const auto plot = getLocalBounds().toFloat().reduced(6.0f * uiScale_);
    const float t = std::log(std::clamp(hz, 20.0f, 5000.0f) / 20.0f) / std::log(5000.0f / 20.0f);
    return plot.getX() + t * plot.getWidth();
}

float DIFilterGraphWidget::xToFreq(float x) const noexcept
{
    const auto plot = getLocalBounds().toFloat().reduced(6.0f * uiScale_);
    const float t = std::clamp((x - plot.getX()) / std::max(10.0f, plot.getWidth()), 0.0f, 1.0f);
    return 20.0f * std::pow(5000.0f / 20.0f, t);
}

void DIFilterGraphWidget::paint(juce::Graphics& g)
{
    const float sc = uiScale_;
    const auto r = getLocalBounds().toFloat().reduced(1.0f * sc);
    const auto bg = darkText_ ? KalyxTheme::dayControlIdleBg : KalyxTheme::nightControlIdleBg;
    const auto bd = darkText_ ? KalyxTheme::dayGridBorder : KalyxTheme::nightGridBorder;
    const auto tx = darkText_ ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary;

    g.setColour(bg);
    g.fillRoundedRectangle(r, 4.0f * sc);
    g.setColour(bd);
    g.drawRoundedRectangle(r, 4.0f * sc, 1.0f * sc);

    const auto plot = r.reduced(6.0f * sc);
    const float hx = freqToX(hpfHz_);
    const float lx = freqToX(lpfHz_);

    juce::Path band;
    band.startNewSubPath(plot.getX(), plot.getBottom());
    band.lineTo(hx, plot.getY() + 6.0f * sc);
    band.lineTo(std::max(hx + 2.0f, lx), plot.getY() + 6.0f * sc);
    band.lineTo(plot.getRight(), plot.getBottom());

    g.setColour(KalyxTheme::kalyxAubergine.withAlpha(0.32f));
    g.fillPath(band);
    g.setColour(KalyxTheme::kalyxRoyalBlueGlow);
    g.strokePath(band, juce::PathStrokeType(1.8f * sc));

    // Handles
    g.setColour(KalyxTheme::classicAmber);
    g.fillEllipse(hx - 4.0f * sc, plot.getY() + 2.0f * sc, 8.0f * sc, 8.0f * sc);
    g.fillEllipse(lx - 4.0f * sc, plot.getY() + 2.0f * sc, 8.0f * sc, 8.0f * sc);

    g.setColour(tx.withAlpha(0.75f));
    g.setFont(KalyxTheme::makeMonoFont(9.5f * sc, juce::Font::plain));
    g.drawFittedText("HP " + juce::String(hpfHz_, 0) + "Hz  LP " + juce::String(lpfHz_, 0) + "Hz",
                     plot.toNearestInt(), juce::Justification::bottomLeft, 1);
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
AmpToneEQGraphWidget::AmpToneEQGraphWidget(KalyxBassRigProcessor& proc)
    : proc_(proc)
{
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
    const auto bg = darkText_ ? KalyxTheme::dayControlIdleBg : KalyxTheme::nightControlIdleBg;
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

    const auto plot = r.reduced(8.0f * sc);
    // Grid lines: 0 dB & key freqs
    g.setColour(tx.withAlpha(0.15f));
    const float y0 = gainToY(0.0f, plot);
    g.drawLine(plot.getX(), y0, plot.getRight(), y0, 1.0f * sc);
    for (float f : { 40.0f, 100.0f, 250.0f, 500.0f, 1000.0f, 4000.0f, 10000.0f })
    {
        const float x = freqToX(f, plot);
        g.drawLine(x, plot.getY(), x, plot.getBottom(), 0.8f * sc);
    }

    const auto st = proc_.getAmpSnapshot();
    const double fs = proc_.getCurrentSampleRate();

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
// TopographicMicPad2D
// ==============================================================================
void TopographicMicPad2D::init(juce::AudioProcessorValueTreeState& apvts)
{
    paramX_ = apvts.getParameter("mic_pos_x");
    paramY_ = apvts.getParameter("mic_pos_y");
    if (paramX_ != nullptr)
    {
        attX_ = std::make_unique<juce::ParameterAttachment>(*paramX_, [this](float v) { posX_ = v; repaint(); });
        attX_->sendInitialUpdate();
    }
    if (paramY_ != nullptr)
    {
        attY_ = std::make_unique<juce::ParameterAttachment>(*paramY_, [this](float v) { posY_ = v; repaint(); });
        attY_->sendInitialUpdate();
    }
}

void TopographicMicPad2D::paint(juce::Graphics& g)
{
    const float sc = uiScale_;
    const auto r = getLocalBounds().toFloat().reduced(1.0f * sc);
    const auto bg = darkText_ ? KalyxTheme::dayControlIdleBg : KalyxTheme::nightControlIdleBg;
    const auto bd = darkText_ ? KalyxTheme::dayGridBorder : KalyxTheme::nightGridBorder;
    const auto tx = darkText_ ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary;

    g.setColour(bg);
    g.fillRoundedRectangle(r, 5.0f * sc);
    g.setColour(bd);
    g.drawRoundedRectangle(r, 5.0f * sc, 1.2f * sc);

    // Topographic concentric speaker cone rings
    const float cx = r.getCentreX();
    const float cy = r.getCentreY();
    const float maxR = std::min(r.getWidth(), r.getHeight()) * 0.43f;
    for (int i = 1; i <= 4; ++i)
    {
        const float rad = maxR * (static_cast<float>(i) / 4.0f);
        g.setColour(tx.withAlpha(i == 1 ? 0.25f : 0.14f));
        g.drawEllipse(cx - rad, cy - rad, rad * 2.0f, rad * 2.0f, 1.0f * sc);
    }

    // Mic dot
    const auto pad = r.reduced(10.0f * sc);
    const float mx = pad.getX() + (posX_ / 100.0f) * pad.getWidth();
    const float my = pad.getBottom() - (posY_ / 100.0f) * pad.getHeight();
    g.setColour(KalyxTheme::kalyxRoyalBlueGlow);
    g.fillEllipse(mx - 6.0f * sc, my - 6.0f * sc, 12.0f * sc, 12.0f * sc);
    g.setColour(KalyxTheme::nightTextPrimary);
    g.drawEllipse(mx - 6.0f * sc, my - 6.0f * sc, 12.0f * sc, 12.0f * sc, 1.4f * sc);

    g.setColour(tx.withAlpha(0.72f));
    g.setFont(KalyxTheme::makeMonoFont(9.0f * sc, juce::Font::plain));
    g.drawFittedText("CAP<->EDGE " + juce::String(static_cast<int>(posX_)) + "%  PROX " + juce::String(static_cast<int>(posY_)) + "%",
                     r.reduced(4.0f * sc).toNearestInt(), juce::Justification::centredBottom, 1);
}

void TopographicMicPad2D::mouseDown(const juce::MouseEvent& e) { mouseDrag(e); }
void TopographicMicPad2D::mouseDrag(const juce::MouseEvent& e)
{
    const auto pad = getLocalBounds().toFloat().reduced(10.0f * uiScale_);
    const float nx = std::clamp((static_cast<float>(e.x) - pad.getX()) / std::max(10.0f, pad.getWidth()), 0.0f, 1.0f) * 100.0f;
    const float ny = std::clamp((pad.getBottom() - static_cast<float>(e.y)) / std::max(10.0f, pad.getHeight()), 0.0f, 1.0f) * 100.0f;
    if (attX_) attX_->setValueAsCompleteGesture(nx);
    if (attY_) attY_->setValueAsCompleteGesture(ny);
}
void TopographicMicPad2D::mouseDoubleClick(const juce::MouseEvent&)
{
    if (attX_) attX_->setValueAsCompleteGesture(25.0f);
    if (attY_) attY_->setValueAsCompleteGesture(75.0f);
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
    g.drawFittedText("KALYX // BASS RIG v1.0.0 — SETTINGS",
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

} // namespace kalyx::bass::ui
