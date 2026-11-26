#include "FairchildUI.h"
#include "../../Core/ConsoleRegistry.h"
#include <algorithm>
#include <cmath>

namespace kalyx::consoles
{

FairchildUI::FairchildUI(KalyxHybridConsoleProcessor& proc, int slotIndex)
    : processor_(proc), slotIndex_(slotIndex)
{
    using namespace kalyx::core;
    auto& apvts = processor_.getAPVTS();
    const auto defs = ModuleRegistry::getDefaultParamsForModule(SlotModuleId::FairchildComp);

    const auto p = processor_.getSlotParamsForGUI(slotIndex_);
    moduleIn_    = (p[0] > 0.5f);
    vintageMode_ = (p[10] > 0.5f);
    tubeIdx_     = mapChoice(p[8], 7);

    const auto gold   = KalyxTheme::fairchildGold;   // #C59B38
    const auto copper = KalyxTheme::fairchildCopper; // #9E5834
    const auto capCol = KalyxTheme::getControlIdleBg(theme_);
    const bool darkTxt = (theme_ == ChassisTheme::Day);

    setupKalyxToggle(*this, inBtn_, inAtt_, apvts, ParamIDs::slotParamId(slotIndex_, 1),
                     "670 IN", gold, darkTxt, "Engage or bypass Fairchild 660/670 Vari-Mu Compressor");

    vintagePill_ = std::make_unique<KalyxSegmentedPill>(
        apvts, ParamIDs::slotParamId(slotIndex_, 11), "",
        juce::StringArray { "MOD", "VINTAGE" },
        gold, darkTxt, "VINTAGE: 6 stepped Time Constants (TC 1..6) / MOD: Continuous interpolation");
    addAndMakeVisible(*vintagePill_);

    auto fmtDb = [](double v) -> juce::String
    {
        return KalyxValueParsers::formatDbClean(mapLin(static_cast<float>(v), -24.0f, 24.0f));
    };

    auto fmtPct = [](double v) -> juce::String
    {
        return juce::String(juce::roundToInt(std::clamp(static_cast<float>(v), 0.0f, 1.0f) * 100.0f)) + " %";
    };

    auto fmtTimeConst = [this](double v) -> juce::String
    {
        const float norm = std::clamp(static_cast<float>(v), 0.0f, 1.0f);
        if (vintageMode_)
        {
            const int tc = std::clamp(static_cast<int>(std::lround(norm * 5.0f)) + 1, 1, 6);
            static constexpr const char* kTcDesc[6] = {
                "TC 1 (0.3s)", "TC 2 (0.8s)", "TC 3 (2.0s)",
                "TC 4 (5.0s)", "TC 5 (Prog)", "TC 6 (Auto)"
            };
            return kTcDesc[tc - 1];
        }
        return "TC " + juce::String(1.0f + norm * 5.0f, 1);
    };

    auto parseTimeConst = [](const juce::String& text) -> double
    {
        auto s = text.trim();
        if (s.startsWithIgnoreCase("TC"))
            s = s.substring(2).trim();
        juce::String suf;
        const double tc = std::clamp(KalyxValueParsers::parseCleanNumber(s, suf), 1.0, 6.0);
        return (tc - 1.0) / 5.0;
    };

    auto parseDb24 = KalyxValueParsers::parseLinDb(-24.0f, 24.0f);
    auto parsePct  = KalyxValueParsers::parseLinPct(0.0f, 100.0f);

    inputKnob_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 2),
                    "INPUT GAIN", gold, capCol, darkTxt, defs[1], fmtDb,
                    "Input Transformer Gain (-24 to +24 dB): drives Vari-Mu grid bias", parseDb24);
    threshKnob_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 3),
                     "AC THRESH", gold, capCol, darkTxt, defs[2], fmtPct,
                     "AC Threshold (0 to 100 %): Vari-Mu tube compression depth", parsePct);
    timeConstKnob_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 4),
                        "TIME CONST", copper, capCol, darkTxt, defs[3], fmtTimeConst,
                        "Time Constant 1-6 (Stepped in VINTAGE, Continuous in MOD)", parseTimeConst);
    timeConstKnob_.setSteppedMode(vintageMode_, 6);
    outputKnob_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 5),
                     "OUTPUT", gold, capCol, darkTxt, defs[4], fmtDb,
                     "Output Stage Level (-24 to +24 dB)", parseDb24);
    transferCurve_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 3),
                        -42.0f, -2.0f, defs[2], true, gold, darkTxt,
                        "Dynamic Vari-Mu Transfer Curve (Drag to adjust AC Threshold, Double-Click to Reset)");

    matchLrBar_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 6),
                     "MATCH L/R", copper, false, darkTxt, defs[5], fmtPct,
                     "L/R Tube Matching (100% = matched, lower = organic 3D stereo drift)", parsePct);
    matchPpBar_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 7),
                     "MATCH P/P", copper, false, darkTxt, defs[6], fmtPct,
                     "Push-Pull Balance (100% = clean symmetry, lower = rich even-order harmonics)", parsePct);
    blendBar_.init  (*this, apvts, ParamIDs::slotParamId(slotIndex_, 8),
                     "BLEND", gold, false, darkTxt, defs[7], fmtPct,
                     "Parallel Dry/Wet Vari-Mu Blend (0 - 100 %)", parsePct);

    tubeSelector_ = std::make_unique<KalyxArrowSelector>(
        apvts, ParamIDs::slotParamId(slotIndex_, 9), "NOS TUBE",
        juce::StringArray { "5749 FSE", "CV4009", "GE 5749", "6386 BLK", "6386 5*", "RCA 6BC8", "ECC82" },
        gold, darkTxt, "Select NOS Vari-Mu Tube Pair (alters knee, saturation & harmonic profile)");
    addAndMakeVisible(*tubeSelector_);

    routingSelector_ = std::make_unique<KalyxArrowSelector>(
        apvts, ParamIDs::slotParamId(slotIndex_, 10), "ROUTING",
        juce::StringArray { "MONO", "STEREO", "DUAL M", "M/S", "INV" },
        copper, darkTxt, "Stereo Detection & Processing Topology");
    addAndMakeVisible(*routingSelector_);

    syncFromAPVTS();
}

void FairchildUI::setUiScale(float s)
{
    uiScale_ = std::clamp(s, 0.75f, 2.20f);
    inBtn_.getProperties().set("kalyxScale", uiScale_);
    transferCurve_.setUiScale(uiScale_);
    if (vintagePill_)     vintagePill_->setUiScale(uiScale_);
    if (tubeSelector_)    tubeSelector_->setUiScale(uiScale_);
    if (routingSelector_) routingSelector_->setUiScale(uiScale_);
    inputKnob_.setUiScale(uiScale_);
    threshKnob_.setUiScale(uiScale_);
    timeConstKnob_.setUiScale(uiScale_);
    outputKnob_.setUiScale(uiScale_);
    matchLrBar_.setUiScale(uiScale_);
    matchPpBar_.setUiScale(uiScale_);
    blendBar_.setUiScale(uiScale_);
    resized();
    repaint();
}

void FairchildUI::setChassisTheme(kalyx::core::ChassisTheme t)
{
    if (theme_ == t)
        return;
    theme_ = t;

    const bool darkTxt = (theme_ == kalyx::core::ChassisTheme::Day);
    const auto capCol  = KalyxTheme::getControlIdleBg(theme_);
    const auto gold    = KalyxTheme::fairchildGold;
    const auto copper  = KalyxTheme::fairchildCopper;

    inBtn_.getProperties().set("kalyxDarkText", darkTxt);
    transferCurve_.setThemeColours(gold, darkTxt);
    if (vintagePill_)     vintagePill_->setThemeColours(gold, darkTxt);
    if (tubeSelector_)    tubeSelector_->setThemeColours(gold, darkTxt);
    if (routingSelector_) routingSelector_->setThemeColours(copper, darkTxt);

    inputKnob_.setThemeColours(gold, capCol, darkTxt);
    threshKnob_.setThemeColours(gold, capCol, darkTxt);
    timeConstKnob_.setThemeColours(copper, capCol, darkTxt);
    outputKnob_.setThemeColours(gold, capCol, darkTxt);
    matchLrBar_.setThemeColours(copper, darkTxt);
    matchPpBar_.setThemeColours(copper, darkTxt);
    blendBar_.setThemeColours(gold, darkTxt);

    repaint();
}

void FairchildUI::setTelemetry(float grDb, float inputPeakDb) noexcept
{
    smoothedGrDb_ = smoothedGrDb_ * 0.72f + grDb * 0.28f;
    transferCurve_.setTelemetry(inputPeakDb, smoothedGrDb_);
}

void FairchildUI::syncFromAPVTS()
{
    using namespace kalyx::core;
    const auto p = processor_.getSlotParamsForGUI(slotIndex_);
    const bool inNow = (p[0] > 0.5f);
    const bool vintNow = (p[10] > 0.5f);
    const int tubeNow = mapChoice(p[8], 7);

    const bool changed = (inNow != moduleIn_) || (vintNow != vintageMode_) || (tubeNow != tubeIdx_);
    moduleIn_    = inNow;
    vintageMode_ = vintNow;
    tubeIdx_     = tubeNow;

    const float activeAlpha = moduleIn_ ? 1.0f : 0.35f;
    inputKnob_.setDimmed(!moduleIn_);
    threshKnob_.setDimmed(!moduleIn_);
    timeConstKnob_.setDimmed(!moduleIn_);
    outputKnob_.setDimmed(!moduleIn_);
    matchLrBar_.setDimmedDisabled(!moduleIn_);
    matchPpBar_.setDimmedDisabled(!moduleIn_);
    blendBar_.setDimmedDisabled(!moduleIn_);
    if (vintagePill_)     vintagePill_->setAlpha(activeAlpha);
    if (tubeSelector_)    tubeSelector_->setAlpha(activeAlpha);
    if (routingSelector_) routingSelector_->setAlpha(activeAlpha);

    transferCurve_.setCurveConfig(InteractiveTransferCurve::CurveType::VariMu,
                                  4.0f, 14.0f, moduleIn_, vintageMode_);

    if (changed)
    {
        timeConstKnob_.setSteppedMode(vintageMode_, 6);
        timeConstKnob_.slider.updateText();
        repaint();
    }
}

void FairchildUI::drawGrBargraph(juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    using namespace kalyx::core;
    const auto bg = KalyxTheme::getControlIdleBg(theme_);
    const auto border = KalyxTheme::getGridBorder(theme_);
    const auto txtDim = KalyxTheme::getTextSecondary(theme_);

    g.setColour(bg);
    g.fillRoundedRectangle(bounds, 4.0f * uiScale_);
    g.setColour(border);
    g.drawRoundedRectangle(bounds, 4.0f * uiScale_, 1.0f);

    g.setFont(KalyxTheme::makeFont(9.5f * uiScale_, juce::Font::bold));
    g.setColour(txtDim);
    g.drawText("GR", bounds.getX() + 6.0f * uiScale_, bounds.getY(), 20.0f * uiScale_, bounds.getHeight(),
               juce::Justification::centredLeft, false);

    constexpr int kNumLeds = 6;
    static constexpr float kThresholds[kNumLeds] = { 0.5f, 1.5f, 3.0f, 6.0f, 10.0f, 15.0f };
    const float ledAreaX = bounds.getX() + 28.0f * uiScale_;
    const float ledAreaW = bounds.getWidth() - 68.0f * uiScale_;
    const float gap = 4.0f * uiScale_;
    const float ledW = std::max(4.0f, (ledAreaW - gap * (kNumLeds - 1)) / kNumLeds);
    const float ledH = std::max(4.0f, bounds.getHeight() * 0.44f);
    const float ledY = bounds.getCentreY() - ledH * 0.5f;

    for (int i = 0; i < kNumLeds; ++i)
    {
        const float lx = ledAreaX + static_cast<float>(i) * (ledW + gap);
        const bool lit = (smoothedGrDb_ >= kThresholds[i]);
        const juce::Colour col = (i >= 4) ? KalyxTheme::fairchildCopper : KalyxTheme::fairchildGold;
        g.setColour(lit ? col : col.withAlpha(0.16f));
        g.fillRoundedRectangle(lx, ledY, ledW, ledH, 2.0f * uiScale_);
    }

    g.setFont(KalyxTheme::makeMonoFont(10.0f * uiScale_, juce::Font::bold));
    g.setColour(smoothedGrDb_ > 0.3f ? KalyxTheme::fairchildGold : txtDim);
    const juce::String dbStr = "-" + juce::String(smoothedGrDb_, 1) + "dB";
    g.drawText(dbStr, bounds.getRight() - 40.0f * uiScale_, bounds.getY(), 36.0f * uiScale_, bounds.getHeight(),
               juce::Justification::centredRight, false);
}

void FairchildUI::paint(juce::Graphics& g)
{
    using namespace kalyx::core;
    const auto b = getLocalBounds().toFloat();

    // Fond de slot unifié (Night #181B22 / Day #F4F5F7) + liseré Or Vintage / Cuivre
    g.setColour(KalyxTheme::getStripBg(theme_));
    g.fillRoundedRectangle(b, 6.0f * uiScale_);

    g.setColour(KalyxTheme::getGridBorder(theme_));
    g.drawRoundedRectangle(b.reduced(0.5f), 6.0f * uiScale_, 1.0f);

    // Liseré supérieur Or Vintage #C59B38
    g.setColour(KalyxTheme::fairchildGold.withAlpha(moduleIn_ ? 0.90f : 0.35f));
    g.fillRoundedRectangle(b.getX() + 1.0f, b.getY() + 1.0f, b.getWidth() - 2.0f, 3.0f * uiScale_, 1.5f);

    // Séparateurs horizontaux
    g.setColour(KalyxTheme::getGridBorder(theme_));
    if (!sepTopRect_.isEmpty()) g.fillRect(sepTopRect_);
    if (!sepBotRect_.isEmpty()) g.fillRect(sepBotRect_);

    // Signature de la lampe NOS sélectionnée
    if (!tubeSigRect_.isEmpty())
    {
        const auto& prof = FairchildDSP::getTubeProfile(tubeIdx_);
        g.setFont(KalyxTheme::makeFont(9.5f * uiScale_, juce::Font::plain));
        g.setColour(KalyxTheme::fairchildGold.withAlpha(moduleIn_ ? 0.92f : 0.40f));
        g.drawFittedText(prof.signature, tubeSigRect_.toNearestInt(), juce::Justification::centred, 1);
    }
}

void FairchildUI::resized()
{
    auto r = getLocalBounds().reduced(static_cast<int>(8.0f * uiScale_), static_cast<int>(6.0f * uiScale_));
    const int s = static_cast<int>(4.0f * uiScale_);

    // Ligne du haut : [● 670 IN]  [ MOD | VINTAGE ]
    const int topH = std::max(20, static_cast<int>(24.0f * uiScale_));
    auto topRow = r.removeFromTop(topH);
    const int inW = std::min(topRow.getWidth() / 2 - s, static_cast<int>(86.0f * uiScale_));
    inBtn_.setBounds(topRow.removeFromLeft(inW));
    if (vintagePill_)
    {
        const int pillW = std::min(topRow.getWidth(), static_cast<int>(116.0f * uiScale_));
        vintagePill_->setBounds(topRow.removeFromRight(pillW));
    }

    r.removeFromTop(s);

    // Courbe de transfert dynamique Vari-Mu
    const int curveH = std::max(66, static_cast<int>(76.0f * uiScale_));
    transferCurve_.setBounds(r.removeFromTop(curveH));
    r.removeFromTop(s);
    sepTopRect_ = r.removeFromTop(1).toFloat();
    r.removeFromTop(s);

    // Réserve bas : sélecteurs NOS TUBE + signature + ROUTING
    const int selH = std::max(22, static_cast<int>(22.0f * uiScale_));
    const int sigH = std::max(12, static_cast<int>(13.0f * uiScale_));
    const int botTotal = selH * 2 + sigH + s * 3;
    auto botArea = r.removeFromBottom(std::min(botTotal, r.getHeight() / 3));
    sepBotRect_ = botArea.removeFromTop(1).toFloat();
    botArea.removeFromTop(s);

    if (tubeSelector_)
        tubeSelector_->setBounds(botArea.removeFromTop(selH));
    tubeSigRect_ = botArea.removeFromTop(sigH).toFloat();
    botArea.removeFromTop(s);
    if (routingSelector_)
        routingSelector_->setBounds(botArea.removeFromTop(selH));

    // 3 TactileFaderBar (MATCH L/R, MATCH P/P, BLEND) au-dessus du bas
    const int faderH = std::max(18, static_cast<int>(20.0f * uiScale_));
    auto faderArea = r.removeFromBottom(faderH * 3 + s * 3);
    faderArea.removeFromTop(s);
    matchLrBar_.setBounds(faderArea.removeFromTop(faderH)); faderArea.removeFromTop(s);
    matchPpBar_.setBounds(faderArea.removeFromTop(faderH)); faderArea.removeFromTop(s);
    blendBar_.setBounds  (faderArea.removeFromTop(faderH));

    // Grille 2x2 de potentiomètres principaux dans l'espace central
    const int topKnobH = r.getHeight() / 2;
    auto row1 = r.removeFromTop(topKnobH);
    auto row2 = r;

    auto placeKnob = [](KalyxRotaryControl& k, juce::Rectangle<int> area, float maxD)
    {
        k.setBoundsTight(static_cast<float>(area.getX()), static_cast<float>(area.getY()),
                         static_cast<float>(area.getWidth()), static_cast<float>(area.getHeight()), maxD);
    };

    const int w2 = row1.getWidth() / 2;
    placeKnob(inputKnob_,     row1.removeFromLeft(w2), 54.0f);
    placeKnob(threshKnob_,    row1,                    54.0f);
    placeKnob(timeConstKnob_, row2.removeFromLeft(w2), 50.0f);
    placeKnob(outputKnob_,    row2,                    50.0f);
}

} // namespace kalyx::consoles
