#include "HarmonicSculptorUI.h"
#include "../../Core/ConsoleRegistry.h"
#include <algorithm>
#include <cmath>

namespace kalyx::consoles
{

HarmonicSculptorUI::HarmonicSculptorUI(KalyxHybridConsoleProcessor& proc, int slotIndex)
    : processor_(proc), slotIndex_(slotIndex)
{
    using namespace kalyx::core;
    auto& apvts = processor_.getAPVTS();
    const auto defs = ModuleRegistry::getDefaultParamsForModule(SlotModuleId::HarmonicSculptor);

    const auto aubergine = KalyxTheme::kalyxAubergine; // #5B2344
    const auto royalBlue = KalyxTheme::kalyxRoyalBlue; // #2348B8
    const auto gold      = KalyxTheme::amberLed;       // #E0A82E
    const auto capCol    = KalyxTheme::nightControlIdleBg;
    constexpr bool darkTxt = false;

    setupKalyxToggle(*this, inBtn_, inAtt_, apvts, ParamIDs::slotParamId(slotIndex_, 1),
                     "SCULPT IN", aubergine, darkTxt, "Engage or bypass Kalyx Harmonic Sculptor");
    setupKalyxToggle(*this, tiltLinkBtn_, tiltLinkAtt_, apvts, ParamIDs::slotParamId(slotIndex_, 2),
                     "TILT LNK", royalBlue, darkTxt, "Compensate pre-saturation Tilt EQ inversely at output");

    auto fmtPct = [](double v) -> juce::String
    {
        return juce::String(juce::roundToInt(std::clamp(static_cast<float>(v), 0.0f, 1.0f) * 100.0f)) + " %";
    };
    auto fmtDyn = [](double v) -> juce::String
    {
        const int pct = juce::roundToInt(mapLin(static_cast<float>(v), -50.0f, 50.0f));
        return (pct > 0 ? "+" : "") + juce::String(pct) + " %";
    };
    auto fmtTilt = [](double v) -> juce::String
    {
        return KalyxValueParsers::formatDbClean(mapLin(static_cast<float>(v), -6.0f, 6.0f));
    };
    auto fmtDrive = [](double v) -> juce::String
    {
        return KalyxValueParsers::formatDbClean(mapLin(static_cast<float>(v), -12.0f, 24.0f));
    };

    for (auto& k : knobs_)
        k = std::make_unique<KalyxRotaryControl>();

    knobs_[0]->init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 3), "EVEN (2nd)", gold, capCol, darkTxt, defs[2],
                    fmtPct, "Asymmetric Tube / Class-A Even Harmonics (0-100%)", KalyxValueParsers::parseLinPct(0.0f, 100.0f));
    knobs_[1]->init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 4), "ODD (3rd)", aubergine, capCol, darkTxt, defs[3],
                    fmtPct, "Symmetric Tape / Transistor Odd Harmonics (0-100%)", KalyxValueParsers::parseLinPct(0.0f, 100.0f));
    knobs_[2]->init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 5), "TILT EQ", royalBlue, capCol, darkTxt, defs[4],
                    fmtTilt, "Pre-saturation Tilt EQ (-6 to +6 dB/oct)", KalyxValueParsers::parseLinDb(-6.0f, 6.0f));
    knobs_[3]->init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 6), "DYNAMICS", royalBlue, capCol, darkTxt, defs[5],
                    fmtDyn, "Pre-clip Transient Punch (-50% to +50%)", KalyxValueParsers::parseLinPct(-50.0f, 50.0f));
    knobs_[4]->init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 7), "DRIVE", aubergine, capCol, darkTxt, defs[6],
                    fmtDrive, "Input Saturation Drive (-12 to +24 dB)", KalyxValueParsers::parseLinDb(-12.0f, 24.0f));
    knobs_[5]->init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 8), "BLEND", royalBlue, capCol, darkTxt, defs[7],
                    fmtPct, "Dry / Wet Parallel Saturation Mix (0-100%)", KalyxValueParsers::parseLinPct(0.0f, 100.0f));

    bandPill_ = std::make_unique<KalyxSegmentedPill>(
        apvts, ParamIDs::slotParamId(slotIndex_, 9), "",
        juce::StringArray { "FULL", "LOW", "MID", "HI" },
        aubergine, darkTxt, "Active Saturation Frequency Band");
    addAndMakeVisible(*bandPill_);

    routingSelector_ = std::make_unique<KalyxArrowSelector>(
        apvts, ParamIDs::slotParamId(slotIndex_, 10), "ROUTING",
        juce::StringArray { "STEREO", "M/S", "MID", "SIDE" },
        royalBlue, darkTxt, "Stereo / Mid-Side Saturation Routing");
    addAndMakeVisible(*routingSelector_);

    syncFromAPVTS();
}

void HarmonicSculptorUI::setUiScale(float s)
{
    uiScale_ = std::clamp(s, 0.75f, 2.20f);
    for (auto& k : knobs_)
        if (k) k->setUiScale(uiScale_);
    if (bandPill_)        bandPill_->setUiScale(uiScale_);
    if (routingSelector_) routingSelector_->setUiScale(uiScale_);
    resized();
    repaint();
}

void HarmonicSculptorUI::setChassisTheme(kalyx::core::ChassisTheme t)
{
    theme_ = t;
    const bool isDay = (t == kalyx::core::ChassisTheme::Day);
    const auto capCol = isDay ? KalyxTheme::dayControlIdleBg : KalyxTheme::nightControlIdleBg;
    if (routingSelector_) routingSelector_->setThemeColours(KalyxTheme::kalyxRoyalBlue, isDay);

    const juce::Colour accents[6] = {
        KalyxTheme::amberLed, KalyxTheme::kalyxAubergine,
        KalyxTheme::kalyxRoyalBlue, KalyxTheme::kalyxRoyalBlue,
        KalyxTheme::kalyxAubergine, KalyxTheme::kalyxRoyalBlue
    };
    for (int i = 0; i < 6; ++i)
        if (knobs_[static_cast<std::size_t>(i)])
            knobs_[static_cast<std::size_t>(i)]->setThemeColours(accents[i], capCol, isDay);
    repaint();
}

void HarmonicSculptorUI::updateDependentStates()
{
    const auto p = processor_.getSlotParamsForGUI(slotIndex_);
    const bool modOn = (p[0] > 0.5f);
    tiltLinkBtn_.setEnabled(modOn);
    if (bandPill_)        bandPill_->setEnabled(modOn);
    if (routingSelector_) routingSelector_->setEnabled(modOn);
    for (auto& k : knobs_)
        if (k) k->setDimmedDisabled(!modOn);
}

void HarmonicSculptorUI::syncFromAPVTS()
{
    updateDependentStates();
    repaint();
}

void HarmonicSculptorUI::updateTelemetryAndRepaint()
{
    updateDependentStates();
    const float nextThd = processor_.getSlotThdPct(slotIndex_);
    if (std::abs(nextThd - thdPct_) > 0.05f)
    {
        thdPct_ = nextThd;
        if (!thdBarRect_.isEmpty())
            repaint(thdBarRect_.expanded(2.0f).toNearestInt());
    }
}

void HarmonicSculptorUI::paint(juce::Graphics& g)
{
    if (thdBarRect_.isEmpty()) return;
    const bool isDay = (theme_ == kalyx::core::ChassisTheme::Day);
    const auto sc = uiScale_;

    g.setFont(juce::Font(juce::FontOptions(9.5f * sc, juce::Font::bold)));
    g.setColour(KalyxTheme::getTextSecondary(theme_));
    g.drawText("THD:", static_cast<int>(thdBarRect_.getX()), static_cast<int>(thdBarRect_.getY()),
               static_cast<int>(32.0f * sc), static_cast<int>(thdBarRect_.getHeight()),
               juce::Justification::centredLeft);

    const float startX = thdBarRect_.getX() + 34.0f * sc;
    const float ledW = 14.0f * sc;
    const float ledH = 8.0f * sc;
    const float ledY = thdBarRect_.getCentreY() - ledH * 0.5f;
    static constexpr float kThresh[5] = { 0.3f, 1.0f, 2.5f, 5.0f, 8.5f };

    for (int i = 0; i < 5; ++i)
    {
        const float lx = startX + static_cast<float>(i) * (ledW + 3.0f * sc);
        const bool lit = (thdPct_ >= kThresh[i]);
        juce::Colour c = isDay ? juce::Colour(0xffcbd2df) : juce::Colour(0xff2a303d);
        if (lit)
            c = (i >= 4) ? KalyxTheme::distressorRed : ((i >= 2) ? KalyxTheme::amberLed : KalyxTheme::kalyxAubergine);

        g.setColour(c);
        g.fillRoundedRectangle(lx, ledY, ledW, ledH, 2.0f * sc);
    }

    g.setColour(KalyxTheme::getTextPrimary(theme_));
    g.drawText(juce::String(thdPct_, 1) + " %",
               static_cast<int>(startX + 5.0f * (ledW + 3.0f * sc) + 4.0f * sc),
               static_cast<int>(thdBarRect_.getY()),
               static_cast<int>(46.0f * sc),
               static_cast<int>(thdBarRect_.getHeight()),
               juce::Justification::centredLeft);
}

void HarmonicSculptorUI::resized()
{
    const auto b = getLocalBounds().toFloat().reduced(4.0f * uiScale_, 2.0f * uiScale_);
    if (b.isEmpty()) return;

    const float hdrH = 22.0f * uiScale_;
    const float halfW = (b.getWidth() - 4.0f * uiScale_) * 0.5f;
    inBtn_.setBounds(static_cast<int>(b.getX()), static_cast<int>(b.getY()),
                     static_cast<int>(halfW), static_cast<int>(hdrH));
    tiltLinkBtn_.setBounds(static_cast<int>(b.getX() + halfW + 4.0f * uiScale_), static_cast<int>(b.getY()),
                           static_cast<int>(halfW), static_cast<int>(hdrH));

    const float thdH = 18.0f * uiScale_;
    thdBarRect_ = { b.getX(), b.getY() + hdrH + 3.0f * uiScale_, b.getWidth(), thdH };

    const float footH = 22.0f * uiScale_;
    const float routH = 22.0f * uiScale_;
    const float footY = b.getBottom() - routH;
    const float bandY = footY - footH - 4.0f * uiScale_;

    if (bandPill_)
        bandPill_->setBounds(static_cast<int>(b.getX()), static_cast<int>(bandY),
                             static_cast<int>(b.getWidth()), static_cast<int>(footH));
    if (routingSelector_)
        routingSelector_->setBounds(static_cast<int>(b.getX()), static_cast<int>(footY),
                                    static_cast<int>(b.getWidth()), static_cast<int>(routH));

    const float gridTop = thdBarRect_.getBottom() + 4.0f * uiScale_;
    const float gridH   = std::max(40.0f, bandY - 4.0f * uiScale_ - gridTop);
    const float rowH    = gridH / 3.0f;
    const float colW    = b.getWidth() * 0.5f;

    for (int r = 0; r < 3; ++r)
    {
        for (int c = 0; c < 2; ++c)
        {
            const int idx = r * 2 + c;
            if (knobs_[static_cast<std::size_t>(idx)])
                knobs_[static_cast<std::size_t>(idx)]->setBoundsTight(
                    b.getX() + static_cast<float>(c) * colW,
                    gridTop + static_cast<float>(r) * rowH,
                    colW, rowH);
        }
    }
}

} // namespace kalyx::consoles
