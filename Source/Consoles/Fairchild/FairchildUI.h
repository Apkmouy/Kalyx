#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Core/ConsoleModuleBase.h"
#include "../../PluginProcessor.h"
#include "../../UI/KalyxLookAndFeel.h"
#include "../../UI/DataWidgets.h"
#include "FairchildDSP.h"
#include <memory>

namespace kalyx::consoles
{

/**
 * Façade « Data-Driven » du compresseur à lampes Vari-Mu 670 (ex-Fairchild 660/670).
 */
class FairchildUI final : public juce::Component
{
public:
    FairchildUI(KalyxHybridConsoleProcessor& proc, int slotIndex);
    ~FairchildUI() override = default;

    void setUiScale(float s);
    void setChassisTheme(kalyx::core::ChassisTheme t);
    void syncFromAPVTS();
    void setTelemetry(float grDb, float inputPeakDb = -60.0f) noexcept;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void drawGrBargraph(juce::Graphics& g, juce::Rectangle<float> bounds) const;

    KalyxHybridConsoleProcessor& processor_;
    int slotIndex_ { 0 };

    float uiScale_ { 1.0f };
    kalyx::core::ChassisTheme theme_ { kalyx::core::ChassisTheme::Night };
    bool  moduleIn_     { true };
    bool  vintageMode_  { true };
    int   tubeIdx_      { 3 }; // RCA 6386 Black Plate par défaut
    float smoothedGrDb_ { 0.0f };

    // Ligne du haut : [● 670 IN]  [ MOD | VINTAGE ]
    juce::ToggleButton inBtn_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> inAtt_;
    std::unique_ptr<KalyxSegmentedPill> vintagePill_;

    InteractiveTransferCurve transferCurve_;

    // 4 potentiomètres principaux (INPUT GAIN, AC THRESHOLD, TIME CONST, OUTPUT) + 3 TactileFaderBar
    KalyxRotaryControl inputKnob_;
    KalyxRotaryControl threshKnob_;
    KalyxRotaryControl timeConstKnob_;
    KalyxRotaryControl outputKnob_;
    TactileFaderBar    matchLrBar_;
    TactileFaderBar    matchPpBar_;
    TactileFaderBar    blendBar_;

    // Sélecteurs bas : NOS TUBE, ROUTING
    std::unique_ptr<KalyxArrowSelector> tubeSelector_;
    std::unique_ptr<KalyxArrowSelector> routingSelector_;

    juce::Rectangle<float> grStripRect_;
    juce::Rectangle<float> tubeSigRect_;
    juce::Rectangle<float> sepTopRect_;
    juce::Rectangle<float> sepBotRect_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FairchildUI)
};

} // namespace kalyx::consoles
