#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Core/ConsoleModuleBase.h"
#include "../../PluginProcessor.h"
#include "../../UI/KalyxLookAndFeel.h"
#include "PrecisionDyn2UDSP.h"
#include <array>
#include <memory>

namespace kalyx::consoles
{

/**
 * Façade « Tactile Flat » 2U Double-Largeur du Kalyx Precision VCA & Transient (2U).
 * Affiche un oscilloscope déroulant temps réel (Waveform + Courbe GR VCA + Action Transitoires)
 * et 12 contrôles (VCA Lookahead + Dual-Band Transient Shaper).
 */
class PrecisionDyn2UUI final : public juce::Component
{
public:
    PrecisionDyn2UUI(KalyxHybridConsoleProcessor& proc, int hostSlotIndex, int partnerSlotIndex);
    ~PrecisionDyn2UUI() override = default;

    void setUiScale(float s);
    void setChassisTheme(kalyx::core::ChassisTheme t);
    void syncFromAPVTS();
    void updateScopeAndRepaint();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    [[nodiscard]] juce::String get2UParamId(int paramZeroBased) const;
    void updateDependentStates();

    KalyxHybridConsoleProcessor& processor_;
    int hostSlotIndex_    { 0 };
    int partnerSlotIndex_ { 1 };
    float uiScale_        { 1.0f };
    kalyx::core::ChassisTheme theme_ { kalyx::core::ChassisTheme::Night };

    juce::ToggleButton inBtn_ { "PRECISION DYN 2U" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> inAtt_;

    juce::ToggleButton autoRelBtn_ { "AUTO REL" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoRelAtt_;

    std::unique_ptr<KalyxSegmentedPill> orderPill_;
    std::unique_ptr<KalyxRotaryControl> lookaheadKnob_;

    // 11 potentiomètres + 1 sélecteur Routing
    std::array<std::unique_ptr<KalyxRotaryControl>, 11> knobs_;
    std::unique_ptr<KalyxArrowSelector> routingSelector_;

    juce::Rectangle<float> headerRect_;
    juce::Rectangle<float> scopeRect_;

    std::array<PrecisionDyn2UDSP::ScopeSample, PrecisionDyn2UDSP::kScopeSize> scopeHist_ {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PrecisionDyn2UUI)
};

} // namespace kalyx::consoles
