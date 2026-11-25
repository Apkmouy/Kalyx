#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Core/ConsoleModuleBase.h"
#include "../Core/ConsoleRegistry.h"
#include "../PluginProcessor.h"
#include "KalyxLookAndFeel.h"
#include "../Consoles/UA1176/UA1176UI.h"
#include "../Consoles/LA2A/LA2AUI.h"
#include "../Consoles/Distressor/DistressorUI.h"
#include "../Consoles/Fairchild/FairchildUI.h"
#include "../Consoles/KalyxModern/SurgicalEQ2UUI.h"
#include "../Consoles/KalyxModern/PrecisionDyn2UUI.h"
#include "../Consoles/KalyxModern/HarmonicSculptorUI.h"
#include "../Consoles/KalyxModern/StereoPhaseUI.h"
#include <functional>
#include <memory>
#include <vector>

/**
 * Conteneur universel d'un emplacement Série 500 (Slot A à E) — Design System « Tactile Flat ».
 */
class ModuleSlotView final : public juce::Component
{
public:
    ModuleSlotView(KalyxHybridConsoleProcessor& proc, int slotIndex);
    ~ModuleSlotView() override = default;

    [[nodiscard]] int getSlotIndex() const noexcept { return slotIndex_; }

    void setUiScale(float s);
    void setChassisTheme(kalyx::core::ChassisTheme t);
    void setVisualPosition(int visualPos, bool canMoveLeft, bool canMoveRight);
    void setReorderCallbacks(std::function<void()> onMoveLeft, std::function<void()> onMoveRight);

    void syncModuleFromAPVTS();
    void updateFFTAndRepaint();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void showCategorizedModuleMenu();
    void rebuildControlsForModule(int modId);
    void updateDependentControlStates();
    void applySslEqCapColours(bool isG);
    void drawArrowButton(juce::Graphics& g, juce::Rectangle<float> bounds, const char* symbol, bool enabled) const;
    void drawLockIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool isLocked) const;
    void drawLedMeterStrip(juce::Graphics& g, juce::Rectangle<float> bounds) const;

    KalyxHybridConsoleProcessor& processor_;
    int slotIndex_       { 0 }; // 0..4 (A..E)
    int visualPos_       { 0 }; // 0..4 (gauche -> droite)
    bool canMoveLeft_    { false };
    bool canMoveRight_   { false };
    int currentModuleId_ { -1 };
    int lastPartnerSlot_ { -1 };
    float uiScale_       { 1.0f };
    kalyx::core::ChassisTheme theme_ { kalyx::core::ChassisTheme::Night };

    // États visuels dépendants & télémétrie LED (30 Hz lock-free)
    bool  isSslGActive_    { false };
    bool  lastApiStepped_  { true };
    bool  lastApiRelFixed_ { false };
    bool  lastVintageMode_ { true };
    bool  lastModuleOn_    { true };
    float smoothedGrDb_    { 0.0f };
    float smoothedDriveDb_ { -100.0f };
    bool  peakLedLit_      { false };

    std::function<void()> onMoveLeft_;
    std::function<void()> onMoveRight_;

    juce::TextButton moduleMenuBtn_;
    juce::ToggleButton agBtn_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> agAtt_;
    juce::Rectangle<float> leftArrowRect_, rightArrowRect_, lockIconRect_, ledMeterRect_, peakLedRect_;

    // Contrôles dynamiques de la façade active (recréés uniquement lors d'un changement de module sur le thread GUI)
    std::vector<std::unique_ptr<KalyxRotaryControl>> rotaries_;
    std::vector<std::unique_ptr<KalyxSegmentedPill>> segmentedPills_;
    std::vector<std::unique_ptr<KalyxRotaryStepSelector>> rotarySteps_;
    std::unique_ptr<KalyxArrowSelector> topArrowSelector_;
    std::unique_ptr<KalyxArrowSelector> bottomArrowSelector_;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles_;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> toggleAtts_;
    std::unique_ptr<KalyxEQCurveComponent> eqCurve_;
    std::unique_ptr<kalyx::consoles::UA1176UI> ua1176UI_;
    std::unique_ptr<kalyx::consoles::LA2AUI> la2aUI_;
    std::unique_ptr<kalyx::consoles::DistressorUI> distressorUI_;
    std::unique_ptr<kalyx::consoles::FairchildUI> fairchildUI_;
    std::unique_ptr<kalyx::consoles::SurgicalEQ2UUI> surgicalEq2uUI_;
    std::unique_ptr<kalyx::consoles::PrecisionDyn2UUI> precisionDyn2uUI_;
    std::unique_ptr<kalyx::consoles::HarmonicSculptorUI> harmonicSculptorUI_;
    std::unique_ptr<kalyx::consoles::StereoPhaseUI> stereoPhaseUI_;

    // Pointeurs non-propriétaires vers des contrôles dépendants (valides uniquement entre deux rebuilds)
    KalyxRotaryControl* releaseKnob_  { nullptr };
    KalyxRotaryControl* threshKnob_   { nullptr };
    KalyxRotaryControl* gainKnob_     { nullptr };
    juce::ToggleButton* sslEGButton_  { nullptr };
    KalyxSegmentedPill* apiRelPill_   { nullptr };
    juce::ToggleButton* neveLinkBtn_  { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModuleSlotView)
};
