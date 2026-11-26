#include "KalyxLookAndFeel.h"
#include "../DSPShared/ZDFFilters.h"
#include "../Core/SlotRackDSP.h"
#include <cmath>
#include <algorithm>
#include <cstring>

// =============================================================================
// KalyxLookAndFeel
// =============================================================================
KalyxLookAndFeel::KalyxLookAndFeel()
{
    setChassisTheme(kalyx::core::ChassisTheme::Night);
}

void KalyxLookAndFeel::setChassisTheme(kalyx::core::ChassisTheme theme) noexcept
{
    theme_ = theme;
    const juce::Colour txtCol = KalyxTheme::getTextPrimary(theme_);

    setColour(juce::Slider::textBoxTextColourId,       txtCol);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId,               txtCol);

    setColour(juce::ComboBox::backgroundColourId,      KalyxTheme::getControlIdleBg(theme_));
    setColour(juce::ComboBox::textColourId,            txtCol);
    setColour(juce::ComboBox::outlineColourId,         KalyxTheme::getGridBorder(theme_));
    setColour(juce::ComboBox::arrowColourId,           txtCol);

    setColour(juce::PopupMenu::backgroundColourId,     KalyxTheme::getStripBg(theme_));
    setColour(juce::PopupMenu::textColourId,           txtCol);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, KalyxTheme::getAccent(theme_));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

juce::Label* KalyxLookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* l = juce::LookAndFeel_V4::createSliderTextBox(slider);
    l->setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f * uiScale_, juce::Font::bold)));
    l->setJustificationType(juce::Justification::centred);
    const juce::Colour txtCol = slider.findColour(juce::Slider::textBoxTextColourId);
    l->setColour(juce::Label::textColourId,            txtCol);
    l->setColour(juce::Label::backgroundColourId,      juce::Colours::transparentBlack);
    l->setColour(juce::Label::outlineColourId,         juce::Colours::transparentBlack);
    l->setColour(juce::TextEditor::textColourId,       KalyxTheme::getTextPrimary(theme_));
    l->setColour(juce::TextEditor::backgroundColourId, KalyxTheme::getControlIdleBg(theme_));
    l->setColour(juce::TextEditor::outlineColourId,    KalyxTheme::getGridBorder(theme_));
    return l;
}

void KalyxLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                        int x, int y, int width, int height,
                                        float sliderPosProportional,
                                        float rotaryStartAngle,
                                        float rotaryEndAngle,
                                        juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                               static_cast<float>(width), static_cast<float>(height)).reduced(1.0f);
    const float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    if (radius < 6.0f) return;

    const bool isEnabled = slider.isEnabled();
    const float alphaMul = isEnabled ? 1.0f : 0.35f;
    const auto centre    = bounds.getCentre();

    const juce::Colour rawArcCol = slider.findColour(juce::Slider::rotarySliderFillColourId);
    const juce::Colour arcCol    = (isEnabled ? rawArcCol : juce::Colour(0xff6b7280)).withMultipliedAlpha(alphaMul);
    const juce::Colour capCol    = slider.findColour(juce::Slider::thumbColourId);
    const juce::Colour tickTxt   = slider.findColour(juce::Slider::textBoxTextColourId).withMultipliedAlpha(alphaMul * 0.55f);
    const bool hasCustomCap      = !capCol.isTransparent();

    const int notchedDots = static_cast<int>(slider.getProperties().getWithDefault("kalyxNotchedDots", 0));
    const bool steppedActive = static_cast<bool>(slider.getProperties().getWithDefault("kalyxSteppedActive", true));
    const bool fineBypass = juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown()
                         || juce::ModifierKeys::getCurrentModifiersRealtime().isCtrlDown();
    const float strokeW   = std::max(1.2f, 1.8f * uiScale_);

    float drawProp = sliderPosProportional;
    if (notchedDots >= 3 && steppedActive && !fineBypass)
    {
        const int stepIndex = std::clamp(static_cast<int>(std::lround(sliderPosProportional * static_cast<float>(notchedDots - 1))),
                                         0, notchedDots - 1);
        drawProp = static_cast<float>(stepIndex) / static_cast<float>(notchedDots - 1);
    }
    const float angle = rotaryStartAngle + drawProp * (rotaryEndAngle - rotaryStartAngle);

    const float tickOuterR = radius * 0.98f;
    if (notchedDots >= 3 && notchedDots <= 8)
    {
        if (steppedActive)
        {
            for (int i = 0; i < notchedDots; ++i)
            {
                const float prop      = static_cast<float>(i) / static_cast<float>(notchedDots - 1);
                const float tickAngle = rotaryStartAngle + prop * (rotaryEndAngle - rotaryStartAngle);
                const float sinA      = std::sin(tickAngle);
                const float cosA      = -std::cos(tickAngle);
                const float dotR      = radius * 0.90f;
                const bool  isAct     = (std::abs(prop - drawProp) < 0.09f);
                const float dotSz     = isAct ? (4.0f * uiScale_) : (2.8f * uiScale_);
                g.setColour(isAct ? arcCol : tickTxt.withAlpha(0.75f * alphaMul));
                g.fillEllipse(centre.x + dotR * sinA - dotSz * 0.5f,
                              centre.y + dotR * cosA - dotSz * 0.5f, dotSz, dotSz);
            }
        }
    }
    else
    {
        const int numTicks = (notchedDots > 8 && steppedActive) ? notchedDots : 11;
        for (int i = 0; i < numTicks; ++i)
        {
            const float prop        = static_cast<float>(i) / static_cast<float>(numTicks - 1);
            const float tickAngle   = rotaryStartAngle + prop * (rotaryEndAngle - rotaryStartAngle);
            const bool isCenterTick = (i == numTicks / 2);
            const float tickInnerR  = isCenterTick ? (radius * 0.84f) : (radius * 0.90f);
            const float sinA        = std::sin(tickAngle);
            const float cosA        = -std::cos(tickAngle);

            g.setColour(isCenterTick ? tickTxt.withAlpha(0.90f * alphaMul) : tickTxt);
            g.drawLine(centre.x + tickInnerR * sinA, centre.y + tickInnerR * cosA,
                       centre.x + tickOuterR * sinA, centre.y + tickOuterR * cosA,
                       isCenterTick ? (1.4f * uiScale_) : (0.9f * uiScale_));
        }
    }

    // Piste d'arc Tactile Flat (#2E3340)
    const float arcRadius = radius * 0.78f;
    const float arcStroke = std::max(1.8f, 2.4f * uiScale_);
    juce::Path trackArc;
    trackArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    const juce::Colour trackCol = (theme_ == kalyx::core::ChassisTheme::Day)
                                      ? KalyxTheme::dayKnobTrackBg
                                      : KalyxTheme::knobTrackBg;
    g.setColour(trackCol.withMultipliedAlpha(alphaMul));
    g.strokePath(trackArc, juce::PathStrokeType(arcStroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (std::abs(angle - rotaryStartAngle) > 0.005f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);
        g.setColour(arcCol);
        g.strokePath(valueArc, juce::PathStrokeType(arcStroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Cercle plat Tactile Flat (#222630 par défaut ou capuchon référencé)
    const float bodyRadius = radius * 0.62f;
    const juce::Rectangle<float> bodyRect(centre.x - bodyRadius, centre.y - bodyRadius,
                                          bodyRadius * 2.0f, bodyRadius * 2.0f);
    const juce::Colour bodyCol = hasCustomCap ? capCol : KalyxTheme::getControlIdleBg(theme_);

    g.setColour(bodyCol.withMultipliedAlpha(alphaMul));
    g.fillEllipse(bodyRect);

    g.setColour(KalyxTheme::getGridBorder(theme_).brighter(0.25f).withMultipliedAlpha(alphaMul));
    g.drawEllipse(bodyRect, std::max(1.0f, 1.1f * uiScale_));

    // Aiguille vectorielle nette
    const bool lightCap = bodyCol.getPerceivedBrightness() > 0.65f;
    const float sinP    = std::sin(angle);
    const float cosP    = -std::cos(angle);
    g.setColour((lightCap ? KalyxTheme::dayTextPrimary : juce::Colours::white).withMultipliedAlpha(alphaMul));
    g.drawLine(centre.x + bodyRadius * 0.24f * sinP, centre.y + bodyRadius * 0.24f * cosP,
               centre.x + bodyRadius * 0.86f * sinP, centre.y + bodyRadius * 0.86f * cosP,
               strokeW);
}

void KalyxLookAndFeel::drawToggleButton(juce::Graphics& g,
                                        juce::ToggleButton& button,
                                        bool shouldDrawButtonAsHighlighted,
                                        bool)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    const bool isOn = button.getToggleState();

    const juce::Colour activeBg = button.findColour(juce::ToggleButton::tickColourId);
    const juce::Colour offTxt   = button.findColour(juce::ToggleButton::textColourId);
    const bool darkOffTxt       = offTxt.getPerceivedBrightness() < 0.4f;
    const juce::Colour idleBg   = darkOffTxt ? juce::Colour(0xffdfe3ea) : KalyxTheme::nightControlIdleBg;
    const float radius          = 3.5f * uiScale_;

    g.setColour(isOn ? activeBg : (shouldDrawButtonAsHighlighted ? idleBg.brighter(0.10f) : idleBg));
    g.fillRoundedRectangle(bounds, radius);

    g.setColour(isOn ? activeBg.brighter(0.25f) : KalyxTheme::getGridBorder(theme_));
    g.drawRoundedRectangle(bounds, radius, std::max(0.8f, 1.0f * uiScale_));

    const float dotR = 2.6f * uiScale_;
    const float dotX = bounds.getX() + 5.0f * uiScale_;
    g.setColour(isOn ? juce::Colours::white : juce::Colour(0xff525a6e));
    g.fillEllipse(dotX, bounds.getCentreY() - dotR, dotR * 2.0f, dotR * 2.0f);

    const bool lightActive = isOn && (activeBg.getPerceivedBrightness() > 0.65f);
    g.setColour(isOn ? (lightActive ? KalyxTheme::dayTextPrimary : juce::Colours::white) : offTxt);
    g.setFont(juce::Font(juce::FontOptions(10.2f * uiScale_, juce::Font::bold)));
    g.drawFittedText(button.getButtonText(),
                     bounds.withTrimmedLeft(dotX - bounds.getX() + dotR * 2.0f + 3.0f * uiScale_)
                           .withTrimmedRight(2.0f * uiScale_)
                           .toNearestInt(),
                     juce::Justification::centred, 1, 0.78f);
}

void KalyxLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                            juce::Button& button,
                                            const juce::Colour& backgroundColour,
                                            bool shouldDrawButtonAsHighlighted,
                                            bool)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    const float radius = 3.5f * uiScale_;
    g.setColour(shouldDrawButtonAsHighlighted ? backgroundColour.brighter(0.12f) : backgroundColour);
    g.fillRoundedRectangle(bounds, radius);
    g.setColour(KalyxTheme::getGridBorder(theme_));
    g.drawRoundedRectangle(bounds, radius, std::max(0.8f, 1.0f * uiScale_));
}

// =============================================================================
// KalyxRotaryControl
// =============================================================================
void KalyxRotaryControl::init(juce::Component& parent,
                              juce::AudioProcessorValueTreeState& apvts,
                              const juce::String& paramID,
                              const juce::String& title,
                              const juce::Colour& initArcColour,
                              const juce::Colour& capColour,
                              bool darkText,
                              double defaultResetValue,
                              std::function<juce::String(double)> valueFormatter,
                              const juce::String& infoHint,
                              std::function<double(const juce::String&)> valueParser)
{
    this->arcColour = initArcColour;
    const juce::Colour txtCol = darkText ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary;

    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 15);
    slider.setTextBoxIsEditable(true);
    slider.setColour(juce::Slider::rotarySliderFillColourId, initArcColour);
    slider.setColour(juce::Slider::thumbColourId,            capColour);
    slider.setColour(juce::Slider::textBoxTextColourId,       txtCol);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    // Shift / Ctrl + glisser : mode réglage fin (0.1x)
    slider.setVelocityBasedMode(false);
    slider.setVelocityModeParameters(0.10, 1, 0.05, true,
                                     static_cast<juce::ModifierKeys::Flags>(juce::ModifierKeys::shiftModifier | juce::ModifierKeys::ctrlAltCommandModifiers));
    slider.setMouseDragSensitivity(180);
    if (infoHint.isNotEmpty())
        slider.setTooltip(infoHint);
    parent.addAndMakeVisible(slider);

    titleLabel.setText(title, juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, txtCol);
    titleLabel.setFont(juce::Font(juce::FontOptions(11.5f * uiScale, juce::Font::bold)));
    if (infoHint.isNotEmpty())
        titleLabel.setTooltip(infoHint);
    parent.addAndMakeVisible(titleLabel);

    // IMPORTANT : Détacher l'ancien attachment AVANT d'en créer un nouveau (évite toute écriture croisée lors d'un rebind),
    // puis calibrer setDoubleClickReturnValue, textFromValueFunction & valueFromTextFunction APRÈS SliderAttachment
    attachment.reset();
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramID, slider);
    slider.setTextBoxIsEditable(true);
    slider.setDoubleClickReturnValue(true, defaultResetValue);
    if (valueFormatter)
    {
        slider.textFromValueFunction = std::move(valueFormatter);
        slider.updateText();
    }
    if (valueParser)
        slider.valueFromTextFunction = std::move(valueParser);
}

void KalyxRotaryControl::setUiScale(float s)
{
    uiScale = std::clamp(s, 0.75f, 2.20f);
    titleLabel.setFont(juce::Font(juce::FontOptions(11.5f * uiScale, juce::Font::bold)));
    slider.lookAndFeelChanged();
}

void KalyxRotaryControl::setTextDark(bool darkText)
{
    const juce::Colour txtCol = darkText ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary;
    slider.setColour(juce::Slider::textBoxTextColourId, txtCol);
    titleLabel.setColour(juce::Label::textColourId, txtCol);
    slider.lookAndFeelChanged();
}

void KalyxRotaryControl::setCapAndArcColour(juce::Colour newArcColour, juce::Colour capColour)
{
    arcColour = newArcColour;
    slider.setColour(juce::Slider::rotarySliderFillColourId, newArcColour);
    slider.setColour(juce::Slider::thumbColourId,            capColour);
    slider.repaint();
}

void KalyxRotaryControl::setDimmedDisabled(bool dimmedDisabled)
{
    const bool shouldBeEnabled = !dimmedDisabled;
    if (slider.isEnabled() != shouldBeEnabled)
    {
        slider.setEnabled(shouldBeEnabled);
        slider.setAlpha(dimmedDisabled ? 0.35f : 1.0f);
        titleLabel.setAlpha(dimmedDisabled ? 0.35f : 1.0f);
        slider.repaint();
    }
}

void KalyxRotaryControl::setBoundsTight(float x, float y, float w, float h, float maxDiameter)
{
    const float labelH   = 14.0f * uiScale;
    const float textH    = 15.0f * uiScale;
    const float propD    = std::min(w * 0.62f, h * 0.52f);
    const float maxKnobD = std::clamp(std::min({ w, h - labelH - textH, propD }), 18.0f * uiScale, maxDiameter * uiScale);
    const float totalUsedH = labelH + maxKnobD + textH;
    const float startY     = y + std::max(0.0f, (h - totalUsedH) * 0.5f);

    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false,
                           std::min(static_cast<int>(w), static_cast<int>(92.0f * uiScale)),
                           static_cast<int>(textH));
    titleLabel.setBounds(static_cast<int>(x), static_cast<int>(startY),
                         static_cast<int>(w), static_cast<int>(labelH));
    slider.setBounds(static_cast<int>(x), static_cast<int>(startY + labelH),
                     static_cast<int>(w), static_cast<int>(maxKnobD + textH));
}

// =============================================================================
// KalyxSegmentedPill (Pilule segmentée Tactile Flat pour choix exclusifs)
// =============================================================================
KalyxSegmentedPill::KalyxSegmentedPill(juce::AudioProcessorValueTreeState& apvts,
                                       const juce::String& paramID,
                                       const juce::String& prefixTitle,
                                       const juce::StringArray& choices,
                                       juce::Colour activeColour,
                                       bool darkText,
                                       const juce::String& infoHint)
    : prefix_(prefixTitle), choices_(choices), activeCol_(activeColour), darkText_(darkText)
{
    if (infoHint.isNotEmpty())
        setTooltip(infoHint);

    if (auto* param = apvts.getParameter(paramID))
    {
        paramPtr_ = param;
        paramAtt_ = std::make_unique<juce::ParameterAttachment>(
            *param,
            [this](float newVal)
            {
                const float norm = paramPtr_ ? paramPtr_->convertTo0to1(newVal) : newVal;
                currentIdx_ = kalyx::core::mapChoice(norm, choices_.size());
                repaint();
            },
            nullptr);
        paramAtt_->sendInitialUpdate();
    }
}

void KalyxSegmentedPill::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced(0.5f);
    const juce::Colour txtCol = darkText_ ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary;
    const float radius = 4.0f * uiScale_;

    float x = b.getX();
    if (prefix_.isNotEmpty())
    {
        const float preW = std::min(58.0f * uiScale_, b.getWidth() * 0.32f);
        g.setColour(txtCol.withAlpha(0.85f));
        g.setFont(juce::Font(juce::FontOptions(10.0f * uiScale_, juce::Font::bold)));
        g.drawFittedText(prefix_,
                         juce::Rectangle<int>(static_cast<int>(x), static_cast<int>(b.getY()),
                                              static_cast<int>(preW), static_cast<int>(b.getHeight())),
                         juce::Justification::centredLeft, 1, 0.78f);
        x += preW + 3.0f * uiScale_;
    }

    const juce::Rectangle<float> pill(x, b.getY(), b.getRight() - x, b.getHeight());
    const juce::Colour bgCol = darkText_ ? juce::Colour(0xffdfe3ea) : KalyxTheme::nightControlIdleBg;
    g.setColour(bgCol);
    g.fillRoundedRectangle(pill, radius);
    g.setColour(darkText_ ? KalyxTheme::dayGridBorder : KalyxTheme::nightGridBorder);
    g.drawRoundedRectangle(pill, radius, std::max(0.8f, 1.0f * uiScale_));

    const int n = std::max(1, choices_.size());
    const float segW = pill.getWidth() / static_cast<float>(n);
    g.setFont(juce::Font(juce::FontOptions(10.2f * uiScale_, juce::Font::bold)));

    for (int i = 0; i < n; ++i)
    {
        const juce::Rectangle<float> seg(pill.getX() + static_cast<float>(i) * segW,
                                         pill.getY(), segW, pill.getHeight());
        const bool isAct = (i == currentIdx_);
        if (isAct)
        {
            g.setColour(activeCol_);
            g.fillRoundedRectangle(seg.reduced(1.5f * uiScale_), std::max(2.0f, radius - 1.5f));
            const bool lightAct = activeCol_.getPerceivedBrightness() > 0.65f;
            g.setColour(lightAct ? KalyxTheme::dayTextPrimary : juce::Colours::white);
        }
        else
        {
            if (i > 0)
            {
                g.setColour((darkText_ ? KalyxTheme::dayGridBorder : KalyxTheme::nightGridBorder).withAlpha(0.7f));
                g.drawLine(seg.getX(), seg.getY() + 3.0f * uiScale_, seg.getX(), seg.getBottom() - 3.0f * uiScale_, 1.0f);
            }
            g.setColour(txtCol.withAlpha(0.68f));
        }

        g.drawFittedText(choices_[i], seg.reduced(2.0f * uiScale_, 0.0f).toNearestInt(),
                         juce::Justification::centred, 1, 0.72f);
    }
}

void KalyxSegmentedPill::mouseDown(const juce::MouseEvent& e)
{
    if (choices_.isEmpty() || !paramAtt_ || !paramPtr_) return;
    const auto b = getLocalBounds().toFloat().reduced(0.5f);
    float x = b.getX();
    if (prefix_.isNotEmpty())
        x += std::min(58.0f * uiScale_, b.getWidth() * 0.32f) + 3.0f * uiScale_;

    const juce::Rectangle<float> pill(x, b.getY(), b.getRight() - x, b.getHeight());
    const int n = choices_.size();
    const float relX = std::clamp(e.position.x - pill.getX(), 0.0f, std::max(1.0f, pill.getWidth() - 0.1f));
    const int idx = std::clamp(static_cast<int>(std::floor((relX / std::max(1.0f, pill.getWidth())) * static_cast<float>(n))), 0, n - 1);
    const float norm = kalyx::core::choiceToNorm(idx, n);
    paramAtt_->setValueAsCompleteGesture(paramPtr_->convertFrom0to1(norm));
}

// =============================================================================
// KalyxMultiTogglePill (Pilule segmentée multi-toggles indépendants)
// =============================================================================
KalyxMultiTogglePill::KalyxMultiTogglePill(juce::AudioProcessorValueTreeState& apvts,
                                           const juce::String& prefixTitle,
                                           const juce::StringArray& paramIDs,
                                           const juce::StringArray& labels,
                                           juce::Colour activeColour,
                                           bool darkText,
                                           const juce::String& infoHint)
    : prefix_(prefixTitle), labels_(labels), activeCol_(activeColour), darkText_(darkText)
{
    if (infoHint.isNotEmpty())
        setTooltip(infoHint);

    const int n = std::min({ 4, paramIDs.size(), labels.size() });
    for (int i = 0; i < n; ++i)
    {
        if (auto* param = apvts.getParameter(paramIDs[i]))
        {
            auto att = std::make_unique<juce::ParameterAttachment>(
                *param,
                [this, i](float newNormVal)
                {
                    states_[static_cast<std::size_t>(i)] = (newNormVal > 0.5f);
                    repaint();
                },
                nullptr);
            att->sendInitialUpdate();
            atts_.push_back(std::move(att));
        }
    }
}

void KalyxMultiTogglePill::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced(0.5f);
    const juce::Colour txtCol = darkText_ ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary;
    const float radius = 4.0f * uiScale_;

    float x = b.getX();
    if (prefix_.isNotEmpty())
    {
        const float preW = std::min(58.0f * uiScale_, b.getWidth() * 0.32f);
        g.setColour(txtCol.withAlpha(0.85f));
        g.setFont(juce::Font(juce::FontOptions(10.0f * uiScale_, juce::Font::bold)));
        g.drawFittedText(prefix_,
                         juce::Rectangle<int>(static_cast<int>(x), static_cast<int>(b.getY()),
                                              static_cast<int>(preW), static_cast<int>(b.getHeight())),
                         juce::Justification::centredLeft, 1, 0.78f);
        x += preW + 3.0f * uiScale_;
    }

    const juce::Rectangle<float> pill(x, b.getY(), b.getRight() - x, b.getHeight());
    const juce::Colour bgCol = darkText_ ? juce::Colour(0xffdfe3ea) : KalyxTheme::nightControlIdleBg;
    g.setColour(bgCol);
    g.fillRoundedRectangle(pill, radius);
    g.setColour(darkText_ ? KalyxTheme::dayGridBorder : KalyxTheme::nightGridBorder);
    g.drawRoundedRectangle(pill, radius, std::max(0.8f, 1.0f * uiScale_));

    const int n = std::max(1, static_cast<int>(atts_.size()));
    const float segW = pill.getWidth() / static_cast<float>(n);
    g.setFont(juce::Font(juce::FontOptions(9.8f * uiScale_, juce::Font::bold)));

    for (int i = 0; i < n; ++i)
    {
        const juce::Rectangle<float> seg(pill.getX() + static_cast<float>(i) * segW,
                                         pill.getY(), segW, pill.getHeight());
        const bool isAct = states_[static_cast<std::size_t>(i)];
        if (isAct)
        {
            g.setColour(activeCol_);
            g.fillRoundedRectangle(seg.reduced(1.5f * uiScale_), std::max(2.0f, radius - 1.5f));
            const bool lightAct = activeCol_.getPerceivedBrightness() > 0.65f;
            g.setColour(lightAct ? KalyxTheme::dayTextPrimary : juce::Colours::white);
        }
        else
        {
            if (i > 0)
            {
                g.setColour((darkText_ ? KalyxTheme::dayGridBorder : KalyxTheme::nightGridBorder).withAlpha(0.7f));
                g.drawLine(seg.getX(), seg.getY() + 3.0f * uiScale_, seg.getX(), seg.getBottom() - 3.0f * uiScale_, 1.0f);
            }
            g.setColour(txtCol.withAlpha(0.68f));
        }

        g.drawFittedText(labels_[i], seg.reduced(2.0f * uiScale_, 0.0f).toNearestInt(),
                         juce::Justification::centred, 1, 0.72f);
    }
}

void KalyxMultiTogglePill::mouseDown(const juce::MouseEvent& e)
{
    if (atts_.empty()) return;
    const auto b = getLocalBounds().toFloat().reduced(0.5f);
    float x = b.getX();
    if (prefix_.isNotEmpty())
        x += std::min(58.0f * uiScale_, b.getWidth() * 0.32f) + 3.0f * uiScale_;

    const juce::Rectangle<float> pill(x, b.getY(), b.getRight() - x, b.getHeight());
    const int n = static_cast<int>(atts_.size());
    const float relX = std::clamp(e.position.x - pill.getX(), 0.0f, std::max(1.0f, pill.getWidth() - 0.1f));
    const int idx = std::clamp(static_cast<int>(std::floor((relX / std::max(1.0f, pill.getWidth())) * static_cast<float>(n))), 0, n - 1);
    const bool nextState = !states_[static_cast<std::size_t>(idx)];
    atts_[static_cast<std::size_t>(idx)]->setValueAsCompleteGesture(nextState ? 1.0f : 0.0f);
}

// =============================================================================
// KalyxSteppedSelector (Sélecteur cranté / bouton cyclique pour paramètres discrets)
// =============================================================================
void KalyxSteppedSelector::init(juce::Component& parent,
                                juce::AudioProcessorValueTreeState& apvts,
                                const juce::String& paramID,
                                const juce::String& title,
                                const juce::StringArray& optionLabels,
                                const juce::Colour& accentColour,
                                bool darkText,
                                const juce::String& infoHint)
{
    choices = optionLabels;
    const juce::Colour txtCol = darkText ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary;

    titleLabel.setText(title, juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, txtCol);
    titleLabel.setFont(juce::Font(juce::FontOptions(11.5f * uiScale, juce::Font::bold)));
    if (infoHint.isNotEmpty())
        titleLabel.setTooltip(infoHint);
    parent.addAndMakeVisible(titleLabel);

    cycleBtn.setColour(juce::TextButton::buttonColourId, accentColour);
    cycleBtn.setColour(juce::TextButton::textColourOffId,
                       accentColour.getPerceivedBrightness() > 0.65f ? KalyxTheme::dayTextPrimary : juce::Colours::white);
    if (infoHint.isNotEmpty())
        cycleBtn.setTooltip(infoHint);
    parent.addAndMakeVisible(cycleBtn);

    if (auto* param = apvts.getParameter(paramID))
    {
        paramAtt = std::make_unique<juce::ParameterAttachment>(
            *param,
            [this](float newNormVal)
            {
                currentIdx = kalyx::core::mapChoice(newNormVal, choices.size());
                if (currentIdx >= 0 && currentIdx < choices.size())
                    cycleBtn.setButtonText(choices[currentIdx]);
            },
            nullptr);
        paramAtt->sendInitialUpdate();

        cycleBtn.onClick = [this]()
        {
            if (choices.isEmpty() || !paramAtt) return;
            const int nextIdx = (currentIdx + 1) % choices.size();
            paramAtt->setValueAsCompleteGesture(kalyx::core::choiceToNorm(nextIdx, choices.size()));
        };
    }
}

void KalyxSteppedSelector::setUiScale(float s)
{
    uiScale = std::clamp(s, 0.75f, 2.20f);
    titleLabel.setFont(juce::Font(juce::FontOptions(11.5f * uiScale, juce::Font::bold)));
}

void KalyxSteppedSelector::setBoundsCompact(float x, float y, float w, float h)
{
    const float lblH = 13.0f * uiScale;
    const float btnH = 22.0f * uiScale;
    const float startY = y + std::max(0.0f, (h - (lblH + btnH + 2.0f * uiScale)) * 0.5f);
    titleLabel.setBounds(static_cast<int>(x + 2.0f), static_cast<int>(startY),
                         static_cast<int>(w - 4.0f), static_cast<int>(lblH));
    cycleBtn.setBounds  (static_cast<int>(x + 3.0f), static_cast<int>(startY + lblH + 2.0f * uiScale),
                         static_cast<int>(w - 6.0f), static_cast<int>(btnH));
}

void KalyxSteppedSelector::setDimmedDisabled(bool dimmedDisabled)
{
    cycleBtn.setEnabled(!dimmedDisabled);
    cycleBtn.setAlpha(dimmedDisabled ? 0.35f : 1.0f);
    titleLabel.setAlpha(dimmedDisabled ? 0.35f : 1.0f);
}

// =============================================================================
// KalyxLedStackSwitch (API 2500 : 3 LEDs verticales + petit poussoir carré [ ■ ])
// =============================================================================
KalyxLedStackSwitch::KalyxLedStackSwitch(juce::AudioProcessorValueTreeState& apvts,
                                         const juce::String& paramID,
                                         const juce::String& title,
                                         const juce::StringArray& labels,
                                         juce::Colour ledColour,
                                         bool darkText,
                                         const juce::String& infoHint)
    : title_(title), labels_(labels), ledColour_(ledColour), darkText_(darkText)
{
    if (infoHint.isNotEmpty())
        setTooltip(infoHint);

    if (auto* param = apvts.getParameter(paramID))
    {
        paramAtt_ = std::make_unique<juce::ParameterAttachment>(
            *param,
            [this](float newNormVal)
            {
                currentIdx_ = kalyx::core::mapChoice(newNormVal, labels_.size());
                repaint();
            },
            nullptr);
        paramAtt_->sendInitialUpdate();
    }
}

void KalyxLedStackSwitch::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const juce::Colour txtCol = darkText_ ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary;

    // Titre centré en haut (proportionnel à la largeur de la pile)
    const float titlePt = std::clamp(b.getWidth() * 0.175f, 9.2f, 11.2f);
    g.setColour(txtCol);
    g.setFont(juce::Font(juce::FontOptions(titlePt, juce::Font::bold)));
    g.drawFittedText(title_, juce::Rectangle<int>(0, 0, getWidth(), 13), juce::Justification::centred, 1, 0.80f);

    // 3 petites LEDs rondes alignées verticalement + libellé à droite
    const int n = std::max(1, labels_.size());
    const float ledTop = 14.0f;
    const float btnH   = 15.0f;
    const float stackH = std::max(24.0f, b.getHeight() - ledTop - btnH - 4.0f);
    const float rowH   = stackH / static_cast<float>(n);
    const float r      = 2.8f;
    const float ledX   = std::max(4.5f, b.getWidth() * 0.11f);
    const float lblPt  = std::clamp(b.getWidth() * 0.185f, 10.5f, 12.0f);

    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), lblPt, juce::Font::bold)));
    for (int i = 0; i < n; ++i)
    {
        const float cy = ledTop + (static_cast<float>(i) + 0.5f) * rowH;
        const bool active = (i == currentIdx_);

        if (active)
        {
            g.setColour(ledColour_.withAlpha(0.35f));
            g.fillEllipse(ledX - r - 2.0f, cy - r - 2.0f, (r + 2.0f) * 2.0f, (r + 2.0f) * 2.0f);
            g.setColour(ledColour_);
        }
        else
        {
            g.setColour(juce::Colour(0xff2b303c));
        }
        g.fillEllipse(ledX - r, cy - r, r * 2.0f, r * 2.0f);
        g.setColour(juce::Colours::black.withAlpha(0.65f));
        g.drawEllipse(ledX - r, cy - r, r * 2.0f, r * 2.0f, 0.8f);

        g.setColour(active ? txtCol : txtCol.withAlpha(0.55f));
        g.drawFittedText(labels_[i],
                         juce::Rectangle<int>(static_cast<int>(ledX + r + 3.0f),
                                              static_cast<int>(cy - rowH * 0.5f),
                                              static_cast<int>(b.getWidth() - (ledX + r + 4.0f)),
                                              static_cast<int>(rowH)),
                         juce::Justification::centredLeft, 1, 0.72f);
    }

    // Petit bouton poussoir carré [ ■ ] centré en bas
    const float sq = std::min(16.0f, b.getWidth() * 0.42f);
    const juce::Rectangle<float> btnRect(b.getCentreX() - sq * 0.5f, b.getBottom() - btnH, sq, btnH - 1.0f);
    g.setColour(juce::Colour(0xff2c3240));
    g.fillRoundedRectangle(btnRect, 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.drawRoundedRectangle(btnRect, 2.0f, 1.0f);
    g.setColour(juce::Colour(0xff9aa5b1));
    g.fillRect(btnRect.reduced(4.0f));
}

void KalyxLedStackSwitch::mouseDown(const juce::MouseEvent&)
{
    if (labels_.isEmpty() || !paramAtt_) return;
    const int nextIdx = (currentIdx_ + 1) % labels_.size();
    paramAtt_->setValueAsCompleteGesture(kalyx::core::choiceToNorm(nextIdx, labels_.size()));
}

// =============================================================================
// KalyxArrowSelector (< SSL 4000 E >, ROUTING : < STEREO >)
// =============================================================================
KalyxArrowSelector::KalyxArrowSelector(juce::AudioProcessorValueTreeState& apvts,
                                       const juce::String& paramID,
                                       const juce::String& prefixLabel,
                                       const juce::StringArray& choices,
                                       juce::Colour accentColour,
                                       bool darkText,
                                       const juce::String& infoHint)
    : prefix_(prefixLabel), choices_(choices), accent_(accentColour), darkText_(darkText)
{
    if (infoHint.isNotEmpty())
        setTooltip(infoHint);

    if (auto* param = apvts.getParameter(paramID))
    {
        paramAtt_ = std::make_unique<juce::ParameterAttachment>(
            *param,
            [this](float newNormVal)
            {
                currentIdx_ = kalyx::core::mapChoice(newNormVal, choices_.size());
                repaint();
            },
            nullptr);
        paramAtt_->sendInitialUpdate();
    }
}

void KalyxArrowSelector::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced(0.5f);
    const juce::Colour txtCol = darkText_ ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary;
    const float radius = 4.0f * uiScale_;

    float x = b.getX();
    if (prefix_.isNotEmpty())
    {
        const float preW = std::min(58.0f * uiScale_, b.getWidth() * 0.32f);
        g.setColour(txtCol.withAlpha(0.85f));
        g.setFont(juce::Font(juce::FontOptions(10.0f * uiScale_, juce::Font::bold)));
        g.drawFittedText(prefix_, juce::Rectangle<int>(static_cast<int>(x), static_cast<int>(b.getY()),
                                                       static_cast<int>(preW), static_cast<int>(b.getHeight())),
                         juce::Justification::centredLeft, 1, 0.78f);
        x += preW + 3.0f * uiScale_;
    }

    const juce::Rectangle<float> box(x, b.getY(), b.getRight() - x, b.getHeight());
    g.setColour(darkText_ ? juce::Colour(0xffdfe3ea) : KalyxTheme::nightControlIdleBg);
    g.fillRoundedRectangle(box, radius);
    g.setColour(accent_.withAlpha(0.75f));
    g.drawRoundedRectangle(box, radius, std::max(0.8f, 1.0f * uiScale_));

    const float aw = 18.0f * uiScale_;
    leftArrow_  = { box.getX(), box.getY(), aw, box.getHeight() };
    rightArrow_ = { box.getRight() - aw, box.getY(), aw, box.getHeight() };

    g.setColour(accent_);
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.5f * uiScale_, juce::Font::bold)));
    g.drawText("<", leftArrow_.toNearestInt(), juce::Justification::centred);
    g.drawText(">", rightArrow_.toNearestInt(), juce::Justification::centred);

    const juce::String curTxt = (currentIdx_ >= 0 && currentIdx_ < choices_.size()) ? choices_[currentIdx_] : "-";
    g.setColour(txtCol);
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.5f * uiScale_, juce::Font::bold)));
    g.drawFittedText(curTxt, box.reduced(aw, 0.0f).toNearestInt(), juce::Justification::centred, 1);
}

void KalyxArrowSelector::mouseDown(const juce::MouseEvent& e)
{
    if (choices_.isEmpty() || !paramAtt_) return;
    int nextIdx = currentIdx_;
    if (leftArrow_.expanded(2.0f).contains(e.position))
        nextIdx = (currentIdx_ - 1 + choices_.size()) % choices_.size();
    else
        nextIdx = (currentIdx_ + 1) % choices_.size();
    paramAtt_->setValueAsCompleteGesture(kalyx::core::choiceToNorm(nextIdx, choices_.size()));
}

// =============================================================================
// KalyxRotaryStepSelector (Neve 5254 RATIO & TIMING : sélecteur rotatif cranté)
// =============================================================================
KalyxRotaryStepSelector::KalyxRotaryStepSelector(juce::AudioProcessorValueTreeState& apvts,
                                                 const juce::String& paramID,
                                                 const juce::String& title,
                                                 const juce::StringArray& stepLabels,
                                                 juce::Colour capColour,
                                                 bool darkText,
                                                 const juce::String& infoHint)
    : title_(title), labels_(stepLabels), capCol_(capColour), darkText_(darkText)
{
    if (infoHint.isNotEmpty())
        setTooltip(infoHint);

    if (auto* param = apvts.getParameter(paramID))
    {
        paramAtt_ = std::make_unique<juce::ParameterAttachment>(
            *param,
            [this](float newNormVal)
            {
                currentIdx_ = kalyx::core::mapChoice(newNormVal, labels_.size());
                repaint();
            },
            nullptr);
        paramAtt_->sendInitialUpdate();
    }
}

void KalyxRotaryStepSelector::setStepIndex(int idx)
{
    if (labels_.isEmpty() || !paramAtt_) return;
    const int clamped = std::clamp(idx, 0, labels_.size() - 1);
    if (clamped != currentIdx_)
        paramAtt_->setValueAsCompleteGesture(kalyx::core::choiceToNorm(clamped, labels_.size()));
}

void KalyxRotaryStepSelector::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const juce::Colour txtCol = darkText_ ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary;

    const float labelH   = 14.0f * uiScale_;
    const float valH     = 15.0f * uiScale_;
    const float propD    = std::min(b.getWidth() * 0.62f, b.getHeight() * 0.52f);
    const float maxKnobD = std::clamp(std::min({ b.getWidth(), b.getHeight() - labelH - valH, propD }),
                                      18.0f * uiScale_, 68.0f * uiScale_);
    const float totalH   = labelH + maxKnobD + valH;
    const float startY   = std::max(0.0f, (b.getHeight() - totalH) * 0.5f);

    // 1. Titre en haut (aligné avec KalyxRotaryControl)
    g.setColour(txtCol);
    g.setFont(juce::Font(juce::FontOptions(11.5f * uiScale_, juce::Font::bold)));
    g.drawFittedText(title_, juce::Rectangle<int>(0, static_cast<int>(startY), getWidth(), static_cast<int>(labelH)),
                     juce::Justification::centred, 1);

    // 2. Cadran rotatif cranté Tactile Flat au centre
    const auto dialArea  = juce::Rectangle<float>(0.0f, startY + labelH, b.getWidth(), maxKnobD);
    const float radius   = (maxKnobD - 2.0f) * 0.5f;
    const auto centre    = dialArea.getCentre();

    const int n = std::max(2, labels_.size());
    constexpr float startAng = -2.356f; // -135 deg
    constexpr float endAng   =  2.356f; // +135 deg

    // Points de crans autour du bouton
    for (int i = 0; i < n; ++i)
    {
        const float prop = static_cast<float>(i) / static_cast<float>(n - 1);
        const float ang  = startAng + prop * (endAng - startAng);
        const float sinA = std::sin(ang);
        const float cosA = -std::cos(ang);
        const bool act   = (i == currentIdx_);
        const float dotR = radius * 0.90f;
        const float sz   = act ? (4.2f * uiScale_) : (3.0f * uiScale_);
        g.setColour(act ? capCol_ : txtCol.withAlpha(0.45f));
        g.fillEllipse(centre.x + dotR * sinA - sz * 0.5f,
                      centre.y + dotR * cosA - sz * 0.5f, sz, sz);
    }

    // Cercle plat + arc actif
    const float arcR = radius * 0.78f;
    const float arcStroke = std::max(1.8f, 2.4f * uiScale_);
    juce::Path trackArc;
    trackArc.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, startAng, endAng, true);
    g.setColour(darkText_ ? KalyxTheme::dayKnobTrackBg : KalyxTheme::knobTrackBg);
    g.strokePath(trackArc, juce::PathStrokeType(arcStroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float curProp = static_cast<float>(currentIdx_) / static_cast<float>(n - 1);
    const float curAng  = startAng + curProp * (endAng - startAng);
    if (curProp > 0.01f)
    {
        juce::Path valArc;
        valArc.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, startAng, curAng, true);
        g.setColour(capCol_);
        g.strokePath(valArc, juce::PathStrokeType(arcStroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float bodyR = radius * 0.62f;
    const juce::Rectangle<float> bodyRect(centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour(darkText_ ? KalyxTheme::dayControlIdleBg : KalyxTheme::nightControlIdleBg);
    g.fillEllipse(bodyRect);
    g.setColour((darkText_ ? KalyxTheme::dayGridBorder : KalyxTheme::nightGridBorder).brighter(0.25f));
    g.drawEllipse(bodyRect, std::max(1.0f, 1.1f * uiScale_));

    const float sinP    = std::sin(curAng);
    const float cosP    = -std::cos(curAng);
    g.setColour(darkText_ ? KalyxTheme::dayTextPrimary : juce::Colours::white);
    g.drawLine(centre.x + bodyR * 0.24f * sinP, centre.y + bodyR * 0.24f * cosP,
               centre.x + bodyR * 0.86f * sinP, centre.y + bodyR * 0.86f * cosP,
               std::max(1.2f, 1.8f * uiScale_));

    // 3. Valeur du cran actif juste dessous en tabular-nums
    const juce::String curTxt = (currentIdx_ >= 0 && currentIdx_ < labels_.size()) ? labels_[currentIdx_] : "-";
    g.setColour(txtCol);
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f * uiScale_, juce::Font::bold)));
    g.drawFittedText(curTxt, juce::Rectangle<int>(0, static_cast<int>(startY + labelH + maxKnobD), getWidth(), static_cast<int>(valH)),
                     juce::Justification::centred, 1);
}

void KalyxRotaryStepSelector::mouseDown(const juce::MouseEvent& e)
{
    dragStartIdx_ = currentIdx_;
    dragStartY_   = e.position.y;
    dragged_      = false;
}

void KalyxRotaryStepSelector::mouseDrag(const juce::MouseEvent& e)
{
    const float dy = dragStartY_ - e.position.y;
    if (std::abs(dy) > 6.0f)
    {
        dragged_ = true;
        const int deltaSteps = static_cast<int>(std::round(dy / 14.0f));
        setStepIndex(dragStartIdx_ + deltaSteps);
    }
}

void KalyxRotaryStepSelector::mouseUp(const juce::MouseEvent&)
{
    if (!dragged_ && !labels_.isEmpty())
        setStepIndex((currentIdx_ + 1) % labels_.size());
}

// =============================================================================
// KalyxEQCurveComponent (lié à un slot 0..4)
// =============================================================================
KalyxEQCurveComponent::KalyxEQCurveComponent(KalyxHybridConsoleProcessor& proc, int slotIndex, bool darkText) noexcept
    : processor_(proc),
      slotIndex_(slotIndex),
      darkText_(darkText),
      forwardFFT_(KalyxHybridConsoleProcessor::kFFTOrder),
      windowing_(static_cast<std::size_t>(KalyxHybridConsoleProcessor::kFFTSize),
                 juce::dsp::WindowingFunction<float>::hann)
{
    fftData_.fill(0.0f);
    smoothedBinsDb_.fill(-100.0f);
}

void KalyxEQCurveComponent::updateFFTFromProcessor() noexcept
{
    const int fftMode = static_cast<int>(std::round(
        processor_.getAPVTS().getRawParameterValue(ParamIDs::fftMode)->load(std::memory_order_relaxed)));
    if (fftMode == 2) return;

    if (processor_.pullNextFFTBlock(fftData_.data()))
    {
        std::memset(fftData_.data() + KalyxHybridConsoleProcessor::kFFTSize, 0,
                    sizeof(float) * static_cast<std::size_t>(KalyxHybridConsoleProcessor::kFFTSize));
        windowing_.multiplyWithWindowingTable(fftData_.data(), static_cast<std::size_t>(KalyxHybridConsoleProcessor::kFFTSize));
        forwardFFT_.performFrequencyOnlyForwardTransform(fftData_.data());

        constexpr int numBins = KalyxHybridConsoleProcessor::kFFTSize / 2;
        constexpr float normFactor = 2.0f / static_cast<float>(KalyxHybridConsoleProcessor::kFFTSize);
        for (int b = 1; b < numBins; ++b)
        {
            const float mag    = fftData_[static_cast<std::size_t>(b)] * normFactor;
            const float tiltDb = 3.0f * std::log2(std::max(static_cast<float>(b) / 32.0f, 0.25f));
            const float db     = std::clamp(20.0f * std::log10(std::max(mag, 1e-5f)) + tiltDb, -90.0f, 6.0f);
            const float prev   = smoothedBinsDb_[static_cast<std::size_t>(b)];
            smoothedBinsDb_[static_cast<std::size_t>(b)] = (db > prev) ? (0.45f * prev + 0.55f * db)
                                                                       : (0.82f * prev + 0.18f * db);
        }
    }
    else
    {
        for (float& binDb : smoothedBinsDb_)
            binDb = std::max(-96.0f, binDb - 1.2f);
    }
}

juce::Rectangle<float> KalyxEQCurveComponent::getPlotArea() const noexcept
{
    return getLocalBounds().toFloat().withTrimmedBottom(14.0f).reduced(6.0f, 4.0f);
}

float KalyxEQCurveComponent::freqToX(float freqHz, const juce::Rectangle<float>& area) const noexcept
{
    return area.getX() + (std::log10(std::clamp(freqHz, 20.0f, 20000.0f) / 20.0f) / 3.0f) * area.getWidth();
}

float KalyxEQCurveComponent::xToFreq(float x, const juce::Rectangle<float>& area) const noexcept
{
    const float normX = std::clamp((x - area.getX()) / std::max(area.getWidth(), 1.0f), 0.0f, 1.0f);
    return 20.0f * std::pow(1000.0f, normX);
}

float KalyxEQCurveComponent::gainToY(float gainDb, const juce::Rectangle<float>& area) const noexcept
{
    return area.getY() + (0.5f - std::clamp(gainDb, -18.0f, 18.0f) / 36.0f) * area.getHeight();
}

float KalyxEQCurveComponent::yToGain(float y, const juce::Rectangle<float>& area) const noexcept
{
    const float normY = std::clamp((y - area.getY()) / std::max(area.getHeight(), 1.0f), 0.0f, 1.0f);
    return (0.5f - normY) * 36.0f;
}

void KalyxEQCurveComponent::setSlotNormParam(int paramOneBased, float normVal) noexcept
{
    if (auto* param = processor_.getAPVTS().getParameter(ParamIDs::slotParamId(slotIndex_, paramOneBased)))
        param->setValueNotifyingHost(std::clamp(normVal, 0.0f, 1.0f));
}

void KalyxEQCurveComponent::mouseDown(const juce::MouseEvent& e)
{
    if (processor_.isSectionLocked(slotIndex_ + 1)) return;
    const auto area   = getPlotArea();
    const int modId   = processor_.getSlotModuleId(slotIndex_);
    const auto p      = processor_.getSlotParamsForGUI(slotIndex_);
    const bool isNeve = (modId == static_cast<int>(kalyx::core::SlotModuleId::Neve1073EQ));
    const bool isApi  = (modId == static_cast<int>(kalyx::core::SlotModuleId::API550BEQ));

    std::array<juce::Point<float>, 4> nodes;
    if (isNeve)
    {
        nodes = {{
            { freqToX(kalyx::core::mapLog(p[1], 35.f, 220.f), area),   gainToY(kalyx::core::mapLin(p[2], -15.f, 15.f), area) },
            { freqToX(kalyx::core::mapLog(p[3], 360.f, 7200.f), area), gainToY(kalyx::core::mapLin(p[4], -15.f, 15.f), area) },
            { -100.f, -100.f },
            { freqToX(12000.f, area),                                  gainToY(kalyx::core::mapLin(p[6], -15.f, 15.f), area) }
        }};
    }
    else if (isApi)
    {
        nodes = {{
            { freqToX(kalyx::core::mapLog(p[1], 30.f, 450.f), area),      gainToY(kalyx::core::mapLin(p[2], -15.f, 15.f), area) },
            { freqToX(kalyx::core::mapLog(p[3], 200.f, 2500.f), area),    gainToY(kalyx::core::mapLin(p[4], -15.f, 15.f), area) },
            { -100.f, -100.f },
            { freqToX(kalyx::core::mapLog(p[9], 1500.f, 18000.f), area),  gainToY(kalyx::core::mapLin(p[10], -15.f, 15.f), area) }
        }};
    }
    else
    {
        nodes = {{
            { freqToX(kalyx::core::mapLog(p[1], 30.f, 450.f), area),     gainToY(kalyx::core::mapLin(p[2], -15.f, 15.f), area) },
            { freqToX(kalyx::core::mapLog(p[3], 200.f, 2500.f), area),   gainToY(kalyx::core::mapLin(p[4], -15.f, 15.f), area) },
            { freqToX(kalyx::core::mapLog(p[6], 600.f, 7000.f), area),   gainToY(kalyx::core::mapLin(p[7], -15.f, 15.f), area) },
            { freqToX(kalyx::core::mapLog(p[9], 1500.f, 18000.f), area), gainToY(kalyx::core::mapLin(p[10], -15.f, 15.f), area) }
        }};
    }

    draggedNodeIndex_ = -1;
    float bestD2 = 13.0f * 13.0f;
    for (int i = 0; i < 4; ++i)
    {
        const float dx = e.position.x - nodes[static_cast<std::size_t>(i)].x;
        const float dy = e.position.y - nodes[static_cast<std::size_t>(i)].y;
        if (dx * dx + dy * dy <= bestD2) { bestD2 = dx * dx + dy * dy; draggedNodeIndex_ = i; }
    }
}

void KalyxEQCurveComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (draggedNodeIndex_ < 0 || processor_.isSectionLocked(slotIndex_ + 1)) return;
    const auto area   = getPlotArea();
    const int modId   = processor_.getSlotModuleId(slotIndex_);
    const bool isNeve = (modId == static_cast<int>(kalyx::core::SlotModuleId::Neve1073EQ));
    const bool isApi  = (modId == static_cast<int>(kalyx::core::SlotModuleId::API550BEQ));

    const float f = xToFreq(e.position.x, area);
    const float g = std::clamp(yToGain(e.position.y, area), -15.0f, 15.0f);
    const float gNorm = kalyx::core::unmapLin(g, -15.0f, 15.0f);

    if (isNeve)
    {
        if (draggedNodeIndex_ == 0)      { setSlotNormParam(2, kalyx::core::unmapLog(f, 35.f, 220.f)); setSlotNormParam(3, gNorm); }
        else if (draggedNodeIndex_ == 1) { setSlotNormParam(4, kalyx::core::unmapLog(f, 360.f, 7200.f)); setSlotNormParam(5, gNorm); }
        else if (draggedNodeIndex_ == 3) { setSlotNormParam(7, gNorm); }
    }
    else
    {
        if (draggedNodeIndex_ == 0)      { setSlotNormParam(2, kalyx::core::unmapLog(f, 30.f, 450.f)); setSlotNormParam(3, gNorm); }
        else if (draggedNodeIndex_ == 1) { setSlotNormParam(4, kalyx::core::unmapLog(f, 200.f, 2500.f)); setSlotNormParam(5, gNorm); }
        else if (draggedNodeIndex_ == 2 && !isApi) { setSlotNormParam(7, kalyx::core::unmapLog(f, 600.f, 7000.f)); setSlotNormParam(8, gNorm); }
        else if (draggedNodeIndex_ == 3) { setSlotNormParam(10, kalyx::core::unmapLog(f, 1500.f, 18000.f)); setSlotNormParam(11, gNorm); }
    }
    repaint();
}

void KalyxEQCurveComponent::mouseUp(const juce::MouseEvent&) { draggedNodeIndex_ = -1; }

void KalyxEQCurveComponent::mouseDoubleClick(const juce::MouseEvent&)
{
    if (processor_.isSectionLocked(slotIndex_ + 1)) return;
    const int modId   = processor_.getSlotModuleId(slotIndex_);
    const bool isNeve = (modId == static_cast<int>(kalyx::core::SlotModuleId::Neve1073EQ));
    const bool isApi  = (modId == static_cast<int>(kalyx::core::SlotModuleId::API550BEQ));
    if (isNeve)     { setSlotNormParam(3, 0.5f); setSlotNormParam(5, 0.5f); setSlotNormParam(7, 0.5f); }
    else if (isApi) { setSlotNormParam(3, 0.5f); setSlotNormParam(5, 0.5f); setSlotNormParam(11, 0.5f); }
    else            { setSlotNormParam(3, 0.5f); setSlotNormParam(5, 0.5f); setSlotNormParam(8, 0.5f); setSlotNormParam(11, 0.5f); }
    repaint();
}

void KalyxEQCurveComponent::paint(juce::Graphics& g)
{
    const auto fullB  = getLocalBounds().toFloat();
    const auto frameB = fullB.withTrimmedBottom(14.0f);
    const auto area   = getPlotArea();

    g.setColour(darkText_ ? juce::Colour(0xffe8ecf2) : juce::Colour(0xff111318));
    g.fillRoundedRectangle(frameB, 3.5f);
    g.setColour(darkText_ ? KalyxTheme::dayGridBorder : KalyxTheme::nightGridBorder);
    g.drawRoundedRectangle(frameB.reduced(0.5f), 3.5f, 1.0f);

    if (area.getWidth() < 12.0f || area.getHeight() < 12.0f) return;

    const juce::Colour gridCol = darkText_ ? juce::Colour(0xffd0d5dd) : juce::Colour(0xff262a35);
    const juce::Colour zeroCol = darkText_ ? juce::Colour(0xff9aa3b2) : juce::Colour(0xff4c556a);
    g.setColour(gridCol);
    for (float f : { 100.f, 500.f, 1000.f, 5000.f, 10000.f })
        g.drawVerticalLine(static_cast<int>(freqToX(f, area)), area.getY(), area.getBottom());
    for (float db : { -12.f, 0.f, 12.f })
    {
        g.setColour(std::abs(db) < 0.1f ? zeroCol : gridCol);
        g.drawHorizontalLine(static_cast<int>(gainToY(db, area)), area.getX(), area.getRight());
    }

    const double fs = std::max(processor_.getCurrentSampleRate(), 44100.0);
    const int fftMode = static_cast<int>(std::round(processor_.getAPVTS().getRawParameterValue(ParamIDs::fftMode)->load(std::memory_order_relaxed)));
    if (fftMode != 2)
    {
        juce::Path fp;
        constexpr int nPts = 72;
        constexpr int nBins = KalyxHybridConsoleProcessor::kFFTSize / 2;
        for (int i = 0; i <= nPts; ++i)
        {
            const float nx = static_cast<float>(i) / static_cast<float>(nPts);
            const float fr = 20.0f * std::pow(1000.0f, nx);
            const int b0   = std::clamp(static_cast<int>(fr * KalyxHybridConsoleProcessor::kFFTSize / fs), 1, nBins - 2);
            const float ny = 1.0f - std::clamp((smoothedBinsDb_[static_cast<std::size_t>(b0)] + 72.0f) / 72.0f, 0.0f, 1.0f);
            const float px = area.getX() + nx * area.getWidth();
            const float py = area.getY() + ny * area.getHeight();
            if (i == 0) fp.startNewSubPath(px, py); else fp.lineTo(px, py);
        }
        g.setColour(KalyxTheme::kalyxAubergine.withAlpha(0.55f));
        g.strokePath(fp, juce::PathStrokeType(1.1f));
    }

    const int modId   = processor_.getSlotModuleId(slotIndex_);
    const auto p      = processor_.getSlotParamsForGUI(slotIndex_);
    const bool eqOn   = p[0] > 0.5f;
    const bool isNeve = (modId == static_cast<int>(kalyx::core::SlotModuleId::Neve1073EQ));
    const bool isApi  = (modId == static_cast<int>(kalyx::core::SlotModuleId::API550BEQ));
    const bool isG    = (modId == static_cast<int>(kalyx::core::SlotModuleId::SSL4000EQ) && p[14] > 0.5f);

    using Bq = kalyx::dsp::BiquadFilter;
    Bq::Coeffs c0, c1, c2, c3;
    std::array<juce::Point<float>, 4> nodes;

    if (isNeve)
    {
        const bool stepped = (p[10] > 0.5f);
        const float lfF = kalyx::core::SlotUnitDSP::quantizeNeve1073LowFreq(kalyx::core::mapLog(p[1], 35.f, 220.f), stepped);
        const float lfG = kalyx::core::mapLin(p[2], -15.f, 15.f);
        const float mF  = kalyx::core::SlotUnitDSP::quantizeNeve1073MidFreq(kalyx::core::mapLog(p[3], 360.f, 7200.f), stepped);
        const float mG  = kalyx::core::mapLin(p[4], -15.f, 15.f);
        const float mQ  = kalyx::core::mapLog(p[5], 0.4f, 3.5f) * 0.72f;
        const float hfG = kalyx::core::mapLin(p[6], -15.f, 15.f);
        c0 = Bq::makeLowShelf(fs, lfF, lfG, 0.65);
        c1 = Bq::makePeakEQ(fs, mF, mG, mQ);
        c2 = Bq::makePeakEQ(fs, 1000.0, 0.0, 1.0);
        c3 = Bq::makeHighShelf(fs, 12000.0, hfG, 0.62);
        nodes = {{ { freqToX(lfF, area), gainToY(lfG, area) }, { freqToX(mF, area), gainToY(mG, area) },
                   { -100.f, -100.f }, { freqToX(12000.f, area), gainToY(hfG, area) } }};
    }
    else
    {
        const bool step = isApi ? (p[14] > 0.5f) : (p[15] > 0.5f);
        const float lfF  = kalyx::core::SlotUnitDSP::quantizeApi550BFreq(0, kalyx::core::mapLog(p[1], 30.f, 450.f), step);
        const float lfG  = kalyx::core::SlotUnitDSP::quantizeApi550BGain(kalyx::core::mapLin(p[2], -15.f, 15.f), step);
        const float lmfF = kalyx::core::SlotUnitDSP::quantizeApi550BFreq(1, kalyx::core::mapLog(p[3], 200.f, 2500.f), step);
        const float lmfG = kalyx::core::SlotUnitDSP::quantizeApi550BGain(kalyx::core::mapLin(p[4], -15.f, 15.f), step);
        const float lmfQ = isApi ? kalyx::core::SlotUnitDSP::computeApi550BQ(kalyx::core::mapLog(p[5], 0.4f, 4.f), lmfG)
                                 : kalyx::core::SlotUnitDSP::computeSslEqQ(kalyx::core::mapLog(p[5], 0.4f, 4.f), lmfG, isG);
        const float hmfF = kalyx::core::SlotUnitDSP::quantizeApi550BFreq(2, kalyx::core::mapLog(p[6], 600.f, 7000.f), step);
        const float hmfG = kalyx::core::SlotUnitDSP::quantizeApi550BGain(kalyx::core::mapLin(p[7], -15.f, 15.f), step);
        const float hmfQ = kalyx::core::SlotUnitDSP::computeSslEqQ(kalyx::core::mapLog(p[8], 0.4f, 4.f), hmfG, isG);
        const float hfF  = kalyx::core::SlotUnitDSP::quantizeApi550BFreq(3, kalyx::core::mapLog(p[9], 1500.f, 18000.f), step);
        const float hfG  = kalyx::core::SlotUnitDSP::quantizeApi550BGain(kalyx::core::mapLin(p[10], -15.f, 15.f), step);

        c0 = (p[12] > 0.5f) ? Bq::makePeakEQ(fs, lfF, lfG, 0.95) : Bq::makeLowShelf(fs, lfF, lfG, 0.72);
        c1 = Bq::makePeakEQ(fs, lmfF, lmfG, lmfQ);
        c2 = isApi ? Bq::makePeakEQ(fs, 1000.0, 0.0, 1.0) : Bq::makePeakEQ(fs, hmfF, hmfG, hmfQ);
        c3 = (p[13] > 0.5f) ? Bq::makePeakEQ(fs, hfF, hfG, 0.95) : Bq::makeHighShelf(fs, hfF, hfG, 0.72);
        nodes = {{ { freqToX(lfF, area), gainToY(lfG, area) }, { freqToX(lmfF, area), gainToY(lmfG, area) },
                   isApi ? juce::Point<float>{ -100.f, -100.f } : juce::Point<float>{ freqToX(hmfF, area), gainToY(hmfG, area) },
                   { freqToX(hfF, area), gainToY(hfG, area) } }};
    }

    auto& apvts = processor_.getAPVTS();
    const bool inHpfOn   = (apvts.getRawParameterValue(ParamIDs::inHpfEn)->load(std::memory_order_relaxed) > 0.5f)
                        && (apvts.getRawParameterValue(ParamIDs::inFiltToSc)->load(std::memory_order_relaxed) <= 0.5f);
    const bool inLpfOn   = (apvts.getRawParameterValue(ParamIDs::inLpfEn)->load(std::memory_order_relaxed) > 0.5f)
                        && (apvts.getRawParameterValue(ParamIDs::inFiltToSc)->load(std::memory_order_relaxed) <= 0.5f);
    const float inHpfHz  = apvts.getRawParameterValue(ParamIDs::inHpfFreq)->load(std::memory_order_relaxed);
    const float inLpfHz  = apvts.getRawParameterValue(ParamIDs::inLpfFreq)->load(std::memory_order_relaxed);
    const auto cHpf      = Bq::makeHighPass(fs, inHpfHz, 0.7071);
    const auto cLpf      = Bq::makeLowPass(fs, inLpfHz, 0.7071);

    juce::Path totalPath;
    constexpr int nPts = 96;
    for (int i = 0; i <= nPts; ++i)
    {
        const float nx = static_cast<float>(i) / static_cast<float>(nPts);
        const double fr = 20.0 * std::pow(1000.0, static_cast<double>(nx));
        float db = 0.0f;
        if (eqOn)
        {
            db = Bq::getMagnitudeDbAtFrequency(c0, fr, fs)
               + Bq::getMagnitudeDbAtFrequency(c1, fr, fs)
               + ((isNeve || isApi) ? 0.0f : Bq::getMagnitudeDbAtFrequency(c2, fr, fs))
               + Bq::getMagnitudeDbAtFrequency(c3, fr, fs);
        }
        if (inHpfOn) db += 1.5f * Bq::getMagnitudeDbAtFrequency(cHpf, fr, fs);
        if (inLpfOn) db += Bq::getMagnitudeDbAtFrequency(cLpf, fr, fs);

        const float px = area.getX() + nx * area.getWidth();
        const float py = gainToY(db, area);
        if (i == 0) totalPath.startNewSubPath(px, py); else totalPath.lineTo(px, py);
    }

    g.setColour(darkText_ ? KalyxTheme::dayTextPrimary : juce::Colours::white);
    g.strokePath(totalPath, juce::PathStrokeType(1.8f));

    const juce::Colour nodeCols[4] = {
        KalyxTheme::eqLfWineRed, KalyxTheme::eqLmfSlateBlue,
        KalyxTheme::eqHmfJadeGreen, KalyxTheme::eqHfPearlSilver
    };
    for (int i = 0; i < 4; ++i)
    {
        if ((isNeve || isApi) && i == 2) continue;
        const auto pt = nodes[static_cast<std::size_t>(i)];
        g.setColour(nodeCols[i]);
        g.fillEllipse(pt.x - 4.5f, pt.y - 4.5f, 9.0f, 9.0f);
        g.setColour(darkText_ ? KalyxTheme::dayTextPrimary : juce::Colours::white);
        g.drawEllipse(pt.x - 4.5f, pt.y - 4.5f, 9.0f, 9.0f, 1.0f);
    }

    const int fy = static_cast<int>(frameB.getBottom() + 1.0f);
    g.setColour(darkText_ ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary);
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 8.5f, juce::Font::bold)));
    g.drawText("20",  static_cast<int>(area.getX()), fy, 24, 12, juce::Justification::left);
    g.drawText("1k",  static_cast<int>(freqToX(1000.f, area)) - 14, fy, 28, 12, juce::Justification::centred);
    g.drawText("20k", static_cast<int>(area.getRight() - 28), fy, 28, 12, juce::Justification::right);
}

// =============================================================================
// KalyxSettingsOverlayComponent
// =============================================================================
KalyxSettingsOverlayComponent::KalyxSettingsOverlayComponent(KalyxHybridConsoleProcessor& proc)
    : processor_(proc)
{
    addAndMakeVisible(closeBtn_);
    closeBtn_.onClick = [this]() { setVisible(false); };

    const float vuPresets[4] = { -20.0f, -18.0f, -14.0f, 0.0f };
    for (int i = 0; i < 4; ++i)
    {
        addAndMakeVisible(vuPresetBtns_[static_cast<std::size_t>(i)]);
        const float targetDb = vuPresets[i];
        vuPresetBtns_[static_cast<std::size_t>(i)].onClick = [this, targetDb]()
        {
            setFloatParam(ParamIDs::vuRefDb, targetDb);
            syncButtonsFromAPVTS();
        };
    }

    vuRefSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    vuRefSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 108, 20);
    addAndMakeVisible(vuRefSlider_);
    vuRefAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.getAPVTS(), ParamIDs::vuRefDb, vuRefSlider_);
    vuRefSlider_.textFromValueFunction = [](double v) -> juce::String
    {
        return juce::String("TRIM: ") + (v >= 0.05 ? "+" : "") + juce::String(v, 1) + " dB";
    };
    vuRefSlider_.updateText();
    vuRefSlider_.onValueChange = [this]() { syncButtonsFromAPVTS(); };

    for (int i = 0; i < 3; ++i)
    {
        addAndMakeVisible(osBtns_[static_cast<std::size_t>(i)]);
        osBtns_[static_cast<std::size_t>(i)].onClick = [this, i]()
        {
            setChoiceParam(ParamIDs::oversampling, i);
            syncButtonsFromAPVTS();
        };
        addAndMakeVisible(fftBtns_[static_cast<std::size_t>(i)]);
        fftBtns_[static_cast<std::size_t>(i)].onClick = [this, i]()
        {
            setChoiceParam(ParamIDs::fftMode, i);
            syncButtonsFromAPVTS();
        };
    }
    syncButtonsFromAPVTS();
}

void KalyxSettingsOverlayComponent::setChoiceParam(const char* paramID, int index) noexcept
{
    if (auto* p = processor_.getAPVTS().getParameter(paramID))
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(index)));
}

void KalyxSettingsOverlayComponent::setFloatParam(const char* paramID, float val) noexcept
{
    if (auto* p = processor_.getAPVTS().getParameter(paramID))
        p->setValueNotifyingHost(p->convertTo0to1(val));
}

void KalyxSettingsOverlayComponent::syncButtonsFromAPVTS() noexcept
{
    auto& apvts = processor_.getAPVTS();
    const int osMode  = static_cast<int>(std::round(apvts.getRawParameterValue(ParamIDs::oversampling)->load()));
    const int fftMode = static_cast<int>(std::round(apvts.getRawParameterValue(ParamIDs::fftMode)->load()));
    const float vuRef = apvts.getRawParameterValue(ParamIDs::vuRefDb)->load();

    const juce::Colour idleCol = KalyxTheme::getControlIdleBg(theme_);
    const juce::Colour txtIdle = KalyxTheme::getTextPrimary(theme_);
    const juce::Colour accent  = KalyxTheme::getAccent(theme_);

    const float vuPresets[4] = { -20.0f, -18.0f, -14.0f, 0.0f };
    for (int i = 0; i < 4; ++i)
    {
        const bool act = (std::abs(vuRef - vuPresets[i]) < 0.15f);
        vuPresetBtns_[static_cast<std::size_t>(i)].setColour(juce::TextButton::buttonColourId, act ? accent : idleCol);
        vuPresetBtns_[static_cast<std::size_t>(i)].setColour(juce::TextButton::textColourOffId, act ? juce::Colours::white : txtIdle);
    }
    for (int i = 0; i < 3; ++i)
    {
        const bool actO = (i == osMode);
        osBtns_[static_cast<std::size_t>(i)].setColour(juce::TextButton::buttonColourId, actO ? accent : idleCol);
        osBtns_[static_cast<std::size_t>(i)].setColour(juce::TextButton::textColourOffId, actO ? juce::Colours::white : txtIdle);
        const bool actF = (i == fftMode);
        fftBtns_[static_cast<std::size_t>(i)].setColour(juce::TextButton::buttonColourId, actF ? accent : idleCol);
        fftBtns_[static_cast<std::size_t>(i)].setColour(juce::TextButton::textColourOffId, actF ? juce::Colours::white : txtIdle);
    }

    vuRefSlider_.setColour(juce::Slider::textBoxTextColourId, txtIdle);
    closeBtn_.setColour(juce::TextButton::buttonColourId, KalyxTheme::alertRed);
    closeBtn_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    repaint();
}

void KalyxSettingsOverlayComponent::mouseDown(const juce::MouseEvent& e)
{
    if (!getLocalBounds().withSizeKeepingCentre(440, 260).contains(e.getPosition()))
        setVisible(false);
}

void KalyxSettingsOverlayComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.58f));
    const auto card = getLocalBounds().withSizeKeepingCentre(440, 278).toFloat();
    g.setColour(KalyxTheme::getStripBg(theme_));
    g.fillRoundedRectangle(card, 5.0f);

    const juce::Rectangle<float> bar(card.getX(), card.getY(), card.getWidth(), 30.0f);
    g.setColour(KalyxTheme::getPrimary(theme_));
    g.fillRoundedRectangle(bar, 5.0f);
    g.setColour(KalyxTheme::getGridBorder(theme_));
    g.drawRoundedRectangle(card, 5.0f, 1.5f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(12.5f, juce::Font::bold)));
    g.drawText("KALYX HYBRID CONSOLE SETTINGS", static_cast<int>(card.getX() + 14.0f), static_cast<int>(card.getY() + 3.0f), 360, 24, juce::Justification::centredLeft);

    g.setColour(KalyxTheme::getTextPrimary(theme_));
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    const int lx = static_cast<int>(card.getX() + 18.0f), cy = static_cast<int>(card.getY());
    g.drawText("1. 0 VU REFERENCE CALIBRATION (300 ms IEC) :", lx, cy + 40,  380, 15, juce::Justification::centredLeft);
    g.drawText("2. NON-LINEAR DSP OVERSAMPLING (ADAA + OS) :", lx, cy + 124, 380, 15, juce::Justification::centredLeft);
    g.drawText("3. EQ REAL-TIME FFT SPECTRUM ANALYZER :",      lx, cy + 186, 380, 15, juce::Justification::centredLeft);

    g.setColour(KalyxTheme::getTextSecondary(theme_));
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::plain)));
    g.drawText(juce::String("Kalyx Hybrid Console v") + JucePlugin_VersionString + " — Build " + __DATE__,
               lx, static_cast<int>(card.getBottom() - 22.0f), static_cast<int>(card.getWidth() - 36.0f), 16,
               juce::Justification::centred);
}

void KalyxSettingsOverlayComponent::resized()
{
    const auto card = getLocalBounds().withSizeKeepingCentre(440, 278);
    const int cx = card.getX() + 18, cy = card.getY(), iw = card.getWidth() - 36;
    closeBtn_.setBounds(card.getRight() - 32, cy + 3, 24, 24);

    const int w4 = (iw - 18) / 4;
    for (int i = 0; i < 4; ++i)
        vuPresetBtns_[static_cast<std::size_t>(i)].setBounds(cx + i * (w4 + 6), cy + 58, w4, 22);
    vuRefSlider_.setBounds(cx, cy + 86, iw, 24);

    const int w3 = (iw - 12) / 3;
    for (int i = 0; i < 3; ++i)
        osBtns_[static_cast<std::size_t>(i)].setBounds(cx + i * (w3 + 6), cy + 144, w3, 24);
    for (int i = 0; i < 3; ++i)
        fftBtns_[static_cast<std::size_t>(i)].setBounds(cx + i * (w3 + 6), cy + 206, w3, 24);
}
