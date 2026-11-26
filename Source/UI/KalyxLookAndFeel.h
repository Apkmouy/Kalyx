#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "../PluginProcessor.h"
#include <memory>
#include <array>
#include <functional>

namespace KalyxTheme
{
    // Signature Kalyx Audio (Châssis, Header, Input, Master & Modules Originaux)
    inline const juce::Colour nightChassisBg      { 0xff111318 }; // Anthracite Nuit
    inline const juce::Colour nightStripBg        { 0xff181b22 }; // Gris Ardoise Profond (surface unifiée Slots & Cartes)
    inline const juce::Colour nightControlIdleBg  { 0xff222630 }; // Cercles plats boutons
    inline const juce::Colour knobTrackBg         { 0xff2e3340 }; // Pistes d'arcs
    inline const juce::Colour nightGridBorder     { 0xff262a35 }; // Bordures fines
    inline const juce::Colour nightEncoderOutline { 0xff3a4050 };
    inline const juce::Colour nightTextPrimary    { 0xfff2f4f8 };

    // Couleurs Primaire & Secondaire Kalyx Audio
    inline const juce::Colour kalyxAubergine      { 0xff5b2344 }; // Primaire Kalyx (Aubergine Profond)
    inline const juce::Colour kalyxAubergineHover { 0xff7a325d }; // Actif / Survol
    inline const juce::Colour kalyxRoyalBlue      { 0xff2348b8 }; // Secondaire Kalyx (Bleu Roi Intense)
    inline const juce::Colour kalyxRoyalBlueGlow  { 0xff3b66e0 }; // Arcs actifs & voyants

    // Day Mode : Aluminium Neutre #E6E8EC, surfaces #F4F5F7, texte #12151C
    inline const juce::Colour dayChassisBg        { 0xffe6e8ec };
    inline const juce::Colour dayStripBg          { 0xfff4f5f7 };
    inline const juce::Colour dayControlIdleBg    { 0xffe2e5ea };
    inline const juce::Colour dayKnobTrackBg      { 0xffcbd2df };
    inline const juce::Colour dayGridBorder       { 0xffd0d5dd };
    inline const juce::Colour dayEncoderOutline   { 0xff6e7685 };
    inline const juce::Colour dayTextPrimary      { 0xff12151c };
    inline const juce::Colour dayAubergine        { 0xff6d2b52 };
    inline const juce::Colour dayRoyalBlue        { 0xff1e3fa3 };

    // Finition Kalyx Heritage (alias vers identité unifiée)
    inline const juce::Colour kalyxForestTeal     { 0xff181b22 };
    inline const juce::Colour kalyxForestDark     { 0xff111318 };
    inline const juce::Colour kalyxControlBg      { 0xff222630 };
    inline const juce::Colour kalyxBorder         { 0xff262a35 };
    inline const juce::Colour kalyxRoseParme      { 0xff5b2344 };

    // Codes Couleurs Référencés par Machine (sur fond de slot #181B22)
    inline const juce::Colour sslEMatteGrey       { 0xff181b22 };
    inline const juce::Colour sslGMatteGrey       { 0xff181b22 };
    inline const juce::Colour sslOrangeAccent     { 0xffd96b27 };
    inline const juce::Colour apiAnthracite       { 0xff181b22 };
    inline const juce::Colour apiCobaltBlue       { 0xff1e64b4 }; // API Bleu Cobalt
    inline const juce::Colour apiCyanLight        { 0xff4da3ff }; // API Cyan Clair
    inline const juce::Colour neveRafGreyBlue     { 0xff467294 }; // Neve Bleu-Gris RAF
    inline const juce::Colour neveBrickRed        { 0xffa62c3b }; // Neve Rouge Brique Marconi

    // SSL 4000 E / G
    inline const juce::Colour eqLfWineRed         { 0xff9e2a3b }; // SSL E Rouge Vin
    inline const juce::Colour eqLmfSlateBlue      { 0xff4a6b82 }; // SSL Ardoise
    inline const juce::Colour eqHmfJadeGreen      { 0xff2d7a64 }; // SSL Jade
    inline const juce::Colour eqHfPearlSilver     { 0xffc8d0dc };
    inline const juce::Colour sslEBrownCap        { 0xff2b2322 };
    inline const juce::Colour sslEBlackCap        { 0xff222630 };
    inline const juce::Colour sslGPinkMidCap      { 0xffc85a7c }; // SSL G Rose
    inline const juce::Colour sslGRedMidCap       { 0xff9e2a3b };

    // UA 1176, LA-2A, Distressor, Fairchild 660/670
    inline const juce::Colour ua1176RevABlue      { 0xff2b6cb0 }; // Rev A Bluestripe
    inline const juce::Colour ua1176RevDAmber     { 0xffd64545 }; // Rev D Rouge Corail / Ambre Vif
    inline const juce::Colour ua1176RevGSilver    { 0xffb4bcc8 }; // Rev G Argent Clair
    inline const juce::Colour la2aAmber           { 0xffd98a2b }; // LA-2A Voyant Ambre
    inline const juce::Colour la2aSteel           { 0xff7e96ad }; // LA-2A Acier Bleuté / Argent
    inline const juce::Colour distressorRed       { 0xffc93030 };
    inline const juce::Colour distressorAmber     { 0xffe0a82e };
    inline const juce::Colour fairchildGold       { 0xffc59b38 }; // Or Vintage
    inline const juce::Colour fairchildCopper     { 0xff9e5834 }; // Cuivre Chaud

    inline const juce::Colour amberLed            { 0xffe0a82e };
    inline const juce::Colour alertRed            { 0xffc93030 };
    inline const juce::Colour royalBlue           { 0xff2348b8 };

    [[nodiscard]] inline juce::Colour getChassisBg(kalyx::core::ChassisTheme t) noexcept
    {
        if (t == kalyx::core::ChassisTheme::Day) return dayChassisBg;
        return nightChassisBg;
    }

    [[nodiscard]] inline juce::Colour getStripBg(kalyx::core::ChassisTheme t) noexcept
    {
        if (t == kalyx::core::ChassisTheme::Day) return dayStripBg;
        return nightStripBg;
    }

    [[nodiscard]] inline juce::Colour getControlIdleBg(kalyx::core::ChassisTheme t) noexcept
    {
        if (t == kalyx::core::ChassisTheme::Day) return dayControlIdleBg;
        return nightControlIdleBg;
    }

    [[nodiscard]] inline juce::Colour getGridBorder(kalyx::core::ChassisTheme t) noexcept
    {
        if (t == kalyx::core::ChassisTheme::Day) return dayGridBorder;
        return nightGridBorder;
    }

    [[nodiscard]] inline juce::Colour getTextPrimary(kalyx::core::ChassisTheme t) noexcept
    {
        return (t == kalyx::core::ChassisTheme::Day) ? dayTextPrimary : nightTextPrimary;
    }

    [[nodiscard]] inline juce::Colour getTextSecondary(kalyx::core::ChassisTheme t) noexcept
    {
        return (t == kalyx::core::ChassisTheme::Day)
             ? dayTextPrimary.withAlpha(0.68f)
             : nightTextPrimary.withAlpha(0.68f);
    }

    [[nodiscard]] inline juce::Font makeFont(float size, int styleFlags = juce::Font::plain)
    {
        return juce::Font(juce::FontOptions(size, styleFlags));
    }

    [[nodiscard]] inline juce::Font makeMonoFont(float size, int styleFlags = juce::Font::plain)
    {
        return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), size, styleFlags));
    }

    [[nodiscard]] inline juce::Colour getAccent(kalyx::core::ChassisTheme t) noexcept
    {
        if (t == kalyx::core::ChassisTheme::Day)           return dayRoyalBlue;
        if (t == kalyx::core::ChassisTheme::KalyxHeritage) return kalyxAubergine;
        return kalyxRoyalBlue;
    }

    [[nodiscard]] inline juce::Colour getPrimary(kalyx::core::ChassisTheme t) noexcept
    {
        return (t == kalyx::core::ChassisTheme::Day) ? dayAubergine : kalyxAubergine;
    }
} // namespace KalyxTheme

class KalyxLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    KalyxLookAndFeel();
    ~KalyxLookAndFeel() override = default;

    void setChassisTheme(kalyx::core::ChassisTheme theme) noexcept;
    [[nodiscard]] kalyx::core::ChassisTheme getChassisTheme() const noexcept { return theme_; }

    void setUiScale(float s) noexcept { uiScale_ = std::clamp(s, 0.75f, 2.20f); }
    [[nodiscard]] float getUiScale() const noexcept { return uiScale_; }

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
    kalyx::core::ChassisTheme theme_ { kalyx::core::ChassisTheme::Night };
    float uiScale_ { 1.0f };
};

/**
 * Contrôle rotatif Tactile Flat proportionnel à uiScale :
 * - Cercle plat #222630, piste #2E3340, arc de valeur coloré
 * - Label 11.5f * uiScale, valeur 12.0f * uiScale tabular-nums
 * - Mode réglage fin (0.1x) avec Shift / Ctrl + glisser
 * - Grisage dynamique (alpha = 0.35f + arc gris neutre) des états dépendants.
 */
namespace KalyxValueParsers
{
    inline double parseCleanNumber(const juce::String& raw, juce::String& outLowerSuffix)
    {
        auto s = raw.trim();
        outLowerSuffix = {};
        int endNum = 0;
        bool seenDot = false;
        while (endNum < s.length())
        {
            const auto ch = s[endNum];
            if ((ch >= '0' && ch <= '9') || (endNum == 0 && (ch == '-' || ch == '+')))
                ++endNum;
            else if (ch == '.' && !seenDot)
                { seenDot = true; ++endNum; }
            else if (ch == ',' && !seenDot)
                { seenDot = true; s = s.replaceCharacter(',', '.'); ++endNum; }
            else
                break;
        }
        outLowerSuffix = s.substring(endNum).trim().toLowerCase();
        return s.substring(0, endNum).getDoubleValue();
    }

    [[nodiscard]] inline juce::String formatDbClean(double db)
    {
        if (std::abs(db) < 0.05) return "0.0 dB";
        return (db >= 0.05 ? "+" : "") + juce::String(db, 1) + " dB";
    }

    [[nodiscard]] inline std::function<double(const juce::String&)> parseLinDb(float minDb, float maxDb)
    {
        return [minDb, maxDb](const juce::String& text) -> double
        {
            juce::String suf;
            const double val = std::clamp(parseCleanNumber(text, suf), static_cast<double>(minDb), static_cast<double>(maxDb));
            return static_cast<double>(kalyx::core::unmapLin(static_cast<float>(val), minDb, maxDb));
        };
    }

    [[nodiscard]] inline std::function<double(const juce::String&)> parseLinPct(float minPct = 0.0f, float maxPct = 100.0f)
    {
        return [minPct, maxPct](const juce::String& text) -> double
        {
            juce::String suf;
            const double val = std::clamp(parseCleanNumber(text, suf), static_cast<double>(minPct), static_cast<double>(maxPct));
            return static_cast<double>(kalyx::core::unmapLin(static_cast<float>(val), minPct, maxPct));
        };
    }

    [[nodiscard]] inline std::function<double(const juce::String&)> parseHzLog(float minHz, float maxHz)
    {
        return [minHz, maxHz](const juce::String& text) -> double
        {
            juce::String suf;
            double hz = parseCleanNumber(text, suf);
            if (suf.startsWithChar('k'))
                hz *= 1000.0;
            hz = std::clamp(hz, static_cast<double>(minHz), static_cast<double>(maxHz));
            return static_cast<double>(kalyx::core::unmapLog(static_cast<float>(hz), minHz, maxHz));
        };
    }

    [[nodiscard]] inline std::function<double(const juce::String&)> parseMsLog(float minMs, float maxMs)
    {
        return [minMs, maxMs](const juce::String& text) -> double
        {
            juce::String suf;
            double ms = parseCleanNumber(text, suf);
            if (suf.startsWithChar('u') || suf.contains(juce::String::fromUTF8("\xC2\xB5")))
                ms *= 0.001;
            else if (suf == "s" || suf == "sec")
                ms *= 1000.0;
            ms = std::clamp(ms, static_cast<double>(minMs), static_cast<double>(maxMs));
            return static_cast<double>(kalyx::core::unmapLog(static_cast<float>(ms), minMs, maxMs));
        };
    }

    [[nodiscard]] inline std::function<double(const juce::String&)> parseRatioLog(float minR, float maxR)
    {
        return [minR, maxR](const juce::String& text) -> double
        {
            juce::String suf;
            const double r = std::clamp(parseCleanNumber(text, suf), static_cast<double>(minR), static_cast<double>(maxR));
            return static_cast<double>(kalyx::core::unmapLog(static_cast<float>(r), minR, maxR));
        };
    }

    [[nodiscard]] inline std::function<double(const juce::String&)> parseQLog(float minQ, float maxQ)
    {
        return [minQ, maxQ](const juce::String& text) -> double
        {
            auto s = text.trim();
            if (s.startsWithIgnoreCase("Q"))
                s = s.substring(1).trim();
            juce::String suf;
            const double q = std::clamp(parseCleanNumber(s, suf), static_cast<double>(minQ), static_cast<double>(maxQ));
            return static_cast<double>(kalyx::core::unmapLog(static_cast<float>(q), minQ, maxQ));
        };
    }

    [[nodiscard]] inline std::function<double(const juce::String&)> parseRawClamped(double minV, double maxV, bool allowK = false)
    {
        return [minV, maxV, allowK](const juce::String& text) -> double
        {
            juce::String suf;
            double v = parseCleanNumber(text, suf);
            if (allowK && suf.startsWithChar('k'))
                v *= 1000.0;
            return std::clamp(v, minV, maxV);
        };
    }
} // namespace KalyxValueParsers

struct KalyxRotaryControl
{
    juce::Slider slider;
    juce::Label  titleLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::Colour arcColour { KalyxTheme::kalyxRoyalBlue };
    float uiScale { 1.0f };

    void init(juce::Component& parent,
              juce::AudioProcessorValueTreeState& apvts,
              const juce::String& paramID,
              const juce::String& title,
              const juce::Colour& arcColour,
              const juce::Colour& capColour,
              bool darkText,
              double defaultResetValue,
              std::function<juce::String(double)> valueFormatter = nullptr,
              const juce::String& infoHint = {},
              std::function<double(const juce::String&)> valueParser = nullptr);

    void setUiScale(float s);
    void setTextDark(bool darkText);
    void setCapAndArcColour(juce::Colour newArcColour, juce::Colour capColour);
    void setThemeColours(juce::Colour newArcColour, juce::Colour capColour, bool darkText)
    {
        setCapAndArcColour(newArcColour, capColour);
        setTextDark(darkText);
    }
    void setSteppedMode(bool stepped, int numSteps = 0)
    {
        slider.getProperties().set("kalyxSteppedActive", stepped);
        if (numSteps > 0)
            slider.getProperties().set("kalyxNotchedDots", numSteps);
        slider.repaint();
    }
    void setDimmedDisabled(bool dimmedDisabled);
    void setDimmed(bool dimmed) { setDimmedDisabled(dimmed); }
    void setBoundsTight(float x, float y, float w, float h, float maxDiameter = 68.0f);
};

/**
 * Pilule segmentée Tactile Flat pour paramètres à choix exclusifs :
 * ex. KNEE [ HARD | MED | SOFT ], THRUST [ LOUD | MED | NORM ],
 *     LINK FLT [ HP | BP | LP ], REL [ FIXED | VAR ], VU [ IN | OUT | GR ], DIST [ CLEAN | DIST 2 | D3 ].
 */
class KalyxSegmentedPill final : public juce::Component,
                                 public juce::SettableTooltipClient
{
public:
    KalyxSegmentedPill(juce::AudioProcessorValueTreeState& apvts,
                       const juce::String& paramID,
                       const juce::String& prefixTitle,
                       const juce::StringArray& choices,
                       juce::Colour activeColour,
                       bool darkText = false,
                       const juce::String& infoHint = {});

    void setUiScale(float s) noexcept { uiScale_ = std::clamp(s, 0.75f, 2.20f); repaint(); }
    void setThemeColours(juce::Colour activeColour, bool darkText) noexcept
    {
        activeCol_ = activeColour;
        darkText_  = darkText;
        repaint();
    }

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    juce::String prefix_;
    juce::StringArray choices_;
    juce::Colour activeCol_;
    bool darkText_ { false };
    float uiScale_ { 1.0f };
    int currentIdx_ { 0 };
    juce::RangedAudioParameter* paramPtr_ { nullptr };
    std::unique_ptr<juce::ParameterAttachment> paramAtt_;
};

/**
 * Pilule segmentée multi-toggles pour paramètres booléens indépendants côte à côte :
 * ex. Distressor FILTER : [ AUD HP | DET HP | 6k EMPH ].
 */
class KalyxMultiTogglePill final : public juce::Component,
                                   public juce::SettableTooltipClient
{
public:
    KalyxMultiTogglePill(juce::AudioProcessorValueTreeState& apvts,
                         const juce::String& prefixTitle,
                         const juce::StringArray& paramIDs,
                         const juce::StringArray& labels,
                         juce::Colour activeColour,
                         bool darkText = false,
                         const juce::String& infoHint = {});

    void setUiScale(float s) noexcept { uiScale_ = std::clamp(s, 0.75f, 2.20f); repaint(); }
    void setThemeColours(juce::Colour activeColour, bool darkText) noexcept
    {
        activeCol_ = activeColour;
        darkText_  = darkText;
        repaint();
    }

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    juce::String prefix_;
    juce::StringArray labels_;
    juce::Colour activeCol_;
    bool darkText_ { false };
    float uiScale_ { 1.0f };
    std::array<bool, 4> states_ {{ false, false, false, false }};
    std::vector<std::unique_ptr<juce::ParameterAttachment>> atts_;
};

/**
 * Sélecteur cranté / bouton cyclique lisible pour paramètres discrets.
 */
struct KalyxSteppedSelector
{
    juce::Label      titleLabel;
    juce::TextButton cycleBtn;
    std::unique_ptr<juce::ParameterAttachment> paramAtt;
    juce::StringArray choices;
    int currentIdx { 0 };
    float uiScale  { 1.0f };

    void init(juce::Component& parent,
              juce::AudioProcessorValueTreeState& apvts,
              const juce::String& paramID,
              const juce::String& title,
              const juce::StringArray& optionLabels,
              const juce::Colour& accentColour,
              bool darkText,
              const juce::String& infoHint = {});

    void setUiScale(float s);
    void setBoundsCompact(float x, float y, float w, float h);
    void setDimmedDisabled(bool dimmedDisabled);
};

/**
 * Pile verticale de 3 LEDs + petit bouton poussoir carré [ ■ ] (API 2500 : KNEE, THRUST, LINK FLT).
 */
class KalyxLedStackSwitch final : public juce::Component,
                                  public juce::SettableTooltipClient
{
public:
    KalyxLedStackSwitch(juce::AudioProcessorValueTreeState& apvts,
                        const juce::String& paramID,
                        const juce::String& title,
                        const juce::StringArray& labels,
                        juce::Colour ledColour,
                        bool darkText,
                        const juce::String& infoHint = {});

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    juce::String title_;
    juce::StringArray labels_;
    juce::Colour ledColour_;
    bool darkText_ { false };
    int currentIdx_ { 0 };
    std::unique_ptr<juce::ParameterAttachment> paramAtt_;
};

/**
 * Sélecteur matériel compact à flèches (< SSL 4000 E >, ROUTING : < STEREO >).
 */
class KalyxArrowSelector final : public juce::Component,
                                 public juce::SettableTooltipClient
{
public:
    KalyxArrowSelector(juce::AudioProcessorValueTreeState& apvts,
                       const juce::String& paramID,
                       const juce::String& prefixLabel,
                       const juce::StringArray& choices,
                       juce::Colour accentColour,
                       bool darkText,
                       const juce::String& infoHint = {});

    void setUiScale(float s) noexcept { uiScale_ = std::clamp(s, 0.75f, 2.20f); repaint(); }
    void setThemeColours(juce::Colour accentColour, bool darkText) noexcept
    {
        accent_   = accentColour;
        darkText_ = darkText;
        repaint();
    }

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    juce::String prefix_;
    juce::StringArray choices_;
    juce::Colour accent_;
    bool darkText_ { false };
    float uiScale_ { 1.0f };
    int currentIdx_ { 0 };
    juce::Rectangle<float> leftArrow_, rightArrow_;
    std::unique_ptr<juce::ParameterAttachment> paramAtt_;
};

/**
 * Sélecteur rotatif cranté matériel (Neve 5254 RATIO & TIMING) avec points de crans autour du bouton
 * et valeur du cran actif affichée dessous.
 */
class KalyxRotaryStepSelector final : public juce::Component,
                                      public juce::SettableTooltipClient
{
public:
    KalyxRotaryStepSelector(juce::AudioProcessorValueTreeState& apvts,
                            const juce::String& paramID,
                            const juce::String& title,
                            const juce::StringArray& stepLabels,
                            juce::Colour capColour,
                            bool darkText,
                            const juce::String& infoHint = {});

    void setUiScale(float s) noexcept { uiScale_ = std::clamp(s, 0.75f, 2.20f); repaint(); }
    void setThemeColours(juce::Colour capColour, bool darkText) noexcept
    {
        capCol_   = capColour;
        darkText_ = darkText;
        repaint();
    }
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    void setStepIndex(int idx);

    juce::String title_;
    juce::StringArray labels_;
    juce::Colour capCol_;
    bool darkText_ { false };
    float uiScale_ { 1.0f };
    int currentIdx_ { 0 };
    int dragStartIdx_ { 0 };
    float dragStartY_ { 0.0f };
    bool dragged_ { false };
    std::unique_ptr<juce::ParameterAttachment> paramAtt_;
};

inline void setupKalyxToggle(juce::Component& parent,
                             juce::ToggleButton& btn,
                             std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& att,
                             juce::AudioProcessorValueTreeState& apvts,
                             const juce::String& paramID,
                             const juce::String& text,
                             const juce::Colour& activeColour,
                             bool darkTextWhenOff = false,
                             const juce::String& infoHint = {})
{
    btn.setButtonText(text);
    btn.setColour(juce::ToggleButton::tickColourId, activeColour);
    btn.setColour(juce::ToggleButton::textColourId, darkTextWhenOff ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary);
    if (infoHint.isNotEmpty())
        btn.setTooltip(infoHint);
    parent.addAndMakeVisible(btn);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, paramID, btn);
}

/**
 * Graphe EQ Interactif + Analyseur de Spectre FFT 2048 pts (PRE / POST / OFF) lié à un slot précis (0..4).
 */
class KalyxEQCurveComponent final : public juce::Component,
                                    public juce::SettableTooltipClient
{
public:
    KalyxEQCurveComponent(KalyxHybridConsoleProcessor& proc, int slotIndex, bool darkText) noexcept;

    void setDarkText(bool dark) noexcept { darkText_ = dark; }
    void updateFFTFromProcessor() noexcept;
    void paint(juce::Graphics& g) override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    [[nodiscard]] juce::Rectangle<float> getPlotArea() const noexcept;
    [[nodiscard]] float freqToX(float freqHz, const juce::Rectangle<float>& area) const noexcept;
    [[nodiscard]] float xToFreq(float x, const juce::Rectangle<float>& area) const noexcept;
    [[nodiscard]] float gainToY(float gainDb, const juce::Rectangle<float>& area) const noexcept;
    [[nodiscard]] float yToGain(float y, const juce::Rectangle<float>& area) const noexcept;
    void setSlotNormParam(int paramOneBased, float normVal) noexcept;

    KalyxHybridConsoleProcessor& processor_;
    int  slotIndex_        { 2 };
    bool darkText_         { false };
    int  draggedNodeIndex_ { -1 };

    juce::dsp::FFT forwardFFT_;
    juce::dsp::WindowingFunction<float> windowing_;
    std::array<float, KalyxHybridConsoleProcessor::kFFTSize * 2> fftData_ {};
    std::array<float, KalyxHybridConsoleProcessor::kFFTSize / 2> smoothedBinsDb_ {};
};

/**
 * Panneau flottant SETTINGS (0 VU Calibration, Oversampling 1x/2x/4x, FFT Mode).
 */
class KalyxSettingsOverlayComponent final : public juce::Component
{
public:
    explicit KalyxSettingsOverlayComponent(KalyxHybridConsoleProcessor& proc);
    ~KalyxSettingsOverlayComponent() override = default;

    void setChassisTheme(kalyx::core::ChassisTheme t) noexcept { theme_ = t; syncButtonsFromAPVTS(); }
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void syncButtonsFromAPVTS() noexcept;
    void setChoiceParam(const char* paramID, int index) noexcept;
    void setFloatParam(const char* paramID, float val) noexcept;

    KalyxHybridConsoleProcessor& processor_;
    kalyx::core::ChassisTheme theme_ { kalyx::core::ChassisTheme::Night };

    juce::TextButton closeBtn_ { "X" };
    std::array<juce::TextButton, 4> vuPresetBtns_ {
        juce::TextButton { "-20 dBFS" }, juce::TextButton { "-18 dBFS" }, juce::TextButton { "-14 dBFS" }, juce::TextButton { "0 dBFS" }
    };
    juce::Slider vuRefSlider_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> vuRefAttachment_;

    std::array<juce::TextButton, 3> osBtns_ {
        juce::TextButton { "1x (Off)" }, juce::TextButton { "2x IIR" }, juce::TextButton { "4x IIR" }
    };
    std::array<juce::TextButton, 3> fftBtns_ {
        juce::TextButton { "POST EQ" }, juce::TextButton { "PRE EQ" }, juce::TextButton { "FFT OFF" }
    };
};
