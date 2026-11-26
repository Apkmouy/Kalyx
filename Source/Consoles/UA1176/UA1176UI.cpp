#include "UA1176UI.h"
#include "../../Core/ConsoleRegistry.h"
#include <algorithm>
#include <cmath>

namespace kalyx::consoles
{

namespace
{
    inline const juce::Colour kRevABg         { 0xffd2d7df }; // Aluminium brossé clair
    inline const juce::Colour kRevABlueStripe { 0xff1d56a5 }; // Bande bleue iconique Bluestripe
    inline const juce::Colour kRevDBg         { 0xff181a1f }; // Noir anodisé mat Blackface
    inline const juce::Colour kRevGBg         { 0xffb8bec6 }; // Aluminium argent satiné Silverface
    inline const juce::Colour kKnobBody       { 0xff1f232b }; // Capuchon noir 1176 classique
} // namespace

UA1176UI::UA1176UI(KalyxHybridConsoleProcessor& proc, int slotIndex)
    : processor_(proc), slotIndex_(slotIndex)
{
    using namespace kalyx::core;
    auto& apvts = processor_.getAPVTS();
    const auto defs = ModuleRegistry::getDefaultParamsForModule(SlotModuleId::UA1176Comp);

    const auto p = processor_.getSlotParamsForGUI(slotIndex_);
    currentRev_   = mapChoice(p[1], 3);
    currentRatio_ = mapChoice(p[8], 4);
    allButtonsOn_ = (p[9]  > 0.5f);
    superSlowOn_  = (p[10] > 0.5f);
    moduleIn_     = (p[0]  > 0.5f);

    const bool darkTxt = (currentRev_ != 1);
    const auto accent  = getCurrentAccentColour();

    // [● 1176 IN] & [ < REV D > ]
    setupKalyxToggle(*this, inBtn_, inAtt_, apvts, ParamIDs::slotParamId(slotIndex_, 1),
                     "1176 IN", accent, darkTxt, "Engage or bypass Universal Audio 1176 FET Compressor");

    revSelector_ = std::make_unique<KalyxArrowSelector>(
        apvts, ParamIDs::slotParamId(slotIndex_, 2), "",
        juce::StringArray { "REV A", "REV D", "REV G" }, accent, darkTxt,
        "Select 1176 Hardware Revision: REV A (Bluestripe), REV D (Blackface LN), REV G (Silverface Op-Amp)");
    addAndMakeVisible(*revSelector_);

    // Formateurs physiques
    auto fmtDb = [](double v) -> juce::String
    {
        return KalyxValueParsers::formatDbClean(mapLin(static_cast<float>(v), -24.0f, 24.0f));
    };

    auto fmtAttack = [this](double v) -> juce::String
    {
        const bool slow = processor_.getSlotParamsForGUI(slotIndex_)[10] > 0.5f;
        const float us  = UA1176DSP::mapAttackUs(static_cast<float>(v), slow);
        if (us >= 1000.0f)
            return juce::String(us * 0.001f, 1) + " ms";
        return juce::String(juce::roundToInt(us)) + juce::String::fromUTF8(" \xC2\xB5s");
    };

    auto parseAttack = [this](const juce::String& text) -> double
    {
        const bool slow = processor_.getSlotParamsForGUI(slotIndex_)[10] > 0.5f;
        const double maxUs = slow ? 10000.0 : 800.0;
        const double minUs = 20.0;
        juce::String suf;
        double us = KalyxValueParsers::parseCleanNumber(text, suf);
        if (suf.startsWithChar('m'))
            us *= 1000.0;
        else if (suf.isEmpty() && us <= 10.0)
            us *= 1000.0; // ex. 2.5 -> 2.5 ms si en mode slow, ou <= 10 sans unité
        us = std::clamp(us, minUs, maxUs);
        return std::clamp(std::log(us / maxUs) / std::log(minUs / maxUs), 0.0, 1.0);
    };

    auto fmtRelease = [](double v) -> juce::String
    {
        const float ms = UA1176DSP::mapReleaseMs(static_cast<float>(v));
        if (ms >= 1000.0f)
            return juce::String(ms * 0.001f, 2) + " s";
        return juce::String(juce::roundToInt(ms)) + " ms";
    };

    auto parseRelease = [](const juce::String& text) -> double
    {
        juce::String suf;
        double ms = KalyxValueParsers::parseCleanNumber(text, suf);
        if (suf == "s" || suf == "sec")
            ms *= 1000.0;
        else if (suf.isEmpty() && ms <= 1.2)
            ms *= 1000.0;
        ms = std::clamp(ms, 50.0, 1100.0);
        return std::clamp(std::log(ms / 1100.0) / std::log(50.0 / 1100.0), 0.0, 1.0);
    };

    auto fmtPct = [](double v) -> juce::String
    {
        return juce::String(juce::roundToInt(std::clamp(static_cast<float>(v), 0.0f, 1.0f) * 100.0f)) + " %";
    };

    auto parseDb24 = KalyxValueParsers::parseLinDb(-24.0f, 24.0f);
    auto parsePct  = KalyxValueParsers::parseLinPct(0.0f, 100.0f);

    transferCurve_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 3),
                        -24.0f, 24.0f, defs[2], false, accent, darkTxt,
                        "Dynamic FET Transfer Curve (Drag to adjust Input/Threshold, Double-Click to Reset)");

    // 4 potentiomètres principaux + 2 barres TactileFaderBar
    inputKnob_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 3),
                    "INPUT", accent, kKnobBody, darkTxt, defs[2], fmtDb,
                    "FET-76 Input Drive (-24 to +24 dB)", parseDb24);

    outputKnob_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 4),
                     "OUTPUT", accent, kKnobBody, darkTxt, defs[3], fmtDb,
                     "FET-76 Output Makeup Stage (-24 to +24 dB)", parseDb24);

    attackKnob_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 5),
                     "ATTACK", accent, kKnobBody, darkTxt, defs[4], fmtAttack,
                     "FET Attack Time (800 us -> 20 us, or 10 ms -> 20 us in SLOW mode)", parseAttack);

    releaseKnob_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 6),
                      "RELEASE", accent, kKnobBody, darkTxt, defs[5], fmtRelease,
                      "FET Release Time (1.1 s -> 50 ms)", parseRelease);

    fetBiasBar_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 7),
                     "FET BIAS", accent, false, darkTxt, defs[6], fmtPct,
                     "FET Gate Bias / Headroom (0 - 100 %)", parsePct);

    blendBar_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 8),
                   "BLEND", accent, false, darkTxt, defs[7], fmtPct,
                   "Parallel Dry/Wet Compression Blend (0 - 100 %)", parsePct);

    // MODS : [ ALL (SLAM) ] [SLOW]
    setupKalyxToggle(*this, slamBtn_, slamAtt_, apvts, ParamIDs::slotParamId(slotIndex_, 10),
                     "ALL (SLAM)", KalyxTheme::alertRed, darkTxt,
                     "All-Buttons In (British / Slam Mode): bias collapse, folded ratio curve, explosive pumping");

    setupKalyxToggle(*this, slowBtn_, slowAtt_, apvts, ParamIDs::slotParamId(slotIndex_, 11),
                     "SLOW", KalyxTheme::amberLed, darkTxt,
                     "Super Slow Attack Mod: extends attack range up to 10 ms to preserve snare/guitar punch");

    // ROUTING : < STEREO >
    routingSelector_ = std::make_unique<KalyxArrowSelector>(
        apvts, ParamIDs::slotParamId(slotIndex_, 12), "ROUTING :",
        juce::StringArray { "MONO", "STEREO", "DUAL M", "M/S", "INV" }, accent, darkTxt,
        "Channel Routing Mode: MONO, STEREO, DUAL MONO, M/S, INVERSE");
    addAndMakeVisible(*routingSelector_);

    applyRevisionTheme(currentRev_);
}

juce::Colour UA1176UI::getCurrentAccentColour() const noexcept
{
    if (currentRev_ == 0) return KalyxTheme::ua1176RevABlue;   // Rev A Bluestripe #2B6CB0
    if (currentRev_ == 2) return KalyxTheme::ua1176RevGSilver; // Rev G Silverface #A0AAB8
    return KalyxTheme::ua1176RevDAmber;                        // Rev D Blackface #D9822B
}

void UA1176UI::setUiScale(float s)
{
    uiScale_ = std::clamp(s, 0.75f, 2.20f);
    transferCurve_.setUiScale(uiScale_);
    inputKnob_.setUiScale(uiScale_);
    outputKnob_.setUiScale(uiScale_);
    attackKnob_.setUiScale(uiScale_);
    releaseKnob_.setUiScale(uiScale_);
    fetBiasBar_.setUiScale(uiScale_);
    blendBar_.setUiScale(uiScale_);
    if (revSelector_)     revSelector_->setUiScale(uiScale_);
    if (routingSelector_) routingSelector_->setUiScale(uiScale_);
    resized();
    repaint();
}

void UA1176UI::setChassisTheme(kalyx::core::ChassisTheme t)
{
    if (theme_ == t)
        return;
    theme_ = t;
    applyRevisionTheme(currentRev_);
}

void UA1176UI::applyRevisionTheme(int revIdx)
{
    currentRev_ = std::clamp(revIdx, 0, 2);
    const bool darkTxt = (theme_ == kalyx::core::ChassisTheme::Day);
    const auto accent  = getCurrentAccentColour();
    const auto txtCol  = KalyxTheme::getTextPrimary(theme_);
    const auto capCol  = KalyxTheme::getControlIdleBg(theme_);

    transferCurve_.setThemeColours(accent, darkTxt);
    inputKnob_.setThemeColours(accent, capCol, darkTxt);
    outputKnob_.setThemeColours(accent, capCol, darkTxt);
    attackKnob_.setThemeColours(accent, capCol, darkTxt);
    releaseKnob_.setThemeColours(accent, capCol, darkTxt);
    fetBiasBar_.setThemeColours(accent, darkTxt);
    blendBar_.setThemeColours(accent, darkTxt);

    inBtn_.getProperties().set("kalyxDarkText", darkTxt);
    slamBtn_.getProperties().set("kalyxDarkText", darkTxt);
    slowBtn_.getProperties().set("kalyxDarkText", darkTxt);
    inBtn_.setColour(juce::ToggleButton::tickColourId, accent);
    inBtn_.setColour(juce::ToggleButton::textColourId, txtCol);
    slamBtn_.setColour(juce::ToggleButton::textColourId, txtCol);
    slowBtn_.setColour(juce::ToggleButton::textColourId, txtCol);

    if (revSelector_)     revSelector_->setThemeColours(accent, darkTxt);
    if (routingSelector_) routingSelector_->setThemeColours(accent, darkTxt);

    repaint();
}

void UA1176UI::syncFromAPVTS()
{
    using namespace kalyx::core;
    const auto p = processor_.getSlotParamsForGUI(slotIndex_);

    const bool modOn   = (p[0]  > 0.5f);
    const int  revIdx  = mapChoice(p[1], 3);
    const int  ratIdx  = mapChoice(p[8], 4);
    const bool slamOn  = (p[9]  > 0.5f);
    const bool slowOn  = (p[10] > 0.5f);

    bool needRepaint = false;

    if (revIdx != currentRev_)
    {
        applyRevisionTheme(revIdx);
        needRepaint = true;
    }

    if (ratIdx != currentRatio_ || slamOn != allButtonsOn_)
    {
        currentRatio_ = ratIdx;
        allButtonsOn_ = slamOn;
        needRepaint = true;
    }

    if (slowOn != superSlowOn_)
    {
        superSlowOn_ = slowOn;
        attackKnob_.slider.updateText();
        needRepaint = true;
    }

    if (modOn != moduleIn_)
    {
        moduleIn_ = modOn;
        const float alpha = modOn ? 1.0f : 0.50f;
        auto setKnobAlpha = [alpha](KalyxRotaryControl& k)
        {
            k.slider.setAlpha(alpha);
            k.titleLabel.setAlpha(alpha);
        };
        setKnobAlpha(inputKnob_);
        setKnobAlpha(outputKnob_);
        setKnobAlpha(attackKnob_);
        setKnobAlpha(releaseKnob_);
        fetBiasBar_.setDimmedDisabled(!modOn);
        blendBar_.setDimmedDisabled(!modOn);
        slamBtn_.setAlpha(alpha);
        slowBtn_.setAlpha(alpha);
        if (routingSelector_) routingSelector_->setAlpha(alpha);
        needRepaint = true;
    }

    static constexpr float kRatios[4] = { 4.0f, 8.0f, 12.0f, 20.0f };
    transferCurve_.setCurveConfig(
        allButtonsOn_ ? InteractiveTransferCurve::CurveType::FETSlam : InteractiveTransferCurve::CurveType::FET,
        kRatios[std::clamp(currentRatio_, 0, 3)], 3.0f, moduleIn_, false);

    if (needRepaint)
        repaint();
}

void UA1176UI::setGainReductionDb(float grDb, float inputPeakDb) noexcept
{
    smoothedGrDb_ = std::max(0.0f, grDb);
    transferCurve_.setTelemetry(inputPeakDb, smoothedGrDb_);
}

void UA1176UI::drawGrLedStrip(juce::Graphics& g, juce::Rectangle<float> bounds) const
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
                                 : (i < 4) ? KalyxTheme::amberLed : KalyxTheme::alertRed;
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

void UA1176UI::drawRatioButtons(juce::Graphics& g) const
{
    const auto txtCol  = KalyxTheme::getTextPrimary(theme_);
    const auto accent  = getCurrentAccentColour();
    const float alpha  = moduleIn_ ? 1.0f : 0.35f;

    g.setColour(txtCol.withMultipliedAlpha(0.88f * alpha));
    g.setFont(juce::Font(juce::FontOptions(10.0f * uiScale_, juce::Font::bold)));
    g.drawFittedText("RATIO :", ratioLabelRect_.toNearestInt(), juce::Justification::centredLeft, 1);

    static constexpr const char* kLabels[4] = { "4:1", "8:1", "12", "20" };
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.2f * uiScale_, juce::Font::bold)));

    for (int i = 0; i < 4; ++i)
    {
        const auto rawBox = ratioBtnRects_[static_cast<std::size_t>(i)].reduced(0.5f);
        // Touches mécaniques : enfoncement simultané quand ALL (SLAM) est actif
        const bool isLit = allButtonsOn_ || (currentRatio_ == i);
        const auto box = isLit ? rawBox.translated(0.0f, 1.2f * uiScale_) : rawBox;
        const juce::Colour litCol = allButtonsOn_ ? KalyxTheme::alertRed : accent;
        const juce::Colour idleBg = KalyxTheme::getControlIdleBg(theme_);

        g.setColour((isLit ? litCol : idleBg).withMultipliedAlpha(alpha));
        g.fillRoundedRectangle(box, 3.5f * uiScale_);

        g.setColour((isLit ? litCol.brighter(0.25f) : KalyxTheme::getGridBorder(theme_)).withMultipliedAlpha(alpha));
        g.drawRoundedRectangle(box, 3.5f * uiScale_, std::max(0.8f, 1.0f * uiScale_));

        const bool lightLit = isLit && (litCol.getPerceivedBrightness() > 0.65f);
        g.setColour((isLit ? (lightLit ? KalyxTheme::dayTextPrimary : juce::Colours::white) : txtCol)
                        .withMultipliedAlpha(alpha));
        g.drawFittedText(kLabels[i], box.toNearestInt(), juce::Justification::centred, 1);
    }
}

void UA1176UI::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    // 1. Fond unifié Tactile Flat (#181B22 Night / #F4F5F7 Day)
    g.setColour(KalyxTheme::getStripBg(theme_));
    g.fillRect(b);

    // Liseré supérieur par révision (Rev A #2B6CB0 / Rev D #D64545 / Rev G #B4BCC8)
    g.setColour(getCurrentAccentColour().withAlpha(moduleIn_ ? 0.90f : 0.35f));
    g.fillRect(b.getX(), b.getY(), b.getWidth(), std::max(2.0f, 2.5f * uiScale_));

    // Liseré subtil Bluestripe sur Rev A derrière INPUT / OUTPUT
    if (currentRev_ == 0 && !inputOutputZone_.isEmpty())
    {
        const auto stripe = inputOutputZone_.reduced(2.0f * uiScale_, 1.0f);
        g.setColour(KalyxTheme::ua1176RevABlue.withAlpha(0.14f));
        g.fillRoundedRectangle(stripe, 4.0f * uiScale_);
        g.setColour(KalyxTheme::ua1176RevABlue.withAlpha(0.55f));
        g.drawRoundedRectangle(stripe, 4.0f * uiScale_, std::max(0.8f, 1.2f * uiScale_));
    }

    // 2. Séparateurs horizontaux Tactile Flat
    g.setColour(KalyxTheme::getGridBorder(theme_));
    if (!sepTopRect_.isEmpty())
        g.fillRect(sepTopRect_);
    if (!sepBotRect_.isEmpty())
        g.fillRect(sepBotRect_);

    // 3. Touches mécaniques RATIO : [4:1] [8:1] [12] [20]
    drawRatioButtons(g);

    // 4. Libellé MODS :
    g.setColour(KalyxTheme::getTextPrimary(theme_).withMultipliedAlpha(moduleIn_ ? 0.88f : 0.35f));
    g.setFont(juce::Font(juce::FontOptions(10.0f * uiScale_, juce::Font::bold)));
    g.drawFittedText("MODS :", modsLabelRect_.toNearestInt(), juce::Justification::centredLeft, 1);
}

void UA1176UI::resized()
{
    const auto b      = getLocalBounds().toFloat();
    const float pad   = 5.0f * uiScale_;
    const float innerW = std::max(40.0f, b.getWidth() - 2.0f * pad);
    float y           = 4.0f * uiScale_;

    // Ligne 1 : [● 1176 IN]     [ < REV D > ]
    const float topH  = 22.0f * uiScale_;
    const float inW   = innerW * 0.48f;
    const float revW  = innerW - inW - 4.0f * uiScale_;
    inBtn_.setBounds(static_cast<int>(pad), static_cast<int>(y), static_cast<int>(inW), static_cast<int>(topH));
    if (revSelector_)
        revSelector_->setBounds(static_cast<int>(pad + inW + 4.0f * uiScale_), static_cast<int>(y),
                                static_cast<int>(revW), static_cast<int>(topH));
    y += topH + 4.0f * uiScale_;

    // Ligne 2 : Courbe de transfert dynamique interactive
    const float curveH = 82.0f * uiScale_;
    transferCurve_.setBounds(static_cast<int>(pad), static_cast<int>(y), static_cast<int>(innerW), static_cast<int>(curveH));
    y += curveH + 4.0f * uiScale_;

    // Séparateur haut
    sepTopRect_ = { pad, y, innerW, std::max(1.0f, 1.0f * uiScale_) };
    y += 4.0f * uiScale_;

    // Zone basse réservée : 2 TactileFaderBar + séparateur + RATIO + MODS + ROUTING
    const float faderH   = 21.0f * uiScale_;
    const float rowCtrlH = 22.0f * uiScale_;
    const float bottomH  = 3.0f * rowCtrlH + 18.0f * uiScale_;
    const float fadersH  = 2.0f * faderH + 6.0f * uiScale_;
    const float knobZoneH = std::max(100.0f * uiScale_, b.getHeight() - y - bottomH - fadersH - 6.0f * uiScale_);

    const float colW  = innerW * 0.5f;
    const float row1H = knobZoneH * 0.55f;
    const float row2H = knobZoneH - row1H;

    const float bigMaxD   = 62.0f;
    const float smallMaxD = 48.0f;

    inputOutputZone_ = { pad, y, innerW, row1H };
    inputKnob_.setBoundsTight (pad,        y, colW, row1H, bigMaxD);
    outputKnob_.setBoundsTight(pad + colW, y, colW, row1H, bigMaxD);
    y += row1H;

    attackKnob_.setBoundsTight (pad,        y, colW, row2H, smallMaxD);
    releaseKnob_.setBoundsTight(pad + colW, y, colW, row2H, smallMaxD);
    y += row2H + 2.0f * uiScale_;

    fetBiasBar_.setBounds(static_cast<int>(pad), static_cast<int>(y), static_cast<int>(innerW), static_cast<int>(faderH));
    y += faderH + 3.0f * uiScale_;
    blendBar_.setBounds(static_cast<int>(pad), static_cast<int>(y), static_cast<int>(innerW), static_cast<int>(faderH));

    // Zone basse ancrée au bas du slot
    float botY = b.getHeight() - bottomH;
    sepBotRect_ = { pad, botY, innerW, std::max(1.0f, 1.0f * uiScale_) };
    botY += 4.0f * uiScale_;

    // Ligne RATIO : [4:1] [8:1] [12] [20]
    const float lblW     = std::min(50.0f * uiScale_, innerW * 0.30f);
    ratioLabelRect_      = { pad, botY, lblW, rowCtrlH };
    const float btnAreaX = pad + lblW + 2.0f * uiScale_;
    const float btnAreaW = innerW - lblW - 2.0f * uiScale_;
    const float btnGap   = 2.0f * uiScale_;
    const float rBtnW    = (btnAreaW - 3.0f * btnGap) * 0.25f;
    for (int i = 0; i < 4; ++i)
        ratioBtnRects_[static_cast<std::size_t>(i)] = {
            btnAreaX + static_cast<float>(i) * (rBtnW + btnGap), botY, rBtnW, rowCtrlH
        };
    botY += rowCtrlH + 4.0f * uiScale_;

    // Ligne MODS : [ ALL (SLAM) ] [SLOW]
    modsLabelRect_    = { pad, botY, lblW, rowCtrlH };
    const float slamW = (btnAreaW - 3.0f * uiScale_) * 0.60f;
    const float slowW = btnAreaW - slamW - 3.0f * uiScale_;
    slamBtn_.setBounds(static_cast<int>(btnAreaX), static_cast<int>(botY),
                       static_cast<int>(slamW), static_cast<int>(rowCtrlH));
    slowBtn_.setBounds(static_cast<int>(btnAreaX + slamW + 3.0f * uiScale_), static_cast<int>(botY),
                       static_cast<int>(slowW), static_cast<int>(rowCtrlH));
    botY += rowCtrlH + 4.0f * uiScale_;

    // Ligne ROUTING : < STEREO >
    if (routingSelector_)
        routingSelector_->setBounds(static_cast<int>(pad), static_cast<int>(botY),
                                    static_cast<int>(innerW), static_cast<int>(rowCtrlH));
}

void UA1176UI::mouseDown(const juce::MouseEvent& e)
{
    auto& apvts = processor_.getAPVTS();
    for (int i = 0; i < 4; ++i)
    {
        if (ratioBtnRects_[static_cast<std::size_t>(i)].expanded(1.0f).contains(e.position))
        {
            if (auto* ratParam = apvts.getParameter(ParamIDs::slotParamId(slotIndex_, 9)))
            {
                ratParam->beginChangeGesture();
                ratParam->setValueNotifyingHost(kalyx::core::choiceToNorm(i, 4));
                ratParam->endChangeGesture();
            }
            // Si ALL (SLAM) était enclenché, cliquer un ratio spécifique le relâche (mécanisme 1176)
            if (allButtonsOn_)
            {
                if (auto* slamParam = apvts.getParameter(ParamIDs::slotParamId(slotIndex_, 10)))
                {
                    slamParam->beginChangeGesture();
                    slamParam->setValueNotifyingHost(0.0f);
                    slamParam->endChangeGesture();
                }
            }
            currentRatio_ = i;
            allButtonsOn_ = false;
            repaint();
            return;
        }
    }
}

} // namespace kalyx::consoles
