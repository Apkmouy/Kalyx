#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Core/ConsoleModuleBase.h"
#include "../../PluginProcessor.h"
#include "../../UI/KalyxLookAndFeel.h"
#include "../../UI/DataWidgets.h"
#include "LA2ADSP.h"
#include <array>
#include <memory>

namespace kalyx::consoles
{

/**
 * Façade matérielle Série 500 Data-Driven du compresseur optique Opto-2A (ex-Teletronix LA-2A).
 */
class LA2AUI final : public juce::Component
{
public:
    LA2AUI(KalyxHybridConsoleProcessor& proc, int slotIndex);
    ~LA2AUI() override = default;

    void setUiScale(float s);
    void setChassisTheme(kalyx::core::ChassisTheme t);
    void syncFromAPVTS();
    void setGainReductionDb(float grDb, float inputPeakDb = -60.0f) noexcept;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void drawGrLedStrip(juce::Graphics& g, juce::Rectangle<float> bounds) const;
    void drawLimitCompSwitch(juce::Graphics& g) const;
    void drawPilotJewelLamp(juce::Graphics& g) const;

    KalyxHybridConsoleProcessor& processor_;
    int slotIndex_ { 0 };

    float uiScale_      { 1.0f };
    kalyx::core::ChassisTheme theme_ { kalyx::core::ChassisTheme::Night };
    bool  moduleIn_     { true };
    bool  isLimitMode_  { false };
    float smoothedGrDb_ { 0.0f };

    // Contrôles haut & bas
    juce::ToggleButton inBtn_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> inAtt_;
    std::unique_ptr<KalyxArrowSelector> routingSelector_;

    InteractiveTransferCurve transferCurve_;

    // 2 gros potentiomètres principaux (GAIN, PEAK REDUCTION) + 3 barres TactileFaderBar
    KalyxRotaryControl gainKnob_;
    KalyxRotaryControl peakRedKnob_;
    TactileFaderBar    r37EmphBar_;
    TactileFaderBar    cellAgeBar_;
    TactileFaderBar    blendBar_;

    // Zones calculées dans resized() pour le rendu vectoriel & les clics
    juce::Rectangle<float> pilotLampRect_;
    juce::Rectangle<float> limitSwitchRect_;
    juce::Rectangle<float> compSwitchRect_;
    juce::Rectangle<float> limitCompFrameRect_;
    juce::Rectangle<float> grStripRect_;
    juce::Rectangle<float> sepTopRect_;
    juce::Rectangle<float> sepBotRect_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LA2AUI)
};

} // namespace kalyx::consoles
