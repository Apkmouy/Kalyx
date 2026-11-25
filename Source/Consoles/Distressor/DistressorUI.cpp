#include "DistressorUI.h"
#include "../../Core/ConsoleRegistry.h"
#include <algorithm>
#include <cmath>

namespace kalyx::consoles
{

DistressorUI::DistressorUI(KalyxHybridConsoleProcessor& proc, int slotIndex)
    : processor_(proc), slotIndex_(slotIndex)
{
    using namespace kalyx::core;
    auto& apvts = processor_.getAPVTS();
    const auto defs = ModuleRegistry::getDefaultParamsForModule(SlotModuleId::DistressorComp);

    const auto p = processor_.getSlotParamsForGUI(slotIndex_);
    moduleIn_  = (p[0] > 0.5f);
    ratioIdx_  = mapChoice(p[5], 8);
    britishOn_ = (p[6] > 0.5f);

    const auto sigRed = KalyxTheme::distressorRed;   // #C93030
    const auto amber  = KalyxTheme::distressorAmber; // #E0A82E
    const auto capCol = KalyxTheme::nightControlIdleBg;
    constexpr bool darkTxt = false;

    setupKalyxToggle(*this, inBtn_, inAtt_, apvts, ParamIDs::slotParamId(slotIndex_, 1),
                     "DIST IN", sigRed, darkTxt, "Engage or bypass Empirical Labs Distressor");
    setupKalyxToggle(*this, britishBtn_, britishAtt_, apvts, ParamIDs::slotParamId(slotIndex_, 7),
                     "BRITISH", amber, darkTxt, "British Mode: 1176 All-Buttons knee & pumping on 2:1..6:1 ratios");

    nukeBtn_.setTooltip("Engage NUKE brickwall limiting curve (100:1)");
    nukeBtn_.onClick = [this]()
    {
        if (auto* param = processor_.getAPVTS().getParameter(ParamIDs::slotParamId(slotIndex_, 6)))
        {
            const int targetIdx = (ratioIdx_ == 7) ? 4 : 7; // Toggle NUKE <-> 6:1
            param->setValueNotifyingHost(choiceToNorm(targetIdx, 8));
        }
    };
    addAndMakeVisible(nukeBtn_);

    auto fmtDb = [](double v) -> juce::String
    {
        return KalyxValueParsers::formatDbClean(mapLin(static_cast<float>(v), -24.0f, 24.0f));
    };

    auto fmtAttack = [](double v) -> juce::String
    {
        const float ms = DistressorDSP::mapAttackMs(static_cast<float>(v));
        if (ms < 1.0f)
            return juce::String(juce::roundToInt(ms * 1000.0f)) + juce::String::fromUTF8(" \xC2\xB5s");
        return juce::String(ms, 1) + " ms";
    };

    auto fmtRelease = [](double v) -> juce::String
    {
        const float ms = DistressorDSP::mapReleaseMs(static_cast<float>(v));
        if (ms >= 1000.0f)
            return juce::String(ms * 0.001f, 2) + " s";
        return juce::String(juce::roundToInt(ms)) + " ms";
    };

    auto fmtPct = [](double v) -> juce::String
    {
        return juce::String(juce::roundToInt(std::clamp(static_cast<float>(v), 0.0f, 1.0f) * 100.0f)) + " %";
    };

    auto parseDb24 = KalyxValueParsers::parseLinDb(-24.0f, 24.0f);
    auto parsePct  = KalyxValueParsers::parseLinPct(0.0f, 100.0f);
    auto parseAtt  = KalyxValueParsers::parseMsLog(0.05f, 30.0f);
    auto parseRel  = KalyxValueParsers::parseMsLog(50.0f, 3500.0f);

    inputKnob_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 2),
                    "INPUT", sigRed, capCol, darkTxt, defs[1], fmtDb,
                    "Input Drive (-24 to +24 dB): drives compression and harmonic distortion", parseDb24);
    outputKnob_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 3),
                     "OUTPUT", amber, capCol, darkTxt, defs[2], fmtDb,
                     "Output Makeup Gain (-24 to +24 dB)", parseDb24);
    attackKnob_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 4),
                     "ATTACK", amber, capCol, darkTxt, defs[3], fmtAttack,
                     "Attack Time (50 us to 30 ms)", parseAtt);
    releaseKnob_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 5),
                      "RELEASE", amber, capCol, darkTxt, defs[4], fmtRelease,
                      "Release Time (50 ms to 3.5 s)", parseRel);
    blendKnob_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 12),
                    "BLEND", sigRed, capCol, darkTxt, defs[11], fmtPct,
                    "Parallel Dry/Wet Compression Blend (0 - 100 %)", parsePct);

    ratioSelector_ = std::make_unique<KalyxArrowSelector>(
        apvts, ParamIDs::slotParamId(slotIndex_, 6), "RATIO",
        juce::StringArray { "1:1", "2:1", "3:1", "4:1", "6:1", "10:1", "20:1", "NUKE" },
        sigRed, darkTxt, "Select Distressor Ratio Curve (1:1 harmonic warmer to NUKE brickwall)");
    addAndMakeVisible(*ratioSelector_);

    distPill_ = std::make_unique<KalyxSegmentedPill>(
        apvts, ParamIDs::slotParamId(slotIndex_, 11), "DIST",
        juce::StringArray { "CLEAN", "DIST 2", "D3" },
        amber, darkTxt, "Harmonic Circuit: CLEAN, DIST 2 (even tube), D3 (odd tape)");
    addAndMakeVisible(*distPill_);

    filterPill_ = std::make_unique<KalyxMultiTogglePill>(
        apvts, "FILTER",
        juce::StringArray {
            ParamIDs::slotParamId(slotIndex_, 8),
            ParamIDs::slotParamId(slotIndex_, 9),
            ParamIDs::slotParamId(slotIndex_, 10)
        },
        juce::StringArray { "AUD HP", "DET HP", "6k EMPH" },
        sigRed, darkTxt, "Audio 80 Hz HPF, Detector 80 Hz HPF, and 6 kHz Band Emphasis");
    addAndMakeVisible(*filterPill_);

    routingSelector_ = std::make_unique<KalyxArrowSelector>(
        apvts, ParamIDs::slotParamId(slotIndex_, 13), "ROUTING",
        juce::StringArray { "MONO", "STEREO", "DUAL M", "M/S", "INV" },
        amber, darkTxt, "Channel Routing Mode: MONO, STEREO, DUAL MONO, M/S, INVERSE");
    addAndMakeVisible(*routingSelector_);

    syncFromAPVTS();
}

void DistressorUI::setUiScale(float s)
{
    uiScale_ = std::clamp(s, 0.75f, 2.20f);
    inBtn_.getProperties().set("kalyxScale", uiScale_);
    britishBtn_.getProperties().set("kalyxScale", uiScale_);
    inputKnob_.setUiScale(uiScale_);
    outputKnob_.setUiScale(uiScale_);
    attackKnob_.setUiScale(uiScale_);
    releaseKnob_.setUiScale(uiScale_);
    blendKnob_.setUiScale(uiScale_);
    if (ratioSelector_)   ratioSelector_->setUiScale(uiScale_);
    if (distPill_)        distPill_->setUiScale(uiScale_);
    if (filterPill_)      filterPill_->setUiScale(uiScale_);
    if (routingSelector_) routingSelector_->setUiScale(uiScale_);
    resized();
    repaint();
}

void DistressorUI::setChassisTheme(kalyx::core::ChassisTheme t)
{
    if (theme_ == t)
        return;
    theme_ = t;

    const bool darkTxt = (theme_ == kalyx::core::ChassisTheme::Day);
    const auto capCol  = KalyxTheme::getControlIdleBg(theme_);
    const auto sigRed  = KalyxTheme::distressorRed;
    const auto amber   = KalyxTheme::distressorAmber;

    inBtn_.getProperties().set("kalyxDarkText", darkTxt);
    britishBtn_.getProperties().set("kalyxDarkText", darkTxt);

    inputKnob_.setThemeColours(sigRed, capCol, darkTxt);
    outputKnob_.setThemeColours(amber, capCol, darkTxt);
    attackKnob_.setThemeColours(amber, capCol, darkTxt);
    releaseKnob_.setThemeColours(amber, capCol, darkTxt);
    blendKnob_.setThemeColours(sigRed, capCol, darkTxt);

    if (ratioSelector_)   ratioSelector_->setThemeColours(sigRed, darkTxt);
    if (distPill_)        distPill_->setThemeColours(amber, darkTxt);
    if (filterPill_)      filterPill_->setThemeColours(sigRed, darkTxt);
    if (routingSelector_) routingSelector_->setThemeColours(amber, darkTxt);

    const bool isNuke = (ratioIdx_ == 7);
    nukeBtn_.setColour(juce::TextButton::buttonColourId,
                       isNuke ? sigRed : capCol);
    nukeBtn_.setColour(juce::TextButton::textColourOffId,
                       isNuke ? juce::Colours::white : KalyxTheme::getTextSecondary(theme_));

    repaint();
}

void DistressorUI::syncFromAPVTS()
{
    const auto p = processor_.getSlotParamsForGUI(slotIndex_);
    const bool modOn  = (p[0] > 0.5f);
    const int  ratIdx = kalyx::core::mapChoice(p[5], 8);
    const bool britOn = (p[6] > 0.5f);

    bool needRepaint = false;

    if (ratIdx != ratioIdx_)
    {
        ratioIdx_ = ratIdx;
        const bool isNuke = (ratioIdx_ == 7);
        nukeBtn_.setColour(juce::TextButton::buttonColourId,
                           isNuke ? KalyxTheme::distressorRed : KalyxTheme::getControlIdleBg(theme_));
        nukeBtn_.setColour(juce::TextButton::textColourOffId,
                           isNuke ? juce::Colours::white : KalyxTheme::getTextSecondary(theme_));
        needRepaint = true;
    }

    if (britOn != britishOn_)
    {
        britishOn_ = britOn;
        needRepaint = true;
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
        setKnobAlpha(inputKnob_);
        setKnobAlpha(outputKnob_);
        setKnobAlpha(attackKnob_);
        setKnobAlpha(releaseKnob_);
        setKnobAlpha(blendKnob_);
        britishBtn_.setAlpha(alpha);
        nukeBtn_.setAlpha(alpha);
        if (ratioSelector_)   ratioSelector_->setAlpha(alpha);
        if (distPill_)        distPill_->setAlpha(alpha);
        if (filterPill_)      filterPill_->setAlpha(alpha);
        if (routingSelector_) routingSelector_->setAlpha(alpha);
        needRepaint = true;
    }

    if (needRepaint)
        repaint();
}

void DistressorUI::setTelemetry(float grDb, float thdPct) noexcept
{
    const float prevGr  = smoothedGrDb_;
    const float prevThd = smoothedThd_;
    smoothedGrDb_ = std::max(0.0f, grDb);
    smoothedThd_  = std::max(0.0f, thdPct);

    if (!grStripRect_.isEmpty() && std::abs(smoothedGrDb_ - prevGr) > 0.05f)
        repaint(grStripRect_.expanded(2.0f).toNearestInt());
    if (!thdStripRect_.isEmpty() && std::abs(smoothedThd_ - prevThd) > 0.05f)
        repaint(thdStripRect_.expanded(2.0f).toNearestInt());
}

void DistressorUI::drawGrBargraph(juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    if (bounds.isEmpty()) return;

    g.setColour(KalyxTheme::getControlIdleBg(theme_));
    g.fillRoundedRectangle(bounds, 3.5f * uiScale_);
    g.setColour(KalyxTheme::getGridBorder(theme_));
    g.drawRoundedRectangle(bounds, 3.5f * uiScale_, std::max(0.8f, 1.0f * uiScale_));

    const float lblW = 24.0f * uiScale_;
    g.setColour(KalyxTheme::getTextSecondary(theme_));
    g.setFont(juce::Font(juce::FontOptions(9.5f * uiScale_, juce::Font::bold)));
    g.drawFittedText("GR:", bounds.withWidth(lblW).toNearestInt(), juce::Justification::centred, 1);

    const auto row = bounds.withTrimmedLeft(lblW).reduced(3.0f * uiScale_, 2.0f * uiScale_);
    static constexpr float kGrThresh[6]   = { 1.0f, 2.0f, 4.0f, 6.0f, 10.0f, 20.0f };
    static constexpr const char* kGrLbl[6] = { "-1", "-2", "-4", "-6", "-10", "-20" };

    const float cellW = row.getWidth() / 6.0f;
    const float sqSz  = std::min(cellW * 0.62f, 8.5f * uiScale_);
    const auto offTrack = (theme_ == kalyx::core::ChassisTheme::Day) ? KalyxTheme::dayKnobTrackBg : KalyxTheme::knobTrackBg;
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 8.5f * uiScale_, juce::Font::bold)));

    for (int i = 0; i < 6; ++i)
    {
        const float cx = row.getX() + (static_cast<float>(i) + 0.5f) * cellW;
        const float cy = row.getY() + sqSz * 0.55f;
        const bool lit = moduleIn_ && (smoothedGrDb_ >= kGrThresh[i]);
        const juce::Colour onCol = (i < 2) ? juce::Colour(0xff22c55e)
                                 : (i < 4) ? KalyxTheme::distressorAmber : KalyxTheme::distressorRed;

        const juce::Rectangle<float> sq(cx - sqSz * 0.5f, cy - sqSz * 0.5f, sqSz, sqSz);
        g.setColour(lit ? onCol : offTrack);
        g.fillRoundedRectangle(sq, 1.5f * uiScale_);

        g.setColour(lit ? KalyxTheme::getTextPrimary(theme_) : KalyxTheme::getTextSecondary(theme_));
        g.drawFittedText(kGrLbl[i],
                         juce::Rectangle<int>(static_cast<int>(cx - cellW * 0.5f),
                                              static_cast<int>(sq.getBottom() + 1.0f),
                                              static_cast<int>(cellW),
                                              static_cast<int>(10.0f * uiScale_)),
                         juce::Justification::centred, 1);
    }
}

void DistressorUI::drawThdLeds(juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    if (bounds.isEmpty()) return;

    const float lblW = 32.0f * uiScale_;
    g.setColour(KalyxTheme::getTextSecondary(theme_));
    g.setFont(juce::Font(juce::FontOptions(9.5f * uiScale_, juce::Font::bold)));
    g.drawFittedText("THD:", bounds.withWidth(lblW).toNearestInt(), juce::Justification::centredLeft, 1);

    const float halfW = (bounds.getWidth() - lblW) * 0.5f;
    const float ledR  = 3.5f * uiScale_;
    const auto offTrack = (theme_ == kalyx::core::ChassisTheme::Day) ? KalyxTheme::dayKnobTrackBg : KalyxTheme::knobTrackBg;

    auto drawLed = [&](float x, const char* lbl, bool lit, juce::Colour onCol)
    {
        const float cx = x + ledR + 2.0f * uiScale_;
        const float cy = bounds.getCentreY();
        if (lit)
        {
            g.setColour(onCol.withAlpha(0.35f));
            g.fillEllipse(cx - ledR - 2.0f * uiScale_, cy - ledR - 2.0f * uiScale_,
                          (ledR + 2.0f * uiScale_) * 2.0f, (ledR + 2.0f * uiScale_) * 2.0f);
            g.setColour(onCol);
        }
        else
        {
            g.setColour(offTrack);
        }
        g.fillEllipse(cx - ledR, cy - ledR, ledR * 2.0f, ledR * 2.0f);
        g.setColour(lit ? KalyxTheme::getTextPrimary(theme_) : KalyxTheme::getTextSecondary(theme_));
        g.setFont(juce::Font(juce::FontOptions(10.0f * uiScale_, juce::Font::bold)));
        g.drawText(lbl,
                   static_cast<int>(cx + ledR + 4.0f * uiScale_),
                   static_cast<int>(bounds.getY()),
                   static_cast<int>(halfW - ledR * 2.0f - 6.0f * uiScale_),
                   static_cast<int>(bounds.getHeight()),
                   juce::Justification::centredLeft);
    };

    const bool lit1 = moduleIn_ && (smoothedThd_ >= 1.0f);
    const bool lit3 = moduleIn_ && (smoothedThd_ >= 3.0f);
    drawLed(bounds.getX() + lblW,         "1% THD", lit1, KalyxTheme::distressorAmber);
    drawLed(bounds.getX() + lblW + halfW, "3% THD", lit3, KalyxTheme::distressorRed);
}

void DistressorUI::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour(KalyxTheme::getStripBg(theme_)); // #181B22 Night / #F4F5F7 Day
    g.fillRect(b);

    // Liseré supérieur Rouge Signal (#C93030)
    g.setColour(KalyxTheme::distressorRed.withAlpha(moduleIn_ ? 0.90f : 0.35f));
    g.fillRect(b.getX(), b.getY(), b.getWidth(), std::max(2.0f, 2.5f * uiScale_));

    // Séparateurs fins Tactile Flat
    g.setColour(KalyxTheme::getGridBorder(theme_));
    if (!sepTopRect_.isEmpty()) g.fillRect(sepTopRect_);
    if (!sepBotRect_.isEmpty()) g.fillRect(sepBotRect_);

    drawGrBargraph(g, grStripRect_);
    drawThdLeds(g, thdStripRect_);
}

void DistressorUI::resized()
{
    const auto b = getLocalBounds().toFloat();
    const float pad = 5.0f * uiScale_;
    const float iw  = b.getWidth() - 2.0f * pad;
    float y = 4.0f * uiScale_;

    // 1. Ligne du haut : [● DIST IN]  [ BRITISH ]  [NUKE]
    const float topH  = 21.0f * uiScale_;
    const float gap   = 3.0f * uiScale_;
    const float wIn   = iw * 0.36f;
    const float wBrit = iw * 0.36f;
    const float wNuke = iw - wIn - wBrit - 2.0f * gap;
    inBtn_.setBounds(static_cast<int>(pad), static_cast<int>(y), static_cast<int>(wIn), static_cast<int>(topH));
    britishBtn_.setBounds(static_cast<int>(pad + wIn + gap), static_cast<int>(y), static_cast<int>(wBrit), static_cast<int>(topH));
    nukeBtn_.setBounds(static_cast<int>(pad + wIn + wBrit + 2.0f * gap), static_cast<int>(y), static_cast<int>(wNuke), static_cast<int>(topH));
    y += topH + 4.0f * uiScale_;

    // 2. Bargraphe GR : [■][■][□][□][□][□]
    const float grH = 26.0f * uiScale_;
    grStripRect_ = { pad, y, iw, grH };
    y += grH + 3.0f * uiScale_;

    // 3. THD LEDs : (●) 1% THD   (○) 3% THD
    const float thdH = 16.0f * uiScale_;
    thdStripRect_ = { pad, y, iw, thdH };
    y += thdH + 3.0f * uiScale_;

    sepTopRect_ = { pad, y, iw, std::max(1.0f, 1.0f * uiScale_) };
    y += 4.0f * uiScale_;

    // 4. Zone basse réservée : RATIO, DIST, FILTER, ROUTING (4 pilules/sélecteurs de 22.0f * uiScale)
    const float rowPillH = 22.0f * uiScale_;
    const float pillGap  = 3.0f * uiScale_;
    const float botBlockH = 4.0f * rowPillH + 3.0f * pillGap + 8.0f * uiScale_;
    const float botY = b.getHeight() - botBlockH;
    sepBotRect_ = { pad, botY - 3.0f * uiScale_, iw, std::max(1.0f, 1.0f * uiScale_) };

    // 5. Zone centrale : 5 potentiomètres (INPUT/OUTPUT, ATTACK/RELEASE, BLEND centré)
    const float knobAreaH = std::max(120.0f * uiScale_, botY - y - 6.0f * uiScale_);
    const float colW = iw * 0.5f;
    const float rowH = knobAreaH / 3.0f;

    inputKnob_.setBoundsTight  (pad,        y, colW, rowH);
    outputKnob_.setBoundsTight (pad + colW, y, colW, rowH);
    y += rowH;

    attackKnob_.setBoundsTight (pad,        y, colW, rowH);
    releaseKnob_.setBoundsTight(pad + colW, y, colW, rowH);
    y += rowH;

    blendKnob_.setBoundsTight  (pad + colW * 0.5f, y, colW, rowH);

    // 6. Empilement bas : RATIO, DIST, FILTER, ROUTING
    float py = botY;
    if (ratioSelector_)   { ratioSelector_->setBounds  (static_cast<int>(pad), static_cast<int>(py), static_cast<int>(iw), static_cast<int>(rowPillH)); py += rowPillH + pillGap; }
    if (distPill_)        { distPill_->setBounds       (static_cast<int>(pad), static_cast<int>(py), static_cast<int>(iw), static_cast<int>(rowPillH)); py += rowPillH + pillGap; }
    if (filterPill_)      { filterPill_->setBounds     (static_cast<int>(pad), static_cast<int>(py), static_cast<int>(iw), static_cast<int>(rowPillH)); py += rowPillH + pillGap; }
    if (routingSelector_) { routingSelector_->setBounds(static_cast<int>(pad), static_cast<int>(py), static_cast<int>(iw), static_cast<int>(rowPillH)); }
}

void DistressorUI::mouseDown(const juce::MouseEvent&)
{
}

} // namespace kalyx::consoles
