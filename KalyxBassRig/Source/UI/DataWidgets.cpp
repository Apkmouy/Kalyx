#include "DataWidgets.h"

namespace kalyx::bass::ui
{

// ==============================================================================
// KalyxRotaryControl
// ==============================================================================
void KalyxRotaryControl::init(juce::Component& parent,
                              juce::AudioProcessorValueTreeState& apvts,
                              const juce::String& paramID,
                              const juce::String& title,
                              juce::Colour accent,
                              bool isBipolar,
                              bool isDarkText,
                              double defaultVal,
                              std::function<juce::String(double)> fmt,
                              const juce::String& infoHint)
{
    arcColour = accent;
    darkText  = isDarkText;
    formatter = std::move(fmt);

    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setDoubleClickReturnValue(true, defaultVal);
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    slider.getProperties().set("kalyxBipolar", isBipolar);
    slider.setVelocityBasedMode(false);
    slider.setMouseDragSensitivity(130);
    slider.setWantsKeyboardFocus(false);
    if (infoHint.isNotEmpty())
        slider.setTooltip(infoHint);
    parent.addAndMakeVisible(slider);

    titleLabel.setText(title, juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, isDarkText ? KalyxTheme::dayTextPrimary.withAlpha(0.75f)
                                                               : KalyxTheme::nightTextPrimary.withAlpha(0.75f));
    parent.addAndMakeVisible(titleLabel);

    valueLabel.setJustificationType(juce::Justification::centred);
    valueLabel.setEditable(false, true, false);
    valueLabel.setColour(juce::Label::textColourId, isDarkText ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary);
    valueLabel.onTextChange = [this]()
    {
        const double v = valueLabel.getText().getDoubleValue();
        slider.setValue(v, juce::sendNotificationSync);
    };
    parent.addAndMakeVisible(valueLabel);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramID, slider);

    auto updateValText = [this]()
    {
        const double v = slider.getValue();
        if (formatter)
            valueLabel.setText(formatter(v), juce::dontSendNotification);
        else
            valueLabel.setText(juce::String(v, 1), juce::dontSendNotification);
    };
    slider.onValueChange = updateValText;
    updateValText();
    setUiScale(uiScale);
}

void KalyxRotaryControl::setUiScale(float s)
{
    uiScale = std::clamp(s, 0.75f, 2.20f);
    titleLabel.setFont(KalyxTheme::makeFont(10.5f * uiScale, juce::Font::bold));
    valueLabel.setFont(KalyxTheme::makeMonoFont(11.0f * uiScale, juce::Font::plain));
}

void KalyxRotaryControl::setThemeColours(juce::Colour accent, bool isDarkText)
{
    arcColour = accent;
    darkText  = isDarkText;
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    titleLabel.setColour(juce::Label::textColourId, isDarkText ? KalyxTheme::dayTextPrimary.withAlpha(0.75f)
                                                               : KalyxTheme::nightTextPrimary.withAlpha(0.75f));
    valueLabel.setColour(juce::Label::textColourId, isDarkText ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary);
    slider.repaint();
}

void KalyxRotaryControl::setDimmed(bool dimmed)
{
    slider.setEnabled(!dimmed);
    titleLabel.setAlpha(dimmed ? 0.35f : 1.0f);
    valueLabel.setAlpha(dimmed ? 0.35f : 1.0f);
    slider.repaint();
}

void KalyxRotaryControl::setBoundsCompact(float x, float y, float w, float h)
{
    const int titleH = static_cast<int>(15.0f * uiScale);
    const int valH   = static_cast<int>(15.0f * uiScale);
    const int availH = std::max(24, static_cast<int>(h) - titleH - valH - static_cast<int>(4.0f * uiScale));
    const int maxKnob = static_cast<int>(std::min({ w * 0.72f, 62.0f * uiScale, static_cast<float>(availH) }));
    const int xi = static_cast<int>(x);
    const int yi = static_cast<int>(y);
    const int wi = static_cast<int>(w);
    const int knobX = xi + (wi - maxKnob) / 2;
    const int knobY = yi + titleH + (availH - maxKnob) / 2;

    titleLabel.setBounds(xi, yi, wi, titleH);
    slider.setBounds(knobX, knobY, maxKnob, maxKnob);
    valueLabel.setBounds(xi, yi + titleH + availH + static_cast<int>(2.0f * uiScale), wi, valH);
}

// ==============================================================================
// TactileFaderBar
// ==============================================================================
void TactileFaderBar::init(juce::Component& parent,
                           juce::AudioProcessorValueTreeState& apvts,
                           const juce::String& paramID,
                           const juce::String& title,
                           juce::Colour accent,
                           bool bipolar,
                           bool darkText,
                           double defaultVal,
                           std::function<juce::String(double)> fmt,
                           const juce::String& infoHint)
{
    title_      = title;
    accent_     = accent;
    bipolar_    = bipolar;
    darkText_   = darkText;
    defaultVal_ = defaultVal;
    fmt_        = std::move(fmt);
    param_      = apvts.getParameter(paramID);
    setWantsKeyboardFocus(false);
    if (infoHint.isNotEmpty())
        setTooltip(infoHint);

    if (param_ != nullptr)
    {
        att_ = std::make_unique<juce::ParameterAttachment>(*param_, [this](float v)
        {
            currentVal_ = static_cast<double>(v);
            repaint();
        });
        att_->sendInitialUpdate();
    }
    parent.addAndMakeVisible(*this);
}

juce::String TactileFaderBar::getFormattedValue() const
{
    return fmt_ ? fmt_(currentVal_) : juce::String(currentVal_, 1);
}

void TactileFaderBar::setThemeColours(juce::Colour accent, bool darkText) noexcept
{
    accent_   = accent;
    darkText_ = darkText;
    repaint();
}

void TactileFaderBar::paint(juce::Graphics& g)
{
    const float sc = uiScale_;
    const auto r = getLocalBounds().toFloat().reduced(1.0f * sc);
    const float alpha = dimmed_ ? 0.35f : 1.0f;

    // Outer housing
    const auto bgCol = darkText_ ? KalyxTheme::dayControlIdleBg : KalyxTheme::nightControlIdleBg;
    const auto bdCol = darkText_ ? KalyxTheme::dayGridBorder : KalyxTheme::nightGridBorder;
    g.setColour(bgCol.withAlpha(alpha));
    g.fillRoundedRectangle(r, 4.0f * sc);
    g.setColour(bdCol.withAlpha(alpha));
    g.drawRoundedRectangle(r, 4.0f * sc, 1.0f * sc);

    // Thick hardware recessed rail/track at the bottom half of the bar
    const auto railR = juce::Rectangle<float>(r.getX() + 4.0f * sc, r.getBottom() - 8.5f * sc,
                                              std::max(10.0f, r.getWidth() - 8.0f * sc), 6.0f * sc);
    const auto railBg = darkText_ ? KalyxTheme::dayKnobTrackBg : KalyxTheme::nightChassisBg;
    g.setColour(railBg.withAlpha(alpha));
    g.fillRoundedRectangle(railR, 2.5f * sc);

    if (param_ != nullptr)
    {
        const float norm = param_->convertTo0to1(static_cast<float>(currentVal_));
        const auto fillCol = (dimmed_ ? juce::Colours::grey : accent_).withAlpha(alpha);
        g.setColour(fillCol);
        float headX = railR.getX();
        if (bipolar_)
        {
            const float cx = railR.getCentreX();
            const float vx = railR.getX() + norm * railR.getWidth();
            headX = vx;
            const auto fillR = juce::Rectangle<float>(std::min(cx, vx), railR.getY(),
                                                      std::max(2.0f, std::abs(vx - cx)), railR.getHeight());
            g.fillRoundedRectangle(fillR, 2.0f * sc);
            g.setColour((darkText_ ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary).withAlpha(0.35f * alpha));
            g.drawLine(cx, railR.getY() - 1.5f * sc, cx, railR.getBottom() + 1.5f * sc, 1.2f * sc);
        }
        else
        {
            const float fw = std::max(3.0f, norm * railR.getWidth());
            headX = railR.getX() + fw;
            const auto fillR = railR.withWidth(fw);
            g.fillRoundedRectangle(fillR, 2.0f * sc);
        }

        // Crisp thick head demarcation cap
        g.setColour((darkText_ ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary).withAlpha(0.94f * alpha));
        g.fillRoundedRectangle(headX - 2.2f * sc, railR.getY() - 2.0f * sc, 4.4f * sc, railR.getHeight() + 4.0f * sc, 1.5f * sc);
    }

    const auto textCol = (darkText_ ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary).withAlpha(alpha);
    const auto textArea = r.withTrimmedBottom(9.5f * sc).reduced(5.0f * sc, 0.0f);
    g.setColour(textCol.withAlpha(0.82f * alpha));
    g.setFont(KalyxTheme::makeFont(9.5f * sc, juce::Font::bold));
    g.drawFittedText(title_, textArea.toNearestInt(), juce::Justification::centredLeft, 1);

    const juce::String valStr = getFormattedValue();
    g.setColour(textCol);
    g.setFont(KalyxTheme::makeMonoFont(10.0f * sc, juce::Font::plain));
    g.drawFittedText(valStr, textArea.toNearestInt(), juce::Justification::centredRight, 1);
}

void TactileFaderBar::resized()
{
    if (editor_ != nullptr)
        editor_->setBounds(getLocalBounds().removeFromRight(getWidth() / 2).reduced(2));
}

void TactileFaderBar::mouseDown(const juce::MouseEvent& e)
{
    if (!isEnabled() || param_ == nullptr) return;
    dragStartX_   = static_cast<float>(e.x);
    dragStartVal_ = static_cast<double>(param_->convertTo0to1(static_cast<float>(currentVal_)));
    if (att_) att_->beginGesture();
}

void TactileFaderBar::mouseDrag(const juce::MouseEvent& e)
{
    if (!isEnabled() || param_ == nullptr) return;
    const float fine = (e.mods.isShiftDown() || e.mods.isCtrlDown()) ? 0.1f : 1.0f;
    const float dx = (static_cast<float>(e.x) - dragStartX_) * fine;
    const float w = std::max(40.0f, static_cast<float>(getWidth()));
    const float newNorm = std::clamp(static_cast<float>(dragStartVal_) + dx / w, 0.0f, 1.0f);
    if (att_) att_->setValueAsPartOfGesture(param_->convertFrom0to1(newNorm));
}

void TactileFaderBar::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (!isEnabled() || param_ == nullptr) return;
    if (e.x > getWidth() * 0.55f)
        openInlineEditor();
    else if (att_)
        att_->setValueAsCompleteGesture(static_cast<float>(defaultVal_));
}

void TactileFaderBar::openInlineEditor()
{
    editor_ = std::make_unique<juce::TextEditor>();
    editor_->setText(juce::String(currentVal_, 1), false);
    editor_->selectAll();
    editor_->onReturnKey = [this]() { closeInlineEditor(true); };
    editor_->onEscapeKey = [this]() { closeInlineEditor(false); };
    editor_->onFocusLost = [this]() { closeInlineEditor(true); };
    addAndMakeVisible(*editor_);
    resized();
    editor_->grabKeyboardFocus();
}

void TactileFaderBar::closeInlineEditor(bool commit)
{
    if (editor_ == nullptr) return;
    if (commit && param_ != nullptr && att_ != nullptr)
    {
        const float v = static_cast<float>(editor_->getText().getDoubleValue());
        const float clamped = param_->getNormalisableRange().snapToLegalValue(
            std::clamp(v, param_->getNormalisableRange().start, param_->getNormalisableRange().end));
        att_->setValueAsCompleteGesture(clamped);
    }
    editor_.reset();
    repaint();
}

// ==============================================================================
// VerticalConsoleFader
// ==============================================================================
void VerticalConsoleFader::init(juce::Component& parent,
                                juce::AudioProcessorValueTreeState& apvts,
                                const juce::String& paramID,
                                const juce::String& title,
                                juce::Colour accent,
                                bool darkText,
                                double minDb,
                                double maxDb,
                                double defaultDb)
{
    title_     = title;
    accent_    = accent;
    darkText_  = darkText;
    minDb_     = minDb;
    maxDb_     = maxDb;
    defaultDb_ = defaultDb;
    param_     = apvts.getParameter(paramID);
    if (param_ != nullptr)
    {
        att_ = std::make_unique<juce::ParameterAttachment>(*param_, [this](float v)
        {
            currentDb_ = static_cast<double>(v);
            repaint();
        });
        att_->sendInitialUpdate();
    }
    parent.addAndMakeVisible(*this);
}

void VerticalConsoleFader::setThemeColours(juce::Colour accent, bool darkText) noexcept
{
    accent_   = accent;
    darkText_ = darkText;
    repaint();
}

float VerticalConsoleFader::dbToY(double db, const juce::Rectangle<float>& track) const noexcept
{
    const float norm = static_cast<float>(std::clamp((db - minDb_) / (maxDb_ - minDb_), 0.0, 1.0));
    return track.getBottom() - norm * track.getHeight();
}

double VerticalConsoleFader::yToDb(float y, const juce::Rectangle<float>& track) const noexcept
{
    const double norm = std::clamp(static_cast<double>(track.getBottom() - y) / std::max(1.0, static_cast<double>(track.getHeight())), 0.0, 1.0);
    return minDb_ + norm * (maxDb_ - minDb_);
}

void VerticalConsoleFader::paint(juce::Graphics& g)
{
    const float sc = uiScale_;
    auto r = getLocalBounds().toFloat();
    const auto textCol = darkText_ ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary;

    g.setColour(textCol.withAlpha(0.78f));
    g.setFont(KalyxTheme::makeFont(10.5f * sc, juce::Font::bold));
    g.drawFittedText(title_, r.withHeight(16.0f * sc).toNearestInt(), juce::Justification::centred, 1);

    trackRect_ = r.withTrimmedTop(18.0f * sc).withTrimmedBottom(20.0f * sc).withSizeKeepingCentre(8.0f * sc, std::max(30.0f, r.getHeight() - 38.0f * sc));
    valueRect_ = r.removeFromBottom(18.0f * sc);

    g.setColour(darkText_ ? KalyxTheme::dayKnobTrackBg : KalyxTheme::knobTrackBg);
    g.fillRoundedRectangle(trackRect_, 4.0f * sc);

    // 0 dB tick
    const float y0 = dbToY(0.0, trackRect_);
    g.setColour(textCol.withAlpha(0.45f));
    g.drawLine(trackRect_.getX() - 6.0f * sc, y0, trackRect_.getRight() + 6.0f * sc, y0, 1.2f * sc);

    // Fill from bottom to currentDb
    const float yVal = dbToY(currentDb_, trackRect_);
    g.setColour(accent_);
    g.fillRoundedRectangle(trackRect_.getX(), yVal, trackRect_.getWidth(), trackRect_.getBottom() - yVal, 3.0f * sc);

    // Fader cap
    const auto capR = juce::Rectangle<float>(trackRect_.getCentreX() - 12.0f * sc, yVal - 6.0f * sc, 24.0f * sc, 12.0f * sc);
    g.setColour(darkText_ ? KalyxTheme::dayControlIdleBg : KalyxTheme::nightControlIdleBg);
    g.fillRoundedRectangle(capR, 3.0f * sc);
    g.setColour(accent_);
    g.drawRoundedRectangle(capR, 3.0f * sc, 1.3f * sc);
    g.drawLine(capR.getX() + 3.0f * sc, capR.getCentreY(), capR.getRight() - 3.0f * sc, capR.getCentreY(), 1.5f * sc);

    g.setColour(textCol);
    g.setFont(KalyxTheme::makeMonoFont(10.5f * sc, juce::Font::plain));
    const juce::String dbStr = (currentDb_ <= minDb_ + 0.1) ? "-inf dB" : KalyxTheme::formatDbClean(currentDb_);
    g.drawFittedText(dbStr, valueRect_.toNearestInt(), juce::Justification::centred, 1);
}

void VerticalConsoleFader::resized()
{
    if (editor_ != nullptr)
        editor_->setBounds(valueRect_.toNearestInt());
}

void VerticalConsoleFader::mouseDown(const juce::MouseEvent& e)
{
    if (param_ == nullptr) return;
    dragStartY_  = static_cast<float>(e.y);
    dragStartDb_ = currentDb_;
    if (att_) att_->beginGesture();
}

void VerticalConsoleFader::mouseDrag(const juce::MouseEvent& e)
{
    if (param_ == nullptr) return;
    const float fine = (e.mods.isShiftDown() || e.mods.isCtrlDown()) ? 0.1f : 1.0f;
    const double dy = static_cast<double>(dragStartY_ - static_cast<float>(e.y)) * fine;
    const double range = maxDb_ - minDb_;
    double nextDb = std::clamp(dragStartDb_ + (dy / std::max(40.0f, trackRect_.getHeight())) * range, minDb_, maxDb_);
    if (std::abs(nextDb) < 0.35 && fine > 0.5f)
        nextDb = 0.0; // 0 dB magnetic snap
    if (att_) att_->setValueAsPartOfGesture(static_cast<float>(nextDb));
}

void VerticalConsoleFader::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (param_ == nullptr) return;
    if (valueRect_.contains(e.position))
        openInlineEditor();
    else if (att_)
        att_->setValueAsCompleteGesture(static_cast<float>(defaultDb_));
}

void VerticalConsoleFader::openInlineEditor()
{
    editor_ = std::make_unique<juce::TextEditor>();
    editor_->setText(juce::String(currentDb_, 1), false);
    editor_->selectAll();
    editor_->onReturnKey = [this]() { closeInlineEditor(true); };
    editor_->onEscapeKey = [this]() { closeInlineEditor(false); };
    editor_->onFocusLost = [this]() { closeInlineEditor(true); };
    addAndMakeVisible(*editor_);
    resized();
    editor_->grabKeyboardFocus();
}

void VerticalConsoleFader::closeInlineEditor(bool commit)
{
    if (editor_ == nullptr) return;
    if (commit && att_ != nullptr)
    {
        const double v = std::clamp(editor_->getText().getDoubleValue(), minDb_, maxDb_);
        att_->setValueAsCompleteGesture(static_cast<float>(v));
    }
    editor_.reset();
    repaint();
}

// ==============================================================================
// KalyxSegmentedPill
// ==============================================================================
KalyxSegmentedPill::KalyxSegmentedPill(juce::AudioProcessorValueTreeState& apvts,
                                       const juce::String& paramID,
                                       const juce::String& prefixTitle,
                                       const juce::StringArray& choices,
                                       juce::Colour activeColour,
                                       bool darkText)
    : prefix_(prefixTitle), choices_(choices), activeCol_(activeColour), darkText_(darkText)
{
    paramPtr_ = apvts.getParameter(paramID);
    if (paramPtr_ != nullptr)
    {
        att_ = std::make_unique<juce::ParameterAttachment>(*paramPtr_, [this](float v)
        {
            currentIdx_ = static_cast<int>(std::round(v));
            repaint();
        });
        att_->sendInitialUpdate();
    }
}

void KalyxSegmentedPill::setThemeColours(juce::Colour activeColour, bool darkText) noexcept
{
    activeCol_ = activeColour;
    darkText_  = darkText;
    repaint();
}

void KalyxSegmentedPill::paint(juce::Graphics& g)
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

    auto area = r;
    if (prefix_.isNotEmpty())
    {
        const float pw = std::min(72.0f * sc, area.getWidth() * 0.28f);
        auto pr = area.removeFromLeft(pw);
        g.setColour(tx.withAlpha(0.68f));
        g.setFont(KalyxTheme::makeFont(10.0f * sc, juce::Font::bold));
        g.drawFittedText(prefix_, pr.toNearestInt(), juce::Justification::centred, 1);
    }

    const int n = std::max(1, choices_.size());
    const float segW = area.getWidth() / static_cast<float>(n);
    for (int i = 0; i < n; ++i)
    {
        auto seg = juce::Rectangle<float>(area.getX() + i * segW, area.getY(), segW, area.getHeight()).reduced(1.5f * sc);
        const bool sel = (i == currentIdx_);
        if (sel)
        {
            g.setColour(activeCol_);
            g.fillRoundedRectangle(seg, 3.0f * sc);
        }
        g.setColour(sel ? KalyxTheme::nightTextPrimary : tx.withAlpha(0.72f));
        g.setFont(KalyxTheme::makeFont(10.0f * sc, sel ? juce::Font::bold : juce::Font::plain));
        g.drawFittedText(choices_[i], seg.toNearestInt(), juce::Justification::centred, 1);
    }
}

void KalyxSegmentedPill::mouseDown(const juce::MouseEvent& e)
{
    if (paramPtr_ == nullptr || choices_.isEmpty()) return;
    const float sc = uiScale_;
    auto area = getLocalBounds().toFloat().reduced(1.0f * sc);
    if (prefix_.isNotEmpty())
        area.removeFromLeft(std::min(72.0f * sc, area.getWidth() * 0.28f));

    if (static_cast<float>(e.x) < area.getX()) return;
    const int n = choices_.size();
    const int idx = std::clamp(static_cast<int>((static_cast<float>(e.x) - area.getX()) / (area.getWidth() / n)), 0, n - 1);
    if (att_)
        att_->setValueAsCompleteGesture(static_cast<float>(idx));
}

// ==============================================================================
// AnalogVUMeter
// ==============================================================================
void AnalogVUMeter::setReadings(float inDb, float outDb, float grDb) noexcept
{
    inDb_  = inDb;
    outDb_ = outDb;
    grDb_  = grDb;
    repaint();
}

void AnalogVUMeter::paint(juce::Graphics& g)
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

    float vuVal = 0.0f;
    juce::String readout;
    if (mode_ == 0)
    {
        vuVal = inDb_ - refDb_;
        readout = "IN " + KalyxTheme::formatDbClean(inDb_);
    }
    else if (mode_ == 1)
    {
        vuVal = outDb_ - refDb_;
        readout = "OUT " + KalyxTheme::formatDbClean(outDb_);
    }
    else
    {
        vuVal = grDb_; // 0 to -20
        readout = "GR " + KalyxTheme::formatDbClean(grDb_);
    }

    // Arc scale -20 to +3 VU
    const float cx = r.getCentreX();
    const float cy = r.getBottom() - 10.0f * sc;
    const float rad = std::min(r.getWidth() * 0.44f, r.getHeight() * 0.72f);
    const float startA = -0.82f;
    const float endA   =  0.82f;

    juce::Path arc;
    arc.addCentredArc(cx, cy, rad, rad, 0.0f, startA, endA, true);
    g.setColour(tx.withAlpha(0.28f));
    g.strokePath(arc, juce::PathStrokeType(1.5f * sc));

    // Red zone (+0 to +3 VU)
    const float zeroA = startA + (20.0f / 23.0f) * (endA - startA);
    juce::Path redArc;
    redArc.addCentredArc(cx, cy, rad, rad, 0.0f, zeroA, endA, true);
    g.setColour(KalyxTheme::alertRed);
    g.strokePath(redArc, juce::PathStrokeType(2.2f * sc));

    const float norm = (mode_ == 2)
                           ? std::clamp(1.0f + vuVal / 20.0f, 0.0f, 1.0f)
                           : std::clamp((vuVal + 20.0f) / 23.0f, 0.0f, 1.0f);
    const float needleA = startA + norm * (endA - startA);

    g.setColour(KalyxTheme::classicAmber);
    g.drawLine(cx, cy, cx + std::sin(needleA) * rad, cy - std::cos(needleA) * rad, 2.0f * sc);

    g.setColour(tx);
    g.setFont(KalyxTheme::makeMonoFont(10.0f * sc, juce::Font::bold));
    g.drawFittedText(readout, r.withTrimmedTop(4.0f * sc).withHeight(14.0f * sc).toNearestInt(), juce::Justification::centred, 1);
}

// ==============================================================================
// VectorPad2D
// ==============================================================================
void VectorPad2D::init(juce::Component& parent,
                       juce::AudioProcessorValueTreeState& apvts,
                       const juce::String& paramXId,
                       const juce::String& paramYId,
                       const juce::String& labelX,
                       const juce::String& labelY,
                       juce::Colour accent,
                       bool darkText,
                       float defX,
                       float defY)
{
    labelX_ = labelX;
    labelY_ = labelY;
    accent_ = accent;
    darkText_ = darkText;
    defX_ = defX;
    defY_ = defY;

    paramX_ = apvts.getParameter(paramXId);
    paramY_ = apvts.getParameter(paramYId);
    if (paramX_ != nullptr)
    {
        attX_ = std::make_unique<juce::ParameterAttachment>(*paramX_, [this](float v)
        {
            normX_ = paramX_->convertTo0to1(v);
            repaint();
        });
        attX_->sendInitialUpdate();
    }
    if (paramY_ != nullptr)
    {
        attY_ = std::make_unique<juce::ParameterAttachment>(*paramY_, [this](float v)
        {
            normY_ = paramY_->convertTo0to1(v);
            repaint();
        });
        attY_->sendInitialUpdate();
    }
    parent.addAndMakeVisible(*this);
}

void VectorPad2D::setThemeColours(juce::Colour accent, bool darkText) noexcept
{
    accent_ = accent;
    darkText_ = darkText;
    repaint();
}

void VectorPad2D::paint(juce::Graphics& g)
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

    const auto pad = r.reduced(6.0f * sc);
    g.setColour(tx.withAlpha(0.12f));
    g.drawLine(pad.getCentreX(), pad.getY(), pad.getCentreX(), pad.getBottom(), 1.0f * sc);
    g.drawLine(pad.getX(), pad.getCentreY(), pad.getRight(), pad.getCentreY(), 1.0f * sc);

    const float px = pad.getX() + normX_ * pad.getWidth();
    const float py = pad.getBottom() - normY_ * pad.getHeight();
    g.setColour(accent_.withAlpha(0.28f));
    g.fillEllipse(px - 8.0f * sc, py - 8.0f * sc, 16.0f * sc, 16.0f * sc);
    g.setColour(accent_);
    g.fillEllipse(px - 4.0f * sc, py - 4.0f * sc, 8.0f * sc, 8.0f * sc);

    g.setColour(tx.withAlpha(0.68f));
    g.setFont(KalyxTheme::makeMonoFont(8.5f * sc, juce::Font::plain));
    g.drawFittedText(labelX_ + " x " + labelY_, pad.toNearestInt(), juce::Justification::bottomLeft, 1);
}

void VectorPad2D::mouseDown(const juce::MouseEvent& e) { mouseDrag(e); }
void VectorPad2D::mouseDrag(const juce::MouseEvent& e)
{
    const auto pad = getLocalBounds().toFloat().reduced(6.0f * uiScale_);
    const float nx = std::clamp((static_cast<float>(e.x) - pad.getX()) / std::max(10.0f, pad.getWidth()), 0.0f, 1.0f);
    const float ny = std::clamp((pad.getBottom() - static_cast<float>(e.y)) / std::max(10.0f, pad.getHeight()), 0.0f, 1.0f);
    if (paramX_ && attX_) attX_->setValueAsCompleteGesture(paramX_->convertFrom0to1(nx));
    if (paramY_ && attY_) attY_->setValueAsCompleteGesture(paramY_->convertFrom0to1(ny));
}
void VectorPad2D::mouseDoubleClick(const juce::MouseEvent&)
{
    if (paramX_ && attX_) attX_->setValueAsCompleteGesture(paramX_->convertFrom0to1(defX_));
    if (paramY_ && attY_) attY_->setValueAsCompleteGesture(paramY_->convertFrom0to1(defY_));
}

} // namespace kalyx::bass::ui
