#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Core/ConsoleModuleBase.h"
#include "../../PluginProcessor.h"
#include "../../UI/KalyxLookAndFeel.h"
#include <array>
#include <memory>

namespace kalyx::consoles
{

/**
 * Façade « Tactile Flat » 1U du Kalyx Harmonic Sculptor (EVEN 2nd / ODD 3rd / TILT LNK / DYNAMICS / SAT BAND).
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

    juce::ToggleButton inBtn_ { "HARMONIC" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> inAtt_;

    juce::ToggleButton tiltLinkBtn_ { "TILT LNK" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> tiltLinkAtt_;

    std::array<std::unique_ptr<KalyxRotaryControl>, 6> knobs_;
    std::unique_ptr<KalyxSegmentedPill> bandPill_;
    std::unique_ptr<KalyxArrowSelector> routingSelector_;

    juce::Rectangle<float> thdBarRect_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HarmonicSculptorUI)
};

} // namespace kalyx::consoles
