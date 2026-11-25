#include "PrecisionDyn2UUI.h"
#include "../../Core/ConsoleRegistry.h"
#include <algorithm>
#include <cmath>

namespace kalyx::consoles
{

PrecisionDyn2UUI::PrecisionDyn2UUI(KalyxHybridConsoleProcessor& proc, int hostSlotIndex, int partnerSlotIndex)
    : processor_(proc), hostSlotIndex_(hostSlotIndex), partnerSlotIndex_(partnerSlotIndex)
{
    using namespace kalyx::core;
    auto& apvts = processor_.getAPVTS();
    const auto defs = ModuleRegistry::getDefaultParams2UForModule(SlotModuleId::PrecisionDyn2U);

    const auto royalBlue = KalyxTheme::kalyxRoyalBlue;  // #2348B8
    const auto aubergine = KalyxTheme::kalyxAubergine;  // #5B2344
    const auto capCol    = KalyxTheme::nightControlIdleBg;
    constexpr bool darkTxt = false;

    setupKalyxToggle(*this, inBtn_, inAtt_, apvts, get2UParamId(0),
                     "PRECISION DYN 2U", royalBlue, darkTxt,
                     "Engage or bypass Kalyx Precision VCA & Transient 2U");

    setupKalyxToggle(*this, autoRelBtn_, autoRelAtt_, apvts, get2UParamId(3),
                     "AUTO REL", aubergine, darkTxt,
                     "Program-dependent auto release");

    orderPill_ = std::make_unique<KalyxSegmentedPill>(
        apvts, get2UParamId(1), "",
        juce::StringArray { "PRE-TRANS", "POST-TRANS" },
        royalBlue, darkTxt,
        "Transient Shaper placement: before or after VCA compression");
    addAndMakeVisible(*orderPill_);

    lookaheadKnob_ = std::make_unique<KalyxRotaryControl>();
    lookaheadKnob_->init(*this, apvts, get2UParamId(2), "LOOKAHEAD", royalBlue, capCol, darkTxt, defs[2],
        [](double v) { return juce::String(static_cast<float>(v) * 5.0f, 1) + " ms"; },
        "Lookahead (0.0 to 5.0 ms) for distortion-free transient limiting",
        [](const juce::String& text) -> double
        {
            juce::String suf;
            const double ms = std::clamp(KalyxValueParsers::parseCleanNumber(text, suf), 0.0, 5.0);
            return ms / 5.0;
        });

    auto fmtDb = [](float minD, float maxD)
    {
        return [minD, maxD](double v) -> juce::String
        {
            return KalyxValueParsers::formatDbClean(mapLin(static_cast<float>(v), minD, maxD));
        };
    };

    auto fmtHz = [](float minH, float maxH)
    {
        return [minH, maxH](double v) -> juce::String
        {
            const float hz = mapLog(static_cast<float>(v), minH, maxH);
            if (hz >= 1000.0f) return juce::String(hz * 0.001f, 2) + " kHz";
            return juce::String(juce::roundToInt(hz)) + " Hz";
        };
    };

    auto fmtMs = [](float minM, float maxM)
    {
        return [minM, maxM](double v) -> juce::String
        {
            const float ms = mapLog(static_cast<float>(v), minM, maxM);
            if (ms < 1.0f) return juce::String(ms, 2) + " ms";
            if (ms >= 1000.0f) return juce::String(ms * 0.001f, 2) + " s";
            return juce::String(ms, 1) + " ms";
        };
    };

    auto fmtRatio = [](double v) -> juce::String
    {
        const double r = PrecisionDyn2UDSP::mapRatioFromNorm(static_cast<float>(v));
        if (r >= 500.0) return juce::String::fromUTF8("\xE2\x88\x9E:1");
        if (r < 1.0)    return juce::String(r, 2) + ":1";
        if (r >= 10.0)  return juce::String(r, 1) + ":1";
        return juce::String(r, 2) + ":1";
    };

    auto parseRatio = [](const juce::String& text) -> double
    {
        auto s = text.trim().toLowerCase();
        if (s.contains("inf") || s.contains(juce::String::fromUTF8("\xE2\x88\x9E")))
            return 1.0;
        juce::String suf;
        const double r = std::clamp(KalyxValueParsers::parseCleanNumber(s, suf), 0.5, 50.0);
        if (r <= 1.0)
            return ((r - 0.5) / 0.5) * 0.15;
        if (r >= 20.0)
            return 0.98;
        const double u = std::log(r / 1.05) / std::log(20.0 / 1.05);
        return 0.15 + std::clamp(u, 0.0, 1.0) * (0.96 - 0.15);
    };

    auto fmtSignedPct = [](double v) -> juce::String
    {
        const int pct = juce::roundToInt(mapLin(static_cast<float>(v), -100.0f, 100.0f));
        return (pct > 0 ? "+" : "") + juce::String(pct) + " %";
    };

    auto fmtPct = [](double v) -> juce::String
    {
        return juce::String(juce::roundToInt(std::clamp(static_cast<float>(v), 0.0f, 1.0f) * 100.0f)) + " %";
    };

    for (auto& k : knobs_)
        k = std::make_unique<KalyxRotaryControl>();

    // Rangée 1 (VCA) : THRESH, RATIO, KNEE, ATTACK, RELEASE, MAKEUP
    knobs_[0]->init(*this, apvts, get2UParamId(4), "THRESH", royalBlue, capCol, darkTxt, defs[4],
                    fmtDb(-50.0f, 0.0f), "VCA Threshold (-50 to 0 dB)", KalyxValueParsers::parseLinDb(-50.0f, 0.0f));
    knobs_[1]->init(*this, apvts, get2UParamId(5), "RATIO", royalBlue, capCol, darkTxt, defs[5],
                    fmtRatio, "Continuous Ratio (0.5:1 expansion to inf:1 limiting)", parseRatio);
    knobs_[2]->init(*this, apvts, get2UParamId(6), "KNEE", royalBlue, capCol, darkTxt, defs[6],
                    fmtDb(0.0f, 24.0f), "Soft Knee (0 dB Hard to 24 dB Ultra-Soft)", KalyxValueParsers::parseLinDb(0.0f, 24.0f));
    knobs_[3]->init(*this, apvts, get2UParamId(7), "ATTACK", royalBlue, capCol, darkTxt, defs[7],
                    fmtMs(0.01f, 250.0f), "VCA Attack (0.01 ms to 250 ms)", KalyxValueParsers::parseMsLog(0.01f, 250.0f));
    knobs_[4]->init(*this, apvts, get2UParamId(8), "RELEASE", royalBlue, capCol, darkTxt, defs[8],
                    fmtMs(5.0f, 2500.0f), "VCA Release (5 ms to 2.5 s)", KalyxValueParsers::parseMsLog(5.0f, 2500.0f));
    knobs_[5]->init(*this, apvts, get2UParamId(9), "MAKEUP", royalBlue, capCol, darkTxt, defs[9],
                    fmtDb(-12.0f, 24.0f), "Output Makeup Gain (-12 to +24 dB)", KalyxValueParsers::parseLinDb(-12.0f, 24.0f));

    // Rangée 2 (Transient & Routing) : PUNCH, SUSTAIN, TRANS FRQ, SC HPF, BLEND
    knobs_[6]->init(*this, apvts, get2UParamId(10), "PUNCH", aubergine, capCol, darkTxt, defs[10],
                    fmtSignedPct, "Transient Attack Shaper (-100% to +100%)", KalyxValueParsers::parseLinPct(-100.0f, 100.0f));
    knobs_[7]->init(*this, apvts, get2UParamId(11), "SUSTAIN", aubergine, capCol, darkTxt, defs[11],
                    fmtSignedPct, "Transient Sustain / Room Tail (-100% to +100%)", KalyxValueParsers::parseLinPct(-100.0f, 100.0f));
    knobs_[8]->init(*this, apvts, get2UParamId(12), "TRANS FRQ", aubergine, capCol, darkTxt, defs[12],
                    fmtHz(100.0f, 8000.0f), "Dual-Band Transient Crossover (100 Hz to 8 kHz)", KalyxValueParsers::parseHzLog(100.0f, 8000.0f));
    knobs_[9]->init(*this, apvts, get2UParamId(13), "SC HPF", royalBlue, capCol, darkTxt, defs[13],
                    fmtHz(20.0f, 500.0f), "Sidechain High-Pass Filter (20 to 500 Hz)", KalyxValueParsers::parseHzLog(20.0f, 500.0f));
    knobs_[10]->init(*this, apvts, get2UParamId(14), "BLEND", royalBlue, capCol, darkTxt, defs[14],
                     fmtPct, "Dry / Wet Parallel Mix (0% to 100%)", KalyxValueParsers::parseLinPct(0.0f, 100.0f));

    routingSelector_ = std::make_unique<KalyxArrowSelector>(
        apvts, get2UParamId(15), "ROUTING",
        juce::StringArray { "STEREO", "M/S", "DUAL" },
        royalBlue, darkTxt, "Channel Processing Topology");
    addAndMakeVisible(*routingSelector_);

    syncFromAPVTS();
}

juce::String PrecisionDyn2UUI::get2UParamId(int paramZeroBased) const
{
    if (paramZeroBased < kalyx::core::kParamsPerSlot)
        return ParamIDs::slotParamId(hostSlotIndex_, paramZeroBased + 1);
    return ParamIDs::slotParamId(partnerSlotIndex_, (paramZeroBased - kalyx::core::kParamsPerSlot) + 1);
}

void PrecisionDyn2UUI::setUiScale(float s)
{
    uiScale_ = std::clamp(s, 0.75f, 2.20f);
    if (orderPill_)       orderPill_->setUiScale(uiScale_);
    if (lookaheadKnob_)   lookaheadKnob_->setUiScale(uiScale_);
    if (routingSelector_) routingSelector_->setUiScale(uiScale_);
    for (auto& k : knobs_)
        if (k) k->setUiScale(uiScale_);
    resized();
    repaint();
}

void PrecisionDyn2UUI::setChassisTheme(kalyx::core::ChassisTheme t)
{
    theme_ = t;
    const bool isDay = (t == kalyx::core::ChassisTheme::Day);
    const auto capCol = isDay ? KalyxTheme::dayControlIdleBg : KalyxTheme::nightControlIdleBg;
    if (lookaheadKnob_)   lookaheadKnob_->setThemeColours(KalyxTheme::kalyxRoyalBlue, capCol, isDay);
    if (routingSelector_) routingSelector_->setThemeColours(KalyxTheme::kalyxRoyalBlue, isDay);
    for (int i = 0; i < 11; ++i)
    {
        const auto accent = (i >= 6 && i <= 8) ? KalyxTheme::kalyxAubergine : KalyxTheme::kalyxRoyalBlue;
        if (knobs_[static_cast<std::size_t>(i)])
            knobs_[static_cast<std::size_t>(i)]->setThemeColours(accent, capCol, isDay);
    }
    repaint();
}

void PrecisionDyn2UUI::updateDependentStates()
{
    const auto p = processor_.getSlotParams2UForGUI(hostSlotIndex_);
    const bool modOn   = (p[0] > 0.5f);
    const bool autoRel = (p[3] > 0.5f);

    if (orderPill_)       orderPill_->setEnabled(modOn);
    if (lookaheadKnob_)   lookaheadKnob_->setDimmedDisabled(!modOn);
    if (routingSelector_) routingSelector_->setEnabled(modOn);
    autoRelBtn_.setEnabled(modOn);

    for (int i = 0; i < 11; ++i)
    {
        bool dim = !modOn;
        if (i == 4 && autoRel) dim = true; // RELEASE grisé lorsque AUTO REL est actif
        if (knobs_[static_cast<std::size_t>(i)])
            knobs_[static_cast<std::size_t>(i)]->setDimmedDisabled(dim);
    }
}

void PrecisionDyn2UUI::syncFromAPVTS()
{
    updateDependentStates();
    repaint();
}

void PrecisionDyn2UUI::updateScopeAndRepaint()
{
    updateDependentStates();
    processor_.copyPrecisionDynScope(hostSlotIndex_, scopeHist_);
    if (!scopeRect_.isEmpty())
        repaint(scopeRect_.expanded(2.0f).toNearestInt());
}

void PrecisionDyn2UUI::paint(juce::Graphics& g)
{
    const bool isDay = (theme_ == kalyx::core::ChassisTheme::Day);
    const auto sc = uiScale_;

    // Fond Tactile Flat de l'oscilloscope temps réel
    if (!scopeRect_.isEmpty())
    {
        const auto scopeBg     = isDay ? juce::Colour(0xffe8ebf2) : juce::Colour(0xff10131a);
        const auto scopeBorder = isDay ? juce::Colour(0xffcbd2df) : juce::Colour(0xff2a3040);
        const auto gridCol     = isDay ? juce::Colour(0xffd5dce8) : juce::Colour(0xff1e2432);

        g.setColour(scopeBg);
        g.fillRoundedRectangle(scopeRect_, 4.0f * sc);
        g.setColour(scopeBorder);
        g.drawRoundedRectangle(scopeRect_, 4.0f * sc, 1.0f);

        const float w = scopeRect_.getWidth() - 6.0f;
        const float h = scopeRect_.getHeight() - 6.0f;
        const float x0 = scopeRect_.getX() + 3.0f;
        const float y0 = scopeRect_.getY() + 3.0f;

        // Grille verticale temporelle (4 divisions)
        g.setColour(gridCol.withAlpha(0.75f));
        for (int c = 1; c <= 3; ++c)
        {
            const float gx = x0 + w * (static_cast<float>(c) / 4.0f);
            g.drawVerticalLine(static_cast<int>(gx), y0, y0 + h);
        }

        // Grille horizontale dB (0 dB, -3 dB, -6 dB, -12 dB sur plage 0..24 dB)
        static constexpr float kDbLines[4] = { 0.0f, 3.0f, 6.0f, 12.0f };
        static constexpr const char* kDbLabels[4] = { "0", "-3", "-6", "-12 dB" };
        g.setFont(juce::Font(juce::FontOptions(8.5f * sc, juce::Font::plain)));
        for (int i = 0; i < 4; ++i)
        {
            const float gy = y0 + (kDbLines[i] / 24.0f) * (h * 0.90f);
            g.setColour(gridCol);
            g.drawHorizontalLine(static_cast<int>(gy), x0, x0 + w);
            g.setColour(KalyxTheme::getTextSecondary(theme_).withAlpha(0.75f));
            g.drawText(kDbLabels[i], static_cast<int>(x0 + w - 42.0f * sc), static_cast<int>(gy + 1.0f),
                       static_cast<int>(39.0f * sc), static_cast<int>(10.0f * sc), juce::Justification::centredRight);
        }

        const float midY = y0 + h * 0.56f;

        juce::Path wavePath, grPath, transPath;
        constexpr int N = PrecisionDyn2UDSP::kScopeSize;

        wavePath.startNewSubPath(x0, midY);
        grPath.startNewSubPath(x0, y0);
        transPath.startNewSubPath(x0, midY);

        for (int i = 0; i < N; ++i)
        {
            const float fx = x0 + (static_cast<float>(i) / static_cast<float>(N - 1)) * w;
            const auto& s = scopeHist_[static_cast<std::size_t>(i)];

            const float wAmp = std::clamp(s.wave, 0.0f, 1.0f) * (h * 0.40f);
            wavePath.lineTo(fx, midY - wAmp);

            const float grY = y0 + std::clamp(s.gr, 0.0f, 1.0f) * (h * 0.90f);
            grPath.lineTo(fx, grY);

            const float trY = midY - std::clamp(s.trans, -1.0f, 1.0f) * (h * 0.38f);
            transPath.lineTo(fx, trY);
        }
        for (int i = N - 1; i >= 0; --i)
        {
            const float fx = x0 + (static_cast<float>(i) / static_cast<float>(N - 1)) * w;
            const auto& s = scopeHist_[static_cast<std::size_t>(i)];
            const float wAmp = std::clamp(s.wave, 0.0f, 1.0f) * (h * 0.40f);
            wavePath.lineTo(fx, midY + wAmp);
        }
        wavePath.closeSubPath();

        g.setColour(isDay ? juce::Colour(0x30556580) : juce::Colour(0x388898b8));
        g.fillPath(wavePath);

        g.setColour(KalyxTheme::amberLed.withAlpha(0.85f));
        g.strokePath(transPath, juce::PathStrokeType(1.4f * sc));

        g.setColour(KalyxTheme::kalyxRoyalBlue);
        g.strokePath(grPath, juce::PathStrokeType(2.0f * sc));

        // Légende discrète
        g.setFont(juce::Font(juce::FontOptions(9.0f * sc, juce::Font::bold)));
        g.setColour(KalyxTheme::kalyxRoyalBlue);
        g.drawText("GR", static_cast<int>(x0 + 4.0f), static_cast<int>(y0 + 2.0f), 24, 12, juce::Justification::left);
        g.setColour(KalyxTheme::amberLed);
        g.drawText("TRANS", static_cast<int>(x0 + 28.0f), static_cast<int>(y0 + 2.0f), 42, 12, juce::Justification::left);
    }
}

void PrecisionDyn2UUI::resized()
{
    const auto b = getLocalBounds().toFloat().reduced(6.0f * uiScale_, 4.0f * uiScale_);
    if (b.isEmpty()) return;

    // En-tête compacte sans bouton rotatif : [IN] [PRE-TRANS | POST-TRANS] [AUTO REL]
    const float hdrH = 24.0f * uiScale_;
    headerRect_ = b.withHeight(hdrH);

    float hx = headerRect_.getX();
    const float wIn = 132.0f * uiScale_;
    inBtn_.setBounds(static_cast<int>(hx), static_cast<int>(headerRect_.getY() + 1.0f * uiScale_),
                     static_cast<int>(wIn), static_cast<int>(hdrH - 2.0f * uiScale_));
    hx += wIn + 8.0f * uiScale_;

    const float wPill = 148.0f * uiScale_;
    if (orderPill_)
        orderPill_->setBounds(static_cast<int>(hx), static_cast<int>(headerRect_.getY() + 1.0f * uiScale_),
                              static_cast<int>(wPill), static_cast<int>(hdrH - 2.0f * uiScale_));

    const float wAuto = 76.0f * uiScale_;
    autoRelBtn_.setBounds(static_cast<int>(headerRect_.getRight() - wAuto),
                          static_cast<int>(headerRect_.getY() + 1.0f * uiScale_),
                          static_cast<int>(wAuto), static_cast<int>(hdrH - 2.0f * uiScale_));

    const float bodyTop = headerRect_.getBottom() + 6.0f * uiScale_;
    const float footH   = 22.0f * uiScale_;
    const float availH  = std::max(100.0f, b.getBottom() - bodyTop - footH - 8.0f * uiScale_);

    // Zone 1 : Viewer / Oscilloscope (~55% de la hauteur utile)
    const float scH = std::max(84.0f * uiScale_, availH * 0.55f);
    scopeRect_ = { b.getX(), bodyTop, b.getWidth(), scH };

    // Zone 3 : Footer ROUTING pleine largeur tout en bas
    if (routingSelector_)
    {
        routingSelector_->setBounds(static_cast<int>(b.getX()),
                                    static_cast<int>(b.getBottom() - footH),
                                    static_cast<int>(b.getWidth()),
                                    static_cast<int>(footH));
    }

    // Zone 2 : 12 contrôles sur 2 rangées de 6, diamètre borné à 45.0f * uiScale
    const float gridTop = scopeRect_.getBottom() + 6.0f * uiScale_;
    const float gridH   = std::max(60.0f, (b.getBottom() - footH - 4.0f * uiScale_) - gridTop);
    const float rowH    = gridH * 0.5f;
    const float colW    = b.getWidth() / 6.0f;

    // Rangée 1 : THRESH(0), RATIO(1), KNEE(2), ATTACK(3), RELEASE(4), MAKEUP(5)
    for (int c = 0; c < 6; ++c)
    {
        const float x = b.getX() + static_cast<float>(c) * colW;
        if (knobs_[static_cast<std::size_t>(c)])
            knobs_[static_cast<std::size_t>(c)]->setBoundsTight(x, gridTop, colW, rowH, 45.0f);
    }

    // Rangée 2 : PUNCH(6), SUSTAIN(7), TRANS FRQ(8), SC HPF(9), LOOKAHEAD, BLEND(10)
    const float y2 = gridTop + rowH;
    const int row2KnobIndices[6] = { 6, 7, 8, 9, -1, 10 };
    for (int c = 0; c < 6; ++c)
    {
        const float x = b.getX() + static_cast<float>(c) * colW;
        const int kIdx = row2KnobIndices[c];
        if (kIdx >= 0)
        {
            if (knobs_[static_cast<std::size_t>(kIdx)])
                knobs_[static_cast<std::size_t>(kIdx)]->setBoundsTight(x, y2, colW, rowH, 45.0f);
        }
        else if (lookaheadKnob_)
        {
            lookaheadKnob_->setBoundsTight(x, y2, colW, rowH, 45.0f);
        }
    }
}

} // namespace kalyx::consoles
