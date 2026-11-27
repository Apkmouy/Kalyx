#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "KalyxTheme.h"
#include <functional>
#include <memory>

namespace kalyx::bass::ui
{

struct KalyxRotaryControl
{
    juce::Slider slider;
    juce::Label  titleLabel;
    juce::Label  valueLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::Colour arcColour { KalyxTheme::kalyxRoyalBlue };
    float uiScale { 1.0f };
    bool darkText { false };
    std::function<juce::String(double)> formatter;

    void init(juce::Component& parent,
              juce::AudioProcessorValueTreeState& apvts,
              const juce::String& paramID,
              const juce::String& title,
              juce::Colour accent,
              bool isBipolar,
              bool isDarkText,
              double defaultVal,
              std::function<juce::String(double)> fmt = nullptr,
              const juce::String& infoHint = {});

    void setUiScale(float s);
    void setThemeColours(juce::Colour accent, bool isDarkText);
    void setDimmed(bool dimmed);
    void setVisible(bool v) { slider.setVisible(v); titleLabel.setVisible(v); valueLabel.setVisible(v); }
    void setBoundsCompact(float x, float y, float w, float h);
};

class TactileFaderBar final : public juce::Component,
                              public juce::SettableTooltipClient
{
public:
    TactileFaderBar() = default;

    void init(juce::Component& parent,
              juce::AudioProcessorValueTreeState& apvts,
              const juce::String& paramID,
              const juce::String& title,
              juce::Colour accent,
              bool bipolar,
              bool darkText,
              double defaultVal,
              std::function<juce::String(double)> fmt = nullptr,
              const juce::String& infoHint = {});

    [[nodiscard]] juce::String getFormattedValue() const;
    void setUiScale(float s) noexcept { uiScale_ = std::clamp(s, 0.75f, 2.20f); repaint(); }
    void setThemeColours(juce::Colour accent, bool darkText) noexcept;
    void setDimmed(bool d) noexcept { dimmed_ = d; setEnabled(!d); repaint(); }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    void openInlineEditor();
    void closeInlineEditor(bool commit);

    juce::String title_;
    juce::Colour accent_ { KalyxTheme::kalyxRoyalBlue };
    bool bipolar_  { false };
    bool darkText_ { false };
    bool dimmed_   { false };
    float uiScale_ { 1.0f };
    double defaultVal_ { 0.0 };
    double currentVal_ { 0.0 };
    double dragStartVal_ { 0.0 };
    float  dragStartX_ { 0.0f };

    juce::RangedAudioParameter* param_ { nullptr };
    std::unique_ptr<juce::ParameterAttachment> att_;
    std::function<juce::String(double)> fmt_;
    std::unique_ptr<juce::TextEditor> editor_;
};

class VerticalConsoleFader final : public juce::Component
{
public:
    VerticalConsoleFader() = default;

    void init(juce::Component& parent,
              juce::AudioProcessorValueTreeState& apvts,
              const juce::String& paramID,
              const juce::String& title,
              juce::Colour accent,
              bool darkText,
              double minDb = -60.0,
              double maxDb = 12.0,
              double defaultDb = 0.0);

    void setUiScale(float s) noexcept { uiScale_ = std::clamp(s, 0.75f, 2.20f); repaint(); }
    void setThemeColours(juce::Colour accent, bool darkText) noexcept;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    void openInlineEditor();
    void closeInlineEditor(bool commit);
    [[nodiscard]] float dbToY(double db, const juce::Rectangle<float>& track) const noexcept;
    [[nodiscard]] double yToDb(float y, const juce::Rectangle<float>& track) const noexcept;

    juce::String title_;
    juce::Colour accent_ { KalyxTheme::kalyxRoyalBlue };
    bool darkText_ { false };
    float uiScale_ { 1.0f };
    double minDb_ { -60.0 }, maxDb_ { 12.0 }, defaultDb_ { 0.0 };
    double currentDb_ { 0.0 }, dragStartDb_ { 0.0 };
    float dragStartY_ { 0.0f };

    juce::Rectangle<float> trackRect_, valueRect_;
    juce::RangedAudioParameter* param_ { nullptr };
    std::unique_ptr<juce::ParameterAttachment> att_;
    std::unique_ptr<juce::TextEditor> editor_;
};

class KalyxSegmentedPill final : public juce::Component
{
public:
    KalyxSegmentedPill(juce::AudioProcessorValueTreeState& apvts,
                       const juce::String& paramID,
                       const juce::String& prefixTitle,
                       const juce::StringArray& choices,
                       juce::Colour activeColour,
                       bool darkText = false);

    void setUiScale(float s) noexcept { uiScale_ = std::clamp(s, 0.75f, 2.20f); repaint(); }
    void setThemeColours(juce::Colour activeColour, bool darkText) noexcept;
    void setChoices(const juce::StringArray& choices) { choices_ = choices; repaint(); }

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    juce::String prefix_;
    juce::StringArray choices_;
    juce::Colour activeCol_;
    bool darkText_ { false };
    float uiScale_ { 1.0f };
    int currentIdx_ { 0 };
    juce::RangedAudioParameter* paramPtr_ { nullptr };
    std::unique_ptr<juce::ParameterAttachment> att_;
};

class AnalogVUMeter final : public juce::Component
{
public:
    AnalogVUMeter() = default;

    void setUiScale(float s) noexcept { uiScale_ = std::clamp(s, 0.75f, 2.20f); repaint(); }
    void setDarkText(bool dark) noexcept { darkText_ = dark; repaint(); }
    void setMode(int mode) noexcept { mode_ = mode; } // 0=IN, 1=OUT, 2=GR
    void setRefDb(float refDb) noexcept { refDb_ = refDb; }
    void setReadings(float inDb, float outDb, float grDb) noexcept;

    void paint(juce::Graphics& g) override;

private:
    float uiScale_  { 1.0f };
    bool  darkText_ { false };
    int   mode_     { 1 };
    float refDb_    { -18.0f };
    float inDb_     { -60.0f };
    float outDb_    { -60.0f };
    float grDb_     { 0.0f };
};

class VectorPad2D final : public juce::Component
{
public:
    VectorPad2D() = default;

    void init(juce::Component& parent,
              juce::AudioProcessorValueTreeState& apvts,
              const juce::String& paramXId,
              const juce::String& paramYId,
              const juce::String& labelX,
              const juce::String& labelY,
              juce::Colour accent,
              bool darkText,
              float defX = 0.5f,
              float defY = 0.5f);

    void setUiScale(float s) noexcept { uiScale_ = std::clamp(s, 0.75f, 2.20f); repaint(); }
    void setThemeColours(juce::Colour accent, bool darkText) noexcept;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    juce::String labelX_, labelY_;
    juce::Colour accent_ { KalyxTheme::kalyxRoyalBlue };
    bool darkText_ { false };
    float uiScale_ { 1.0f };
    float normX_ { 0.5f }, normY_ { 0.5f };
    float defX_ { 0.5f }, defY_ { 0.5f };
    juce::RangedAudioParameter* paramX_ { nullptr };
    juce::RangedAudioParameter* paramY_ { nullptr };
    std::unique_ptr<juce::ParameterAttachment> attX_, attY_;
};

} // namespace kalyx::bass::ui
