#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/KalyxTheme.h"
#include "UI/DataWidgets.h"
#include "UI/BassRigWidgets.h"
#include <memory>

class KalyxBassRigEditor final : public juce::AudioProcessorEditor,
                                 private juce::Timer
{
public:
    explicit KalyxBassRigEditor(KalyxBassRigProcessor& proc);
    ~KalyxBassRigEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void applyTheme();
    void updateAmpModelVisibility();
    void openCustomIRChooser(int cabSlot);

    KalyxBassRigProcessor& proc_;
    KalyxBassLookAndFeel lnf_;
    float uiScale_ { 1.0f };
    kalyx::bass::ChassisTheme theme_ { kalyx::bass::ChassisTheme::Night };
    int lastModelIdx_ { -1 };

    // Header
    juce::TextButton tunerMuteBtn_ { "MUTE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> tunerMuteAtt_;
    kalyx::bass::ui::StrobeTunerBar tunerBar_;
    juce::ComboBox presetBox_;
    juce::TextButton abBtn_ { "A / B" };
    juce::TextButton copyBtn_ { "COPY" };
    juce::TextButton themeBtn_ { "DAY" };
    juce::TextButton settingsBtn_ { "SETTINGS" };

    // Left Column: Input & DI Path
    kalyx::bass::ui::KalyxRotaryControl inTrimKnob_;
    juce::ToggleButton padBtn_ { "PAD -15" };
    juce::ToggleButton gateBtn_ { "GATE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> padAtt_, gateAtt_;
    kalyx::bass::ui::NoiseGateCurveWidget gateCurve_;

    juce::ToggleButton diEnBtn_ { "DI PATH" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> diEnAtt_;
    juce::ComboBox diCircuitBox_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> diCircuitAtt_;
    kalyx::bass::ui::TactileFaderBar diSubMixBar_;
    kalyx::bass::ui::DIFilterGraphWidget diFilterGraph_;
    kalyx::bass::ui::TactileFaderBar diCompBar_;
    juce::ToggleButton diAutoPhaseBtn_ { "DI AUTO-PHASE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> diAutoPhaseAtt_;

    // Center Top: Amp Head
    juce::ToggleButton ampEnBtn_ { "AMP" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ampEnAtt_;
    std::unique_ptr<kalyx::bass::ui::KalyxSegmentedPill> ampModelPill_;
    kalyx::bass::ui::AmpToneEQGraphWidget ampGraph_;

    std::unique_ptr<kalyx::bass::ui::KalyxSegmentedPill> ampChannelPill_;
    juce::ToggleButton brightBtn_ { "BRIGHT" };
    juce::ToggleButton ultraHiBtn_ { "ULTRA HI" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> brightAtt_, ultraHiAtt_;
    juce::ComboBox ultraLoBox_, midFreqBox_, depthBox_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ultraLoAtt_, midFreqAtt_, depthAtt_;

    kalyx::bass::ui::KalyxRotaryControl gainKnob_, bassKnob_, midKnob_, trebleKnob_, driveKnob_, compBoostKnob_, presenceKnob_, masterKnob_;
    kalyx::bass::ui::TactileFaderBar sagBar_, biasBar_;

    // Center Bottom: Dual Cabinet & Mic 2D
    juce::ToggleButton cabEnBtn_ { "CABINET" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> cabEnAtt_;
    juce::ComboBox cabABox_, cabBBox_, micTypeBox_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cabAAtt_, cabBAtt_, micTypeAtt_;
    juce::ToggleButton phaseABtn_ { "O A" }, phaseBBtn_ { "O B" }, cabAutoPhaseBtn_ { "AUTO PHASE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> phaseAAtt_, phaseBAtt_, cabAutoPhaseAtt_;
    juce::TextButton loadIrABtn_ { "LOAD IR A" }, loadIrBBtn_ { "LOAD IR B" };
    std::unique_ptr<juce::FileChooser> fileChooser_;

    kalyx::bass::ui::TopographicMicPad2D micPad2D_;
    kalyx::bass::ui::TactileFaderBar cabMixBar_, cabBreakupBar_, cabRoomBar_;

    // Right Column: Master & Mixer
    kalyx::bass::ui::AnalogVUMeter vuMeter_;
    std::unique_ptr<kalyx::bass::ui::KalyxSegmentedPill> vuModePill_;
    kalyx::bass::ui::PhaseCorrelometerBar phaseCorrBar_;
    kalyx::bass::ui::VerticalConsoleFader ampLevelFader_, diLevelFader_, outFader_;

    juce::ToggleButton kickEnBtn_ { "KICK SC" }, limEnBtn_ { "LIMITER" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> kickEnAtt_, limEnAtt_;
    kalyx::bass::ui::TactileFaderBar kickAmtBar_, limCeilBar_;

    // Settings Overlay
    kalyx::bass::ui::SettingsOverlay settingsOverlay_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KalyxBassRigEditor)
};
