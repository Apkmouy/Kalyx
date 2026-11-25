#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Core/ConsoleModuleBase.h"
#include "../../PluginProcessor.h"
#include "../../UI/KalyxLookAndFeel.h"
#include "LA2ADSP.h"
#include <array>
#include <memory>

namespace kalyx::consoles
{

/**
 * Façade matérielle Série 500 authentique du compresseur optique Teletronix LA-2A.
 * Rendu 100 % vectoriel juce::Graphics :
 * - Châssis gris acier martelé vintage (#7A8088) avec sérigraphie noire nette, bordures chromées fines
 *   et voyant témoin ambré chaleureux à côté de [● LA-2A IN]
 * - Commutateur [ LIMIT | COMP ] à 2 positions en haut à droite
 * - Barre GR 6-LEDs : GR: (●)-1 (●)-2 (○)-4 -6 -10 -20
 * - Gros boutons bakélite noirs à repère blanc pour GAIN & PEAK REDUCTION
 * - Petits boutons secondaires pour R37 EMPHASIS, CELL AGE & BLEND
 * - Conventions du châssis (Double-clic = Reset, Shift/Ctrl + Glisser = Réglage fin, zéro troncature à 80 %).
 */
class LA2AUI final : public juce::Component
{
public:
    LA2AUI(KalyxHybridConsoleProcessor& proc, int slotIndex);
    ~LA2AUI() override = default;

    void setUiScale(float s);
    void setChassisTheme(kalyx::core::ChassisTheme t);
    void syncFromAPVTS();
    void setGainReductionDb(float grDb) noexcept;

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

    // 2 gros potentiomètres principaux (GAIN, PEAK REDUCTION) + 3 petits secondaires (R37 EMPHASIS, CELL AGE, BLEND)
    KalyxRotaryControl gainKnob_;
    KalyxRotaryControl peakRedKnob_;
    KalyxRotaryControl r37EmphKnob_;
    KalyxRotaryControl cellAgeKnob_;
    KalyxRotaryControl blendKnob_;

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
