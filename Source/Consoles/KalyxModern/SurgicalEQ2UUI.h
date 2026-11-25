#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../../Core/ConsoleModuleBase.h"
#include "../../PluginProcessor.h"
#include "../../UI/KalyxLookAndFeel.h"
#include "SurgicalEQ2UDSP.h"
#include <array>
#include <memory>

namespace kalyx::consoles
{

/**
 * Façade « Tactile Flat » 2U Double-Largeur du Kalyx Surgical EQ (SplitEQ + Dynamic EQ 6 bandes).
 * Combine les 36 paramètres des deux slots adjacents (hostSlotIndex_ + partnerSlotIndex_).
 */
class SurgicalEQ2UUI final : public juce::Component
{
public:
    SurgicalEQ2UUI(KalyxHybridConsoleProcessor& proc, int hostSlotIndex, int partnerSlotIndex);
    ~SurgicalEQ2UUI() override = default;

    void setUiScale(float s);
    void setChassisTheme(kalyx::core::ChassisTheme t);
    void syncFromAPVTS();
    void updateFFTAndRepaint();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    void selectBand(int bandIdx);
    void rebindBandKnobs();
    void updateMetaFromProcessor();
    void setBandMeta(int bandIdx, const SurgicalEQ2UDSP::BandMeta& m);
    void set2UParamNorm(int paramZeroBased, float normVal);
    [[nodiscard]] juce::String get2UParamId(int paramZeroBased) const;

    [[nodiscard]] float freqToX(float hz) const noexcept;
    [[nodiscard]] float xToFreq(float x) const noexcept;
    [[nodiscard]] float gainToY(float db) const noexcept;
    [[nodiscard]] float yToGain(float y) const noexcept;
    void showBandContextMenu(int bandIdx, juce::Point<int> screenPos);

    KalyxHybridConsoleProcessor& processor_;
    int hostSlotIndex_    { 0 };
    int partnerSlotIndex_ { 1 };

    float uiScale_ { 1.0f };
    kalyx::core::ChassisTheme theme_ { kalyx::core::ChassisTheme::Night };
    bool moduleIn_     { true };
    int  selectedBand_ { 2 }; // Band 3 par défaut (1 kHz)
    int  draggedNode_  { -1 };
    float dragStartQ_  { 1.0f };
    float dragStartY_  { 0.0f };

    // Top bar : [● SURGICAL EQ 2U] [ ZERO LAT | NATURAL ]
    juce::ToggleButton inBtn_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> inAtt_;
    std::unique_ptr<KalyxSegmentedPill> phasePill_;

    // 5 potentiomètres de la bande sélectionnée
    KalyxRotaryControl freqKnob_;
    KalyxRotaryControl tonalKnob_;
    KalyxRotaryControl transKnob_;
    KalyxRotaryControl qKnob_;
    KalyxRotaryControl dynKnob_;

    // Contrôles discrets de bande
    std::array<juce::TextButton, 6> bandBtns_;
    std::array<juce::TextButton, 5> shapeBtns_;
    juce::TextButton slopeLeftBtn_ { "<" }, slopeRightBtn_ { ">" };
    std::array<juce::TextButton, 5> stereoBtns_;
    juce::TextButton soloBtn_ { "SOLO BAND" };

    // Zones géométriques
    juce::Rectangle<float> graphRect_;
    juce::Rectangle<float> slopeLabelRect_;
    juce::Rectangle<float> sepTopRect_, sepBotRect_;

    // FFT Haute Résolution 2048 pts (Pré & Post simultanés)
    juce::dsp::FFT forwardFFT_ { KalyxHybridConsoleProcessor::kFFTOrder };
    juce::dsp::WindowingFunction<float> windowing_ { KalyxHybridConsoleProcessor::kFFTSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, KalyxHybridConsoleProcessor::kFFTSize * 2> fftData_ {};
    std::array<float, KalyxHybridConsoleProcessor::kFFTSize / 2> smoothedBinsDb_ {};
    std::array<float, KalyxHybridConsoleProcessor::kFFTSize * 2> preFftData_ {};
    std::array<float, KalyxHybridConsoleProcessor::kFFTSize / 2> preSmoothedBinsDb_ {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SurgicalEQ2UUI)
};

} // namespace kalyx::consoles
