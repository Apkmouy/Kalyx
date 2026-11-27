#include "KalyxTheme.h"

void KalyxBassLookAndFeel::setChassisTheme(kalyx::bass::ChassisTheme t) noexcept
{
    theme_ = t;
    const auto bg   = KalyxTheme::getControlIdleBg(t);
    const auto strip = KalyxTheme::getStripBg(t);
    const auto bd   = KalyxTheme::getGridBorder(t);
    const auto txt  = KalyxTheme::getTextPrimary(t);
    const auto acc  = KalyxTheme::getAccent(t);

    setColour(juce::ComboBox::backgroundColourId, bg);
    setColour(juce::ComboBox::textColourId, txt);
    setColour(juce::ComboBox::outlineColourId, bd);
    setColour(juce::ComboBox::arrowColourId, txt.withAlpha(0.75f));

    setColour(juce::PopupMenu::backgroundColourId, strip);
    setColour(juce::PopupMenu::textColourId, txt);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, acc);
    setColour(juce::PopupMenu::highlightedTextColourId, KalyxTheme::nightTextPrimary);

    setColour(juce::TextButton::buttonColourId, bg);
    setColour(juce::TextButton::buttonOnColourId, acc);
    setColour(juce::TextButton::textColourOffId, txt);
    setColour(juce::TextButton::textColourOnId, KalyxTheme::nightTextPrimary);
}

void KalyxBassLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                            juce::Slider& slider)
{
    const float sc = uiScale_;
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                               static_cast<float>(width), static_cast<float>(height)).reduced(2.0f * sc);
    const float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();

    const bool disabled = !slider.isEnabled();
    const float alpha = disabled ? 0.35f : 1.0f;

    // Flat circle #222630 (or Day equivalent)
    const float innerR = std::max(4.0f, radius - 4.0f * sc);
    g.setColour(KalyxTheme::getControlIdleBg(theme_).withAlpha(alpha));
    g.fillEllipse(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f);

    g.setColour(KalyxTheme::getGridBorder(theme_).withAlpha(alpha));
    g.drawEllipse(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f, 1.2f * sc);

    // Track arc
    const float arcR = radius - 1.5f * sc;
    const float stroke = 2.6f * sc;
    juce::Path track;
    track.addCentredArc(cx, cy, arcR, arcR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour((theme_ == kalyx::bass::ChassisTheme::Day ? KalyxTheme::dayKnobTrackBg : KalyxTheme::knobTrackBg).withAlpha(alpha));
    g.strokePath(track, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Active value arc
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    auto arcCol = slider.findColour(juce::Slider::rotarySliderFillColourId);
    if (disabled)
        arcCol = juce::Colours::grey;

    juce::Path valArc;
    const bool bipolar = static_cast<bool>(slider.getProperties().getWithDefault("kalyxBipolar", false));
    const float startA = bipolar ? (0.5f * (rotaryStartAngle + rotaryEndAngle)) : rotaryStartAngle;
    if (std::abs(angle - startA) > 0.01f)
    {
        valArc.addCentredArc(cx, cy, arcR, arcR, 0.0f, std::min(startA, angle), std::max(startA, angle), true);
        g.setColour(arcCol.withAlpha(alpha));
        g.strokePath(valArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Pointer line
    const float px = cx + std::sin(angle) * (innerR * 0.72f);
    const float py = cy - std::cos(angle) * (innerR * 0.72f);
    g.setColour(KalyxTheme::getTextPrimary(theme_).withAlpha(alpha));
    g.drawLine(cx, cy, px, py, 1.8f * sc);
}

void KalyxBassLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                            bool isHighlighted, bool)
{
    const float sc = uiScale_;
    const auto r = button.getLocalBounds().toFloat().reduced(1.0f * sc);
    const bool on = button.getToggleState();
    const auto activeCol = button.findColour(juce::ToggleButton::tickColourId);

    g.setColour(on ? activeCol.withAlpha(0.24f) : KalyxTheme::getControlIdleBg(theme_));
    g.fillRoundedRectangle(r, 4.0f * sc);

    g.setColour(on ? activeCol : (isHighlighted ? activeCol.withAlpha(0.55f) : KalyxTheme::getGridBorder(theme_)));
    g.drawRoundedRectangle(r, 4.0f * sc, 1.2f * sc);

    // Small LED dot on left
    const float ledR = 3.2f * sc;
    const float ledX = r.getX() + 7.0f * sc;
    const float ledY = r.getCentreY();
    g.setColour(on ? activeCol : KalyxTheme::knobTrackBg);
    g.fillEllipse(ledX - ledR, ledY - ledR, ledR * 2.0f, ledR * 2.0f);

    g.setColour(KalyxTheme::getTextPrimary(theme_).withAlpha(on ? 1.0f : 0.72f));
    g.setFont(KalyxTheme::makeFont(11.0f * sc, on ? juce::Font::bold : juce::Font::plain));
    auto textR = r.withTrimmedLeft(13.0f * sc);
    g.drawFittedText(button.getButtonText(), textR.toNearestInt(), juce::Justification::centred, 1);
}

void KalyxBassLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                const juce::Colour& backgroundColour,
                                                bool isHighlighted, bool isDown)
{
    const float sc = uiScale_;
    const auto r = button.getLocalBounds().toFloat().reduced(1.0f * sc);
    auto bg = button.getToggleState() ? KalyxTheme::getAccent(theme_) : backgroundColour;
    if (isDown) bg = bg.brighter(0.15f);
    else if (isHighlighted) bg = bg.brighter(0.08f);

    g.setColour(bg);
    g.fillRoundedRectangle(r, 4.0f * sc);
    g.setColour(KalyxTheme::getGridBorder(theme_));
    g.drawRoundedRectangle(r, 4.0f * sc, 1.0f * sc);
}
