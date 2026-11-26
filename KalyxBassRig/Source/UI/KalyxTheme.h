#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Core/BassRigParams.h"
#include <algorithm>
#include <cmath>

namespace KalyxTheme
{
    inline const juce::Colour nightChassisBg      { 0xff111318 }; // Anthracite Nuit
    inline const juce::Colour nightStripBg        { 0xff181b22 }; // Ardoise Profond
    inline const juce::Colour nightControlIdleBg  { 0xff222630 };
    inline const juce::Colour knobTrackBg         { 0xff2e3340 };
    inline const juce::Colour nightGridBorder     { 0xff262a35 };
    inline const juce::Colour nightEncoderOutline { 0xff3a4050 };
    inline const juce::Colour nightTextPrimary    { 0xfff2f4f8 };

    inline const juce::Colour kalyxAubergine      { 0xff5b2344 }; // Primaire
    inline const juce::Colour kalyxAubergineHover { 0xff7a325d };
    inline const juce::Colour kalyxRoyalBlue      { 0xff2348b8 }; // Secondaire
    inline const juce::Colour kalyxRoyalBlueGlow  { 0xff3b66e0 };

    // Day Mode
    inline const juce::Colour dayChassisBg        { 0xffe6e8ec };
    inline const juce::Colour dayStripBg          { 0xfff4f5f7 };
    inline const juce::Colour dayControlIdleBg    { 0xffe2e5ea };
    inline const juce::Colour dayKnobTrackBg      { 0xffcbd2df };
    inline const juce::Colour dayGridBorder       { 0xffd0d5dd };
    inline const juce::Colour dayTextPrimary      { 0xff12151c };
    inline const juce::Colour dayAubergine        { 0xff6d2b52 };
    inline const juce::Colour dayRoyalBlue        { 0xff1e3fa3 };

    // Amp-specific Data-Driven accents
    inline const juce::Colour heritageBlue        { 0xff2b6cb0 };
    inline const juce::Colour classicAmber        { 0xffd98a2b };
    inline const juce::Colour hybridCyan          { 0xff4da3ff };
    inline const juce::Colour tangerineOrange     { 0xffd96b27 };
    inline const juce::Colour alertRed            { 0xffc93030 };
    inline const juce::Colour jadeGreen           { 0xff2d7a64 };

    [[nodiscard]] inline juce::Colour getChassisBg(kalyx::bass::ChassisTheme t) noexcept
    {
        return (t == kalyx::bass::ChassisTheme::Day) ? dayChassisBg : nightChassisBg;
    }

    [[nodiscard]] inline juce::Colour getStripBg(kalyx::bass::ChassisTheme t) noexcept
    {
        return (t == kalyx::bass::ChassisTheme::Day) ? dayStripBg : nightStripBg;
    }

    [[nodiscard]] inline juce::Colour getControlIdleBg(kalyx::bass::ChassisTheme t) noexcept
    {
        return (t == kalyx::bass::ChassisTheme::Day) ? dayControlIdleBg : nightControlIdleBg;
    }

    [[nodiscard]] inline juce::Colour getGridBorder(kalyx::bass::ChassisTheme t) noexcept
    {
        return (t == kalyx::bass::ChassisTheme::Day) ? dayGridBorder : nightGridBorder;
    }

    [[nodiscard]] inline juce::Colour getTextPrimary(kalyx::bass::ChassisTheme t) noexcept
    {
        return (t == kalyx::bass::ChassisTheme::Day) ? dayTextPrimary : nightTextPrimary;
    }

    [[nodiscard]] inline juce::Colour getTextSecondary(kalyx::bass::ChassisTheme t) noexcept
    {
        return getTextPrimary(t).withAlpha(0.68f);
    }

    [[nodiscard]] inline juce::Colour getAccent(kalyx::bass::ChassisTheme t) noexcept
    {
        return (t == kalyx::bass::ChassisTheme::Day) ? dayRoyalBlue : kalyxRoyalBlue;
    }

    [[nodiscard]] inline juce::Colour getPrimary(kalyx::bass::ChassisTheme t) noexcept
    {
        return (t == kalyx::bass::ChassisTheme::Day) ? dayAubergine : kalyxAubergine;
    }

    [[nodiscard]] inline juce::Font makeFont(float size, int styleFlags = juce::Font::plain)
    {
        return juce::Font(juce::FontOptions(size, styleFlags));
    }

    [[nodiscard]] inline juce::Font makeMonoFont(float size, int styleFlags = juce::Font::plain)
    {
        return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), size, styleFlags));
    }

    [[nodiscard]] inline juce::String formatDbClean(double db)
    {
        if (std::abs(db) < 0.05) return "0.0 dB";
        return (db >= 0.05 ? "+" : "") + juce::String(db, 1) + " dB";
    }
} // namespace KalyxTheme

class KalyxBassLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    KalyxBassLookAndFeel() = default;

    void setChassisTheme(kalyx::bass::ChassisTheme t) noexcept { theme_ = t; }
    [[nodiscard]] kalyx::bass::ChassisTheme getChassisTheme() const noexcept { return theme_; }

    void setUiScale(float s) noexcept { uiScale_ = std::clamp(s, 0.75f, 2.20f); }
    [[nodiscard]] float getUiScale() const noexcept { return uiScale_; }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override;

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool isHighlighted, bool isDown) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool isHighlighted, bool isDown) override;

private:
    kalyx::bass::ChassisTheme theme_ { kalyx::bass::ChassisTheme::Night };
    float uiScale_ { 1.0f };
};
