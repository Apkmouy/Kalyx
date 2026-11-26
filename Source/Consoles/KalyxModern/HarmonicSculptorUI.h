#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Core/ConsoleModuleBase.h"
#include "../../PluginProcessor.h"
#include "../../UI/KalyxLookAndFeel.h"
#include "../../UI/DataWidgets.h"
#include <array>
#include <memory>

namespace kalyx::consoles
{

/**
 * Façade « Data-Driven 2D » 1U de Kalyx Harmonic Matrix (ex-Harmonic Sculptor).
 * - Pad 2D Vectoriel EVEN (X) x ODD (Y)
 * - 2 potentiomètres principaux : TILT EQ, DYNAMICS
 * - 2 barres TactileFaderBar : DRIVE, BLEND
 * - Sélecteurs BAND [FULL|LOW|MID|HI] et ROUTING
 */
class HarmonicSculptorUI final : public juce::Component
{
public:
    HarmonicSculptorUI(KalyxHybridConsoleProcessor& proc, int slotIndex);
    ~HarmonicSculptorUI() override = default;

    void setUiScale(float s);
    void setChassisTheme(kalyx::core::ChassisTheme t);
    void syncFromAPVTS();
    void updateTelemetryAndRepaint();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void updateDependentStates();

    KalyxHybridConsoleProcessor& processor_;
    int slotIndex_ { 0 };
    float uiScale_ { 1.0f };
    kalyx::core::ChassisTheme theme_ { kalyx::core::ChassisTheme::Night };
    float thdPct_ { 0.0f };

    juce::ToggleButton inBtn_ { "MATRIX IN" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> inAtt_;

    juce::ToggleButton tiltLinkBtn_ { "TILT LNK" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> tiltLinkAtt_;

    VectorPad2D evenOddPad_;
    std::array<std::unique_ptr<KalyxRotaryControl>, 2> knobs_; // TILT EQ, DYNAMICS
    TactileFaderBar driveBar_, blendBar_;
    std::unique_ptr<KalyxSegmentedPill> bandPill_;
    std::unique_ptr<KalyxArrowSelector> routingSelector_;

    juce::Rectangle<float> thdBarRect_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HarmonicSculptorUI)
};

} // namespace kalyx::consoles
