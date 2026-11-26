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
 * Façade « Data-Driven 2D » 1U de Kalyx Spatial Vector (ex-Stereo & Phase Studio).
 * - Corrélomètre temps réel (-1..+1)
 * - 4 gros potentiomètres : WIDTH, MONO MAKER, PHASE ROT, SIDE TILT
 * - 2 barres TactileFaderBar bipolaires : MID GAIN, SIDE GAIN
 * - Ø L, Ø R, L<>R et MONITOR [STEREO | MID | SIDE]
 */
class StereoPhaseUI final : public juce::Component
{
public:
    StereoPhaseUI(KalyxHybridConsoleProcessor& proc, int slotIndex);
    ~StereoPhaseUI() override = default;

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
    float correlation_ { 1.0f };

    juce::ToggleButton inBtn_ { "SPATIAL IN" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> inAtt_;

    std::unique_ptr<KalyxSegmentedPill> modePill_;
    std::array<std::unique_ptr<KalyxRotaryControl>, 4> knobs_;
    TactileFaderBar midGainBar_, sideGainBar_;

    juce::ToggleButton invLBtn_ { "O L" };
    juce::ToggleButton invRBtn_ { "O R" };
    juce::ToggleButton swapBtn_ { "L<>R" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> invLAtt_, invRAtt_, swapAtt_;

    std::unique_ptr<KalyxSegmentedPill> monitorPill_;

    juce::Rectangle<float> corrRect_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoPhaseUI)
};

} // namespace kalyx::consoles
