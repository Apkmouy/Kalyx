#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/KalyxTheme.h"
#include "UI/DataWidgets.h"
#include "UI/BassRigWidgets.h"
#include "UI/PedalboardUI.h"
#include <memory>

class KalyxBassRigEditor final : public juce::AudioProcessorEditor,
                                 private juce::Timer
{
public:
    explicit KalyxBassRigEditor(KalyxBassRigProcessor& proc);
    ~KalyxBassRigEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    void applyTheme();
    void updateAmpModelVisibility();
    void openCustomIRChooser(int cabSlot);
    void refreshPresetList();
    void promptPresetName(bool isRename);
    void updateInfoFromComponent(juce::Component* comp);

    KalyxBassRigProcessor& proc_;
    KalyxBassLookAndFeel lnf_;
    float uiScale_ { 1.0f };
    kalyx::bass::ChassisTheme theme_ { kalyx::bass::ChassisTheme::Night };
    int lastModelIdx_ { -1 };
    juce::String infoBarText_ { juce::String::fromUTF8("Kalyx Bass Rig v" JucePlugin_VersionString " — Survolez un contrôle pour afficher son aide et sa valeur exacte.") };

    // Header
    juce::TextButton tunerBtn_ { "TUNER" };
    juce::TextButton tunerMuteBtn_ { "TUNER MUTE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> tunerMuteAtt_;
    juce::ComboBox presetBox_;
    juce::TextButton savePresetBtn_ { "SAVE" };
    juce::TextButton saveAsPresetBtn_ { "SAVE AS" };
    juce::TextButton renamePresetBtn_ { "REN" };
    juce::TextButton deletePresetBtn_ { "DEL" };
    juce::TextButton themeBtn_ { "DAY" };
    juce::TextButton settingsBtn_ { "SETTINGS" };
    juce::TextButton rigTabBtn_ { "RIG" };
    juce::TextButton pedalsTabBtn_ { "PEDALS" };
    bool showPedalboard_ { false };
    kalyx::bass::ui::PedalboardView pedalboardView_;

    // Left Column: Input Stage & DI Path
    kalyx::bass::ui::KalyxRotaryControl inTrimKnob_;
    juce::ToggleButton padBtn_ { "PAD -15" };
    juce::ToggleButton gateBtn_ { "GATE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> padAtt_, gateAtt_;
    kalyx::bass::ui::TactileFaderBar gateThreshBar_;
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

    kalyx::bass::ui::KalyxRotaryControl gainKnob_, bassKnob_, midKnob_, trebleKnob_, driveKnob_, compBoostKnob_, presenceKnob_, masterKnob_, headOutKnob_;
    kalyx::bass::ui::TactileFaderBar sagBar_, biasBar_;

    // Center Bottom: Dual Cabinet IR
    juce::ToggleButton cabEnBtn_ { "CABINET" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> cabEnAtt_;
    juce::ComboBox cabABox_, cabBBox_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cabAAtt_, cabBAtt_;
    juce::ToggleButton phaseABtn_ { juce::String::fromUTF8("Ø INVERT") }, phaseBBtn_ { juce::String::fromUTF8("Ø INVERT") };
    juce::ToggleButton muteABtn_ { "MUTE" }, soloABtn_ { "SOLO" }, muteBBtn_ { "MUTE" }, soloBBtn_ { "SOLO" };
    juce::ToggleButton cabAutoPhaseBtn_ { "AUTO PHASE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> phaseAAtt_, phaseBAtt_, muteAAtt_, soloAAtt_, muteBAtt_, soloBAtt_, cabAutoPhaseAtt_;
    kalyx::bass::ui::TactileFaderBar cabAPanBar_, cabBPanBar_;
    juce::TextButton loadIrABtn_ { "LOAD IR A" }, loadIrBBtn_ { "LOAD IR B" };
    std::unique_ptr<juce::FileChooser> fileChooser_;

    kalyx::bass::ui::TactileFaderBar cabALevelBar_, cabBLevelBar_, cabBreakupBar_, cabRoomBar_;

    // Right Column: Master & Mixer
    kalyx::bass::ui::AnalogVUMeter vuMeter_;
    std::unique_ptr<kalyx::bass::ui::KalyxSegmentedPill> vuModePill_;
    kalyx::bass::ui::PhaseCorrelometerBar phaseCorrBar_;
    kalyx::bass::ui::VerticalConsoleFader ampLevelFader_, diLevelFader_, outFader_;

    juce::ToggleButton kickEnBtn_ { "KICK SC" }, kickScListenBtn_ { "SC LISTEN" }, limEnBtn_ { "LIMITER" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> kickEnAtt_, kickScListenAtt_, limEnAtt_;
    kalyx::bass::ui::TactileFaderBar kickScInBar_, kickScHpfBar_, kickAmtBar_, limCeilBar_;

    // Overlays
    kalyx::bass::ui::TunerOverlay tunerOverlay_;
    kalyx::bass::ui::SettingsOverlay settingsOverlay_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KalyxBassRigEditor)
};
