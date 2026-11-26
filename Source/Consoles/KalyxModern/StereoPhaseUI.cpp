#include "StereoPhaseUI.h"
#include "../../Core/ConsoleRegistry.h"
#include <algorithm>
#include <cmath>

namespace kalyx::consoles
{

StereoPhaseUI::StereoPhaseUI(KalyxHybridConsoleProcessor& proc, int slotIndex)
    : processor_(proc), slotIndex_(slotIndex)
{
    using namespace kalyx::core;
    auto& apvts = processor_.getAPVTS();
    const auto defs = ModuleRegistry::getDefaultParamsForModule(SlotModuleId::StereoPhase);

    const auto royalBlue = KalyxTheme::kalyxRoyalBlue;
    const auto aubergine = KalyxTheme::kalyxAubergine;
    const auto capCol    = KalyxTheme::nightControlIdleBg;
    constexpr bool darkTxt = false;

    setupKalyxToggle(*this, inBtn_, inAtt_, apvts, ParamIDs::slotParamId(slotIndex_, 1),
                     "SPATIAL IN", royalBlue, darkTxt, "Engage or bypass Kalyx Spatial Vector");

    modePill_ = std::make_unique<KalyxSegmentedPill>(
        apvts, ParamIDs::slotParamId(slotIndex_, 2), "",
        juce::StringArray { "L/R", "M/S" },
        royalBlue, darkTxt, "Processing Mode: L/R Energy-Compensated or Pure M/S");
    addAndMakeVisible(*modePill_);

    auto fmtWidth = [](double v) -> juce::String
    {
        return juce::String(juce::roundToInt(std::clamp(static_cast<float>(v), 0.0f, 1.0f) * 200.0f)) + " %";
    };
    auto fmtHz = [](double v) -> juce::String
    {
        return juce::String(juce::roundToInt(mapLog(static_cast<float>(v), 20.0f, 500.0f))) + " Hz";
    };
    auto fmtDeg = [](double v) -> juce::String
    {
        const int deg = juce::roundToInt(mapLin(static_cast<float>(v), -180.0f, 180.0f));
        return (deg > 0 ? "+" : "") + juce::String(deg) + juce::String::fromUTF8("\xC2\xB0");
    };
    auto fmtTilt = [](double v) -> juce::String
    {
        return KalyxValueParsers::formatDbClean(mapLin(static_cast<float>(v), -6.0f, 6.0f));
    };
    auto fmtDb12 = [](double v) -> juce::String
    {
        return KalyxValueParsers::formatDbClean(mapLin(static_cast<float>(v), -12.0f, 12.0f));
    };
    auto parseDeg = [](const juce::String& text) -> double
    {
        juce::String suf;
        const double deg = std::clamp(KalyxValueParsers::parseCleanNumber(text, suf), -180.0, 180.0);
        return static_cast<double>(unmapLin(static_cast<float>(deg), -180.0f, 180.0f));
    };

    for (auto& k : knobs_)
        k = std::make_unique<KalyxRotaryControl>();

    knobs_[0]->init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 3), "WIDTH", royalBlue, capCol, darkTxt, defs[2],
                    fmtWidth, "Stereo Width (0% Mono to 200% Extra-Wide)", KalyxValueParsers::parseLinPct(0.0f, 200.0f));
    knobs_[1]->init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 4), "MONO MAKER", royalBlue, capCol, darkTxt, defs[3],
                    fmtHz, "Linkwitz-Riley 24 dB/oct Mono Maker (20 to 500 Hz)", KalyxValueParsers::parseHzLog(20.0f, 500.0f));
    knobs_[2]->init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 5), "PHASE ROT", aubergine, capCol, darkTxt, defs[4],
                    fmtDeg, "Continuous All-Pass Phase Rotation (-180 to +180 deg)", parseDeg);
    knobs_[3]->init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 6), "SIDE TILT", royalBlue, capCol, darkTxt, defs[5],
                    fmtTilt, "Side Channel Tilt EQ (-6 to +6 dB/oct)", KalyxValueParsers::parseLinDb(-6.0f, 6.0f));

    midGainBar_.init (*this, apvts, ParamIDs::slotParamId(slotIndex_, 7), "MID GAIN",  royalBlue, true, darkTxt, defs[6],
                      fmtDb12, "Mid Channel Gain (-12 to +12 dB)", KalyxValueParsers::parseLinDb(-12.0f, 12.0f));
    sideGainBar_.init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 8), "SIDE GAIN", royalBlue, true, darkTxt, defs[7],
                      fmtDb12, "Side Channel Gain (-12 to +12 dB)", KalyxValueParsers::parseLinDb(-12.0f, 12.0f));

    setupKalyxToggle(*this, invLBtn_, invLAtt_, apvts, ParamIDs::slotParamId(slotIndex_, 9),
                     juce::String::fromUTF8("\xC3\x98 L"), aubergine, darkTxt, "Invert Left Channel Polarity");
    setupKalyxToggle(*this, invRBtn_, invRAtt_, apvts, ParamIDs::slotParamId(slotIndex_, 10),
                     juce::String::fromUTF8("\xC3\x98 R"), aubergine, darkTxt, "Invert Right Channel Polarity");
    setupKalyxToggle(*this, swapBtn_, swapAtt_, apvts, ParamIDs::slotParamId(slotIndex_, 11),
                     "L<>R", royalBlue, darkTxt, "Swap Left and Right Channels");

    monitorPill_ = std::make_unique<KalyxSegmentedPill>(
        apvts, ParamIDs::slotParamId(slotIndex_, 12), "",
        juce::StringArray { "STEREO", "MID", "SIDE" },
        aubergine, darkTxt, "Isolated Monitoring: Stereo, Mid Only, or Side Only");
    addAndMakeVisible(*monitorPill_);

    syncFromAPVTS();
}

void StereoPhaseUI::setUiScale(float s)
{
    uiScale_ = std::clamp(s, 0.75f, 2.20f);
    if (modePill_)    modePill_->setUiScale(uiScale_);
    if (monitorPill_) monitorPill_->setUiScale(uiScale_);
    for (auto& k : knobs_)
        if (k) k->setUiScale(uiScale_);
    midGainBar_.setUiScale(uiScale_);
    sideGainBar_.setUiScale(uiScale_);
    resized();
    repaint();
}

void StereoPhaseUI::setChassisTheme(kalyx::core::ChassisTheme t)
{
    theme_ = t;
    const bool isDay = (t == kalyx::core::ChassisTheme::Day);
    const auto capCol = isDay ? KalyxTheme::dayControlIdleBg : KalyxTheme::nightControlIdleBg;
    midGainBar_.setThemeColours(KalyxTheme::kalyxRoyalBlue, isDay);
    sideGainBar_.setThemeColours(KalyxTheme::kalyxRoyalBlue, isDay);
    for (int i = 0; i < 4; ++i)
        if (knobs_[static_cast<std::size_t>(i)])
            knobs_[static_cast<std::size_t>(i)]->setThemeColours(
                (i == 2) ? KalyxTheme::kalyxAubergine : KalyxTheme::kalyxRoyalBlue, capCol, isDay);
    repaint();
}

void StereoPhaseUI::updateDependentStates()
{
    const auto p = processor_.getSlotParamsForGUI(slotIndex_);
    const bool modOn = (p[0] > 0.5f);
    if (modePill_)    modePill_->setEnabled(modOn);
    if (monitorPill_) monitorPill_->setEnabled(modOn);
    invLBtn_.setEnabled(modOn);
    invRBtn_.setEnabled(modOn);
    swapBtn_.setEnabled(modOn);
    midGainBar_.setDimmedDisabled(!modOn);
    sideGainBar_.setDimmedDisabled(!modOn);
    for (auto& k : knobs_)
        if (k) k->setDimmedDisabled(!modOn);
}

void StereoPhaseUI::syncFromAPVTS()
{
    updateDependentStates();
    repaint();
}

void StereoPhaseUI::updateTelemetryAndRepaint()
{
    updateDependentStates();
    const float nextCorr = processor_.getSlotStereoCorrelation(slotIndex_);
    if (std::abs(nextCorr - correlation_) > 0.01f)
    {
        correlation_ = nextCorr;
        if (!corrRect_.isEmpty())
            repaint(corrRect_.expanded(2.0f).toNearestInt());
    }
}

void StereoPhaseUI::paint(juce::Graphics& g)
{
    if (corrRect_.isEmpty()) return;
    const bool isDay = (theme_ == kalyx::core::ChassisTheme::Day);
    const auto sc = uiScale_;

    g.setFont(juce::Font(juce::FontOptions(9.0f * sc, juce::Font::bold)));
    g.setColour(KalyxTheme::getTextSecondary(theme_));
    g.drawText("CORR -1", static_cast<int>(corrRect_.getX()), static_cast<int>(corrRect_.getY()),
               static_cast<int>(44.0f * sc), static_cast<int>(corrRect_.getHeight()),
               juce::Justification::centredLeft);
    g.drawText("+1", static_cast<int>(corrRect_.getRight() - 18.0f * sc), static_cast<int>(corrRect_.getY()),
               static_cast<int>(18.0f * sc), static_cast<int>(corrRect_.getHeight()),
               juce::Justification::centredRight);

    const float barX = corrRect_.getX() + 45.0f * sc;
    const float barW = std::max(20.0f, corrRect_.getWidth() - 65.0f * sc);
    const float barY = corrRect_.getCentreY() - 3.0f * sc;
    const float barH = 6.0f * sc;

    g.setColour(isDay ? juce::Colour(0xffcbd2df) : juce::Colour(0xff232834));
    g.fillRoundedRectangle(barX, barY, barW, barH, 3.0f * sc);

    const float midX = barX + barW * 0.5f;
    g.setColour(isDay ? juce::Colour(0xff8c96a8) : juce::Colour(0xff4a5266));
    g.drawVerticalLine(static_cast<int>(midX), barY - 2.0f * sc, barY + barH + 2.0f * sc);

    const float norm = std::clamp((correlation_ + 1.0f) * 0.5f, 0.0f, 1.0f);
    const float dotX = barX + norm * barW;
    const auto dotCol = (correlation_ < -0.15f) ? KalyxTheme::distressorRed
                      : (correlation_ < 0.25f)  ? KalyxTheme::amberLed
                                                : KalyxTheme::kalyxRoyalBlue;

    const float fillX = std::min(midX, dotX);
    const float fillW = std::abs(dotX - midX);
    g.setColour(dotCol.withAlpha(0.75f));
    g.fillRoundedRectangle(fillX, barY, fillW, barH, 2.0f * sc);

    g.setColour(dotCol);
    g.fillEllipse(dotX - 4.5f * sc, corrRect_.getCentreY() - 4.5f * sc, 9.0f * sc, 9.0f * sc);
}

void StereoPhaseUI::resized()
{
    const auto b = getLocalBounds().toFloat().reduced(5.0f * uiScale_, 3.0f * uiScale_);
    if (b.isEmpty()) return;

    const float hdrH = 22.0f * uiScale_;
    const float halfW = (b.getWidth() - 4.0f * uiScale_) * 0.5f;
    inBtn_.setBounds(static_cast<int>(b.getX()), static_cast<int>(b.getY()),
                     static_cast<int>(halfW), static_cast<int>(hdrH));
    if (modePill_)
        modePill_->setBounds(static_cast<int>(b.getX() + halfW + 4.0f * uiScale_), static_cast<int>(b.getY()),
                             static_cast<int>(halfW), static_cast<int>(hdrH));

    const float corrH = 18.0f * uiScale_;
    corrRect_ = { b.getX(), b.getY() + hdrH + 3.0f * uiScale_, b.getWidth(), corrH };

    const float monH   = 22.0f * uiScale_;
    const float phH    = 22.0f * uiScale_;
    const float faderH = 22.0f * uiScale_;
    const float monY   = b.getBottom() - monH;
    const float phY    = monY - phH - 4.0f * uiScale_;
    const float sideY  = phY - faderH - 4.0f * uiScale_;
    const float midY   = sideY - faderH - 4.0f * uiScale_;

    midGainBar_.setBounds (static_cast<int>(b.getX()), static_cast<int>(midY),  static_cast<int>(b.getWidth()), static_cast<int>(faderH));
    sideGainBar_.setBounds(static_cast<int>(b.getX()), static_cast<int>(sideY), static_cast<int>(b.getWidth()), static_cast<int>(faderH));

    if (monitorPill_)
        monitorPill_->setBounds(static_cast<int>(b.getX()), static_cast<int>(monY),
                                static_cast<int>(b.getWidth()), static_cast<int>(monH));

    const float btnW = (b.getWidth() - 8.0f * uiScale_) / 3.0f;
    invLBtn_.setBounds(static_cast<int>(b.getX()), static_cast<int>(phY), static_cast<int>(btnW), static_cast<int>(phH));
    invRBtn_.setBounds(static_cast<int>(b.getX() + btnW + 4.0f * uiScale_), static_cast<int>(phY), static_cast<int>(btnW), static_cast<int>(phH));
    swapBtn_.setBounds(static_cast<int>(b.getX() + 2.0f * (btnW + 4.0f * uiScale_)), static_cast<int>(phY), static_cast<int>(btnW), static_cast<int>(phH));

    const float gridTop = corrRect_.getBottom() + 4.0f * uiScale_;
    const float gridH   = std::max(50.0f, midY - 4.0f * uiScale_ - gridTop);
    const float rowH    = gridH * 0.5f;
    const float colW    = b.getWidth() * 0.5f;

    for (int r = 0; r < 2; ++r)
    {
        for (int c = 0; c < 2; ++c)
        {
            const int idx = r * 2 + c;
            if (knobs_[static_cast<std::size_t>(idx)])
                knobs_[static_cast<std::size_t>(idx)]->setBoundsTight(
                    b.getX() + static_cast<float>(c) * colW,
                    gridTop + static_cast<float>(r) * rowH,
                    colW, rowH, 48.0f);
        }
    }
}

} // namespace kalyx::consoles
