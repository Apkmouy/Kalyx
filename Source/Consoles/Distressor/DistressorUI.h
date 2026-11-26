#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Core/ConsoleModuleBase.h"
#include "../../PluginProcessor.h"
#include "../../UI/KalyxLookAndFeel.h"
#include "../../UI/DataWidgets.h"
#include "DistressorDSP.h"
#include <array>
#include <memory>

namespace kalyx::consoles
{

/**
 * Façade « Data-Driven » de Stress-VCA (ex-Distressor).
 */
class DistressorUI final : public juce::Component
{
public:
    DistressorUI(KalyxHybridConsoleProcessor& proc, int slotIndex);
    ~DistressorUI() override = default;

    void setUiScale(float s);
    void setChassisTheme(kalyx::core::ChassisTheme t);
    void syncFromAPVTS();
    void setTelemetry(float grDb, float thdPct, float inputPeakDb = -60.0f) noexcept;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void drawGrBargraph(juce::Graphics& g, juce::Rectangle<float> bounds) const;
    void drawThdLeds(juce::Graphics& g, juce::Rectangle<float> bounds) const;

    KalyxHybridConsoleProcessor& processor_;
    int slotIndex_ { 0 };

    float uiScale_      { 1.0f };
    kalyx::core::ChassisTheme theme_ { kalyx::core::ChassisTheme::Night };
    bool  moduleIn_     { true };
    bool  britishOn_    { false };
    int   ratioIdx_     { 4 }; // 6:1 par défaut
    float smoothedGrDb_ { 0.0f };
    float smoothedThd_  { 0.0f };

    // Ligne du haut : [● STRESS IN]  [ BRITISH ]  [NUKE]
    juce::ToggleButton inBtn_;
    juce::ToggleButton britishBtn_;
    juce::TextButton   nukeBtn_ { "NUKE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> inAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> britishAtt_;

    InteractiveTransferCurve transferCurve_;

    // 4 potentiomètres principaux (INPUT, OUTPUT, ATTACK, RELEASE) + BLEND en TactileFaderBar
    KalyxRotaryControl inputKnob_;
    KalyxRotaryControl outputKnob_;
    KalyxRotaryControl attackKnob_;
    KalyxRotaryControl releaseKnob_;
    TactileFaderBar    blendBar_;

    // Bas : RATIO, DIST, FILTER, ROUTING
    std::unique_ptr<KalyxArrowSelector>   ratioSelector_;
    std::unique_ptr<KalyxSegmentedPill>   distPill_;
    std::unique_ptr<KalyxMultiTogglePill> filterPill_;
    std::unique_ptr<KalyxArrowSelector>   routingSelector_;

    juce::Rectangle<float> grStripRect_;
    juce::Rectangle<float> thdStripRect_;
    juce::Rectangle<float> sepTopRect_;
    juce::Rectangle<float> sepBotRect_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DistressorUI)
};

} // namespace kalyx::consoles
