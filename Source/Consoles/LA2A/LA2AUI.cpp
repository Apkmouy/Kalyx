#include "LA2AUI.h"
#include "../../Core/ConsoleRegistry.h"
#include <algorithm>
#include <cmath>

namespace kalyx::consoles
{

namespace
{
    inline const juce::Colour kLa2aChassisBg    { 0xff7a8088 }; // Gris acier martelé vintage
    inline const juce::Colour kLa2aChromeBorder { 0xffcbd2da }; // Bordure chromée fine
    inline const juce::Colour kLa2aBakeliteKnob { 0xff16181e }; // Gros bouton bakélite noir
    inline const juce::Colour kLa2aSmallKnob    { 0xff262a33 }; // Petit bouton secondaire
    inline const juce::Colour kLa2aAmberJewel   { 0xfff59e0b }; // Voyant témoin ambré chaleureux
    inline const juce::Colour kLa2aSilkscreen   { 0xff12151c }; // Sérigraphie noire nette
} // namespace

LA2AUI::LA2AUI(KalyxHybridConsoleProcessor& proc, int slotIndex)
    : processor_(proc), slotIndex_(slotIndex)
{
    using namespace kalyx::core;
    auto& apvts = processor_.getAPVTS();
    const auto defs = ModuleRegistry::getDefaultParamsForModule(SlotModuleId::LA2AComp);

    const auto p = processor_.getSlotParamsForGUI(slotIndex_);
    moduleIn_    = (p[0] > 0.5f);
    isLimitMode_ = (p[1] > 0.5f);

    const bool darkTxt = (theme_ == ChassisTheme::Day);
    const auto steel  = KalyxTheme::la2aSteel; // #7E96AD Acier Bleuté / Argent
    const auto capCol = KalyxTheme::getControlIdleBg(theme_);

    // [● LA-2A IN]
    setupKalyxToggle(*this, inBtn_, inAtt_, apvts, ParamIDs::slotParamId(slotIndex_, 1),
                     "LA-2A IN", steel, darkTxt,
                     "Engage or bypass Teletronix LA-2A Optical Leveling Amplifier");

    auto fmtDb = [](double v) -> juce::String
    {
        return KalyxValueParsers::formatDbClean(mapLin(static_cast<float>(v), -24.0f, 24.0f));
    };

    auto fmtPct = [](double v) -> juce::String
    {
        return juce::String(juce::roundToInt(std::clamp(static_cast<float>(v), 0.0f, 1.0f) * 100.0f)) + " %";
    };

    auto parseDb24 = KalyxValueParsers::parseLinDb(-24.0f, 24.0f);
    auto parsePct  = KalyxValueParsers::parseLinPct(0.0f, 100.0f);

    transferCurve_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 4),
                        -42.0f, -2.0f, defs[3], true, steel, darkTxt,
                        "Dynamic Optical T4B Transfer Curve (Drag to adjust Peak Reduction, Double-Click to Reset)");

    // 2 gros potentiomètres principaux : GAIN et PEAK REDUCTION
    gainKnob_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 3),
                   "GAIN", steel, capCol, darkTxt, defs[2], fmtDb,
                   "Opto-2A Tube Output Gain (-24 to +24 dB)", parseDb24);

    peakRedKnob_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 4),
                      "PEAK REDUCTION", steel, capCol, darkTxt, defs[3], fmtPct,
                      "T4B Electroluminescent Panel Drive (0 - 100 %)", parsePct);

    // 3 barres TactileFaderBar : R37 EMPHASIS, CELL AGE, BLEND
    r37EmphBar_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 5),
                     "R37 EMPHASIS", steel, false, darkTxt, defs[4], fmtPct,
                     "R37 HF Sidechain Emphasis (0 % Flat -> 100 % HF Sensitive)", parsePct);

    cellAgeBar_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 6),
                     "CELL AGE", steel, false, darkTxt, defs[5], fmtPct,
                     "T4B Optical Cell Age (0 % Fast -> 100 % Vintage Memory Tail)", parsePct);

    blendBar_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 7),
                   "BLEND", steel, false, darkTxt, defs[6], fmtPct,
                   "Parallel Dry/Wet Compression Blend (0 - 100 %)", parsePct);

    // ROUTING : < STEREO >
    routingSelector_ = std::make_unique<KalyxArrowSelector>(
        apvts, ParamIDs::slotParamId(slotIndex_, 8), "ROUTING :",
        juce::StringArray { "MONO", "STEREO", "DUAL M", "M/S", "INV" }, steel, darkTxt,
        "Channel Routing Mode: MONO, STEREO, DUAL MONO, M/S, INVERSE");
    addAndMakeVisible(*routingSelector_);
}

void LA2AUI::setUiScale(float s)
{
    uiScale_ = std::clamp(s, 0.75f, 2.20f);
    inBtn_.getProperties().set("kalyxScale", uiScale_);
    transferCurve_.setUiScale(uiScale_);
    gainKnob_.setUiScale(uiScale_);
    peakRedKnob_.setUiScale(uiScale_);
    r37EmphBar_.setUiScale(uiScale_);
    cellAgeBar_.setUiScale(uiScale_);
    blendBar_.setUiScale(uiScale_);
    if (routingSelector_) routingSelector_->setUiScale(uiScale_);
    resized();
    repaint();
}

void LA2AUI::setChassisTheme(kalyx::core::ChassisTheme t)
{
    if (theme_ == t)
        return;
    theme_ = t;

    const bool darkTxt = (theme_ == kalyx::core::ChassisTheme::Day);
    const auto steel   = KalyxTheme::la2aSteel;
    const auto capCol  = KalyxTheme::getControlIdleBg(theme_);

    inBtn_.getProperties().set("kalyxDarkText", darkTxt);
    transferCurve_.setThemeColours(steel, darkTxt);
    gainKnob_.setThemeColours(steel, capCol, darkTxt);
    peakRedKnob_.setThemeColours(steel, capCol, darkTxt);
    r37EmphBar_.setThemeColours(steel, darkTxt);
    cellAgeBar_.setThemeColours(steel, darkTxt);
    blendBar_.setThemeColours(steel, darkTxt);
    if (routingSelector_) routingSelector_->setThemeColours(steel, darkTxt);

    repaint();
}

void LA2AUI::syncFromAPVTS()
{
    const auto p = processor_.getSlotParamsForGUI(slotIndex_);
    const bool modOn  = (p[0] > 0.5f);
    const bool limOn  = (p[1] > 0.5f);

    bool needRepaint = false;

    if (limOn != isLimitMode_)
    {
        isLimitMode_ = limOn;
        needRepaint  = true;
    }

    if (modOn != moduleIn_)
    {
        moduleIn_ = modOn;
        const float alpha = modOn ? 1.0f : 0.35f;
        auto setKnobAlpha = [alpha](KalyxRotaryControl& k)
        {
            k.slider.setAlpha(alpha);
            k.titleLabel.setAlpha(alpha);
        };
        setKnobAlpha(gainKnob_);
        setKnobAlpha(peakRedKnob_);
        r37EmphBar_.setDimmedDisabled(!modOn);
        cellAgeBar_.setDimmedDisabled(!modOn);
        blendBar_.setDimmedDisabled(!modOn);
        if (routingSelector_) routingSelector_->setAlpha(alpha);
        needRepaint = true;
    }

    transferCurve_.setCurveConfig(InteractiveTransferCurve::CurveType::Opto,
                                  isLimitMode_ ? 10.0f : 3.2f, 12.0f, moduleIn_, false);

    if (needRepaint)
        repaint();
}

void LA2AUI::setGainReductionDb(float grDb, float inputPeakDb) noexcept
{
    smoothedGrDb_ = std::max(0.0f, grDb);
    transferCurve_.setTelemetry(inputPeakDb, smoothedGrDb_);
}

void LA2AUI::drawPilotJewelLamp(juce::Graphics& g) const
{
    if (pilotLampRect_.isEmpty()) return;

    const float r = 3.5f * uiScale_;
    const auto  c = pilotLampRect_.getCentre();

    if (moduleIn_)
    {
        g.setColour(KalyxTheme::la2aAmber.withAlpha(0.35f));
        g.fillEllipse(c.x - r - 2.0f * uiScale_, c.y - r - 2.0f * uiScale_,
                      (r + 2.0f * uiScale_) * 2.0f, (r + 2.0f * uiScale_) * 2.0f);
        g.setColour(KalyxTheme::la2aAmber);
        g.fillEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    }
    else
    {
        g.setColour(KalyxTheme::knobTrackBg);
        g.fillEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    }
}

void LA2AUI::drawLimitCompSwitch(juce::Graphics& g) const
{
    if (limitCompFrameRect_.isEmpty()) return;

    const float alpha  = moduleIn_ ? 1.0f : 0.35f;
    const float radius = 4.0f * uiScale_;
    g.setColour(KalyxTheme::getControlIdleBg(theme_).withMultipliedAlpha(alpha));
    g.fillRoundedRectangle(limitCompFrameRect_, radius);
    g.setColour(KalyxTheme::getGridBorder(theme_).withMultipliedAlpha(alpha));
    g.drawRoundedRectangle(limitCompFrameRect_, radius, std::max(0.8f, 1.0f * uiScale_));

    auto drawSeg = [&](juce::Rectangle<float> r, const char* txt, bool active)
    {
        const auto box = r.reduced(1.5f * uiScale_);
        if (active)
        {
            g.setColour(KalyxTheme::la2aSteel.withMultipliedAlpha(alpha));
            g.fillRoundedRectangle(box, std::max(2.0f, radius - 1.5f));
            g.setColour(KalyxTheme::nightTextPrimary.withMultipliedAlpha(alpha));
        }
        else
        {
            g.setColour(KalyxTheme::getTextSecondary(theme_).withMultipliedAlpha(alpha));
        }
        g.setFont(juce::Font(juce::FontOptions(10.0f * uiScale_, juce::Font::bold)));
        g.drawFittedText(txt, box.toNearestInt(), juce::Justification::centred, 1);
    };

    drawSeg(limitSwitchRect_, "LIMIT", isLimitMode_);
    drawSeg(compSwitchRect_,  "COMP",  !isLimitMode_);
}

void LA2AUI::drawGrLedStrip(juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    if (bounds.isEmpty()) return;

    g.setColour(KalyxTheme::getControlIdleBg(theme_));
    g.fillRoundedRectangle(bounds, 3.5f * uiScale_);
    g.setColour(KalyxTheme::getGridBorder(theme_));
    g.drawRoundedRectangle(bounds, 3.5f * uiScale_, std::max(0.8f, 1.0f * uiScale_));

    const float grLblW = 24.0f * uiScale_;
    const auto grLabelBox = bounds.withWidth(grLblW).reduced(2.0f * uiScale_, 1.0f);
    g.setColour(KalyxTheme::getTextSecondary(theme_));
    g.setFont(juce::Font(juce::FontOptions(9.0f * uiScale_, juce::Font::bold)));
    g.drawFittedText("GR:", grLabelBox.toNearestInt(), juce::Justification::centred, 1);

    const auto row = bounds.withTrimmedLeft(grLblW).reduced(3.0f * uiScale_, 2.0f * uiScale_);
    static constexpr float kGrThresh[6]   = { 1.0f, 2.0f, 4.0f, 6.0f, 10.0f, 20.0f };
    static constexpr const char* kGrLbl[6] = { "-1", "-2", "-4", "-6", "-10", "-20" };

    const float cellW = row.getWidth() / 6.0f;
    const float rad = 3.2f * uiScale_;
    const auto offTrack = (theme_ == kalyx::core::ChassisTheme::Day) ? KalyxTheme::dayKnobTrackBg : KalyxTheme::knobTrackBg;
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 8.5f * uiScale_, juce::Font::bold)));

    for (int i = 0; i < 6; ++i)
    {
        const float cx  = row.getX() + (static_cast<float>(i) + 0.5f) * cellW;
        const float cy  = row.getY() + 5.0f * uiScale_;
        const bool  lit = moduleIn_ && (smoothedGrDb_ >= kGrThresh[i]);
        const juce::Colour onCol = (i < 2) ? juce::Colour(0xff22c55e)
                                 : (i < 4) ? KalyxTheme::la2aAmber : KalyxTheme::alertRed;
        if (lit)
        {
            g.setColour(onCol.withAlpha(0.35f));
            g.fillEllipse(cx - rad - 1.8f * uiScale_, cy - rad - 1.8f * uiScale_,
                          (rad + 1.8f * uiScale_) * 2.0f, (rad + 1.8f * uiScale_) * 2.0f);
            g.setColour(onCol);
        }
        else
        {
            g.setColour(offTrack);
        }
        g.fillEllipse(cx - rad, cy - rad, rad * 2.0f, rad * 2.0f);

        g.setColour(lit ? KalyxTheme::getTextPrimary(theme_) : KalyxTheme::getTextSecondary(theme_));
        g.drawFittedText(kGrLbl[i],
                         juce::Rectangle<int>(static_cast<int>(cx - cellW * 0.5f), static_cast<int>(cy + 3.2f * uiScale_),
                                              static_cast<int>(cellW), static_cast<int>(10.0f * uiScale_)),
                         juce::Justification::centred, 1);
    }
}

void LA2AUI::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    // Fond unifié Tactile Flat (#181B22 Night / #F4F5F7 Day)
    g.setColour(KalyxTheme::getStripBg(theme_));
    g.fillRect(b);

    // Liseré supérieur Acier Bleuté (#7E96AD)
    g.setColour(KalyxTheme::la2aSteel.withAlpha(moduleIn_ ? 0.90f : 0.35f));
    g.fillRect(b.getX(), b.getY(), b.getWidth(), std::max(2.0f, 2.5f * uiScale_));

    // Séparateurs horizontaux (#262A35 Night / #D0D5DD Day)
    g.setColour(KalyxTheme::getGridBorder(theme_));
    if (!sepTopRect_.isEmpty())
        g.fillRect(sepTopRect_);
    if (!sepBotRect_.isEmpty())
        g.fillRect(sepBotRect_);

    drawPilotJewelLamp(g);
    drawLimitCompSwitch(g);
}

void LA2AUI::resized()
{
    const auto b       = getLocalBounds().toFloat();
    const float pad    = 5.0f * uiScale_;
    const float innerW = std::max(40.0f, b.getWidth() - 2.0f * pad);
    float y            = 4.0f * uiScale_;

    // Ligne 1 : [● OPTO-2A IN] (●)   [ LIMIT | COMP ]
    const float topH    = 22.0f * uiScale_;
    const float pilotW  = 14.0f * uiScale_;
    const float inW     = std::max(54.0f * uiScale_, innerW * 0.42f);
    const float swW     = std::max(58.0f * uiScale_, innerW - inW - pilotW - 6.0f * uiScale_);

    inBtn_.setBounds(static_cast<int>(pad), static_cast<int>(y),
                     static_cast<int>(inW), static_cast<int>(topH));
    pilotLampRect_      = { pad + inW + 2.0f * uiScale_, y, pilotW, topH };
    limitCompFrameRect_ = { pad + innerW - swW, y, swW, topH };
    limitSwitchRect_    = limitCompFrameRect_.withWidth(swW * 0.5f);
    compSwitchRect_     = limitCompFrameRect_.withTrimmedLeft(swW * 0.5f);
    y += topH + 4.0f * uiScale_;

    // Ligne 2 : Courbe de transfert dynamique optique
    const float curveH = 86.0f * uiScale_;
    transferCurve_.setBounds(static_cast<int>(pad), static_cast<int>(y), static_cast<int>(innerW), static_cast<int>(curveH));
    y += curveH + 4.0f * uiScale_;

    // Séparateur haut
    sepTopRect_ = { pad, y, innerW, std::max(1.0f, 1.0f * uiScale_) };
    y += 4.0f * uiScale_;

    // Zone basse : 3 TactileFaderBar + séparateur + ROUTING
    const float faderH    = 22.0f * uiScale_;
    const float fadersH   = 3.0f * faderH + 10.0f * uiScale_;
    const float rowCtrlH  = 22.0f * uiScale_;
    const float bottomH   = rowCtrlH + 10.0f * uiScale_;
    const float knobZoneH = std::max(80.0f * uiScale_, b.getHeight() - y - bottomH - fadersH - 6.0f * uiScale_);

    const float colW = innerW * 0.5f;
    gainKnob_.setBoundsTight   (pad,        y, colW, knobZoneH, 68.0f);
    peakRedKnob_.setBoundsTight(pad + colW, y, colW, knobZoneH, 68.0f);
    y += knobZoneH + 4.0f * uiScale_;

    r37EmphBar_.setBounds(static_cast<int>(pad), static_cast<int>(y), static_cast<int>(innerW), static_cast<int>(faderH)); y += faderH + 3.0f * uiScale_;
    cellAgeBar_.setBounds(static_cast<int>(pad), static_cast<int>(y), static_cast<int>(innerW), static_cast<int>(faderH)); y += faderH + 3.0f * uiScale_;
    blendBar_.setBounds  (static_cast<int>(pad), static_cast<int>(y), static_cast<int>(innerW), static_cast<int>(faderH));

    // Zone basse ancrée au bas du slot
    float botY = b.getHeight() - bottomH;
    sepBotRect_ = { pad, botY, innerW, std::max(1.0f, 1.0f * uiScale_) };
    botY += 4.0f * uiScale_;

    if (routingSelector_)
        routingSelector_->setBounds(static_cast<int>(pad), static_cast<int>(botY),
                                    static_cast<int>(innerW), static_cast<int>(rowCtrlH));
}

void LA2AUI::mouseDown(const juce::MouseEvent& e)
{
    auto& apvts = processor_.getAPVTS();
    if (limitSwitchRect_.expanded(1.0f).contains(e.position) || compSwitchRect_.expanded(1.0f).contains(e.position))
    {
        const bool nextLimit = limitSwitchRect_.expanded(1.0f).contains(e.position) ? true
                             : (compSwitchRect_.expanded(1.0f).contains(e.position) ? false : !isLimitMode_);
        if (auto* param = apvts.getParameter(ParamIDs::slotParamId(slotIndex_, 2)))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost(nextLimit ? 1.0f : 0.0f);
            param->endChangeGesture();
        }
        isLimitMode_ = nextLimit;
        repaint();
    }
}

} // namespace kalyx::consoles
