#include "SurgicalEQ2UUI.h"
#include "../../Core/ConsoleRegistry.h"
#include <algorithm>
#include <cmath>

namespace kalyx::consoles
{

SurgicalEQ2UUI::SurgicalEQ2UUI(KalyxHybridConsoleProcessor& proc, int hostSlotIndex, int partnerSlotIndex)
    : processor_(proc), hostSlotIndex_(hostSlotIndex), partnerSlotIndex_(partnerSlotIndex)
{
    using namespace kalyx::core;
    smoothedBinsDb_.fill(-96.0f);
    preSmoothedBinsDb_.fill(-96.0f);

    auto& apvts = processor_.getAPVTS();
    const bool darkTxt = (theme_ == ChassisTheme::Day);
    const auto royal   = KalyxTheme::kalyxRoyalBlueGlow;
    const auto aub     = KalyxTheme::kalyxAubergineHover;

    setupKalyxToggle(*this, inBtn_, inAtt_, apvts, get2UParamId(0),
                     "SURGICAL EQ 2U", royal, darkTxt,
                     "Engage or bypass Kalyx Surgical EQ 2U (SplitEQ + Dynamic EQ)");

    phasePill_ = std::make_unique<KalyxSegmentedPill>(
        apvts, get2UParamId(1), "",
        juce::StringArray { "ZERO LAT", "NATURAL" }, royal, darkTxt,
        "Phase Mode: ZERO LAT (Minimum Phase) or NATURAL (Analog De-Cramped Phase)");
    addAndMakeVisible(*phasePill_);

    for (int b = 0; b < 6; ++b)
    {
        auto& btn = bandBtns_[static_cast<std::size_t>(b)];
        btn.setButtonText(juce::String(b + 1));
        btn.setTooltip("Select Surgical EQ Band " + juce::String(b + 1));
        btn.onClick = [this, b]() { selectBand(b); };
        addAndMakeVisible(btn);
    }

    static constexpr const char* kShapeLbls[5] = { "BELL", "SHELF", "CUT", "NOTCH", "TILT" };
    for (int i = 0; i < 5; ++i)
    {
        auto& btn = shapeBtns_[static_cast<std::size_t>(i)];
        btn.setButtonText(kShapeLbls[i]);
        btn.setTooltip("Set active band filter shape (click again to toggle Low/High variant)");
        btn.onClick = [this, i]()
        {
            auto m = SurgicalEQ2UDSP::unpackMeta(processor_.getSurgicalBandMeta(hostSlotIndex_, selectedBand_));
            if (i == 0)      m.shape = static_cast<int>(SurgicalEQ2UDSP::BandShape::Bell);
            else if (i == 1) m.shape = (m.shape == 1) ? 2 : 1; // LowShelf <-> HighShelf
            else if (i == 2) m.shape = (m.shape == 3) ? 4 : 3; // LowCut <-> HighCut
            else if (i == 3) m.shape = (m.shape == 5) ? 6 : 5; // Notch <-> BandPass
            else             m.shape = (m.shape == 7) ? 8 : 7; // TiltShelf <-> FlatTilt
            setBandMeta(selectedBand_, m);
        };
        addAndMakeVisible(btn);
    }

    slopeLeftBtn_.onClick = [this]()
    {
        auto m = SurgicalEQ2UDSP::unpackMeta(processor_.getSurgicalBandMeta(hostSlotIndex_, selectedBand_));
        m.slope = std::clamp(m.slope - 1, 0, 6);
        setBandMeta(selectedBand_, m);
    };
    slopeRightBtn_.onClick = [this]()
    {
        auto m = SurgicalEQ2UDSP::unpackMeta(processor_.getSurgicalBandMeta(hostSlotIndex_, selectedBand_));
        m.slope = std::clamp(m.slope + 1, 0, 6);
        setBandMeta(selectedBand_, m);
    };
    addAndMakeVisible(slopeLeftBtn_);
    addAndMakeVisible(slopeRightBtn_);

    static constexpr const char* kStereoLbls[5] = { "STEREO", "MID", "SIDE", "L", "R" };
    for (int i = 0; i < 5; ++i)
    {
        auto& btn = stereoBtns_[static_cast<std::size_t>(i)];
        btn.setButtonText(kStereoLbls[i]);
        btn.setTooltip("Band Stereo Placement: STEREO, MID, SIDE, LEFT, or RIGHT");
        btn.onClick = [this, i]()
        {
            auto m = SurgicalEQ2UDSP::unpackMeta(processor_.getSurgicalBandMeta(hostSlotIndex_, selectedBand_));
            m.stereo = i;
            setBandMeta(selectedBand_, m);
        };
        addAndMakeVisible(btn);
    }

    soloBtn_.setTooltip("Solo Active Band Frequency (Headphone Monitor)");
    soloBtn_.onClick = [this]()
    {
        const auto p2u = processor_.getSlotParams2UForGUI(hostSlotIndex_);
        const int curSolo = mapChoice(p2u[3], 7);
        const int nextSolo = (curSolo == selectedBand_ + 1) ? 0 : (selectedBand_ + 1);
        set2UParamNorm(3, choiceToNorm(nextSolo, 7));
        updateMetaFromProcessor();
    };
    addAndMakeVisible(soloBtn_);

    rebindBandKnobs();
    updateMetaFromProcessor();
    syncFromAPVTS();
}

juce::String SurgicalEQ2UUI::get2UParamId(int paramZeroBased) const
{
    const int idx = std::clamp(paramZeroBased, 0, 35);
    if (idx < 18)
        return ParamIDs::slotParamId(hostSlotIndex_, idx + 1);
    return ParamIDs::slotParamId(partnerSlotIndex_, (idx - 18) + 1);
}

void SurgicalEQ2UUI::set2UParamNorm(int paramZeroBased, float normVal)
{
    if (auto* param = processor_.getAPVTS().getParameter(get2UParamId(paramZeroBased)))
        param->setValueNotifyingHost(std::clamp(normVal, 0.0f, 1.0f));
}

void SurgicalEQ2UUI::selectBand(int bandIdx)
{
    selectedBand_ = std::clamp(bandIdx, 0, 5);
    rebindBandKnobs();
    updateMetaFromProcessor();
    repaint();
}

void SurgicalEQ2UUI::rebindBandKnobs()
{
    using namespace kalyx::core;
    auto& apvts = processor_.getAPVTS();
    const auto defs2u = ModuleRegistry::getDefaultParams2UForModule(SlotModuleId::SurgicalEQ2U);
    const int base = 6 + selectedBand_ * 5;

    const bool darkTxt = (theme_ == ChassisTheme::Day);
    const auto capCol  = KalyxTheme::getControlIdleBg(theme_);
    const auto royal   = KalyxTheme::kalyxRoyalBlueGlow;
    const auto gold    = KalyxTheme::fairchildGold;
    const auto aub     = KalyxTheme::kalyxAubergineHover;

    auto fmtHz = [](double v) -> juce::String
    {
        const float f = mapLog(static_cast<float>(v), 20.0f, 20000.0f);
        if (f >= 1000.0f) return juce::String(f * 0.001f, 2) + " kHz";
        return juce::String(juce::roundToInt(f)) + " Hz";
    };
    auto fmtDb24 = [](double v) -> juce::String { return KalyxValueParsers::formatDbClean(mapLin(static_cast<float>(v), -24.0f, 24.0f)); };
    auto fmtDyn  = [](double v) -> juce::String { return KalyxValueParsers::formatDbClean(mapLin(static_cast<float>(v), -18.0f, 18.0f)); };
    auto fmtQ    = [](double v) -> juce::String { return "Q " + juce::String(mapLog(static_cast<float>(v), 0.2f, 18.0f), 2); };

    freqKnob_.init (*this, apvts, get2UParamId(base + 0), "FREQ",     royal, capCol, darkTxt, defs2u[static_cast<std::size_t>(base + 0)], fmtHz,
                    "Band Center/Cutoff Frequency (20 Hz - 20 kHz)", KalyxValueParsers::parseHzLog(20.0f, 20000.0f));
    tonalKnob_.init(*this, apvts, get2UParamId(base + 1), "TONAL GN", royal, capCol, darkTxt, defs2u[static_cast<std::size_t>(base + 1)], fmtDb24,
                    "Tonal Component Gain (-24 to +24 dB)", KalyxValueParsers::parseLinDb(-24.0f, 24.0f));
    transKnob_.init(*this, apvts, get2UParamId(base + 2), "TRANS GN", gold,  capCol, darkTxt, defs2u[static_cast<std::size_t>(base + 2)], fmtDb24,
                    "Transient Component Gain (-24 to +24 dB)", KalyxValueParsers::parseLinDb(-24.0f, 24.0f));
    qKnob_.init    (*this, apvts, get2UParamId(base + 3), "Q",        royal, capCol, darkTxt, defs2u[static_cast<std::size_t>(base + 3)], fmtQ,
                    "Filter Resonance / Bandwidth (Q 0.20 - 18.0)", KalyxValueParsers::parseQLog(0.2f, 18.0f));
    dynKnob_.init  (*this, apvts, get2UParamId(base + 4), "DYN",      aub,   capCol, darkTxt, defs2u[static_cast<std::size_t>(base + 4)], fmtDyn,
                    "Dynamic EQ Range (-18 to +18 dB)", KalyxValueParsers::parseLinDb(-18.0f, 18.0f));

    freqKnob_.setUiScale(uiScale_);
    tonalKnob_.setUiScale(uiScale_);
    transKnob_.setUiScale(uiScale_);
    qKnob_.setUiScale(uiScale_);
    dynKnob_.setUiScale(uiScale_);
    resized();
}

void SurgicalEQ2UUI::setBandMeta(int bandIdx, const SurgicalEQ2UDSP::BandMeta& m)
{
    processor_.setSurgicalBandMeta(hostSlotIndex_, bandIdx, SurgicalEQ2UDSP::packMeta(m));
    updateMetaFromProcessor();
    repaint();
}

void SurgicalEQ2UUI::updateMetaFromProcessor()
{
    const auto m = SurgicalEQ2UDSP::unpackMeta(processor_.getSurgicalBandMeta(hostSlotIndex_, selectedBand_));
    const auto p2u = processor_.getSlotParams2UForGUI(hostSlotIndex_);
    const int soloBand = kalyx::core::mapChoice(p2u[3], 7);
    const auto idleBg = KalyxTheme::getControlIdleBg(theme_);
    const auto txtCol = KalyxTheme::getTextPrimary(theme_);
    const auto royal  = KalyxTheme::kalyxRoyalBlueGlow;
    const auto aub    = KalyxTheme::kalyxAubergineHover;

    for (int b = 0; b < 6; ++b)
    {
        const auto bm = SurgicalEQ2UDSP::unpackMeta(processor_.getSurgicalBandMeta(hostSlotIndex_, b));
        const bool isSel = (b == selectedBand_);
        bandBtns_[static_cast<std::size_t>(b)].setColour(juce::TextButton::buttonColourId,
            isSel ? royal : (bm.enabled ? idleBg : idleBg.withAlpha(0.4f)));
        bandBtns_[static_cast<std::size_t>(b)].setColour(juce::TextButton::textColourOffId,
            isSel ? juce::Colours::white : txtCol.withAlpha(bm.enabled ? 0.9f : 0.4f));
    }

    int shapeGroup = 0;
    if (m.shape == 1 || m.shape == 2) shapeGroup = 1;
    else if (m.shape == 3 || m.shape == 4) shapeGroup = 2;
    else if (m.shape == 5 || m.shape == 6) shapeGroup = 3;
    else if (m.shape == 7 || m.shape == 8) shapeGroup = 4;

    for (int i = 0; i < 5; ++i)
    {
        const bool act = (i == shapeGroup);
        shapeBtns_[static_cast<std::size_t>(i)].setColour(juce::TextButton::buttonColourId, act ? royal : idleBg);
        shapeBtns_[static_cast<std::size_t>(i)].setColour(juce::TextButton::textColourOffId, act ? juce::Colours::white : txtCol);
    }

    for (int i = 0; i < 5; ++i)
    {
        const bool act = (i == m.stereo);
        stereoBtns_[static_cast<std::size_t>(i)].setColour(juce::TextButton::buttonColourId, act ? aub : idleBg);
        stereoBtns_[static_cast<std::size_t>(i)].setColour(juce::TextButton::textColourOffId, act ? juce::Colours::white : txtCol);
    }

    const bool isSolo = (soloBand == selectedBand_ + 1);
    soloBtn_.setColour(juce::TextButton::buttonColourId, isSolo ? KalyxTheme::amberLed : idleBg);
    soloBtn_.setColour(juce::TextButton::textColourOffId, isSolo ? KalyxTheme::dayTextPrimary : txtCol);
}

void SurgicalEQ2UUI::setUiScale(float s)
{
    uiScale_ = std::clamp(s, 0.75f, 2.20f);
    inBtn_.getProperties().set("kalyxScale", uiScale_);
    if (phasePill_) phasePill_->setUiScale(uiScale_);
    freqKnob_.setUiScale(uiScale_);
    tonalKnob_.setUiScale(uiScale_);
    transKnob_.setUiScale(uiScale_);
    qKnob_.setUiScale(uiScale_);
    dynKnob_.setUiScale(uiScale_);
    resized();
    repaint();
}

void SurgicalEQ2UUI::setChassisTheme(kalyx::core::ChassisTheme t)
{
    if (theme_ == t) return;
    theme_ = t;

    const bool darkTxt = (theme_ == kalyx::core::ChassisTheme::Day);
    const auto capCol  = KalyxTheme::getControlIdleBg(theme_);
    const auto royal   = KalyxTheme::kalyxRoyalBlueGlow;
    const auto gold    = KalyxTheme::fairchildGold;
    const auto aub     = KalyxTheme::kalyxAubergineHover;

    inBtn_.getProperties().set("kalyxDarkText", darkTxt);
    if (phasePill_) phasePill_->setThemeColours(royal, darkTxt);

    freqKnob_.setThemeColours (royal, capCol, darkTxt);
    tonalKnob_.setThemeColours(royal, capCol, darkTxt);
    transKnob_.setThemeColours(gold,  capCol, darkTxt);
    qKnob_.setThemeColours    (royal, capCol, darkTxt);
    dynKnob_.setThemeColours  (aub,   capCol, darkTxt);

    slopeLeftBtn_.setColour (juce::TextButton::buttonColourId, capCol);
    slopeRightBtn_.setColour(juce::TextButton::buttonColourId, capCol);
    slopeLeftBtn_.setColour (juce::TextButton::textColourOffId, KalyxTheme::getTextPrimary(theme_));
    slopeRightBtn_.setColour(juce::TextButton::textColourOffId, KalyxTheme::getTextPrimary(theme_));

    updateMetaFromProcessor();
    repaint();
}

void SurgicalEQ2UUI::syncFromAPVTS()
{
    const auto p2u = processor_.getSlotParams2UForGUI(hostSlotIndex_);
    const bool inNow = (p2u[0] > 0.5f);
    if (inNow != moduleIn_)
    {
        moduleIn_ = inNow;
        const float a = moduleIn_ ? 1.0f : 0.35f;
        freqKnob_.setDimmed(!moduleIn_);
        tonalKnob_.setDimmed(!moduleIn_);
        transKnob_.setDimmed(!moduleIn_);
        qKnob_.setDimmed(!moduleIn_);
        dynKnob_.setDimmed(!moduleIn_);
        if (phasePill_) phasePill_->setAlpha(a);
        repaint();
    }
}

void SurgicalEQ2UUI::updateFFTAndRepaint()
{
    syncFromAPVTS();
    constexpr int numBins = KalyxHybridConsoleProcessor::kFFTSize / 2;

    if (processor_.pullNextPreFFTBlock(preFftData_.data()))
    {
        windowing_.multiplyWithWindowingTable(preFftData_.data(), KalyxHybridConsoleProcessor::kFFTSize);
        forwardFFT_.performFrequencyOnlyForwardTransform(preFftData_.data());
        for (int i = 1; i < numBins; ++i)
        {
            const float mag = preFftData_[static_cast<std::size_t>(i)] / static_cast<float>(numBins);
            const float db = std::clamp(20.0f * std::log10(std::max(mag, 1.0e-5f)), -96.0f, 6.0f);
            preSmoothedBinsDb_[static_cast<std::size_t>(i)] = 0.72f * preSmoothedBinsDb_[static_cast<std::size_t>(i)] + 0.28f * db;
        }
    }

    if (processor_.pullNextFFTBlock(fftData_.data()))
    {
        windowing_.multiplyWithWindowingTable(fftData_.data(), KalyxHybridConsoleProcessor::kFFTSize);
        forwardFFT_.performFrequencyOnlyForwardTransform(fftData_.data());
        for (int i = 1; i < numBins; ++i)
        {
            const float mag = fftData_[static_cast<std::size_t>(i)] / static_cast<float>(numBins);
            const float db = std::clamp(20.0f * std::log10(std::max(mag, 1.0e-5f)), -96.0f, 6.0f);
            smoothedBinsDb_[static_cast<std::size_t>(i)] = 0.72f * smoothedBinsDb_[static_cast<std::size_t>(i)] + 0.28f * db;
        }
    }
    repaint(graphRect_.toNearestInt());
}

float SurgicalEQ2UUI::freqToX(float hz) const noexcept
{
    const float norm = std::log(std::clamp(hz, 20.0f, 20000.0f) / 20.0f) / std::log(1000.0f);
    return graphRect_.getX() + norm * graphRect_.getWidth();
}

float SurgicalEQ2UUI::xToFreq(float x) const noexcept
{
    const float norm = std::clamp((x - graphRect_.getX()) / std::max(1.0f, graphRect_.getWidth()), 0.0f, 1.0f);
    return 20.0f * std::pow(1000.0f, norm);
}

float SurgicalEQ2UUI::gainToY(float db) const noexcept
{
    const float norm = std::clamp((db + 24.0f) / 48.0f, 0.0f, 1.0f);
    return graphRect_.getBottom() - norm * graphRect_.getHeight();
}

float SurgicalEQ2UUI::yToGain(float y) const noexcept
{
    const float norm = std::clamp((graphRect_.getBottom() - y) / std::max(1.0f, graphRect_.getHeight()), 0.0f, 1.0f);
    return -24.0f + norm * 48.0f;
}

void SurgicalEQ2UUI::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour(KalyxTheme::getStripBg(theme_));
    g.fillRect(b);

    // Liseré supérieur Bleu Roi (#2348B8)
    g.setColour(KalyxTheme::kalyxRoyalBlueGlow.withAlpha(moduleIn_ ? 0.92f : 0.35f));
    g.fillRect(b.getX(), b.getY(), b.getWidth(), std::max(2.0f, 2.5f * uiScale_));

    g.setColour(KalyxTheme::getGridBorder(theme_));
    if (!sepTopRect_.isEmpty()) g.fillRect(sepTopRect_);
    if (!sepBotRect_.isEmpty()) g.fillRect(sepBotRect_);

    // Graphe Analyseur FFT + Courbes Tonal (Bleu Roi) & Transient (Or)
    if (!graphRect_.isEmpty())
    {
        const bool isDay = (theme_ == kalyx::core::ChassisTheme::Day);
        g.setColour(isDay ? juce::Colour(0xffe9ecf2) : KalyxTheme::nightChassisBg);
        g.fillRoundedRectangle(graphRect_, 4.0f * uiScale_);
        g.setColour(KalyxTheme::getGridBorder(theme_));
        g.drawRoundedRectangle(graphRect_, 4.0f * uiScale_, 1.0f);

        // Grille fréquences (100Hz, 500Hz, 2kHz, 8kHz) & 0 dB
        static constexpr float kGridHz[5] = { 100.0f, 500.0f, 2000.0f, 8000.0f, 16000.0f };
        static constexpr const char* kGridLbl[5] = { "100", "500", "2k", "8k", "16k" };
        g.setFont(KalyxTheme::makeMonoFont(8.5f * uiScale_, juce::Font::plain));
        for (int i = 0; i < 5; ++i)
        {
            const float gx = freqToX(kGridHz[i]);
            g.setColour(KalyxTheme::getGridBorder(theme_).withAlpha(0.55f));
            g.drawVerticalLine(static_cast<int>(gx), graphRect_.getY() + 2.0f, graphRect_.getBottom() - 2.0f);
            g.setColour(KalyxTheme::getTextSecondary(theme_));
            g.drawText(kGridLbl[i], static_cast<int>(gx + 2.0f), static_cast<int>(graphRect_.getBottom() - 12.0f * uiScale_),
                       28, static_cast<int>(11.0f * uiScale_), juce::Justification::centredLeft);
        }
        const float y0 = gainToY(0.0f);
        g.setColour(KalyxTheme::getGridBorder(theme_).brighter(0.2f));
        g.drawHorizontalLine(static_cast<int>(y0), graphRect_.getX() + 2.0f, graphRect_.getRight() - 2.0f);

        // Spectre FFT Haute Résolution (PRE rempli 30% + POST ligne continue 1.5f)
        {
            juce::Path preFillPath, postLinePath;
            const double fs = processor_.getCurrentSampleRate();
            constexpr int numBins = KalyxHybridConsoleProcessor::kFFTSize / 2;
            const int pts = 96;
            for (int i = 0; i <= pts; ++i)
            {
                const float hz = 20.0f * std::pow(1000.0f, static_cast<float>(i) / static_cast<float>(pts));
                const int bin = std::clamp(static_cast<int>(std::round(hz * KalyxHybridConsoleProcessor::kFFTSize / fs)), 1, numBins - 1);

                const float preDb = preSmoothedBinsDb_[static_cast<std::size_t>(bin)];
                const float preY  = graphRect_.getBottom() - std::clamp((preDb + 90.0f) / 90.0f, 0.0f, 1.0f) * graphRect_.getHeight();

                const float postDb = smoothedBinsDb_[static_cast<std::size_t>(bin)];
                const float postY  = graphRect_.getBottom() - std::clamp((postDb + 90.0f) / 90.0f, 0.0f, 1.0f) * graphRect_.getHeight();

                const float px = graphRect_.getX() + (static_cast<float>(i) / static_cast<float>(pts)) * graphRect_.getWidth();
                if (i == 0)
                {
                    preFillPath.startNewSubPath(px, graphRect_.getBottom());
                    preFillPath.lineTo(px, preY);
                    postLinePath.startNewSubPath(px, postY);
                }
                else
                {
                    preFillPath.lineTo(px, preY);
                    postLinePath.lineTo(px, postY);
                }
            }
            preFillPath.lineTo(graphRect_.getRight(), graphRect_.getBottom());
            preFillPath.closeSubPath();

            const auto preFillCol = isDay ? juce::Colour(0xff5b6e8f).withAlpha(0.30f)
                                          : juce::Colour(0xff3e5070).withAlpha(0.30f);
            g.setColour(preFillCol);
            g.fillPath(preFillPath);

            const auto postLineCol = isDay ? KalyxTheme::kalyxRoyalBlue.withAlpha(0.90f)
                                           : juce::Colour(0xff5c8cff).withAlpha(0.90f);
            g.setColour(postLineCol);
            g.strokePath(postLinePath, juce::PathStrokeType(1.5f));
        }

        // Courbes Tonal (Bleu Roi #3B66E0) & Transient (Or #E0A82E)
        const auto p2u = processor_.getSlotParams2UForGUI(hostSlotIndex_);
        std::array<uint32_t, 6> meta {};
        for (int bIdx = 0; bIdx < 6; ++bIdx)
            meta[static_cast<std::size_t>(bIdx)] = processor_.getSurgicalBandMeta(hostSlotIndex_, bIdx);

        const double fs = processor_.getCurrentSampleRate();
        juce::Path tonalPath, transPath;
        constexpr int kCurvePts = 120;
        for (int i = 0; i <= kCurvePts; ++i)
        {
            const float hz = 20.0f * std::pow(1000.0f, static_cast<float>(i) / static_cast<float>(kCurvePts));
            const float tonDb = static_cast<float>(SurgicalEQ2UDSP::computeCombinedMagnitudeDb(hz, fs, p2u, meta, false));
            const float trnDb = static_cast<float>(SurgicalEQ2UDSP::computeCombinedMagnitudeDb(hz, fs, p2u, meta, true));
            const float px = freqToX(hz);
            const float pyTon = gainToY(tonDb);
            const float pyTrn = gainToY(trnDb);
            if (i == 0) { tonalPath.startNewSubPath(px, pyTon); transPath.startNewSubPath(px, pyTrn); }
            else        { tonalPath.lineTo(px, pyTon);          transPath.lineTo(px, pyTrn); }
        }
        const float alpha = moduleIn_ ? 1.0f : 0.35f;
        g.setColour(KalyxTheme::fairchildGold.withAlpha(0.85f * alpha));
        g.strokePath(transPath, juce::PathStrokeType(1.4f * uiScale_));
        g.setColour(KalyxTheme::kalyxRoyalBlueGlow.withAlpha(0.95f * alpha));
        g.strokePath(tonalPath, juce::PathStrokeType(2.0f * uiScale_));

        // 6 Noeuds interactifs (1)..(6)
        g.setFont(KalyxTheme::makeMonoFont(9.5f * uiScale_, juce::Font::bold));
        for (int bIdx = 0; bIdx < 6; ++bIdx)
        {
            const auto bm = SurgicalEQ2UDSP::unpackMeta(meta[static_cast<std::size_t>(bIdx)]);
            const std::size_t base = static_cast<std::size_t>(6 + bIdx * 5);
            const float fc = kalyx::core::mapLog(p2u[base + 0], 20.0f, 20000.0f);
            const float gn = kalyx::core::mapLin(p2u[base + 1], -24.0f, 24.0f);
            const float nx = freqToX(fc);
            const float ny = gainToY(gn);
            const bool isSel = (bIdx == selectedBand_);
            const float r = (isSel ? 8.0f : 6.0f) * uiScale_;
            const juce::Colour nodeCol = isSel ? KalyxTheme::kalyxRoyalBlueGlow : KalyxTheme::kalyxAubergineHover;

            g.setColour((bm.enabled ? nodeCol : KalyxTheme::getTextSecondary(theme_)).withMultipliedAlpha(alpha));
            g.fillEllipse(nx - r, ny - r, r * 2.0f, r * 2.0f);
            g.setColour(juce::Colours::white.withAlpha(isSel ? 0.95f : 0.65f));
            g.drawEllipse(nx - r, ny - r, r * 2.0f, r * 2.0f, isSel ? 1.5f : 1.0f);
            g.drawText(juce::String(bIdx + 1), static_cast<int>(nx - r), static_cast<int>(ny - r),
                       static_cast<int>(r * 2.0f), static_cast<int>(r * 2.0f), juce::Justification::centred);
        }
    }

    // Libellé pente active (SLOPE: 24 dB/oct)
    if (!slopeLabelRect_.isEmpty())
    {
        static constexpr const char* kSlopeNames[7] = { "6 dB", "12 dB", "18 dB", "24 dB", "36 dB", "48 dB", "96 dB" };
        const auto m = SurgicalEQ2UDSP::unpackMeta(processor_.getSurgicalBandMeta(hostSlotIndex_, selectedBand_));
        g.setColour(KalyxTheme::getTextPrimary(theme_));
        g.setFont(KalyxTheme::makeMonoFont(9.5f * uiScale_, juce::Font::bold));
        g.drawText(kSlopeNames[std::clamp(m.slope, 0, 6)], slopeLabelRect_.toNearestInt(), juce::Justification::centred);
    }
}

void SurgicalEQ2UUI::resized()
{
    auto r = getLocalBounds().reduced(static_cast<int>(8.0f * uiScale_), static_cast<int>(6.0f * uiScale_));
    const int s = static_cast<int>(4.0f * uiScale_);

    // Top bar
    const int topH = std::max(20, static_cast<int>(23.0f * uiScale_));
    auto topRow = r.removeFromTop(topH);
    inBtn_.setBounds(topRow.removeFromLeft(static_cast<int>(130.0f * uiScale_)));
    topRow.removeFromLeft(s);
    if (phasePill_)
        phasePill_->setBounds(topRow.removeFromRight(static_cast<int>(158.0f * uiScale_)));

    r.removeFromTop(s);
    sepTopRect_ = r.removeFromTop(1).toFloat();
    r.removeFromTop(s);

    // Bottom section reserved for band buttons + 5 knobs + shape/slope/stereo/solo
    const int botH = std::max(140, static_cast<int>(r.getHeight() * 0.44f));
    auto bot = r.removeFromBottom(botH);
    graphRect_ = r.toFloat();

    sepBotRect_ = bot.removeFromTop(1).toFloat();
    bot.removeFromTop(s);

    // Band selector row [1][2][3][4][5][6] + SOLO
    const int rowH = std::max(18, static_cast<int>(20.0f * uiScale_));
    auto bandRow = bot.removeFromTop(rowH);
    const int bW = static_cast<int>(28.0f * uiScale_);
    for (int b = 0; b < 6; ++b)
    {
        bandBtns_[static_cast<std::size_t>(b)].setBounds(bandRow.removeFromLeft(bW));
        bandRow.removeFromLeft(static_cast<int>(3.0f * uiScale_));
    }
    soloBtn_.setBounds(bandRow.removeFromRight(static_cast<int>(92.0f * uiScale_)));
    bot.removeFromTop(s);

    // Shape + Slope + Stereo rows at bottom
    auto foot2 = bot.removeFromBottom(rowH);
    bot.removeFromBottom(s);
    auto foot1 = bot.removeFromBottom(rowH);
    bot.removeFromBottom(s);

    // Foot 1 : SHAPE (5 buttons) + SLOPE (< 24 dB >)
    const int shW = static_cast<int>(46.0f * uiScale_);
    for (int i = 0; i < 5; ++i)
    {
        shapeBtns_[static_cast<std::size_t>(i)].setBounds(foot1.removeFromLeft(shW));
        foot1.removeFromLeft(static_cast<int>(2.0f * uiScale_));
    }
    const int arrowW = static_cast<int>(20.0f * uiScale_);
    slopeRightBtn_.setBounds(foot1.removeFromRight(arrowW));
    slopeLabelRect_ = foot1.removeFromRight(static_cast<int>(54.0f * uiScale_)).toFloat();
    slopeLeftBtn_.setBounds(foot1.removeFromRight(arrowW));

    // Foot 2 : STEREO (5 buttons)
    const int stW = foot2.getWidth() / 5;
    for (int i = 0; i < 5; ++i)
        stereoBtns_[static_cast<std::size_t>(i)].setBounds(foot2.removeFromLeft(stW).reduced(1, 0));

    // 5 Knobs in remaining center of bot
    const int kW = bot.getWidth() / 5;
    auto place = [](KalyxRotaryControl& k, juce::Rectangle<int> area)
    {
        k.setBoundsTight(static_cast<float>(area.getX()), static_cast<float>(area.getY()),
                         static_cast<float>(area.getWidth()), static_cast<float>(area.getHeight()), 58.0f);
    };
    place(freqKnob_,  bot.removeFromLeft(kW));
    place(tonalKnob_, bot.removeFromLeft(kW));
    place(transKnob_, bot.removeFromLeft(kW));
    place(qKnob_,     bot.removeFromLeft(kW));
    place(dynKnob_,   bot);
}

void SurgicalEQ2UUI::mouseDown(const juce::MouseEvent& e)
{
    if (!graphRect_.contains(e.position))
        return;

    const auto p2u = processor_.getSlotParams2UForGUI(hostSlotIndex_);
    int hitNode = -1;
    float bestDist = 16.0f * uiScale_;
    for (int b = 0; b < 6; ++b)
    {
        const std::size_t base = static_cast<std::size_t>(6 + b * 5);
        const float nx = freqToX(kalyx::core::mapLog(p2u[base + 0], 20.0f, 20000.0f));
        const float ny = gainToY(kalyx::core::mapLin(p2u[base + 1], -24.0f, 24.0f));
        const float d = e.position.getDistanceFrom({ nx, ny });
        if (d < bestDist) { bestDist = d; hitNode = b; }
    }

    if (hitNode >= 0)
    {
        if (hitNode != selectedBand_)
            selectBand(hitNode);
        if (e.mods.isPopupMenu())
        {
            showBandContextMenu(hitNode, e.getScreenPosition());
            return;
        }
        draggedNode_ = hitNode;
        dragStartQ_ = kalyx::core::mapLog(p2u[static_cast<std::size_t>(6 + hitNode * 5 + 3)], 0.2f, 18.0f);
        dragStartY_ = e.position.y;
    }
    else if (e.mods.isPopupMenu())
    {
        showBandContextMenu(selectedBand_, e.getScreenPosition());
    }
}

void SurgicalEQ2UUI::mouseDrag(const juce::MouseEvent& e)
{
    if (draggedNode_ < 0)
        return;

    const int base = 6 + draggedNode_ * 5;
    if (e.mods.isShiftDown())
    {
        const float dy = (dragStartY_ - e.position.y) * 0.015f;
        const float newQ = std::clamp(dragStartQ_ * std::exp(dy), 0.2f, 18.0f);
        set2UParamNorm(base + 3, kalyx::core::unmapLog(newQ, 0.2f, 18.0f));
    }
    else
    {
        const float newHz = xToFreq(e.position.x);
        const float newGn = yToGain(e.position.y);
        set2UParamNorm(base + 0, kalyx::core::unmapLog(newHz, 20.0f, 20000.0f));
        set2UParamNorm(base + 1, kalyx::core::unmapLin(newGn, -24.0f, 24.0f));
    }
    repaint(graphRect_.toNearestInt());
}

void SurgicalEQ2UUI::mouseUp(const juce::MouseEvent&)
{
    draggedNode_ = -1;
}

void SurgicalEQ2UUI::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (!graphRect_.contains(e.position))
        return;

    const int base = 6 + selectedBand_ * 5;
    const float hz = xToFreq(e.position.x);
    const float gn = yToGain(e.position.y);
    auto m = SurgicalEQ2UDSP::unpackMeta(processor_.getSurgicalBandMeta(hostSlotIndex_, selectedBand_));
    m.enabled = true;
    setBandMeta(selectedBand_, m);
    set2UParamNorm(base + 0, kalyx::core::unmapLog(hz, 20.0f, 20000.0f));
    set2UParamNorm(base + 1, kalyx::core::unmapLin(gn, -24.0f, 24.0f));
    repaint(graphRect_.toNearestInt());
}

void SurgicalEQ2UUI::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (!graphRect_.contains(e.position))
        return;

    const auto p2u = processor_.getSlotParams2UForGUI(hostSlotIndex_);
    const int base = 6 + selectedBand_ * 5;
    const float curQ = kalyx::core::mapLog(p2u[static_cast<std::size_t>(base + 3)], 0.2f, 18.0f);
    const float nextQ = std::clamp(curQ * std::exp(wheel.deltaY * 0.45f), 0.2f, 18.0f);
    set2UParamNorm(base + 3, kalyx::core::unmapLog(nextQ, 0.2f, 18.0f));
    repaint(graphRect_.toNearestInt());
}

void SurgicalEQ2UUI::showBandContextMenu(int bandIdx, juce::Point<int> screenPos)
{
    auto m = SurgicalEQ2UDSP::unpackMeta(processor_.getSurgicalBandMeta(hostSlotIndex_, bandIdx));
    juce::PopupMenu menu;
    menu.addItem(1, m.enabled ? "Bypass Band " + juce::String(bandIdx + 1) : "Enable Band " + juce::String(bandIdx + 1));
    menu.addItem(2, "Reset Band Gains to 0.0 dB");
    menu.addSeparator();
    static constexpr const char* kShapes[9] = { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch", "Band Pass", "Tilt Shelf", "Flat Tilt" };
    for (int i = 0; i < 9; ++i)
        menu.addItem(10 + i, kShapes[i], true, m.shape == i);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({ screenPos.x, screenPos.y, 1, 1 }),
                       [this, bandIdx, m](int res) mutable
                       {
                           if (res == 1) { m.enabled = !m.enabled; setBandMeta(bandIdx, m); }
                           else if (res == 2)
                           {
                               const int base = 6 + bandIdx * 5;
                               set2UParamNorm(base + 1, 0.5f);
                               set2UParamNorm(base + 2, 0.5f);
                               set2UParamNorm(base + 4, 0.5f);
                           }
                           else if (res >= 10 && res < 19)
                           {
                               m.shape = res - 10;
                               setBandMeta(bandIdx, m);
                           }
                       });
}

} // namespace kalyx::consoles
