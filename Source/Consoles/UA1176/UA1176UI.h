#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Core/ConsoleModuleBase.h"
#include "../../PluginProcessor.h"
#include "../../UI/KalyxLookAndFeel.h"
#include "../../UI/DataWidgets.h"
#include "UA1176DSP.h"
#include <array>
#include <memory>

namespace kalyx::consoles
{

/**
 * Façade matérielle Série 500 Data-Driven du compresseur FET-76 (ex-UA 1176).
 */
class UA1176UI final : public juce::Component
{
public:
    UA1176UI(KalyxHybridConsoleProcessor& proc, int slotIndex);
    ~UA1176UI() override = default;

    void setUiScale(float s);
    void setChassisTheme(kalyx::core::ChassisTheme t);
    void syncFromAPVTS();
    void setGainReductionDb(float grDb, float inputPeakDb = -60.0f) noexcept;

    [[nodiscard]] juce::Colour getCurrentAccentColour() const noexcept;
    [[nodiscard]] int getCurrentRevision() const noexcept { return currentRev_; }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void applyRevisionTheme(int revIdx);
    void drawGrLedStrip(juce::Graphics& g, juce::Rectangle<float> bounds) const;
    void drawRatioButtons(juce::Graphics& g) const;

    KalyxHybridConsoleProcessor& processor_;
    int slotIndex_ { 0 };

    float uiScale_      { 1.0f };
    kalyx::core::ChassisTheme theme_ { kalyx::core::ChassisTheme::Night };
    int   currentRev_   { 1 };     // 0=Rev A, 1=Rev D, 2=Rev G
    int   currentRatio_ { 0 };     // 0=4:1, 1=8:1, 2=12:1, 3=20:1
    bool  allButtonsOn_ { false };
    bool  superSlowOn_  { false };
    bool  moduleIn_     { true };
    float smoothedGrDb_ { 0.0f };

    // Contrôles haut & bas
    juce::ToggleButton inBtn_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> inAtt_;
    std::unique_ptr<KalyxArrowSelector> revSelector_;
    std::unique_ptr<KalyxArrowSelector> routingSelector_;

    // Courbe de transfert interactive en haut
    InteractiveTransferCurve transferCurve_;

    // 4 potentiomètres principaux (INPUT, OUTPUT, ATTACK, RELEASE) + 2 barres TactileFaderBar (FET BIAS, BLEND)
    KalyxRotaryControl inputKnob_;
    KalyxRotaryControl outputKnob_;
    KalyxRotaryControl attackKnob_;
    KalyxRotaryControl releaseKnob_;
    TactileFaderBar    fetBiasBar_;
    TactileFaderBar    blendBar_;

    // Boutons MODS : ALL (SLAM) & SLOW
    juce::ToggleButton slamBtn_;
    juce::ToggleButton slowBtn_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> slamAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> slowAtt_;

    // Zones calculées dans resized() pour le rendu vectoriel & les clics
    juce::Rectangle<float> grStripRect_;
    juce::Rectangle<float> sepTopRect_;
    juce::Rectangle<float> sepBotRect_;
    juce::Rectangle<float> inputOutputZone_;
    juce::Rectangle<float> ratioLabelRect_;
    std::array<juce::Rectangle<float>, 4> ratioBtnRects_ {};
    juce::Rectangle<float> modsLabelRect_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UA1176UI)
};

} // namespace kalyx::consoles
