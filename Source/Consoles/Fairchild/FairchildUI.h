#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Core/ConsoleModuleBase.h"
#include "../../PluginProcessor.h"
#include "../../UI/KalyxLookAndFeel.h"
#include "FairchildDSP.h"
#include <memory>

namespace kalyx::consoles
{

/**
 * Façade « Tactile Flat » du compresseur à lampes Fairchild 660 / 670 (Vari-Mu).
 * Palette Or Vintage (#C59B38) & Cuivre Chaud (#9E5834) sur fond #181B22 (Night) / #F4F5F7 (Day).
 * Toutes les dimensions et polices sont indexées sur uiScale.
 */
class FairchildUI final : public juce::Component
{
public:
    FairchildUI(KalyxHybridConsoleProcessor& proc, int slotIndex);
    ~FairchildUI() override = default;

    void setUiScale(float s);
    void setChassisTheme(kalyx::core::ChassisTheme t);
    void syncFromAPVTS();
    void setTelemetry(float grDb) noexcept;

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

    // 7 potentiomètres : INPUT GAIN, AC THRESHOLD, TIME CONST, OUTPUT, MATCH L/R, MATCH P/P, BLEND
    KalyxRotaryControl inputKnob_;
    KalyxRotaryControl threshKnob_;
    KalyxRotaryControl timeConstKnob_;
    KalyxRotaryControl outputKnob_;
    KalyxRotaryControl matchLrKnob_;
    KalyxRotaryControl matchPpKnob_;
    KalyxRotaryControl blendKnob_;

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
