#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Core/ConsoleModuleBase.h"
#include "KalyxLookAndFeel.h"
#include <functional>
#include <memory>

/**
 * Barre fader horizontale « Data-Driven » (hauteur ~22 * uiScale).
 * Remplissage proportionnel (ou symétrique depuis le centre si bipolar = true),
 * libellé à gauche, valeur numérique nette à droite.
 * - Glisser horizontal (Shift/Ctrl = fin 0.1x)
 * - Double-clic sur la valeur (droite) : saisie clavier absolue inline
 * - Double-clic sur la barre : retour à defaultResetValue
 */
class TactileFaderBar final : public juce::Component,
                              public juce::SettableTooltipClient
{
public:
    TactileFaderBar() = default;
    ~TactileFaderBar() override = default;

    void init(juce::Component& parent,
              juce::AudioProcessorValueTreeState& apvts,
              const juce::String& paramID,
              const juce::String& title,
              juce::Colour accentColour,
              bool bipolar,
              bool darkText,
              double defaultResetValue,
              std::function<juce::String(double)> valueFormatter,
              const juce::String& infoHint = {},
              std::function<double(const juce::String&)> valueParser = {});

    void setUiScale(float s) noexcept { uiScale_ = std::clamp(s, 0.75f, 2.20f); repaint(); }
    void setThemeColours(juce::Colour accentColour, bool darkText) noexcept;
    void setDimmedDisabled(bool dimmed) noexcept { dimmed_ = dimmed; setEnabled(!dimmed); repaint(); }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    void openInlineEditor();
    void closeInlineEditor(bool commit);

    juce::String title_;
    juce::Colour accent_ { KalyxTheme::kalyxRoyalBlue };
    bool bipolar_  { false };
    bool darkText_ { false };
    bool dimmed_   { false };
    float uiScale_ { 1.0f };
    double defaultResetValue_ { 0.5 };
    double currentValue_ { 0.5 };
    double dragStartValue_ { 0.5 };
    float  dragStartX_ { 0.0f };

    juce::RangedAudioParameter* param_ { nullptr };
    std::unique_ptr<juce::ParameterAttachment> att_;
    std::function<juce::String(double)> formatter_;
    std::function<double(const juce::String&)> parser_;
    std::unique_ptr<juce::TextEditor> inlineEditor_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TactileFaderBar)
};

/**
 * Fader vertical gradué de console (-inf à +12 dB, repère 0.0 dB magnétique) pour OUT FADER.
 */
class VerticalConsoleFader final : public juce::Component,
                                   public juce::SettableTooltipClient
{
public:
    VerticalConsoleFader() = default;
    ~VerticalConsoleFader() override = default;

    void init(juce::Component& parent,
              juce::AudioProcessorValueTreeState& apvts,
              const juce::String& paramID,
              const juce::String& title,
              juce::Colour accentColour,
              bool darkText,
              double minDb = -60.0,
              double maxDb = 12.0,
              double defaultDb = 0.0,
              const juce::String& infoHint = {});

    void setUiScale(float s) noexcept { uiScale_ = std::clamp(s, 0.75f, 2.20f); repaint(); }
    void setThemeColours(juce::Colour accentColour, bool darkText) noexcept;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    void openInlineEditor();
    void closeInlineEditor(bool commit);
    [[nodiscard]] float dbToY(double db, const juce::Rectangle<float>& track) const noexcept;
    [[nodiscard]] double yToDb(float y, const juce::Rectangle<float>& track) const noexcept;

    juce::String title_;
    juce::Colour accent_ { KalyxTheme::kalyxRoyalBlue };
    bool darkText_ { false };
    float uiScale_ { 1.0f };
    double minDb_ { -60.0 };
    double maxDb_ { 12.0 };
    double defaultDb_ { 0.0 };
    double currentDb_ { 0.0 };
    double dragStartDb_ { 0.0 };
    float  dragStartY_ { 0.0f };

    juce::Rectangle<float> trackRect_;
    juce::Rectangle<float> valueRect_;
    juce::RangedAudioParameter* param_ { nullptr };
    std::unique_ptr<juce::ParameterAttachment> att_;
    std::unique_ptr<juce::TextEditor> inlineEditor_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VerticalConsoleFader)
};

/**
 * Courbe de transfert dynamique interactive (-48 dB à 0 dB sur X et Y) pour tous les compresseurs & limiteurs.
 * - Affiche la courbe caractéristique de la machine (VCA, FET, Slam, Opto, Vari-Mu, Diode, Distressor, Limiter)
 * - Bille lumineuse temps réel selon inputPeakDb et grDb + valeur GR dans le coin
 * - Glisser à la souris pour régler le Threshold (aimanté par paliers de 2 dB si steppedVintage)
 */
class InteractiveTransferCurve final : public juce::Component,
                                       public juce::SettableTooltipClient
{
public:
    enum class CurveType
    {
        VCA,
        FET,
        FETSlam,
        Opto,
        VariMu,
        Diode,
        Distressor,
        DistressorBrit,
        Limiter
    };

    InteractiveTransferCurve() = default;
    ~InteractiveTransferCurve() override = default;

    void init(juce::Component& parent,
              juce::AudioProcessorValueTreeState& apvts,
              const juce::String& threshParamID,
              float minThreshDb,
              float maxThreshDb,
              float defaultThreshNorm,
              bool isOptoPeakReduction,
              juce::Colour accentColour,
              bool darkText,
              const juce::String& infoHint = {});

    void setCurveConfig(CurveType type, float ratio, float kneeDb, bool active, bool steppedVintage) noexcept;
    void setTelemetry(float inputPeakDb, float grDb) noexcept;
    void setUiScale(float s) noexcept { uiScale_ = std::clamp(s, 0.75f, 2.20f); repaint(); }
    void setThemeColours(juce::Colour accentColour, bool darkText) noexcept;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    [[nodiscard]] float computeOutputDb(float inDb) const noexcept;
    [[nodiscard]] float getCurrentThreshDb() const noexcept;

    CurveType curveType_ { CurveType::VCA };
    float ratio_       { 4.0f };
    float kneeDb_      { 4.0f };
    bool  active_      { true };
    bool  stepped_     { false };
    bool  isOpto_      { false };
    float minThreshDb_ { -40.0f };
    float maxThreshDb_ { 0.0f };
    float defNorm_     { 0.5f };
    float curNorm_     { 0.5f };
    float dragStartNorm_ { 0.5f };
    float dragStartX_  { 0.0f };
    float dragStartY_  { 0.0f };

    float inPeakDb_    { -60.0f };
    float grDb_        { 0.0f };

    juce::Colour accent_ { KalyxTheme::kalyxRoyalBlue };
    bool  darkText_    { false };
    float uiScale_     { 1.0f };

    juce::RangedAudioParameter* param_ { nullptr };
    std::unique_ptr<juce::ParameterAttachment> att_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InteractiveTransferCurve)
};

/**
 * Pad 2D Vectoriel pour paramètres couplés (ex. EVEN x ODD sur Harmonic Matrix, PUNCH x SUSTAIN sur Kinetic Dynamics).
 */
class VectorPad2D final : public juce::Component,
                          public juce::SettableTooltipClient
{
public:
    VectorPad2D() = default;
    ~VectorPad2D() override = default;

    void init(juce::Component& parent,
              juce::AudioProcessorValueTreeState& apvts,
              const juce::String& paramXId,
              const juce::String& paramYId,
              const juce::String& labelX,
              const juce::String& labelY,
              float defNormX,
              float defNormY,
              bool bipolar,
              juce::Colour accentColour,
              bool darkText,
              std::function<juce::String(double)> fmtX = {},
              std::function<juce::String(double)> fmtY = {},
              const juce::String& infoHint = {});

    void setSteppedMagnetic(bool stepped) noexcept { stepped_ = stepped; }
    void setDimmedDisabled(bool dimmed) noexcept { dimmed_ = dimmed; setEnabled(!dimmed); repaint(); }
    void setUiScale(float s) noexcept { uiScale_ = std::clamp(s, 0.75f, 2.20f); repaint(); }
    void setThemeColours(juce::Colour accentColour, bool darkText) noexcept;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    juce::String labelX_, labelY_;
    float normX_ { 0.5f }, normY_ { 0.5f };
    float defX_  { 0.5f }, defY_  { 0.5f };
    float dragStartX_ { 0.0f }, dragStartY_ { 0.0f };
    float dragStartNormX_ { 0.5f }, dragStartNormY_ { 0.5f };
    bool bipolar_  { false };
    bool stepped_  { false };
    bool dimmed_   { false };
    bool darkText_ { false };
    float uiScale_ { 1.0f };
    juce::Colour accent_ { KalyxTheme::kalyxAubergine };

    juce::RangedAudioParameter* paramX_ { nullptr };
    juce::RangedAudioParameter* paramY_ { nullptr };
    std::unique_ptr<juce::ParameterAttachment> attX_, attY_;
    std::function<juce::String(double)> fmtX_, fmtY_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VectorPad2D)
};

/**
 * Graphe de bande passante interactif (20 Hz - 20 kHz) pour la tranche INPUT (HPF + LPF glissables).
 */
class InputFilterGraph final : public juce::Component,
                               public juce::SettableTooltipClient
{
public:
    InputFilterGraph() = default;
    ~InputFilterGraph() override = default;

    void init(juce::Component& parent,
              juce::AudioProcessorValueTreeState& apvts,
              const juce::String& hpfEnId,
              const juce::String& hpfFreqId,
              const juce::String& lpfEnId,
              const juce::String& lpfFreqId,
              juce::Colour accentColour,
              bool darkText);

    void setUiScale(float s) noexcept { uiScale_ = std::clamp(s, 0.75f, 2.20f); repaint(); }
    void setThemeColours(juce::Colour accentColour, bool darkText) noexcept;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    [[nodiscard]] float freqToX(float hz) const noexcept;
    [[nodiscard]] float xToFreq(float x) const noexcept;

    bool  hpfOn_   { false };
    float hpfFreq_ { 40.0f };
    bool  lpfOn_   { false };
    float lpfFreq_ { 18000.0f };
    int   dragHandle_ { 0 }; // 0=none, 1=HPF, 2=LPF

    juce::Colour accent_ { KalyxTheme::kalyxRoyalBlue };
    bool  darkText_ { false };
    float uiScale_  { 1.0f };

    juce::RangedAudioParameter* pHpfEn_   { nullptr };
    juce::RangedAudioParameter* pHpfFreq_ { nullptr };
    juce::RangedAudioParameter* pLpfEn_   { nullptr };
    juce::RangedAudioParameter* pLpfFreq_ { nullptr };
    std::unique_ptr<juce::ParameterAttachment> attHpfEn_, attHpfFreq_, attLpfEn_, attLpfFreq_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputFilterGraph)
};
