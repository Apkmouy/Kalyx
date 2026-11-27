#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "KalyxTheme.h"
#include "../PluginProcessor.h"
#include <array>

namespace kalyx::bass::ui
{

class StrobeTunerBar final : public juce::Component,
                             public juce::SettableTooltipClient
{
public:
    explicit StrobeTunerBar(KalyxBassRigProcessor& proc);
    void setUiScale(float s) noexcept { uiScale_ = std::clamp(s, 0.75f, 2.20f); repaint(); }
    void setDarkText(bool d) noexcept { darkText_ = d; repaint(); }
    void updateFromProcessor();
    void paint(juce::Graphics& g) override;

private:
    KalyxBassRigProcessor& proc_;
    float uiScale_  { 1.0f };
    bool  darkText_ { false };
    int   midi_     { -1 };
    float cents_    { 0.0f };
    float conf_     { 0.0f };
    float freqHz_   { 0.0f };
    float strobePhase_ { 0.0f };
};

class NoiseGateCurveWidget final : public juce::Component,
                                   public juce::SettableTooltipClient
{
public:
    void init(juce::AudioProcessorValueTreeState& apvts);
    void setUiScale(float s) noexcept { uiScale_ = std::clamp(s, 0.75f, 2.20f); repaint(); }
    void setDarkText(bool d) noexcept { darkText_ = d; repaint(); }
    void setGateGain(float g) noexcept { gateGain_ = g; repaint(); }

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

private:
    juce::RangedAudioParameter* threshParam_ { nullptr };
    juce::RangedAudioParameter* enParam_     { nullptr };
    std::unique_ptr<juce::ParameterAttachment> threshAtt_, enAtt_;
    float threshDb_ { -55.0f };
    bool  enabled_  { false };
    float gateGain_ { 1.0f };
    float uiScale_  { 1.0f };
    bool  darkText_ { false };
};

class DIFilterGraphWidget final : public juce::Component,
                                  public juce::SettableTooltipClient
{
public:
    void init(juce::AudioProcessorValueTreeState& apvts);
    void setUiScale(float s) noexcept { uiScale_ = std::clamp(s, 0.75f, 2.20f); repaint(); }
    void setDarkText(bool d) noexcept { darkText_ = d; repaint(); }

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    [[nodiscard]] float freqToX(float hz) const noexcept;
    [[nodiscard]] float xToFreq(float x) const noexcept;

    juce::RangedAudioParameter* hpfParam_ { nullptr };
    juce::RangedAudioParameter* lpfParam_ { nullptr };
    std::unique_ptr<juce::ParameterAttachment> hpfAtt_, lpfAtt_;
    float hpfHz_ { 30.0f };
    float lpfHz_ { 800.0f };
    int   dragHandle_ { 0 };
    float uiScale_ { 1.0f };
    bool  darkText_ { false };
};

class AmpToneEQGraphWidget final : public juce::Component
{
public:
    explicit AmpToneEQGraphWidget(KalyxBassRigProcessor& proc);
    void setUiScale(float s) noexcept { uiScale_ = std::clamp(s, 0.75f, 2.20f); repaint(); }
    void setDarkText(bool d) noexcept { darkText_ = d; repaint(); }
    void setAccentColour(juce::Colour c) noexcept { accent_ = c; repaint(); }
    void setThdHeatmap(float thd) noexcept { thdLevel_ = thd; repaint(); }
    void updateFFTFromProcessor() noexcept;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    [[nodiscard]] float freqToX(float hz, const juce::Rectangle<float>& area) const noexcept;
    [[nodiscard]] float gainToY(float db, const juce::Rectangle<float>& area) const noexcept;
    [[nodiscard]] float yToGain(float y, const juce::Rectangle<float>& area) const noexcept;

    KalyxBassRigProcessor& proc_;
    juce::Colour accent_ { KalyxTheme::heritageBlue };
    float thdLevel_ { 0.0f };
    float uiScale_  { 1.0f };
    bool  darkText_ { false };
    int   dragEqIdx_ { -1 };

    juce::dsp::FFT forwardFFT_ { 11 };
    juce::dsp::WindowingFunction<float> window_ { 2048, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, 4096> fftData_ {};
    std::array<float, 1024> smoothedBinsDb_ {};
};

class TunerOverlay final : public juce::Component
{
public:
    explicit TunerOverlay(KalyxBassRigProcessor& proc);
    void setUiScale(float s) noexcept;
    void setTheme(kalyx::bass::ChassisTheme t) noexcept;
    void updateFromProcessor();
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    KalyxBassRigProcessor& proc_;
    kalyx::bass::ChassisTheme theme_ { kalyx::bass::ChassisTheme::Night };
    float uiScale_ { 1.0f };
    StrobeTunerBar tunerBar_;
    juce::TextButton muteBtn_ { "TUNER MUTE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAtt_;
    juce::TextButton closeBtn_ { "X" };
};

class PhaseCorrelometerBar final : public juce::Component
{
public:
    void setUiScale(float s) noexcept { uiScale_ = std::clamp(s, 0.75f, 2.20f); repaint(); }
    void setDarkText(bool d) noexcept { darkText_ = d; repaint(); }
    void setCorrelation(float c) noexcept { corr_ = std::clamp(c, -1.0f, 1.0f); repaint(); }
    void paint(juce::Graphics& g) override;

private:
    float corr_     { 1.0f };
    float uiScale_  { 1.0f };
    bool  darkText_ { false };
};

class SettingsOverlay final : public juce::Component
{
public:
    explicit SettingsOverlay(KalyxBassRigProcessor& proc);
    void setUiScale(float s) noexcept;
    void setTheme(kalyx::bass::ChassisTheme t) noexcept { theme_ = t; repaint(); }
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    KalyxBassRigProcessor& proc_;
    kalyx::bass::ChassisTheme theme_ { kalyx::bass::ChassisTheme::Night };
    float uiScale_ { 1.0f };
    juce::TextButton closeBtn_ { "X" };
    std::array<juce::TextButton, 4> vuBtns_ {
        juce::TextButton("-20 dBFS"), juce::TextButton("-18 dBFS"), juce::TextButton("-14 dBFS"), juce::TextButton("0 dBFS")
    };
    std::array<juce::TextButton, 3> osBtns_ {
        juce::TextButton("1x (Off)"), juce::TextButton("2x IIR"), juce::TextButton("4x IIR")
    };
};

} // namespace kalyx::bass::ui
