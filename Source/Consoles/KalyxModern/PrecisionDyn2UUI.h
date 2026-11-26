#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Core/ConsoleModuleBase.h"
#include "../../PluginProcessor.h"
#include "../../UI/KalyxLookAndFeel.h"
#include "../../UI/DataWidgets.h"
#include "PrecisionDyn2UDSP.h"
#include <array>
#include <memory>

namespace kalyx::consoles
{

/**
 * Façade « Data-Driven 2D » 2U Double-Largeur de Kalyx Kinetic Dynamics (ex-Precision Dyn 2U).
 * - En-tête : [KINETIC DYN 2U] [PRE-TRANS | POST-TRANS] [LOOKAHEAD TactileFaderBar] [AUTO REL]
 * - Zone 1 (55%) : Oscilloscope gradué temps réel (0/-3/-6/-12 dB + temps, Waveform + GR + Transients)
 * - Zone 2 (35%) : Pad 2D Vectoriel PUNCH x SUSTAIN + 9 contrôles rotatifs contraints (<= 45 * uiScale)
 * - Zone 3 (10%) : Barre horizontale pleine largeur ROUTING : < STEREO >
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

    juce::ToggleButton inBtn_ { "KINETIC DYN 2U" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> inAtt_;

    juce::ToggleButton autoRelBtn_ { "AUTO REL" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoRelAtt_;

    std::unique_ptr<KalyxSegmentedPill> orderPill_;
    TactileFaderBar lookaheadBar_;

    VectorPad2D punchSustainPad_;
    // 9 potentiomètres : THRESH(0), RATIO(1), KNEE(2), ATTACK(3), RELEASE(4), MAKEUP(5), TRANS FRQ(6), SC HPF(7), BLEND(8)
    std::array<std::unique_ptr<KalyxRotaryControl>, 9> knobs_;
    std::unique_ptr<KalyxArrowSelector> routingSelector_;

    juce::Rectangle<float> headerRect_;
    juce::Rectangle<float> scopeRect_;

    std::array<PrecisionDyn2UDSP::ScopeSample, PrecisionDyn2UDSP::kScopeSize> scopeHist_ {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PrecisionDyn2UUI)
};

} // namespace kalyx::consoles
