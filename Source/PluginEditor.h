#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include <memory>
#include <array>

/**
 * Palette Vectorielle Stricte "Aubergine & Bleu Roi" (Night Mode & Day Mode)
 * + Palette SSL Harmonisée 4-Band EQ (Vin / Bleu Ardoise / Vert Jade / Argent).
 */
namespace KalyxTheme
{
    inline const juce::Colour nightChassisBg      { 0xff0f0e17 };
    inline const juce::Colour nightStripBg        { 0xff171524 };
    inline const juce::Colour nightControlIdleBg  { 0xff211e33 };
    inline const juce::Colour nightGridBorder     { 0xff322d4d };
    inline const juce::Colour nightEncoderOutline { 0xff4d4575 };
    inline const juce::Colour nightTextPrimary    { 0xfff3f2f8 };
    inline const juce::Colour nightTextSecondary  { 0xff9d99b8 };

    // Day Mode (Façade gris clair / aluminium chaud SSL #DCDFE4, textes noir/anthracite #12151C)
    inline const juce::Colour dayChassisBg        { 0xffc8cdd5 };
    inline const juce::Colour dayStripBg          { 0xffdcdfe4 };
    inline const juce::Colour dayControlIdleBg    { 0xffeceef2 };
    inline const juce::Colour dayGridBorder       { 0xffaeb5c0 };
    inline const juce::Colour dayEncoderOutline   { 0xff5a6270 };
    inline const juce::Colour dayTextPrimary      { 0xff12151c };
    inline const juce::Colour dayTextSecondary    { 0xff12151c };

    inline const juce::Colour headerAnalog        { 0xff4a1942 };
    inline const juce::Colour headerSurgical      { 0xff1d3557 };

    inline const juce::Colour aubergineBtn        { 0xff7b2cbf };
    inline const juce::Colour aubergineArc        { 0xffc77dff };
    inline const juce::Colour royalBlueBtn        { 0xff2563eb };
    inline const juce::Colour royalBlueArc        { 0xff60a5fa };

    inline const juce::Colour eqLfWineRed         { 0xff8e2438 }; // LF  : Rouge Vin profond (#8E2438)
    inline const juce::Colour eqLmfSlateBlue      { 0xff3b6e8c }; // LMF : Bleu Ardoise / Acier (#3B6E8C)
    inline const juce::Colour eqHmfJadeGreen      { 0xff276b58 }; // HMF : Vert Jade / Forêt (#276B58)
    inline const juce::Colour eqHfPearlSilver     { 0xffc5cdd6 }; // HF  : Argent / Gris perle clair (#C5CDD6)

    inline const juce::Colour amberGr             { 0xfff59e0b };
    inline const juce::Colour cyanDeessGr         { 0xff38bdf8 };
    inline const juce::Colour alertRed            { 0xffef4444 };

    [[nodiscard]] inline juce::Colour getChassisBg(bool day) noexcept      { return day ? dayChassisBg      : nightChassisBg; }
    [[nodiscard]] inline juce::Colour getStripBg(bool day) noexcept        { return day ? dayStripBg        : nightStripBg; }
    [[nodiscard]] inline juce::Colour getControlIdleBg(bool day) noexcept  { return day ? dayControlIdleBg  : nightControlIdleBg; }
    [[nodiscard]] inline juce::Colour getGridBorder(bool day) noexcept     { return day ? dayGridBorder     : nightGridBorder; }
    [[nodiscard]] inline juce::Colour getEncoderOutline(bool day) noexcept { return day ? dayEncoderOutline : nightEncoderOutline; }
    [[nodiscard]] inline juce::Colour getTextPrimary(bool day) noexcept    { return day ? dayTextPrimary    : nightTextPrimary; }
    [[nodiscard]] inline juce::Colour getTextSecondary(bool day) noexcept  { return day ? dayTextSecondary  : nightTextSecondary; }
} // namespace KalyxTheme

/**
 * LookAndFeel vectoriel pur (juce::Graphics) supportant le Night Mode et le Day Mode.
 */
class SSLConsoleLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    SSLConsoleLookAndFeel();
    ~SSLConsoleLookAndFeel() override = default;

    void setDayMode(bool isDayMode) noexcept;
    [[nodiscard]] bool isDayMode() const noexcept { return dayMode_; }

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override;

    void drawToggleButton(juce::Graphics& g,
                          juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    juce::Label* createSliderTextBox(juce::Slider& slider) override;

private:
    bool dayMode_ { false };
};

/**
 * Visualiseur et Éditeur Graphique Interactif Temps Réel de l'EQ 4 Bandes + Analyseur de Spectre FFT 2048 pts.
 */
class SSLEQCurveComponent final : public juce::Component
{
public:
    explicit SSLEQCurveComponent(KalyxSSLConsoleProcessor& proc) noexcept;

    void setDayMode(bool isDay) noexcept { dayMode_ = isDay; }
    void updateFFTFromProcessor() noexcept;
    void paint(juce::Graphics& g) override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    [[nodiscard]] juce::Rectangle<float> getPlotArea() const noexcept;
    [[nodiscard]] float freqToX(float freqHz, const juce::Rectangle<float>& area) const noexcept;
    [[nodiscard]] float xToFreq(float x, const juce::Rectangle<float>& area) const noexcept;
    [[nodiscard]] float gainToY(float gainDb, const juce::Rectangle<float>& area) const noexcept;
    [[nodiscard]] float yToGain(float y, const juce::Rectangle<float>& area) const noexcept;
    [[nodiscard]] std::array<juce::Point<float>, 4> getBandNodePositions(const juce::Rectangle<float>& area) const noexcept;
    [[nodiscard]] int findNodeAtPosition(juce::Point<float> pos, const juce::Rectangle<float>& area) const noexcept;
    void setPlainParamValue(const char* paramID, float plainValue) noexcept;

    KalyxSSLConsoleProcessor& processor_;
    bool dayMode_ { false };
    int draggedNodeIndex_ { -1 };
    int hoveredNodeIndex_ { -1 };
    float dragStartQ_ { 1.2f };
    float dragStartY_ { 0.0f };

    // Moteur FFT pré-alloué (2048 points, fenêtre de Hann)
    juce::dsp::FFT forwardFFT_;
    juce::dsp::WindowingFunction<float> windowing_;
    std::array<float, KalyxSSLConsoleProcessor::kFFTSize * 2> fftData_ {};
    std::array<float, KalyxSSLConsoleProcessor::kFFTSize / 2> smoothedBinsDb_ {};
};

/**
 * Panneau flottant (Overlay) de Paramètres (SETTINGS) :
 * - Calibration des VU-mètres : dBFS / VU (dB) / LUFS
 * - Référence 0 VU : boutons rapides (-20, -18, -14, -12 dBFS) + slider libre
 * - Oversampling DSP : 1x (Off) / 2x / 4x
 * - Analyseur FFT : POST EQ / PRE EQ / OFF
 * - Tolérance Analogique L/R : 0 % à 100 %
 */
class SSLSettingsOverlayComponent final : public juce::Component
{
public:
    explicit SSLSettingsOverlayComponent(KalyxSSLConsoleProcessor& proc);
    ~SSLSettingsOverlayComponent() override = default;

    void setDayMode(bool isDay) noexcept { dayMode_ = isDay; syncButtonsFromAPVTS(); }
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void syncButtonsFromAPVTS() noexcept;
    void setChoiceParam(const char* paramID, int index) noexcept;
    void setFloatParam(const char* paramID, float val) noexcept;

    KalyxSSLConsoleProcessor& processor_;
    bool dayMode_ { false };

    juce::TextButton closeBtn_ { "X" };

    std::array<juce::TextButton, 3> scaleBtns_ {
        juce::TextButton { "dBFS (Peak)" },
        juce::TextButton { "VU (dB)" },
        juce::TextButton { "LUFS (ITU)" }
    };

    std::array<juce::TextButton, 4> vuPresetBtns_ {
        juce::TextButton { "-20 dBFS" },
        juce::TextButton { "-18 dBFS" },
        juce::TextButton { "-14 dBFS" },
        juce::TextButton { "-12 dBFS" }
    };
    juce::Slider vuRefSlider_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> vuRefAttachment_;

    std::array<juce::TextButton, 3> osBtns_ {
        juce::TextButton { "1x (Off)" },
        juce::TextButton { "2x IIR" },
        juce::TextButton { "4x IIR" }
    };

    std::array<juce::TextButton, 3> fftBtns_ {
        juce::TextButton { "POST EQ" },
        juce::TextButton { "PRE EQ" },
        juce::TextButton { "FFT OFF" }
    };

    juce::Slider analogTolSlider_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> analogTolAttachment_;
};

/**
 * Éditeur principal redimensionnable de la tranche de console SSL Kalyx v5.
 */
class KalyxSSLConsoleEditor final : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit KalyxSSLConsoleEditor(KalyxSSLConsoleProcessor&);
    ~KalyxSSLConsoleEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    void applyThemeToComponents(bool isDayMode);
    void updateLimiterAndCircuitUIState();
    void drawLockIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool isLocked);

    struct RotaryControl
    {
        juce::Slider slider;
        juce::Label  titleLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupRotary(RotaryControl& ctrl,
                     const char* paramID,
                     const juce::String& title,
                     const juce::Colour& arcColour,
                     double defaultResetValue);

    void setupToggle(juce::ToggleButton& btn,
                     std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& att,
                     const char* paramID,
                     const juce::String& text,
                     const juce::Colour& activeColour);

    KalyxSSLConsoleProcessor& processorRef_;
    SSLConsoleLookAndFeel sslLookAndFeel_;
    SSLEQCurveComponent eqCurveView_;
    SSLSettingsOverlayComponent settingsOverlay_;

    // Bandeau supérieur sur une seule ligne : Presets, A/B, Copy A>B, Mode M/S, Day Mode, Settings
    juce::ComboBox presetBox_;
    juce::TextButton abToggleBtn_ { "A / B : A" };
    juce::TextButton abCopyBtn_   { "COPY A>B" };
    juce::ComboBox msModeBox_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> msModeAttachment_;
    juce::ToggleButton dayModeBtn_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> dayModeAtt_;
    juce::TextButton settingsBtn_ { "SETTINGS" };

    // Section 1 : Input, Preamp Sat, Filters & FILT/PRE TRIM
    RotaryControl inputGainCtrl_;
    RotaryControl inputSatCtrl_;
    RotaryControl hpfFreqCtrl_;
    RotaryControl hpfQCtrl_;
    RotaryControl lpfFreqCtrl_;
    RotaryControl lpfQCtrl_;
    RotaryControl filtPreTrimCtrl_;
    juce::ToggleButton phaseInvertBtn_;
    juce::ToggleButton hpfEnableBtn_;
    juce::ToggleButton lpfEnableBtn_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> phaseInvertAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hpfEnableAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lpfEnableAtt_;

    // Section 2 : Dynamics, De-Esser, Sidechain, AUTO-REL & Dedicated SC HPF
    juce::ToggleButton dynEnableBtn_;
    juce::ToggleButton limiterModeBtn_;
    juce::ToggleButton deessEnableBtn_;
    juce::ToggleButton extScBtn_;
    juce::ToggleButton filtersToScBtn_;
    juce::ToggleButton scListenBtn_;
    juce::ToggleButton compAutoRelBtn_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> dynEnableAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> limiterModeAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> deessEnableAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> extScAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> filtersToScAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> scListenAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> compAutoRelAtt_;
    RotaryControl gateThreshCtrl_;
    RotaryControl gateRangeCtrl_;
    RotaryControl compThreshCtrl_;
    RotaryControl compRatioCtrl_;
    RotaryControl compAttackCtrl_;
    RotaryControl compReleaseCtrl_;
    RotaryControl scHpfFreqCtrl_;
    RotaryControl deessFreqCtrl_;
    RotaryControl deessThreshCtrl_;
    RotaryControl dynMakeupCtrl_;

    // Section 3 : Console Color (COLOR DRIVE, 4000 E/G, SAT TONE, CROSSTALK, COLOR TRIM)
    RotaryControl colorAmountCtrl_;
    RotaryControl satToneCtrl_;
    RotaryControl crosstalkCtrl_;
    RotaryControl colorTrimCtrl_;
    juce::ToggleButton colorLinkBtn_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> colorLinkAtt_;
    juce::TextButton circuitEBtn_ { "4000 E" };
    juce::TextButton circuitGBtn_ { "4000 G" };

    // Section 4 : Parametric Equaliser (4-Band SSL EQ + EQ PRE DYN + LF/HF Bell + EQ TRIM)
    juce::ToggleButton eqEnableBtn_;
    juce::ToggleButton eqPreDynBtn_;
    juce::ToggleButton lfBellBtn_;
    juce::ToggleButton hfBellBtn_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> eqEnableAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> eqPreDynAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lfBellAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hfBellAtt_;
    RotaryControl lfFreqCtrl_;
    RotaryControl lfGainCtrl_;
    RotaryControl lmfFreqCtrl_;
    RotaryControl lmfGainCtrl_;
    RotaryControl lmfQCtrl_;
    RotaryControl hmfFreqCtrl_;
    RotaryControl hmfGainCtrl_;
    RotaryControl hmfQCtrl_;
    RotaryControl hfFreqCtrl_;
    RotaryControl hfGainCtrl_;
    RotaryControl eqTrimCtrl_;

    // Section 5 : Output, True Auto-Gain, Analog Tolerance, Console Mix & Out Fader
    juce::ToggleButton autoGainBtn_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAtt_;
    RotaryControl analogTolCtrl_;
    RotaryControl outputGainCtrl_;
    RotaryControl mixCtrl_;

    // Zones interactives des 5 cadenas de verrouillage de section (Parameter Lock)
    std::array<juce::Rectangle<float>, 5> lockIconRects_ {};

    // Zones interactives des flèches < et > de réorganisation des 3 modules centraux (Slots 0, 1, 2)
    std::array<juce::Rectangle<float>, 3> arrowLeftRects_ {};
    std::array<juce::Rectangle<float>, 3> arrowRightRects_ {};

    // Zones interactives des 3 bargraphes de la section 2 (COMP, GATE, DE-ESS)
    std::array<juce::Rectangle<float>, 3> dynMeterRects_ {};
    int draggedMeterThresholdIndex_ { -1 };

    kalyx::dsp::SSLMeterSnapshot meterSnap_ {};
    bool lastDayMode_ { false };
    int lastModuleOrder_ { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KalyxSSLConsoleEditor)
};
