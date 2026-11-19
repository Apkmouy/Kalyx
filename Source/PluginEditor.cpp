#include "PluginEditor.h"
#include "DSP/BiquadFilter.h"
#include <cmath>
#include <algorithm>
#include <cstring>

// =============================================================================
// SSLConsoleLookAndFeel - Vectoriel Plat HiDPI (Night Mode & Day Mode #12151C)
// =============================================================================
SSLConsoleLookAndFeel::SSLConsoleLookAndFeel()
{
    setDayMode(false);
}

void SSLConsoleLookAndFeel::setDayMode(bool isDayMode) noexcept
{
    dayMode_ = isDayMode;
    const juce::Colour txtCol = KalyxTheme::getTextPrimary(dayMode_);

    setColour(juce::Slider::textBoxTextColourId,       txtCol);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId,               txtCol);

    setColour(juce::ComboBox::backgroundColourId,      KalyxTheme::getControlIdleBg(dayMode_));
    setColour(juce::ComboBox::textColourId,            txtCol);
    setColour(juce::ComboBox::outlineColourId,         KalyxTheme::getEncoderOutline(dayMode_));
    setColour(juce::ComboBox::arrowColourId,           txtCol);

    setColour(juce::PopupMenu::backgroundColourId,     KalyxTheme::getStripBg(dayMode_));
    setColour(juce::PopupMenu::textColourId,           txtCol);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, KalyxTheme::royalBlueBtn);
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

juce::Label* SSLConsoleLookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* l = juce::LookAndFeel_V4::createSliderTextBox(slider);
    l->setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.5f, juce::Font::bold)));
    l->setJustificationType(juce::Justification::centred);
    const juce::Colour txtCol = KalyxTheme::getTextPrimary(dayMode_);
    l->setColour(juce::Label::textColourId,            txtCol);
    l->setColour(juce::Label::backgroundColourId,      juce::Colours::transparentBlack);
    l->setColour(juce::Label::outlineColourId,         juce::Colours::transparentBlack);
    l->setColour(juce::TextEditor::textColourId,       txtCol);
    l->setColour(juce::TextEditor::backgroundColourId, KalyxTheme::getControlIdleBg(dayMode_));
    l->setColour(juce::TextEditor::outlineColourId,    KalyxTheme::getEncoderOutline(dayMode_));
    return l;
}

void SSLConsoleLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                             int x, int y, int width, int height,
                                             float sliderPosProportional,
                                             float rotaryStartAngle,
                                             float rotaryEndAngle,
                                             juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                               static_cast<float>(width), static_cast<float>(height)).reduced(1.0f);
    const float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    if (radius < 6.0f)
        return;

    const bool isEnabled = slider.isEnabled();
    const float alphaMul = isEnabled ? 1.0f : 0.38f;
    const auto centre = bounds.getCentre();
    const float angle = isEnabled
                            ? (rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle))
                            : rotaryEndAngle;
    const juce::Colour arcColour = slider.findColour(juce::Slider::rotarySliderFillColourId).withMultipliedAlpha(alphaMul);

    constexpr int numTicks = 11;
    const float tickOuterR = radius * 0.98f;
    for (int i = 0; i < numTicks; ++i)
    {
        const float prop = static_cast<float>(i) / static_cast<float>(numTicks - 1);
        const float tickAngle = rotaryStartAngle + prop * (rotaryEndAngle - rotaryStartAngle);
        const bool isCenterTick = (i == numTicks / 2);
        const float tickInnerR = isCenterTick ? (radius * 0.84f) : (radius * 0.90f);

        const float sinA = std::sin(tickAngle);
        const float cosA = -std::cos(tickAngle);

        g.setColour((isCenterTick ? KalyxTheme::getTextPrimary(dayMode_) : KalyxTheme::getEncoderOutline(dayMode_))
                        .withMultipliedAlpha(alphaMul));
        g.drawLine(centre.x + tickInnerR * sinA, centre.y + tickInnerR * cosA,
                   centre.x + tickOuterR * sinA, centre.y + tickOuterR * cosA,
                   isCenterTick ? 1.5f : 1.0f);
    }

    const float arcRadius = radius * 0.78f;
    juce::Path trackArc;
    trackArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(KalyxTheme::getGridBorder(dayMode_).withMultipliedAlpha(alphaMul));
    g.strokePath(trackArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    const bool isBipolar = (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0 &&
                            std::abs(slider.getMinimum() + slider.getMaximum()) < 1.0);
    const float startValAngle = isBipolar
        ? (rotaryStartAngle + 0.5f * (rotaryEndAngle - rotaryStartAngle))
        : rotaryStartAngle;

    if (std::abs(angle - startValAngle) > 0.005f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                               std::min(startValAngle, angle), std::max(startValAngle, angle), true);
        g.setColour(arcColour);
        g.strokePath(valueArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    }

    const float bodyRadius = radius * 0.62f;
    const juce::Rectangle<float> bodyRect(centre.x - bodyRadius, centre.y - bodyRadius,
                                          bodyRadius * 2.0f, bodyRadius * 2.0f);
    g.setColour(KalyxTheme::getControlIdleBg(dayMode_).withMultipliedAlpha(alphaMul));
    g.fillEllipse(bodyRect);

    g.setColour(KalyxTheme::getEncoderOutline(dayMode_).withMultipliedAlpha(alphaMul));
    g.drawEllipse(bodyRect, 1.0f);

    const float sinP = std::sin(angle);
    const float cosP = -std::cos(angle);
    const float innerPtr = bodyRadius * 0.22f;
    const float outerPtr = bodyRadius * 0.90f;

    g.setColour(KalyxTheme::getTextPrimary(dayMode_).withMultipliedAlpha(alphaMul));
    g.drawLine(centre.x + innerPtr * sinP, centre.y + innerPtr * cosP,
               centre.x + outerPtr * sinP, centre.y + outerPtr * cosP,
               1.8f);
}

void SSLConsoleLookAndFeel::drawToggleButton(juce::Graphics& g,
                                             juce::ToggleButton& button,
                                             bool shouldDrawButtonAsHighlighted,
                                             bool /*shouldDrawButtonAsDown*/)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    const bool isOn = button.getToggleState();

    const juce::Colour customCol = button.findColour(juce::ToggleButton::tickColourId);
    const juce::Colour activeBg  = customCol.isTransparent() ? KalyxTheme::royalBlueBtn : customCol;
    const juce::Colour idleBg    = KalyxTheme::getControlIdleBg(dayMode_);

    if (isOn)
        g.setColour(activeBg);
    else
        g.setColour(shouldDrawButtonAsHighlighted ? idleBg.brighter(0.08f) : idleBg);

    g.fillRoundedRectangle(bounds, 3.0f);

    g.setColour(isOn ? activeBg.brighter(0.28f) : KalyxTheme::getEncoderOutline(dayMode_));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

    const float dotSize = 5.0f;
    const juce::Rectangle<float> dotRect(bounds.getX() + 5.0f, bounds.getCentreY() - dotSize * 0.5f, dotSize, dotSize);
    g.setColour(isOn ? juce::Colours::white : KalyxTheme::getEncoderOutline(dayMode_));
    g.fillEllipse(dotRect);

    g.setColour(isOn ? juce::Colours::white : KalyxTheme::getTextPrimary(dayMode_));
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    const auto textArea = bounds.withTrimmedLeft(12.0f).withTrimmedRight(2.0f);
    g.drawFittedText(button.getButtonText(), textArea.toNearestInt(), juce::Justification::centred, 1);
}

void SSLConsoleLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                                 juce::Button& button,
                                                 const juce::Colour& backgroundColour,
                                                 bool shouldDrawButtonAsHighlighted,
                                                 bool /*shouldDrawButtonAsDown*/)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    const juce::Colour bg = shouldDrawButtonAsHighlighted ? backgroundColour.brighter(0.10f) : backgroundColour;
    g.setColour(bg);
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(KalyxTheme::getEncoderOutline(dayMode_));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
}

// =============================================================================
// SSLEQCurveComponent - Graphe EQ Interactif + Analyseur de Spectre FFT 2048 pts
// =============================================================================
SSLEQCurveComponent::SSLEQCurveComponent(KalyxSSLConsoleProcessor& proc) noexcept
    : processor_(proc),
      forwardFFT_(KalyxSSLConsoleProcessor::kFFTOrder),
      windowing_(static_cast<std::size_t>(KalyxSSLConsoleProcessor::kFFTSize),
                 juce::dsp::WindowingFunction<float>::hann)
{
    fftData_.fill(0.0f);
    smoothedBinsDb_.fill(-100.0f);
}

void SSLEQCurveComponent::updateFFTFromProcessor() noexcept
{
    const int fftMode = static_cast<int>(std::round(
        processor_.getAPVTS().getRawParameterValue(ParamIDs::fftMode)->load(std::memory_order_relaxed)));

    if (fftMode == 2) // FFT OFF
        return;

    if (processor_.pullNextFFTBlock(fftData_.data()))
    {
        std::memset(fftData_.data() + KalyxSSLConsoleProcessor::kFFTSize, 0,
                    sizeof(float) * static_cast<std::size_t>(KalyxSSLConsoleProcessor::kFFTSize));
        windowing_.multiplyWithWindowingTable(fftData_.data(),
                                              static_cast<std::size_t>(KalyxSSLConsoleProcessor::kFFTSize));
        forwardFFT_.performFrequencyOnlyForwardTransform(fftData_.data());

        constexpr int numBins = KalyxSSLConsoleProcessor::kFFTSize / 2;
        constexpr float normFactor = 2.0f / static_cast<float>(KalyxSSLConsoleProcessor::kFFTSize);

        for (int b = 1; b < numBins; ++b)
        {
            const float mag = fftData_[static_cast<std::size_t>(b)] * normFactor;
            // Tilt visuel +3 dB/octave standard des analyseurs RTA de mixage
            const float freqRatio = static_cast<float>(b) / 32.0f;
            const float tiltDb = 3.0f * std::log2(std::max(freqRatio, 0.25f));
            const float db = std::clamp(20.0f * std::log10(std::max(mag, 1e-5f)) + tiltDb, -90.0f, 6.0f);
            const float prev = smoothedBinsDb_[static_cast<std::size_t>(b)];
            smoothedBinsDb_[static_cast<std::size_t>(b)] = (db > prev) ? (0.45f * prev + 0.55f * db)
                                                                       : (0.82f * prev + 0.18f * db);
        }
    }
    else
    {
        for (float& binDb : smoothedBinsDb_)
            binDb = std::max(-96.0f, binDb - 1.2f);
    }
}

juce::Rectangle<float> SSLEQCurveComponent::getPlotArea() const noexcept
{
    auto b = getLocalBounds().toFloat();
    return b.withTrimmedBottom(15.0f).reduced(8.0f, 5.0f);
}

float SSLEQCurveComponent::freqToX(float freqHz, const juce::Rectangle<float>& area) const noexcept
{
    const float clamped = std::clamp(freqHz, 20.0f, 20000.0f);
    const float normX = std::log10(clamped / 20.0f) / 3.0f;
    return area.getX() + normX * area.getWidth();
}

float SSLEQCurveComponent::xToFreq(float x, const juce::Rectangle<float>& area) const noexcept
{
    const float normX = std::clamp((x - area.getX()) / std::max(area.getWidth(), 1.0f), 0.0f, 1.0f);
    return 20.0f * std::pow(1000.0f, normX);
}

float SSLEQCurveComponent::gainToY(float gainDb, const juce::Rectangle<float>& area) const noexcept
{
    const float clamped = std::clamp(gainDb, -18.0f, 18.0f);
    const float normY = 0.5f - (clamped / 36.0f);
    return area.getY() + normY * area.getHeight();
}

float SSLEQCurveComponent::yToGain(float y, const juce::Rectangle<float>& area) const noexcept
{
    const float normY = std::clamp((y - area.getY()) / std::max(area.getHeight(), 1.0f), 0.0f, 1.0f);
    return (0.5f - normY) * 36.0f;
}

std::array<juce::Point<float>, 4> SSLEQCurveComponent::getBandNodePositions(const juce::Rectangle<float>& area) const noexcept
{
    const auto p = processor_.getCurrentParametersForGUI();
    return {{
        { freqToX(p.lfFreqHz,  area), gainToY(p.lfGainDb,  area) },
        { freqToX(p.lmfFreqHz, area), gainToY(p.lmfGainDb, area) },
        { freqToX(p.hmfFreqHz, area), gainToY(p.hmfGainDb, area) },
        { freqToX(p.hfFreqHz,  area), gainToY(p.hfGainDb,  area) }
    }};
}

int SSLEQCurveComponent::findNodeAtPosition(juce::Point<float> pos, const juce::Rectangle<float>& area) const noexcept
{
    const auto nodes = getBandNodePositions(area);
    int bestIdx = -1;
    float bestDistSq = 12.0f * 12.0f;

    for (int i = 0; i < 4; ++i)
    {
        const float dx = pos.x - nodes[static_cast<std::size_t>(i)].x;
        const float dy = pos.y - nodes[static_cast<std::size_t>(i)].y;
        const float d2 = dx * dx + dy * dy;
        if (d2 <= bestDistSq)
        {
            bestDistSq = d2;
            bestIdx = i;
        }
    }
    return bestIdx;
}

void SSLEQCurveComponent::setPlainParamValue(const char* paramID, float plainValue) noexcept
{
    if (auto* param = processor_.getAPVTS().getParameter(paramID))
        param->setValueNotifyingHost(param->convertTo0to1(plainValue));
}

void SSLEQCurveComponent::mouseMove(const juce::MouseEvent& e)
{
    const int idx = findNodeAtPosition(e.position, getPlotArea());
    if (idx != hoveredNodeIndex_)
    {
        hoveredNodeIndex_ = idx;
        setMouseCursor(hoveredNodeIndex_ >= 0 ? juce::MouseCursor::PointingHandCursor
                                              : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void SSLEQCurveComponent::mouseDown(const juce::MouseEvent& e)
{
    const auto area = getPlotArea();
    draggedNodeIndex_ = findNodeAtPosition(e.position, area);
    if (draggedNodeIndex_ >= 0)
    {
        const auto p = processor_.getCurrentParametersForGUI();
        dragStartQ_ = (draggedNodeIndex_ == 1) ? p.lmfQ : ((draggedNodeIndex_ == 2) ? p.hmfQ : 1.0f);
        dragStartY_ = e.position.y;
        repaint();
    }
}

void SSLEQCurveComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (draggedNodeIndex_ < 0 || processor_.isSectionLocked(3))
        return;

    const auto area = getPlotArea();

    if (e.mods.isShiftDown() && (draggedNodeIndex_ == 1 || draggedNodeIndex_ == 2))
    {
        const float dy = dragStartY_ - e.position.y;
        const float newQ = std::clamp(dragStartQ_ + dy * 0.025f, 0.40f, 4.00f);
        setPlainParamValue(draggedNodeIndex_ == 1 ? ParamIDs::lmfQ : ParamIDs::hmfQ, newQ);
        repaint();
        return;
    }

    const float rawFreq = xToFreq(e.position.x, area);
    const float newGain = std::clamp(yToGain(e.position.y, area), -15.0f, 15.0f);

    switch (draggedNodeIndex_)
    {
        case 0:
            setPlainParamValue(ParamIDs::lfFreq, std::clamp(rawFreq, 30.0f, 450.0f));
            setPlainParamValue(ParamIDs::lfGain, newGain);
            break;
        case 1:
            setPlainParamValue(ParamIDs::lmfFreq, std::clamp(rawFreq, 200.0f, 2500.0f));
            setPlainParamValue(ParamIDs::lmfGain, newGain);
            break;
        case 2:
            setPlainParamValue(ParamIDs::hmfFreq, std::clamp(rawFreq, 600.0f, 7000.0f));
            setPlainParamValue(ParamIDs::hmfGain, newGain);
            break;
        case 3:
            setPlainParamValue(ParamIDs::hfFreq, std::clamp(rawFreq, 1500.0f, 18000.0f));
            setPlainParamValue(ParamIDs::hfGain, newGain);
            break;
        default:
            break;
    }
    repaint();
}

void SSLEQCurveComponent::mouseUp(const juce::MouseEvent&)
{
    draggedNodeIndex_ = -1;
    repaint();
}

void SSLEQCurveComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (processor_.isSectionLocked(3))
        return;

    const int idx = findNodeAtPosition(e.position, getPlotArea());
    if (idx == 0)      setPlainParamValue(ParamIDs::lfGain,  0.0f);
    else if (idx == 1) setPlainParamValue(ParamIDs::lmfGain, 0.0f);
    else if (idx == 2) setPlainParamValue(ParamIDs::hmfGain, 0.0f);
    else if (idx == 3) setPlainParamValue(ParamIDs::hfGain,  0.0f);
    repaint();
}

void SSLEQCurveComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (processor_.isSectionLocked(3))
        return;

    const int idx = (draggedNodeIndex_ >= 0) ? draggedNodeIndex_ : findNodeAtPosition(e.position, getPlotArea());
    if (idx == 1 || idx == 2)
    {
        const auto p = processor_.getCurrentParametersForGUI();
        const float curQ = (idx == 1) ? p.lmfQ : p.hmfQ;
        const float nextQ = std::clamp(curQ + wheel.deltaY * 0.45f, 0.40f, 4.00f);
        setPlainParamValue(idx == 1 ? ParamIDs::lmfQ : ParamIDs::hmfQ, nextQ);
        repaint();
    }
}

void SSLEQCurveComponent::paint(juce::Graphics& g)
{
    const auto fullBounds = getLocalBounds().toFloat();
    const auto frameBounds = fullBounds.withTrimmedBottom(15.0f);
    const auto plotArea = getPlotArea();

    const juce::Colour plotBg = dayMode_ ? juce::Colour(0xff191d26) : KalyxTheme::nightChassisBg;
    g.setColour(plotBg);
    g.fillRoundedRectangle(frameBounds, 3.0f);

    g.setColour(KalyxTheme::getGridBorder(dayMode_));
    g.drawRoundedRectangle(frameBounds.reduced(0.5f), 3.0f, 1.0f);

    const float w = plotArea.getWidth();
    const float h = plotArea.getHeight();
    if (w <= 10.0f || h <= 10.0f)
        return;

    // 1. Grille logarithmique vectorielle (20 Hz .. 20 kHz, -18 dB .. +18 dB)
    g.setColour(KalyxTheme::nightGridBorder.withAlpha(0.70f));
    const float freqs[] = { 30.0f, 60.0f, 100.0f, 250.0f, 500.0f, 1000.0f, 2500.0f, 5000.0f, 10000.0f };
    for (float f : freqs)
    {
        const float x = freqToX(f, plotArea);
        g.drawVerticalLine(static_cast<int>(x), plotArea.getY(), plotArea.getBottom());
    }

    const float dbLines[] = { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f };
    for (float db : dbLines)
    {
        const float y = gainToY(db, plotArea);
        g.setColour(std::abs(db) < 0.1f ? KalyxTheme::nightEncoderOutline : KalyxTheme::nightGridBorder.withAlpha(0.70f));
        g.drawHorizontalLine(static_cast<int>(y), plotArea.getX(), plotArea.getRight());
    }

    const double fs = std::max(processor_.getCurrentSampleRate(), 44100.0);

    // 2. Analyseur de spectre FFT temps réel en arrière-plan discret et translucide (POST / PRE / OFF)
    const int fftMode = static_cast<int>(std::round(
        processor_.getAPVTS().getRawParameterValue(ParamIDs::fftMode)->load(std::memory_order_relaxed)));

    if (fftMode != 2)
    {
        juce::Path fftPath, fftFill;
        constexpr int numFftPoints = 110;
        constexpr int numBins = KalyxSSLConsoleProcessor::kFFTSize / 2;

        for (int i = 0; i <= numFftPoints; ++i)
        {
            const float normX = static_cast<float>(i) / static_cast<float>(numFftPoints);
            const float freq = 20.0f * std::pow(1000.0f, normX);
            const float binFloat = std::clamp(freq * static_cast<float>(KalyxSSLConsoleProcessor::kFFTSize) / static_cast<float>(fs),
                                              1.0f, static_cast<float>(numBins - 2));
            const int b0 = static_cast<int>(binFloat);
            const float frac = binFloat - static_cast<float>(b0);
            const float binDb = (1.0f - frac) * smoothedBinsDb_[static_cast<std::size_t>(b0)]
                              + frac * smoothedBinsDb_[static_cast<std::size_t>(b0 + 1)];

            // Échelle FFT : -72 dB (en bas du graphe) à 0 dB (en haut du graphe)
            const float fftNormY = 1.0f - std::clamp((binDb - (-72.0f)) / 72.0f, 0.0f, 1.0f);
            const float px = plotArea.getX() + normX * w;
            const float py = plotArea.getY() + fftNormY * h;

            if (i == 0)
            {
                fftPath.startNewSubPath(px, py);
                fftFill.startNewSubPath(px, plotArea.getBottom());
                fftFill.lineTo(px, py);
            }
            else
            {
                fftPath.lineTo(px, py);
                fftFill.lineTo(px, py);
            }
        }
        fftFill.lineTo(plotArea.getRight(), plotArea.getBottom());
        fftFill.closeSubPath();

        const juce::Colour fftCol = (fftMode == 0) ? KalyxTheme::aubergineArc : KalyxTheme::royalBlueArc;
        g.setColour(fftCol.withAlpha(0.14f));
        g.fillPath(fftFill);
        g.setColour(fftCol.withAlpha(0.38f));
        g.strokePath(fftPath, juce::PathStrokeType(1.0f));
    }

    // 3. Calcul des filtres individuels et de la réponse combinée (incluant creux gyrateur 4000 G)
    const auto p = processor_.getCurrentParametersForGUI();
    const auto snap = processor_.getMeterSnapshot();

    using Biquad = kalyx::dsp::BiquadFilter;
    using DSP    = kalyx::dsp::SSLChannelStripDSP;

    const float bellRefQ = p.circuit4000G ? 1.15f : 0.90f;
    const float shelfQ   = p.circuit4000G ? 0.82f : 0.68f;
    const float effLmfQ  = DSP::computeEffectiveEqQ(p.lmfQ, p.lmfGainDb, p.circuit4000G);
    const float effHmfQ  = DSP::computeEffectiveEqQ(p.hmfQ, p.hmfGainDb, p.circuit4000G);

    const auto hpfC = Biquad::makeHighPass(fs, p.hpfFreqHz, p.hpfQ);
    const auto lpfC = Biquad::makeLowPass(fs, p.lpfFreqHz, p.lpfQ);
    const auto lfC  = p.lfBell
                        ? Biquad::makePeakEQ(fs, p.lfFreqHz, p.lfGainDb, bellRefQ)
                        : Biquad::makeLowShelf(fs, p.lfFreqHz, p.lfGainDb, shelfQ);
    const auto lmfC = Biquad::makePeakEQ(fs, p.lmfFreqHz, p.lmfGainDb, effLmfQ);
    const auto hmfC = Biquad::makePeakEQ(fs, p.hmfFreqHz, p.hmfGainDb, effHmfQ);
    const auto lmfDipC = Biquad::makePeakEQ(fs, p.lmfFreqHz, -0.12f * p.lmfGainDb, std::max(0.35f, effLmfQ * 0.42f));
    const auto hmfDipC = Biquad::makePeakEQ(fs, p.hmfFreqHz, -0.12f * p.hmfGainDb, std::max(0.35f, effHmfQ * 0.42f));
    const auto hfC  = p.hfBell
                        ? Biquad::makePeakEQ(fs, p.hfFreqHz, p.hfGainDb, bellRefQ)
                        : Biquad::makeHighShelf(fs, p.hfFreqHz, p.hfGainDb, shelfQ);
    const auto deessC = Biquad::makePeakEQ(fs, p.deessFreqHz, -snap.deessGainRedDb, 2.0f);

    juce::Path totalPath, totalFill;
    juce::Path hpfPath, lpfPath;
    std::array<juce::Path, 4> bandPaths;
    constexpr int numPoints = 150;

    for (int i = 0; i <= numPoints; ++i)
    {
        const float normX = static_cast<float>(i) / static_cast<float>(numPoints);
        const double freq = 20.0 * std::pow(1000.0, static_cast<double>(normX));
        const float px = plotArea.getX() + normX * w;

        const float hpfDb = p.hpfEnabled ? Biquad::getMagnitudeDbAtFrequency(hpfC, freq, fs) : 0.0f;
        const float lpfDb = p.lpfEnabled ? Biquad::getMagnitudeDbAtFrequency(lpfC, freq, fs) : 0.0f;

        const float lfDb  = p.eqEnabled ? Biquad::getMagnitudeDbAtFrequency(lfC,  freq, fs) : 0.0f;
        float lmfDb       = p.eqEnabled ? Biquad::getMagnitudeDbAtFrequency(lmfC, freq, fs) : 0.0f;
        float hmfDb       = p.eqEnabled ? Biquad::getMagnitudeDbAtFrequency(hmfC, freq, fs) : 0.0f;
        if (p.eqEnabled && p.circuit4000G)
        {
            lmfDb += Biquad::getMagnitudeDbAtFrequency(lmfDipC, freq, fs);
            hmfDb += Biquad::getMagnitudeDbAtFrequency(hmfDipC, freq, fs);
        }
        const float hfDb  = p.eqEnabled ? Biquad::getMagnitudeDbAtFrequency(hfC,  freq, fs) : 0.0f;
        const float dsDb  = (p.deessEnabled && snap.deessGainRedDb > 0.05f)
                                ? Biquad::getMagnitudeDbAtFrequency(deessC, freq, fs)
                                : 0.0f;

        const float mainFilterDb = (!p.filtersToSc) ? (hpfDb + lpfDb) : 0.0f;
        const float totalDb = std::clamp(mainFilterDb + lfDb + lmfDb + hmfDb + hfDb + dsDb, -18.0f, 18.0f);
        const float py = gainToY(totalDb, plotArea);

        const float yLf  = gainToY(lfDb,  plotArea);
        const float yLmf = gainToY(lmfDb, plotArea);
        const float yHmf = gainToY(hmfDb, plotArea);
        const float yHf  = gainToY(hfDb,  plotArea);
        const float yHpf = gainToY(hpfDb, plotArea);
        const float yLpf = gainToY(lpfDb, plotArea);

        if (i == 0)
        {
            totalPath.startNewSubPath(px, py);
            totalFill.startNewSubPath(px, plotArea.getCentreY());
            totalFill.lineTo(px, py);

            bandPaths[0].startNewSubPath(px, yLf);
            bandPaths[1].startNewSubPath(px, yLmf);
            bandPaths[2].startNewSubPath(px, yHmf);
            bandPaths[3].startNewSubPath(px, yHf);
            hpfPath.startNewSubPath(px, yHpf);
            lpfPath.startNewSubPath(px, yLpf);
        }
        else
        {
            totalPath.lineTo(px, py);
            totalFill.lineTo(px, py);

            bandPaths[0].lineTo(px, yLf);
            bandPaths[1].lineTo(px, yLmf);
            bandPaths[2].lineTo(px, yHmf);
            bandPaths[3].lineTo(px, yHf);
            hpfPath.lineTo(px, yHpf);
            lpfPath.lineTo(px, yLpf);
        }
    }

    totalFill.lineTo(plotArea.getRight(), plotArea.getCentreY());
    totalFill.closeSubPath();

    const float dashLengths[2] = { 4.0f, 3.0f };
    if (p.hpfEnabled)
    {
        juce::Path dashedHpf;
        juce::PathStrokeType(1.3f).createDashedStroke(dashedHpf, hpfPath, dashLengths, 2);
        g.setColour(KalyxTheme::royalBlueArc.withAlpha(0.75f));
        g.fillPath(dashedHpf);
    }
    if (p.lpfEnabled)
    {
        juce::Path dashedLpf;
        juce::PathStrokeType(1.3f).createDashedStroke(dashedLpf, lpfPath, dashLengths, 2);
        g.setColour(KalyxTheme::royalBlueArc.withAlpha(0.75f));
        g.fillPath(dashedLpf);
    }

    const std::array<juce::Colour, 4> bandCols = {{
        KalyxTheme::eqLfWineRed,
        KalyxTheme::eqLmfSlateBlue,
        KalyxTheme::eqHmfJadeGreen,
        KalyxTheme::eqHfPearlSilver
    }};

    if (p.eqEnabled)
    {
        for (std::size_t b = 0; b < 4; ++b)
        {
            g.setColour(bandCols[b].withAlpha(0.82f));
            g.strokePath(bandPaths[b], juce::PathStrokeType(1.35f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    g.setColour(KalyxTheme::royalBlueArc.withAlpha(0.14f));
    g.fillPath(totalFill);
    g.setColour(KalyxTheme::nightTextPrimary);
    g.strokePath(totalPath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto nodes = getBandNodePositions(plotArea);
    const char* bandNames[4] = { "LF", "LMF", "HMF", "HF" };
    for (int i = 0; i < 4; ++i)
    {
        const auto pt = nodes[static_cast<std::size_t>(i)];
        const bool isActive = (i == draggedNodeIndex_ || i == hoveredNodeIndex_);
        const float r = isActive ? 6.5f : 5.0f;
        const juce::Colour col = bandCols[static_cast<std::size_t>(i)];

        if (isActive)
        {
            g.setColour(col.withAlpha(0.35f));
            g.fillEllipse(pt.x - r - 3.0f, pt.y - r - 3.0f, (r + 3.0f) * 2.0f, (r + 3.0f) * 2.0f);
        }

        g.setColour(col);
        g.fillEllipse(pt.x - r, pt.y - r, r * 2.0f, r * 2.0f);

        g.setColour(KalyxTheme::nightTextPrimary);
        g.drawEllipse(pt.x - r, pt.y - r, r * 2.0f, r * 2.0f, 1.2f);

        if (isActive)
        {
            g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::bold)));
            g.drawText(bandNames[i], static_cast<int>(pt.x - 14.0f), static_cast<int>(pt.y - r - 13.0f),
                       28, 11, juce::Justification::centred);
        }
    }

    g.setColour(KalyxTheme::nightTextSecondary);
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain)));
    g.drawText("+12dB", static_cast<int>(plotArea.getX() + 2), static_cast<int>(gainToY(12.0f, plotArea) - 10.0f), 38, 10, juce::Justification::left);
    g.drawText("-12dB", static_cast<int>(plotArea.getX() + 2), static_cast<int>(gainToY(-12.0f, plotArea) - 10.0f), 38, 10, juce::Justification::left);

    const int freqLabelY = static_cast<int>(frameBounds.getBottom() + 2.0f);
    g.setColour(KalyxTheme::getTextPrimary(dayMode_));
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::bold)));

    const int x100 = static_cast<int>(freqToX(100.0f,  plotArea));
    const int x1k  = static_cast<int>(freqToX(1000.0f, plotArea));
    const int x5k  = static_cast<int>(freqToX(5000.0f, plotArea));

    g.drawText("20 Hz",  static_cast<int>(plotArea.getX()),         freqLabelY, 36, 12, juce::Justification::left);
    g.drawText("100 Hz", x100 - 20,                                 freqLabelY, 40, 12, juce::Justification::centred);
    g.drawText("1 kHz",  x1k  - 20,                                 freqLabelY, 40, 12, juce::Justification::centred);
    g.drawText("5 kHz",  x5k  - 20,                                 freqLabelY, 40, 12, juce::Justification::centred);
    g.drawText("20 kHz", static_cast<int>(plotArea.getRight() - 40), freqLabelY, 40, 12, juce::Justification::right);
}

// =============================================================================
// SSLSettingsOverlayComponent - Panneau flottant (SETTINGS)
// =============================================================================
SSLSettingsOverlayComponent::SSLSettingsOverlayComponent(KalyxSSLConsoleProcessor& proc)
    : processor_(proc)
{
    addAndMakeVisible(closeBtn_);
    closeBtn_.onClick = [this]() { setVisible(false); };

    for (int i = 0; i < 3; ++i)
    {
        addAndMakeVisible(scaleBtns_[static_cast<std::size_t>(i)]);
        scaleBtns_[static_cast<std::size_t>(i)].onClick = [this, i]()
        {
            setChoiceParam(ParamIDs::meterMode, i);
            syncButtonsFromAPVTS();
        };
    }

    const float vuPresets[4] = { -20.0f, -18.0f, -14.0f, -12.0f };
    for (int i = 0; i < 4; ++i)
    {
        addAndMakeVisible(vuPresetBtns_[static_cast<std::size_t>(i)]);
        const float targetDb = vuPresets[i];
        vuPresetBtns_[static_cast<std::size_t>(i)].onClick = [this, targetDb]()
        {
            setFloatParam(ParamIDs::vuRefDb, targetDb);
            syncButtonsFromAPVTS();
        };
    }

    vuRefSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    vuRefSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 68, 20);
    addAndMakeVisible(vuRefSlider_);
    vuRefAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.getAPVTS(), ParamIDs::vuRefDb, vuRefSlider_);
    vuRefSlider_.onValueChange = [this]() { syncButtonsFromAPVTS(); };

    for (int i = 0; i < 3; ++i)
    {
        addAndMakeVisible(osBtns_[static_cast<std::size_t>(i)]);
        osBtns_[static_cast<std::size_t>(i)].onClick = [this, i]()
        {
            setChoiceParam(ParamIDs::oversampling, i);
            syncButtonsFromAPVTS();
        };
    }

    for (int i = 0; i < 3; ++i)
    {
        addAndMakeVisible(fftBtns_[static_cast<std::size_t>(i)]);
        fftBtns_[static_cast<std::size_t>(i)].onClick = [this, i]()
        {
            setChoiceParam(ParamIDs::fftMode, i);
            syncButtonsFromAPVTS();
        };
    }

    analogTolSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    analogTolSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 68, 20);
    addAndMakeVisible(analogTolSlider_);
    analogTolAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.getAPVTS(), ParamIDs::analogTolerance, analogTolSlider_);

    syncButtonsFromAPVTS();
}

void SSLSettingsOverlayComponent::setChoiceParam(const char* paramID, int index) noexcept
{
    if (auto* param = processor_.getAPVTS().getParameter(paramID))
        param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(index)));
}

void SSLSettingsOverlayComponent::setFloatParam(const char* paramID, float val) noexcept
{
    if (auto* param = processor_.getAPVTS().getParameter(paramID))
        param->setValueNotifyingHost(param->convertTo0to1(val));
}

void SSLSettingsOverlayComponent::syncButtonsFromAPVTS() noexcept
{
    auto& apvts = processor_.getAPVTS();
    const int meterMode = static_cast<int>(std::round(apvts.getRawParameterValue(ParamIDs::meterMode)->load()));
    const int osMode    = static_cast<int>(std::round(apvts.getRawParameterValue(ParamIDs::oversampling)->load()));
    const int fftMode   = static_cast<int>(std::round(apvts.getRawParameterValue(ParamIDs::fftMode)->load()));
    const float vuRef   = apvts.getRawParameterValue(ParamIDs::vuRefDb)->load();

    const juce::Colour idleCol = KalyxTheme::getControlIdleBg(dayMode_);
    const juce::Colour txtIdle = KalyxTheme::getTextPrimary(dayMode_);

    for (int i = 0; i < 3; ++i)
    {
        const bool active = (i == meterMode);
        scaleBtns_[static_cast<std::size_t>(i)].setColour(juce::TextButton::buttonColourId, active ? KalyxTheme::royalBlueBtn : idleCol);
        scaleBtns_[static_cast<std::size_t>(i)].setColour(juce::TextButton::textColourOffId, active ? juce::Colours::white : txtIdle);
    }

    const float vuPresets[4] = { -20.0f, -18.0f, -14.0f, -12.0f };
    for (int i = 0; i < 4; ++i)
    {
        const bool active = (std::abs(vuRef - vuPresets[i]) < 0.15f);
        vuPresetBtns_[static_cast<std::size_t>(i)].setColour(juce::TextButton::buttonColourId, active ? KalyxTheme::aubergineBtn : idleCol);
        vuPresetBtns_[static_cast<std::size_t>(i)].setColour(juce::TextButton::textColourOffId, active ? juce::Colours::white : txtIdle);
    }

    for (int i = 0; i < 3; ++i)
    {
        const bool active = (i == osMode);
        osBtns_[static_cast<std::size_t>(i)].setColour(juce::TextButton::buttonColourId, active ? KalyxTheme::royalBlueBtn : idleCol);
        osBtns_[static_cast<std::size_t>(i)].setColour(juce::TextButton::textColourOffId, active ? juce::Colours::white : txtIdle);
    }

    for (int i = 0; i < 3; ++i)
    {
        const bool active = (i == fftMode);
        fftBtns_[static_cast<std::size_t>(i)].setColour(juce::TextButton::buttonColourId, active ? KalyxTheme::aubergineBtn : idleCol);
        fftBtns_[static_cast<std::size_t>(i)].setColour(juce::TextButton::textColourOffId, active ? juce::Colours::white : txtIdle);
    }

    vuRefSlider_.setColour(juce::Slider::textBoxTextColourId, txtIdle);
    analogTolSlider_.setColour(juce::Slider::textBoxTextColourId, txtIdle);
    closeBtn_.setColour(juce::TextButton::buttonColourId, KalyxTheme::alertRed);
    closeBtn_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    repaint();
}

void SSLSettingsOverlayComponent::mouseDown(const juce::MouseEvent& e)
{
    const auto card = getLocalBounds().withSizeKeepingCentre(460, 360);
    if (!card.contains(e.getPosition()))
        setVisible(false);
}

void SSLSettingsOverlayComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.58f));

    const auto card = getLocalBounds().withSizeKeepingCentre(460, 360).toFloat();
    g.setColour(KalyxTheme::getStripBg(dayMode_));
    g.fillRoundedRectangle(card, 5.0f);

    const juce::Rectangle<float> titleBar(card.getX(), card.getY(), card.getWidth(), 32.0f);
    g.setColour(KalyxTheme::headerSurgical);
    g.fillRoundedRectangle(titleBar, 5.0f);

    g.setColour(KalyxTheme::getGridBorder(dayMode_));
    g.drawRoundedRectangle(card, 5.0f, 1.5f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    g.drawText("KALYX CONSOLE SETTINGS & CALIBRATION",
               static_cast<int>(card.getX() + 14.0f), static_cast<int>(card.getY() + 4.0f), 380, 24,
               juce::Justification::centredLeft);

    g.setColour(KalyxTheme::getTextPrimary(dayMode_));
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    const int lx = static_cast<int>(card.getX() + 18.0f);
    const int cy = static_cast<int>(card.getY());

    g.drawText("1. METER SCALE (IN / OUT) :",                  lx, cy + 40,  380, 15, juce::Justification::centredLeft);
    g.drawText("2. 0 VU REFERENCE CALIBRATION (dBFS) :",      lx, cy + 94,  380, 15, juce::Justification::centredLeft);
    g.drawText("3. NON-LINEAR DSP OVERSAMPLING (ADAA + OS) :", lx, cy + 178, 380, 15, juce::Justification::centredLeft);
    g.drawText("4. EQ REAL-TIME FFT SPECTRUM ANALYZER :",      lx, cy + 234, 380, 15, juce::Justification::centredLeft);
    g.drawText("5. ANALOG L/R COMPONENT TOLERANCE (%) :",      lx, cy + 292, 380, 15, juce::Justification::centredLeft);
}

void SSLSettingsOverlayComponent::resized()
{
    const auto card = getLocalBounds().withSizeKeepingCentre(460, 360);
    const int cx = card.getX() + 18;
    const int cy = card.getY();
    const int innerW = card.getWidth() - 36;

    closeBtn_.setBounds(card.getRight() - 34, cy + 4, 26, 24);

    const int w3 = (innerW - 12) / 3;
    for (int i = 0; i < 3; ++i)
        scaleBtns_[static_cast<std::size_t>(i)].setBounds(cx + i * (w3 + 6), cy + 58, w3, 24);

    const int w4 = (innerW - 18) / 4;
    for (int i = 0; i < 4; ++i)
        vuPresetBtns_[static_cast<std::size_t>(i)].setBounds(cx + i * (w4 + 6), cy + 112, w4, 22);

    vuRefSlider_.setBounds(cx, cy + 140, innerW, 24);

    for (int i = 0; i < 3; ++i)
        osBtns_[static_cast<std::size_t>(i)].setBounds(cx + i * (w3 + 6), cy + 196, w3, 24);

    for (int i = 0; i < 3; ++i)
        fftBtns_[static_cast<std::size_t>(i)].setBounds(cx + i * (w3 + 6), cy + 252, w3, 24);

    analogTolSlider_.setBounds(cx, cy + 312, innerW, 26);
}

// =============================================================================
// KalyxSSLConsoleEditor
// =============================================================================
KalyxSSLConsoleEditor::KalyxSSLConsoleEditor(KalyxSSLConsoleProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef_(p),
      eqCurveView_(p),
      settingsOverlay_(p)
{
    setLookAndFeel(&sslLookAndFeel_);
    addAndMakeVisible(eqCurveView_);

    // -------------------------------------------------------------------------
    // Bandeau supérieur sur une seule ligne : Presets, A/B, Copy, Mode M/S, Day Mode, Settings
    // -------------------------------------------------------------------------
    presetBox_.addItem("Preset: Init",           1);
    presetBox_.addItem("Preset: Vocal Strip",    2);
    presetBox_.addItem("Preset: Drum Bus Punch", 3);
    presetBox_.addItem("Preset: Acoustic Glue",  4);
    presetBox_.addItem("Preset: Bass Control",   5);
    presetBox_.setSelectedId(processorRef_.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox_.onChange = [this]()
    {
        const int idx = presetBox_.getSelectedId() - 1;
        if (idx >= 0)
            processorRef_.applyFactoryPreset(idx);
    };
    addAndMakeVisible(presetBox_);

    abToggleBtn_.onClick = [this]()
    {
        processorRef_.toggleABState();
        abToggleBtn_.setButtonText(processorRef_.isSlotBActive() ? "A / B : B" : "A / B : A");
        abCopyBtn_.setButtonText(processorRef_.isSlotBActive() ? "COPY B>A" : "COPY A>B");
    };
    addAndMakeVisible(abToggleBtn_);

    abCopyBtn_.onClick = [this]()
    {
        processorRef_.copyActiveToInactiveAB();
    };
    addAndMakeVisible(abCopyBtn_);

    msModeBox_.addItem("L/R STEREO",    1);
    msModeBox_.addItem("M/S (MID+SIDE)", 2);
    msModeBox_.addItem("M/S (MID ONLY)", 3);
    msModeBox_.addItem("M/S (SIDE ONLY)", 4);
    addAndMakeVisible(msModeBox_);
    msModeAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef_.getAPVTS(), ParamIDs::msMode, msModeBox_);

    setupToggle(dayModeBtn_, dayModeAtt_, ParamIDs::dayMode, "DAY MODE", KalyxTheme::aubergineBtn);

    settingsBtn_.onClick = [this]()
    {
        settingsOverlay_.setDayMode(lastDayMode_);
        settingsOverlay_.setVisible(!settingsOverlay_.isVisible());
        settingsOverlay_.toFront(true);
    };
    addAndMakeVisible(settingsBtn_);

    // -------------------------------------------------------------------------
    // 1. INPUT & FILTERS
    // -------------------------------------------------------------------------
    setupToggle(phaseInvertBtn_, phaseInvertAtt_, ParamIDs::phaseInvert, "PHASE INV", KalyxTheme::alertRed);
    setupToggle(hpfEnableBtn_,   hpfEnableAtt_,   ParamIDs::hpfEnabled,  "HPF IN",    KalyxTheme::royalBlueBtn);
    setupToggle(lpfEnableBtn_,   lpfEnableAtt_,   ParamIDs::lpfEnabled,  "LPF IN",    KalyxTheme::royalBlueBtn);

    setupRotary(inputGainCtrl_,   ParamIDs::inputGain,     "LINE TRIM",     KalyxTheme::royalBlueArc, 0.0);
    setupRotary(inputSatCtrl_,    ParamIDs::inputSatDrive, "PREAMP SAT",    KalyxTheme::aubergineArc, 0.0);
    setupRotary(hpfFreqCtrl_,     ParamIDs::hpfFreq,       "HPF (ZDF)",     KalyxTheme::royalBlueArc, 40.0);
    setupRotary(hpfQCtrl_,        ParamIDs::hpfQ,          "HPF Q",         KalyxTheme::royalBlueArc, 0.71);
    setupRotary(lpfFreqCtrl_,     ParamIDs::lpfFreq,       "LPF (ZDF)",     KalyxTheme::royalBlueArc, 18000.0);
    setupRotary(lpfQCtrl_,        ParamIDs::lpfQ,          "LPF Q",         KalyxTheme::royalBlueArc, 0.71);
    setupRotary(filtPreTrimCtrl_, ParamIDs::filtPreTrim,   "FILT/PRE TRIM", KalyxTheme::royalBlueArc, 0.0);

    // -------------------------------------------------------------------------
    // 2. DYNAMICS & DE-ESSER + SIDECHAIN + AUTO-RELEASE RC + SC HPF
    // -------------------------------------------------------------------------
    setupToggle(dynEnableBtn_,   dynEnableAtt_,   ParamIDs::dynEnabled,      "DYN IN",        KalyxTheme::royalBlueBtn);
    setupToggle(limiterModeBtn_, limiterModeAtt_, ParamIDs::limiterMode,     "LIMITER INF:1", KalyxTheme::alertRed);
    setupToggle(deessEnableBtn_, deessEnableAtt_, ParamIDs::deessIn,         "DE-ESS IN",     KalyxTheme::royalBlueBtn);

    setupToggle(extScBtn_,       extScAtt_,       ParamIDs::extSidechain,    "EXT SC",        KalyxTheme::royalBlueBtn);
    setupToggle(filtersToScBtn_, filtersToScAtt_, ParamIDs::filtersToSc,     "FILT > SC",     KalyxTheme::royalBlueBtn);
    setupToggle(scListenBtn_,    scListenAtt_,    ParamIDs::scListen,        "SC LISTEN",     KalyxTheme::amberGr);
    setupToggle(compAutoRelBtn_, compAutoRelAtt_, ParamIDs::compAutoRelease, "AUTO REL (RC)", KalyxTheme::aubergineBtn);

    setupRotary(compThreshCtrl_,  ParamIDs::compThresh,  "COMP THR",   KalyxTheme::royalBlueArc, -14.0);
    setupRotary(compRatioCtrl_,   ParamIDs::compRatio,   "RATIO",      KalyxTheme::royalBlueArc, 4.0);
    setupRotary(compAttackCtrl_,  ParamIDs::compAttack,  "ATTACK",     KalyxTheme::royalBlueArc, 10.0);
    setupRotary(compReleaseCtrl_, ParamIDs::compRelease, "RELEASE",    KalyxTheme::royalBlueArc, 120.0);
    setupRotary(scHpfFreqCtrl_,   ParamIDs::scHpfFreq,   "SC HPF",     KalyxTheme::royalBlueArc, 20.0);
    setupRotary(gateThreshCtrl_,  ParamIDs::gateThresh,  "GATE THR",   KalyxTheme::royalBlueArc, -55.0);
    setupRotary(gateRangeCtrl_,   ParamIDs::gateRange,   "GATE RNG",   KalyxTheme::royalBlueArc, 0.0);
    setupRotary(deessFreqCtrl_,   ParamIDs::deessFreq,   "DE-ESS FRQ", KalyxTheme::royalBlueArc, 6500.0);
    setupRotary(deessThreshCtrl_, ParamIDs::deessThresh, "DE-ESS THR", KalyxTheme::royalBlueArc, -18.0);
    setupRotary(dynMakeupCtrl_,   ParamIDs::makeupGain,  "DYN MAKEUP", KalyxTheme::royalBlueArc, 0.0);

    compRatioCtrl_.slider.textFromValueFunction = [this](double val) -> juce::String
    {
        if (limiterModeBtn_.getToggleState())
            return "INF:1";
        return juce::String(val, 1) + ":1";
    };

    compReleaseCtrl_.slider.textFromValueFunction = [this](double val) -> juce::String
    {
        if (compAutoRelBtn_.getToggleState())
            return "AUTO RC";
        return juce::String(juce::roundToInt(val)) + " ms";
    };

    // -------------------------------------------------------------------------
    // 3. CONSOLE COLOR
    // -------------------------------------------------------------------------
    setupRotary(colorAmountCtrl_, ParamIDs::colorAmount, "COLOR DRIVE", KalyxTheme::aubergineArc, 25.0);
    setupRotary(satToneCtrl_,     ParamIDs::satTone,     "SAT TONE",    KalyxTheme::aubergineArc, 0.0);
    setupRotary(crosstalkCtrl_,   ParamIDs::crosstalk,   "CROSSTALK",   KalyxTheme::aubergineArc, 0.0);
    setupRotary(colorTrimCtrl_,   ParamIDs::colorTrim,   "COLOR TRIM",  KalyxTheme::aubergineArc, 0.0);
    setupToggle(colorLinkBtn_,    colorLinkAtt_,         ParamIDs::colorLinkComp, "LINK TO COMP GR", KalyxTheme::aubergineBtn);

    circuitEBtn_.onClick = [this]()
    {
        if (!processorRef_.isSectionLocked(2))
        {
            if (auto* param = processorRef_.getAPVTS().getParameter(ParamIDs::circuitMode))
                param->setValueNotifyingHost(0.0f);
            updateLimiterAndCircuitUIState();
        }
    };
    circuitGBtn_.onClick = [this]()
    {
        if (!processorRef_.isSectionLocked(2))
        {
            if (auto* param = processorRef_.getAPVTS().getParameter(ParamIDs::circuitMode))
                param->setValueNotifyingHost(1.0f);
            updateLimiterAndCircuitUIState();
        }
    };
    addAndMakeVisible(circuitEBtn_);
    addAndMakeVisible(circuitGBtn_);

    // -------------------------------------------------------------------------
    // 4. 4-BAND PARAMETRIC EQ (ZDF/TPT + FFT + EQ TRIM)
    // -------------------------------------------------------------------------
    setupToggle(eqEnableBtn_, eqEnableAtt_, ParamIDs::eqEnabled, "EQ IN",      KalyxTheme::royalBlueBtn);
    setupToggle(eqPreDynBtn_, eqPreDynAtt_, ParamIDs::eqPreDyn,  "EQ PRE DYN", KalyxTheme::aubergineBtn);
    eqPreDynBtn_.onClick = [this]()
    {
        if (eqPreDynBtn_.getToggleState())
            processorRef_.setModuleOrderIndex(4); // EQ -> DYNAMICS -> COLOR
        else
            processorRef_.setModuleOrderIndex(0); // DYNAMICS -> COLOR -> EQ
        resized();
        repaint();
    };
    setupToggle(lfBellBtn_,   lfBellAtt_,   ParamIDs::lfBell,    "LF BELL",    KalyxTheme::eqLfWineRed);
    setupToggle(hfBellBtn_,   hfBellAtt_,   ParamIDs::hfBell,    "HF BELL",    KalyxTheme::royalBlueBtn);

    setupRotary(lfFreqCtrl_,  ParamIDs::lfFreq,  "LF FREQ",  KalyxTheme::eqLfWineRed,     80.0);
    setupRotary(lfGainCtrl_,  ParamIDs::lfGain,  "LF GAIN",  KalyxTheme::eqLfWineRed,     0.0);
    setupRotary(lmfFreqCtrl_, ParamIDs::lmfFreq, "LMF FREQ", KalyxTheme::eqLmfSlateBlue,  450.0);
    setupRotary(lmfGainCtrl_, ParamIDs::lmfGain, "LMF GAIN", KalyxTheme::eqLmfSlateBlue,  0.0);
    setupRotary(lmfQCtrl_,    ParamIDs::lmfQ,    "LMF Q",    KalyxTheme::eqLmfSlateBlue,  1.20);
    setupRotary(hmfFreqCtrl_, ParamIDs::hmfFreq, "HMF FREQ", KalyxTheme::eqHmfJadeGreen,  2500.0);
    setupRotary(hmfGainCtrl_, ParamIDs::hmfGain, "HMF GAIN", KalyxTheme::eqHmfJadeGreen,  0.0);
    setupRotary(hmfQCtrl_,    ParamIDs::hmfQ,    "HMF Q",    KalyxTheme::eqHmfJadeGreen,  1.20);
    setupRotary(hfFreqCtrl_,  ParamIDs::hfFreq,  "HF FREQ",  KalyxTheme::eqHfPearlSilver, 10000.0);
    setupRotary(hfGainCtrl_,  ParamIDs::hfGain,  "HF GAIN",  KalyxTheme::eqHfPearlSilver, 0.0);
    setupRotary(eqTrimCtrl_,  ParamIDs::eqTrim,  "EQ TRIM",  KalyxTheme::royalBlueArc,    0.0);

    // -------------------------------------------------------------------------
    // 5. OUTPUT & METERS (AUTO-GAIN, ANALOG TOLERANCE, CONSOLE MIX, OUT FADER)
    // -------------------------------------------------------------------------
    setupToggle(autoGainBtn_, autoGainAtt_, ParamIDs::autoGain, "AUTO-GAIN", KalyxTheme::royalBlueBtn);

    setupRotary(analogTolCtrl_,  ParamIDs::analogTolerance, "ANALOG TOL",  KalyxTheme::aubergineArc, 25.0);
    setupRotary(mixCtrl_,        ParamIDs::mix,             "CONSOLE MIX", KalyxTheme::royalBlueArc, 100.0);
    setupRotary(outputGainCtrl_, ParamIDs::outputGain,      "OUT FADER",   KalyxTheme::royalBlueArc, 0.0);

    addChildComponent(settingsOverlay_);

    lastDayMode_     = (processorRef_.getAPVTS().getRawParameterValue(ParamIDs::dayMode)->load() > 0.5f);
    lastModuleOrder_ = processorRef_.getModuleOrderIndex();
    applyThemeToComponents(lastDayMode_);
    updateLimiterAndCircuitUIState();

    // Redimensionnement libre avec maintien proportionnel du ratio d'aspect (80 % à 175 % de 1260x720)
    setResizable(true, true);
    getConstrainer()->setFixedAspectRatio(1260.0 / 720.0);
    setResizeLimits(1008, 576, 2205, 1260);
    setSize(1260, 720);

    startTimerHz(30);
}

KalyxSSLConsoleEditor::~KalyxSSLConsoleEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void KalyxSSLConsoleEditor::setupRotary(RotaryControl& ctrl,
                                        const char* paramID,
                                        const juce::String& title,
                                        const juce::Colour& arcColour,
                                        double defaultResetValue)
{
    ctrl.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    ctrl.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 78, 16);
    ctrl.slider.setColour(juce::Slider::rotarySliderFillColourId, arcColour);
    ctrl.slider.setColour(juce::Slider::textBoxTextColourId,       KalyxTheme::getTextPrimary(lastDayMode_));
    ctrl.slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    ctrl.slider.setColour(juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);

    ctrl.slider.setDoubleClickReturnValue(true, defaultResetValue);
    ctrl.slider.setVelocityModeParameters(1.0, 1, 0.08, true, juce::ModifierKeys::ctrlAltCommandModifiers);
    addAndMakeVisible(ctrl.slider);

    ctrl.titleLabel.setText(title, juce::dontSendNotification);
    ctrl.titleLabel.setJustificationType(juce::Justification::centred);
    ctrl.titleLabel.setColour(juce::Label::textColourId, KalyxTheme::getTextPrimary(lastDayMode_));
    ctrl.titleLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    addAndMakeVisible(ctrl.titleLabel);

    ctrl.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef_.getAPVTS(), paramID, ctrl.slider);
}

void KalyxSSLConsoleEditor::setupToggle(juce::ToggleButton& btn,
                                        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& att,
                                        const char* paramID,
                                        const juce::String& text,
                                        const juce::Colour& activeColour)
{
    btn.setButtonText(text);
    btn.setColour(juce::ToggleButton::tickColourId, activeColour);
    addAndMakeVisible(btn);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processorRef_.getAPVTS(), paramID, btn);
}

void KalyxSSLConsoleEditor::applyThemeToComponents(bool isDayMode)
{
    sslLookAndFeel_.setDayMode(isDayMode);
    eqCurveView_.setDayMode(isDayMode);
    settingsOverlay_.setDayMode(isDayMode);

    const juce::Colour txtCol = KalyxTheme::getTextPrimary(isDayMode);
    const juce::Colour idleBg = KalyxTheme::getControlIdleBg(isDayMode);

    RotaryControl* allRotaries[] = {
        &inputGainCtrl_, &inputSatCtrl_, &hpfFreqCtrl_, &hpfQCtrl_, &lpfFreqCtrl_, &lpfQCtrl_, &filtPreTrimCtrl_,
        &gateThreshCtrl_, &gateRangeCtrl_, &compThreshCtrl_, &compRatioCtrl_, &compAttackCtrl_, &compReleaseCtrl_,
        &scHpfFreqCtrl_, &deessFreqCtrl_, &deessThreshCtrl_, &dynMakeupCtrl_,
        &colorAmountCtrl_, &satToneCtrl_, &crosstalkCtrl_, &colorTrimCtrl_,
        &lfFreqCtrl_, &lfGainCtrl_, &lmfFreqCtrl_, &lmfGainCtrl_, &lmfQCtrl_,
        &hmfFreqCtrl_, &hmfGainCtrl_, &hmfQCtrl_, &hfFreqCtrl_, &hfGainCtrl_, &eqTrimCtrl_,
        &analogTolCtrl_, &outputGainCtrl_, &mixCtrl_
    };

    for (auto* rc : allRotaries)
    {
        rc->slider.setColour(juce::Slider::textBoxTextColourId, txtCol);
        rc->titleLabel.setColour(juce::Label::textColourId, txtCol);
        rc->slider.lookAndFeelChanged();
    }

    abToggleBtn_.setColour(juce::TextButton::buttonColourId, KalyxTheme::royalBlueBtn);
    abToggleBtn_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);

    abCopyBtn_.setColour(juce::TextButton::buttonColourId, idleBg);
    abCopyBtn_.setColour(juce::TextButton::textColourOffId, txtCol);

    settingsBtn_.setColour(juce::TextButton::buttonColourId, idleBg);
    settingsBtn_.setColour(juce::TextButton::textColourOffId, txtCol);

    presetBox_.lookAndFeelChanged();
    msModeBox_.lookAndFeelChanged();
}

void KalyxSSLConsoleEditor::updateLimiterAndCircuitUIState()
{
    // 1. Limiter ∞:1 grise RATIO
    const bool limiterActive = limiterModeBtn_.getToggleState();
    if (compRatioCtrl_.slider.isEnabled() == limiterActive)
    {
        compRatioCtrl_.slider.setEnabled(!limiterActive);
        compRatioCtrl_.titleLabel.setAlpha(limiterActive ? 0.45f : 1.0f);
        compRatioCtrl_.slider.updateText();
    }

    // 2. AUTO REL (RC) grise RELEASE et affiche "AUTO RC"
    const bool autoRelActive = compAutoRelBtn_.getToggleState();
    if (compReleaseCtrl_.slider.isEnabled() == autoRelActive)
    {
        compReleaseCtrl_.slider.setEnabled(!autoRelActive);
        compReleaseCtrl_.titleLabel.setAlpha(autoRelActive ? 0.45f : 1.0f);
        compReleaseCtrl_.slider.updateText();
    }

    // 3. Sélecteur de circuit 4000 E / 4000 G
    const bool is4000G = (processorRef_.getAPVTS().getRawParameterValue(ParamIDs::circuitMode)->load() > 0.5f);
    const juce::Colour idleCol = KalyxTheme::getControlIdleBg(lastDayMode_);
    const juce::Colour txtCol  = KalyxTheme::getTextPrimary(lastDayMode_);

    circuitEBtn_.setColour(juce::TextButton::buttonColourId, !is4000G ? KalyxTheme::aubergineBtn : idleCol);
    circuitEBtn_.setColour(juce::TextButton::textColourOffId, !is4000G ? juce::Colours::white : txtCol);

    circuitGBtn_.setColour(juce::TextButton::buttonColourId, is4000G ? KalyxTheme::aubergineBtn : idleCol);
    circuitGBtn_.setColour(juce::TextButton::textColourOffId, is4000G ? juce::Colours::white : txtCol);
}

void KalyxSSLConsoleEditor::timerCallback()
{
    meterSnap_ = processorRef_.getMeterSnapshot();
    eqCurveView_.updateFFTFromProcessor();

    const bool curDayMode = (processorRef_.getAPVTS().getRawParameterValue(ParamIDs::dayMode)->load() > 0.5f);
    if (curDayMode != lastDayMode_)
    {
        lastDayMode_ = curDayMode;
        applyThemeToComponents(lastDayMode_);
    }

    const int curOrder = processorRef_.getModuleOrderIndex();
    if (curOrder != lastModuleOrder_)
    {
        lastModuleOrder_ = curOrder;
        resized();
    }

    updateLimiterAndCircuitUIState();
    eqCurveView_.repaint();
    repaint();
}

void KalyxSSLConsoleEditor::drawLockIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool isLocked)
{
    const auto box = bounds.reduced(2.0f);
    g.setColour(isLocked ? KalyxTheme::amberGr : juce::Colours::white.withAlpha(0.18f));
    g.fillRoundedRectangle(box, 2.5f);
    g.setColour(isLocked ? juce::Colours::white : juce::Colours::white.withAlpha(0.55f));
    g.drawRoundedRectangle(box, 2.5f, 1.0f);

    // Dessin vectoriel du cadenas (corps + anse)
    const float cx = box.getCentreX();
    const float cy = box.getCentreY();
    const juce::Rectangle<float> body(cx - 4.0f, cy - 0.5f, 8.0f, 6.0f);
    g.setColour(isLocked ? KalyxTheme::nightChassisBg : juce::Colours::white);
    g.fillRoundedRectangle(body, 1.0f);

    juce::Path shackle;
    if (isLocked)
        shackle.addCentredArc(cx, cy - 1.0f, 2.6f, 3.2f, 0.0f, -2.8f, 2.8f, true);
    else
        shackle.addCentredArc(cx + 1.5f, cy - 2.2f, 2.6f, 3.2f, 0.0f, -2.5f, 1.8f, true);

    g.strokePath(shackle, juce::PathStrokeType(1.4f));
}

void KalyxSSLConsoleEditor::mouseDown(const juce::MouseEvent& e)
{
    // 0. Clic sur les flèches < et > de réorganisation des 3 modules centraux (Slots 0, 1, 2)
    for (int slot = 0; slot < 3; ++slot)
    {
        if (slot > 0 && arrowLeftRects_[static_cast<std::size_t>(slot)].expanded(2.0f).contains(e.position))
        {
            processorRef_.swapCentralModuleSlots(slot, slot - 1);
            lastModuleOrder_ = processorRef_.getModuleOrderIndex();
            resized();
            repaint();
            return;
        }
        if (slot < 2 && arrowRightRects_[static_cast<std::size_t>(slot)].expanded(2.0f).contains(e.position))
        {
            processorRef_.swapCentralModuleSlots(slot, slot + 1);
            lastModuleOrder_ = processorRef_.getModuleOrderIndex();
            resized();
            repaint();
            return;
        }
    }

    // 1. Clic sur l'une des 5 icônes cadenas (Parameter Lock)
    for (int s = 0; s < 5; ++s)
    {
        if (lockIconRects_[static_cast<std::size_t>(s)].expanded(3.0f).contains(e.position))
        {
            const bool curLocked = processorRef_.isSectionLocked(s);
            processorRef_.setSectionLocked(s, !curLocked);
            repaint();
            return;
        }
    }

    // 2. Clic sur l'un des 3 bargraphes de la section DYNAMICS (COMP, GATE, DE-ESS)
    draggedMeterThresholdIndex_ = -1;
    if (processorRef_.isSectionLocked(1))
        return;

    for (int i = 0; i < 3; ++i)
    {
        if (dynMeterRects_[static_cast<std::size_t>(i)].expanded(8.0f, 4.0f).contains(e.position))
        {
            draggedMeterThresholdIndex_ = i;
            mouseDrag(e);
            break;
        }
    }
}

void KalyxSSLConsoleEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (draggedMeterThresholdIndex_ < 0 || draggedMeterThresholdIndex_ >= 3 || processorRef_.isSectionLocked(1))
        return;

    const auto& r = dynMeterRects_[static_cast<std::size_t>(draggedMeterThresholdIndex_)];
    if (r.getHeight() <= 4.0f)
        return;

    const float normFromTop = std::clamp((e.position.y - r.getY()) / r.getHeight(), 0.0f, 1.0f);
    const float rawDb = -normFromTop * 48.0f;

    const char* paramID = nullptr;
    float targetDb = rawDb;
    if (draggedMeterThresholdIndex_ == 0)
    {
        paramID  = ParamIDs::compThresh;
        targetDb = std::clamp(rawDb, -40.0f, 0.0f);
    }
    else if (draggedMeterThresholdIndex_ == 1)
    {
        paramID  = ParamIDs::gateThresh;
        targetDb = std::clamp(rawDb, -70.0f, 0.0f);
    }
    else
    {
        paramID  = ParamIDs::deessThresh;
        targetDb = std::clamp(rawDb, -40.0f, 0.0f);
    }

    if (auto* param = processorRef_.getAPVTS().getParameter(paramID))
        param->setValueNotifyingHost(param->convertTo0to1(targetDb));

    repaint();
}

void KalyxSSLConsoleEditor::mouseUp(const juce::MouseEvent&)
{
    draggedMeterThresholdIndex_ = -1;
}

void KalyxSSLConsoleEditor::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const bool day = lastDayMode_;
    const juce::Colour txtCol = KalyxTheme::getTextPrimary(day);

    // 1. Fond châssis plat (Night #0F0E17 ou Day #C8CDD5)
    g.fillAll(KalyxTheme::getChassisBg(day));

    // 2. Bandeau supérieur vectoriel sur une seule ligne non tronquée
    const juce::Rectangle<float> headerRect(10.0f, 8.0f, bounds.getWidth() - 20.0f, 38.0f);
    g.setColour(KalyxTheme::getStripBg(day));
    g.fillRoundedRectangle(headerRect, 3.0f);
    g.setColour(KalyxTheme::getGridBorder(day));
    g.drawRoundedRectangle(headerRect, 3.0f, 1.0f);

    g.setColour(txtCol);
    g.setFont(juce::Font(juce::FontOptions(14.5f, juce::Font::bold)));
    g.drawText("KALYX // SSL 4000", 16, 8, 165, 38, juce::Justification::centredLeft);

    const bool is4000G = (processorRef_.getAPVTS().getRawParameterValue(ParamIDs::circuitMode)->load() > 0.5f);
    const juce::String statusText = juce::String::formatted(
        "CIRCUIT: %s | COLOR: %3.0f%% | GR: -%4.1f dB | AUTO-GAIN: %+4.1f dB",
        is4000G ? "4000 G" : "4000 E",
        meterSnap_.dynamicColorDrive * 100.0f,
        meterSnap_.compGainRedDb,
        meterSnap_.autoGainAppliedDb);

    g.setColour(day ? KalyxTheme::dayTextPrimary : KalyxTheme::aubergineArc);
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::bold)));
    const int statusW = 468;
    g.drawText(statusText, static_cast<int>(bounds.getWidth() - statusW - 18.0f), 8, statusW, 38,
               juce::Justification::centredRight);

    // 3. Dessin des 5 colonnes avec réorganisation dynamique des modules centraux (Slots 0, 1, 2 -> 2., 3., 4.)
    const float topY   = 52.0f;
    const float panelH = bounds.getHeight() - topY - 10.0f;
    const float totalW = bounds.getWidth() - 20.0f;
    const float gap    = 6.0f;
    const float availW = totalW - 4.0f * gap;

    const float w1   = std::floor(availW * 0.185f);
    const float wDyn = std::floor(availW * 0.265f);
    const float wCol = std::floor(availW * 0.130f);
    const float wEq  = std::floor(availW * 0.280f);
    const float w5   = availW - (w1 + wDyn + wCol + wEq);

    auto drawArrowButton = [&](const juce::Rectangle<float>& btnRect, const char* symbol, bool enabled)
    {
        const auto box = btnRect.reduced(1.5f);
        g.setColour(enabled ? juce::Colours::white.withAlpha(0.22f) : juce::Colours::white.withAlpha(0.06f));
        g.fillRoundedRectangle(box, 2.0f);
        g.setColour(enabled ? juce::Colours::white.withAlpha(0.75f) : juce::Colours::white.withAlpha(0.22f));
        g.drawRoundedRectangle(box, 2.0f, 1.0f);
        g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.5f, juce::Font::bold)));
        g.drawText(symbol, box.toNearestInt(), juce::Justification::centred);
    };

    auto drawSectionPanel = [&](int secIdx, int centralSlotIdx, float x, float w,
                                const juce::String& title, const juce::Colour& headerBgCol)
    {
        const juce::Rectangle<float> r(x, topY, w, panelH);
        g.setColour(KalyxTheme::getStripBg(day));
        g.fillRoundedRectangle(r, 3.0f);

        const juce::Rectangle<float> titleBar(x, topY, w, 24.0f);
        g.setColour(headerBgCol);
        g.fillRoundedRectangle(titleBar, 3.0f);

        g.setColour(KalyxTheme::getGridBorder(day));
        g.drawRoundedRectangle(r, 3.0f, 1.0f);
        g.drawLine(x, topY + 24.0f, x + w, topY + 24.0f, 1.0f);

        float titleLeftTrim = 4.0f;
        if (centralSlotIdx >= 0 && centralSlotIdx < 3)
        {
            const juce::Rectangle<float> leftBtn (x + 3.0f,  topY + 3.0f, 16.0f, 18.0f);
            const juce::Rectangle<float> rightBtn(x + 20.0f, topY + 3.0f, 16.0f, 18.0f);
            arrowLeftRects_[static_cast<std::size_t>(centralSlotIdx)]  = leftBtn;
            arrowRightRects_[static_cast<std::size_t>(centralSlotIdx)] = rightBtn;
            drawArrowButton(leftBtn,  "<", centralSlotIdx > 0);
            drawArrowButton(rightBtn, ">", centralSlotIdx < 2);
            titleLeftTrim = 38.0f;
        }

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        g.drawFittedText(title,
                         titleBar.withTrimmedLeft(titleLeftTrim).withTrimmedRight(24.0f).toNearestInt(),
                         juce::Justification::centred, 1);

        // Icône cadenas à droite de l'en-tête de section
        const juce::Rectangle<float> lockRect(x + w - 22.0f, topY + 3.0f, 18.0f, 18.0f);
        lockIconRects_[static_cast<std::size_t>(secIdx)] = lockRect;
        drawLockIcon(g, lockRect, processorRef_.isSectionLocked(secIdx));
    };

    float curX = 10.0f;
    drawSectionPanel(0, -1, curX, w1, "1. INPUT & FILTERS", KalyxTheme::headerAnalog);
    const float x1 = curX;
    curX += w1 + gap;

    const int orderIdx = processorRef_.getModuleOrderIndex();
    const auto& perm = kalyx::dsp::kModulePermutations[static_cast<std::size_t>(orderIdx)];

    float xDyn = curX;
    float xCol = curX;
    float xEq  = curX;

    for (int slot = 0; slot < 3; ++slot)
    {
        const int modId = perm[static_cast<std::size_t>(slot)];
        const juce::String numPrefix = juce::String(slot + 2) + ". ";

        if (modId == 0) // DYNAMICS & DE-ESSER
        {
            xDyn = curX;
            drawSectionPanel(1, slot, curX, wDyn, numPrefix + "DYNAMICS & DE-ESSER", KalyxTheme::headerSurgical);
            curX += wDyn + gap;
        }
        else if (modId == 1) // CONSOLE COLOR
        {
            xCol = curX;
            drawSectionPanel(2, slot, curX, wCol, numPrefix + "CONSOLE COLOR", KalyxTheme::headerAnalog);
            curX += wCol + gap;
        }
        else // 4-BAND PARAMETRIC EQ
        {
            xEq = curX;
            drawSectionPanel(3, slot, curX, wEq, numPrefix + "4-BAND PARAMETRIC EQ", KalyxTheme::headerSurgical);
            curX += wEq + gap;
        }
    }
    (void)xEq;

    drawSectionPanel(4, -1, curX, w5, "5. OUTPUT & METERS", KalyxTheme::headerSurgical);
    const float x5 = curX;

    // =========================================================================
    // VU-mètres Stéréo (2 barres fines L / R) calibrés (dBFS / VU / LUFS)
    // =========================================================================
    const int meterMode = static_cast<int>(std::round(processorRef_.getAPVTS().getRawParameterValue(ParamIDs::meterMode)->load()));
    const float vuRefDb = processorRef_.getAPVTS().getRawParameterValue(ParamIDs::vuRefDb)->load();

    float minScale = -48.0f;
    float maxScale = 6.0f;
    juce::String scaleUnitLabel = "dBFS";

    float inValL  = meterSnap_.inputPeakDbL;
    float inValR  = meterSnap_.inputPeakDbR;
    float outValL = meterSnap_.outputPeakDbL;
    float outValR = meterSnap_.outputPeakDbR;

    if (meterMode == 1)
    {
        inValL         = meterSnap_.inputVuDbL  - vuRefDb;
        inValR         = meterSnap_.inputVuDbR  - vuRefDb;
        outValL        = meterSnap_.outputVuDbL - vuRefDb;
        outValR        = meterSnap_.outputVuDbR - vuRefDb;
        minScale       = -24.0f;
        maxScale       = 6.0f;
        scaleUnitLabel = juce::String::formatted("VU(%.0f)", vuRefDb);
    }
    else if (meterMode == 2)
    {
        inValL         = meterSnap_.inputLufsDbL;
        inValR         = meterSnap_.inputLufsDbR;
        outValL        = meterSnap_.outputLufsDbL;
        outValR        = meterSnap_.outputLufsDbR;
        minScale       = -48.0f;
        maxScale       = 0.0f;
        scaleUnitLabel = "LUFS";
    }

    auto drawStereoPairMeter = [&](float mx, float my, float pairW, float mh,
                                   float valL, float valR, float minDb, float maxDb,
                                   const juce::String& titleLabel, bool ticksOnLeft)
    {
        const juce::Rectangle<float> box(mx, my, pairW, mh);
        g.setColour(KalyxTheme::nightChassisBg);
        g.fillRoundedRectangle(box, 2.0f);
        g.setColour(KalyxTheme::getGridBorder(day));
        g.drawRoundedRectangle(box, 2.0f, 1.0f);

        g.setColour(txtCol);
        g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 8.0f, juce::Font::bold)));
        g.drawText(scaleUnitLabel, static_cast<int>(mx - 14.0f), static_cast<int>(my - 13.0f),
                   static_cast<int>(pairW + 28.0f), 11, juce::Justification::centred);

        const float barW = (pairW - 5.0f) * 0.5f;
        const float lx   = mx + 2.0f;
        const float rx   = lx + barW + 1.0f;

        constexpr int numSegments = 24;
        constexpr float segGap = 1.3f;
        const float segH = (mh - 6.0f - static_cast<float>(numSegments - 1) * segGap) / static_cast<float>(numSegments);
        const float normL    = std::clamp((valL - minDb) / (maxDb - minDb), 0.0f, 1.0f);
        const float normR    = std::clamp((valR - minDb) / (maxDb - minDb), 0.0f, 1.0f);
        const float zeroNorm = std::clamp((0.0f - minDb) / (maxDb - minDb), 0.0f, 1.0f);

        for (int i = 0; i < numSegments; ++i)
        {
            const float segNorm = static_cast<float>(i + 1) / static_cast<float>(numSegments);
            const float ly = my + mh - 3.0f - static_cast<float>(i + 1) * segH - static_cast<float>(i) * segGap;
            const bool litL = normL >= (segNorm - 0.02f);
            const bool litR = normR >= (segNorm - 0.02f);

            const juce::Colour segCol = (segNorm > zeroNorm)         ? KalyxTheme::alertRed
                                      : (segNorm > zeroNorm - 0.16f) ? KalyxTheme::amberGr
                                                                     : KalyxTheme::royalBlueArc;

            g.setColour(litL ? segCol : KalyxTheme::nightControlIdleBg);
            g.fillRect(lx, ly, barW, segH);

            g.setColour(litR ? segCol : KalyxTheme::nightControlIdleBg);
            g.fillRect(rx, ly, barW, segH);
        }

        g.setColour(txtCol);
        g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
        g.drawText(titleLabel + " L/R", static_cast<int>(mx - 12.0f), static_cast<int>(my + mh + 2.0f),
                   static_cast<int>(pairW + 24.0f), 11, juce::Justification::centred);

        const float maxLR = std::max(valL, valR);
        const juce::String numReadout = (maxLR <= -90.0f) ? "-INF" : juce::String::formatted("%+4.1f", maxLR);
        g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 8.5f, juce::Font::bold)));
        g.drawText(numReadout, static_cast<int>(mx - 14.0f), static_cast<int>(my + mh + 13.0f),
                   static_cast<int>(pairW + 28.0f), 11, juce::Justification::centred);

        g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 8.0f, juce::Font::bold)));
        const int tickX = ticksOnLeft ? static_cast<int>(mx - 20.0f) : static_cast<int>(mx + pairW + 2.0f);
        const auto just = ticksOnLeft ? juce::Justification::right   : juce::Justification::left;

        if (meterMode == 1)
        {
            const float vuMarks[] = { 6.0f, 3.0f, 0.0f, -3.0f, -6.0f, -12.0f, -20.0f };
            for (float dbMark : vuMarks)
            {
                const float norm = (dbMark - minDb) / (maxDb - minDb);
                const float yPos = my + mh - 3.0f - norm * (mh - 6.0f);
                const juce::String lbl = (dbMark > 0.1f) ? juce::String::formatted("+%.0f", dbMark)
                                       : (std::abs(dbMark) < 0.1f ? "0" : juce::String::formatted("%.0f", dbMark));
                g.drawText(lbl, tickX, static_cast<int>(yPos - 5.0f), 18, 10, just);
            }
        }
        else
        {
            const float ioMarks[] = { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f };
            for (float dbMark : ioMarks)
            {
                const float norm = std::clamp((dbMark - minDb) / (maxDb - minDb), 0.0f, 1.0f);
                const float yPos = my + mh - 3.0f - norm * (mh - 6.0f);
                const juce::String lbl = (std::abs(dbMark) < 0.1f) ? "0" : juce::String::formatted("%.0f", dbMark);
                g.drawText(lbl, tickX, static_cast<int>(yPos - 5.0f), 18, 10, just);
            }
        }
    };

    const float inMeterY = topY + 42.0f;
    const float inMeterH = panelH - 72.0f;
    drawStereoPairMeter(x1 + 8.0f, inMeterY, 18.0f, inMeterH, inValL, inValR, minScale, maxScale, "IN", false);

    const float outMeterY = topY + 42.0f;
    const float outMeterH = panelH - 72.0f;
    drawStereoPairMeter(x5 + w5 - 26.0f, outMeterY, 18.0f, outMeterH, outValL, outValR, minScale, maxScale, "OUT", true);

    // =========================================================================
    // VU-mètres Dynamiques de la Section DYNAMICS (COMP, GATE, DE-ESS) - Échelle -48..0 dB
    // =========================================================================
    const auto guiParams = processorRef_.getCurrentParametersForGUI();

    auto drawDualActionDynMeter = [&](int meterIdx, float mx, float my, float mw, float mh,
                                      float detectorDb, float grDb, float threshDb,
                                      const juce::Colour& grColour, const juce::String& label)
    {
        const juce::Rectangle<float> box(mx, my, mw, mh);
        dynMeterRects_[static_cast<std::size_t>(meterIdx)] = box;

        g.setColour(KalyxTheme::nightChassisBg);
        g.fillRoundedRectangle(box, 2.0f);
        g.setColour(KalyxTheme::getGridBorder(day));
        g.drawRoundedRectangle(box, 2.0f, 1.0f);

        constexpr int numSegments = 24;
        constexpr float segGap = 1.3f;
        const float segH = (mh - 6.0f - static_cast<float>(numSegments - 1) * segGap) / static_cast<float>(numSegments);

        const float sigNormFromBottom = std::clamp((detectorDb - (-48.0f)) / 48.0f, 0.0f, 1.0f);
        const float grNormFromTop     = std::clamp(grDb / 48.0f, 0.0f, 1.0f);

        for (int i = 0; i < numSegments; ++i)
        {
            const float topFrac    = static_cast<float>(i + 1) / static_cast<float>(numSegments);
            const float bottomFrac = static_cast<float>(numSegments - i) / static_cast<float>(numSegments);
            const float ly = my + 3.0f + static_cast<float>(i) * (segH + segGap);

            const bool isGrLit  = (grDb > 0.15f) && (grNormFromTop >= (topFrac - 0.02f));
            const bool isSigLit = (sigNormFromBottom >= (bottomFrac - 0.02f));

            g.setColour(KalyxTheme::nightControlIdleBg);
            g.fillRect(mx + 2.0f, ly, mw - 4.0f, segH);

            if (isSigLit)
            {
                g.setColour(KalyxTheme::royalBlueArc.withAlpha(0.85f));
                g.fillRect(mx + 2.0f, ly, mw - 4.0f, segH);
            }

            if (isGrLit)
            {
                g.setColour(grColour);
                g.fillRect(mx + 2.0f, ly, mw - 4.0f, segH);
            }
        }

        const float clampedThr = std::clamp(threshDb, -48.0f, 0.0f);
        const float thrNormFromTop = std::clamp(-clampedThr / 48.0f, 0.0f, 1.0f);
        const float ty = my + 3.0f + thrNormFromTop * (mh - 6.0f);

        const juce::Colour handleCol = (meterIdx == 0 && guiParams.limiterMode)
                                           ? KalyxTheme::alertRed
                                           : juce::Colours::white;

        g.setColour(handleCol);
        g.drawHorizontalLine(static_cast<int>(ty), mx - 2.0f, mx + mw + 2.0f);

        juce::Path handleTri;
        handleTri.addTriangle(mx + mw - 1.0f, ty,
                              mx + mw + 6.0f, ty - 4.0f,
                              mx + mw + 6.0f, ty + 4.0f);
        g.fillPath(handleTri);

        g.setColour(txtCol);
        g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
        g.drawFittedText(label, static_cast<int>(mx - 10.0f), static_cast<int>(my + mh + 4.0f),
                         static_cast<int>(mw + 20.0f), 13, juce::Justification::centred, 1);
    };

    const float dynMeterY = topY + 86.0f;
    const float dynMeterH = panelH - 114.0f;
    const float mCol1 = xDyn + wDyn - 96.0f;
    const float mCol2 = xDyn + wDyn - 64.0f;
    const float mCol3 = xDyn + wDyn - 32.0f;
    const float barW  = 14.0f;

    drawDualActionDynMeter(0, mCol1, dynMeterY, barW, dynMeterH,
                           meterSnap_.compDetectorDb, meterSnap_.compGainRedDb, guiParams.compThreshDb,
                           KalyxTheme::amberGr, "COMP");
    drawDualActionDynMeter(1, mCol2, dynMeterY, barW, dynMeterH,
                           meterSnap_.gateDetectorDb, meterSnap_.gateGainRedDb, guiParams.gateThreshDb,
                           KalyxTheme::eqLfWineRed.brighter(0.25f), "GATE");
    drawDualActionDynMeter(2, mCol3, dynMeterY, barW, dynMeterH,
                           meterSnap_.deessDetectorDb, meterSnap_.deessGainRedDb, guiParams.deessThreshDb,
                           KalyxTheme::cyanDeessGr, "DE-ESS");

    g.setColour(txtCol);
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 8.0f, juce::Font::bold)));
    const float dynMarks[] = { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f };
    for (float dbMark : dynMarks)
    {
        const float normFromTop = -dbMark / 48.0f;
        const float yPos = dynMeterY + 3.0f + normFromTop * (dynMeterH - 6.0f);
        const juce::String lbl = (std::abs(dbMark) < 0.1f) ? " 0" : juce::String::formatted("%.0f", dbMark);
        g.drawText(lbl, static_cast<int>(mCol1 - 21.0f), static_cast<int>(yPos - 5.0f), 19, 10, juce::Justification::right);
    }

    // -------------------------------------------------------------------------
    // Jauge vectorielle d'intensité harmonique dans CONSOLE COLOR
    // -------------------------------------------------------------------------
    const float colBarX = xCol + 8.0f;
    const float colBarY = topY + panelH - 36.0f;
    const float colBarW = wCol - 16.0f;
    const float colBarH = 18.0f;

    g.setColour(KalyxTheme::nightChassisBg);
    g.fillRoundedRectangle(colBarX, colBarY, colBarW, colBarH, 2.0f);
    g.setColour(KalyxTheme::getGridBorder(day));
    g.drawRoundedRectangle(colBarX, colBarY, colBarW, colBarH, 2.0f, 1.0f);

    const float driveNorm = std::clamp(meterSnap_.dynamicColorDrive, 0.0f, 1.25f) / 1.25f;
    if (driveNorm > 0.005f)
    {
        g.setColour(KalyxTheme::aubergineBtn);
        g.fillRect(colBarX + 2.0f, colBarY + 2.0f, (colBarW - 4.0f) * driveNorm, colBarH - 4.0f);
    }

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::bold)));
    g.drawText(juce::String::formatted("ADAA DRIVE: %3.0f%%", meterSnap_.dynamicColorDrive * 100.0f),
               static_cast<int>(colBarX), static_cast<int>(colBarY),
               static_cast<int>(colBarW), static_cast<int>(colBarH), juce::Justification::centred);
}

void KalyxSSLConsoleEditor::resized()
{
    const auto bounds = getLocalBounds().toFloat();
    settingsOverlay_.setBounds(getLocalBounds());

    // -------------------------------------------------------------------------
    // Bandeau supérieur sur une seule ligne non tronquée
    // -------------------------------------------------------------------------
    {
        juce::FlexBox headerFb;
        headerFb.flexDirection  = juce::FlexBox::Direction::row;
        headerFb.justifyContent = juce::FlexBox::JustifyContent::flexStart;
        headerFb.alignItems     = juce::FlexBox::AlignItems::center;

        const float margin = 2.5f;
        headerFb.items.add(juce::FlexItem(presetBox_).withWidth(124.0f).withHeight(24.0f).withMargin({ 0.0f, margin, 0.0f, margin }));
        headerFb.items.add(juce::FlexItem(abToggleBtn_).withWidth(56.0f).withHeight(24.0f).withMargin({ 0.0f, margin, 0.0f, margin }));
        headerFb.items.add(juce::FlexItem(abCopyBtn_).withWidth(58.0f).withHeight(24.0f).withMargin({ 0.0f, margin, 0.0f, margin }));
        headerFb.items.add(juce::FlexItem(msModeBox_).withWidth(108.0f).withHeight(24.0f).withMargin({ 0.0f, margin, 0.0f, margin }));
        headerFb.items.add(juce::FlexItem(dayModeBtn_).withWidth(72.0f).withHeight(24.0f).withMargin({ 0.0f, margin, 0.0f, margin }));
        headerFb.items.add(juce::FlexItem(settingsBtn_).withWidth(68.0f).withHeight(24.0f).withMargin({ 0.0f, margin, 0.0f, margin }));

        const juce::Rectangle<float> centerHeaderArea(180.0f, 8.0f, bounds.getWidth() - 180.0f - 472.0f, 38.0f);
        headerFb.performLayout(centerHeaderArea);
    }

    const float topY   = 52.0f;
    const float panelH = bounds.getHeight() - topY - 10.0f;
    const float totalW = bounds.getWidth() - 20.0f;
    const float gap    = 6.0f;
    const float availW = totalW - 4.0f * gap;

    const float w1   = std::floor(availW * 0.185f);
    const float wDyn = std::floor(availW * 0.265f);
    const float wCol = std::floor(availW * 0.130f);
    const float wEq  = std::floor(availW * 0.280f);
    const float w5   = availW - (w1 + wDyn + wCol + wEq);

    // Placement resserré à 2 px autour du cercle du potentiomètre :
    // - titleLabel (15 px de haut, police 12 pt Gras) juste au-dessus du cercle
    // - TextBox de valeur (16 px de haut, police 12.5 pt Monospace Gras) juste en dessous du cercle
    auto placeRotary = [](RotaryControl& ctrl, float x, float y, float w, float h)
    {
        constexpr float labelH = 15.0f;
        constexpr float textH  = 16.0f;
        const float maxKnobD   = std::max(16.0f, std::min(w, h - labelH - textH));
        const float totalUsedH = labelH + maxKnobD + textH;
        const float startY     = y + std::max(0.0f, (h - totalUsedH) * 0.5f);

        ctrl.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false,
                                    std::min(static_cast<int>(w), 86), static_cast<int>(textH));
        ctrl.titleLabel.setBounds(static_cast<int>(x), static_cast<int>(startY),
                                  static_cast<int>(w), static_cast<int>(labelH));
        ctrl.slider.setBounds(static_cast<int>(x), static_cast<int>(startY + labelH),
                              static_cast<int>(w), static_cast<int>(maxKnobD + textH));
    };

    // -------------------------------------------------------------------------
    // 1. INPUT & FILTERS (Toujours à gauche)
    // -------------------------------------------------------------------------
    float curX = 10.0f;
    {
        const float leftMeterReserve = 48.0f;
        const float pad = 6.0f;
        const float ctrlX = curX + leftMeterReserve;
        const float innerW = w1 - leftMeterReserve - pad;
        const float halfW = (innerW - 4.0f) * 0.5f;
        const float btnH = 22.0f;
        float y = topY + 28.0f;

        phaseInvertBtn_.setBounds(static_cast<int>(ctrlX), static_cast<int>(y), static_cast<int>(innerW), static_cast<int>(btnH));
        y += btnH + 4.0f;

        const float rowH = (panelH - 110.0f) / 3.0f;
        placeRotary(inputGainCtrl_, ctrlX,              y, halfW, rowH);
        placeRotary(inputSatCtrl_,  ctrlX + halfW + 4.0f, y, halfW, rowH);
        y += rowH + 4.0f;

        hpfEnableBtn_.setBounds(static_cast<int>(ctrlX),              static_cast<int>(y), static_cast<int>(halfW), static_cast<int>(btnH));
        lpfEnableBtn_.setBounds(static_cast<int>(ctrlX + halfW + 4.0f), static_cast<int>(y), static_cast<int>(halfW), static_cast<int>(btnH));
        y += btnH + 4.0f;

        placeRotary(hpfFreqCtrl_, ctrlX,              y, halfW, rowH);
        placeRotary(hpfQCtrl_,    ctrlX + halfW + 4.0f, y, halfW, rowH);
        y += rowH + 2.0f;

        const float thirdW = (innerW - 4.0f) / 3.0f;
        placeRotary(lpfFreqCtrl_,     ctrlX,                         y, thirdW, rowH);
        placeRotary(lpfQCtrl_,        ctrlX + thirdW + 2.0f,         y, thirdW, rowH);
        placeRotary(filtPreTrimCtrl_, ctrlX + 2.0f * (thirdW + 2.0f), y, thirdW, rowH);
    }
    curX += w1 + gap;

    // Calcul dynamique des abscisses X des 3 modules centraux selon module_order
    const int orderIdx = processorRef_.getModuleOrderIndex();
    const auto& perm = kalyx::dsp::kModulePermutations[static_cast<std::size_t>(orderIdx)];

    float xDyn = curX;
    float xCol = curX;
    float xEq  = curX;

    for (int slot = 0; slot < 3; ++slot)
    {
        const int modId = perm[static_cast<std::size_t>(slot)];
        if (modId == 0)      { xDyn = curX; curX += wDyn + gap; }
        else if (modId == 1) { xCol = curX; curX += wCol + gap; }
        else                 { xEq  = curX; curX += wEq  + gap; }
    }
    const float x5 = curX;

    // -------------------------------------------------------------------------
    // Module Central A : DYNAMICS & DE-ESSER + SIDECHAIN + AUTO-REL (RC) + SC HPF
    // -------------------------------------------------------------------------
    {
        const float pad = 8.0f;
        const float usableW = wDyn - 126.0f;
        const float btnH = 21.0f;
        float y = topY + 28.0f;

        const float thirdBtnW = (wDyn - 2.0f * pad - 10.0f) / 3.0f;
        dynEnableBtn_.setBounds  (static_cast<int>(xDyn + pad),                          static_cast<int>(y), static_cast<int>(thirdBtnW), static_cast<int>(btnH));
        limiterModeBtn_.setBounds(static_cast<int>(xDyn + pad + thirdBtnW + 5.0f),       static_cast<int>(y), static_cast<int>(thirdBtnW), static_cast<int>(btnH));
        deessEnableBtn_.setBounds(static_cast<int>(xDyn + pad + 2.0f * (thirdBtnW + 5.0f)), static_cast<int>(y), static_cast<int>(thirdBtnW), static_cast<int>(btnH));
        y += btnH + 4.0f;

        const float fourthBtnW = (wDyn - 2.0f * pad - 12.0f) / 4.0f;
        extScBtn_.setBounds      (static_cast<int>(xDyn + pad),                           static_cast<int>(y), static_cast<int>(fourthBtnW), static_cast<int>(btnH));
        filtersToScBtn_.setBounds(static_cast<int>(xDyn + pad + fourthBtnW + 4.0f),       static_cast<int>(y), static_cast<int>(fourthBtnW), static_cast<int>(btnH));
        scListenBtn_.setBounds   (static_cast<int>(xDyn + pad + 2.0f * (fourthBtnW + 4.0f)), static_cast<int>(y), static_cast<int>(fourthBtnW), static_cast<int>(btnH));
        compAutoRelBtn_.setBounds(static_cast<int>(xDyn + pad + 3.0f * (fourthBtnW + 4.0f)), static_cast<int>(y), static_cast<int>(fourthBtnW), static_cast<int>(btnH));
        y += btnH + 6.0f;

        const float colW      = (usableW - 6.0f) * 0.5f;
        const float thirdColW = (usableW - 4.0f) / 3.0f;
        const float rowH      = (panelH - 92.0f) / 4.0f;

        placeRotary(compThreshCtrl_,  xDyn + pad,              y, colW, rowH);
        placeRotary(compRatioCtrl_,   xDyn + pad + colW + 6.0f, y, colW, rowH);
        y += rowH + 2.0f;

        placeRotary(compAttackCtrl_,  xDyn + pad,                           y, thirdColW, rowH);
        placeRotary(compReleaseCtrl_, xDyn + pad + thirdColW + 2.0f,        y, thirdColW, rowH);
        placeRotary(scHpfFreqCtrl_,   xDyn + pad + 2.0f * (thirdColW + 2.0f), y, thirdColW, rowH);
        y += rowH + 2.0f;

        placeRotary(gateThreshCtrl_,  xDyn + pad,              y, colW, rowH);
        placeRotary(gateRangeCtrl_,   xDyn + pad + colW + 6.0f, y, colW, rowH);
        y += rowH + 2.0f;

        placeRotary(deessFreqCtrl_,   xDyn + pad,                           y, thirdColW, rowH);
        placeRotary(deessThreshCtrl_, xDyn + pad + thirdColW + 2.0f,        y, thirdColW, rowH);
        placeRotary(dynMakeupCtrl_,   xDyn + pad + 2.0f * (thirdColW + 2.0f), y, thirdColW, rowH);
    }

    // -------------------------------------------------------------------------
    // Module Central B : CONSOLE COLOR
    // -------------------------------------------------------------------------
    {
        const float pad = 6.0f;
        const float innerW = wCol - 2.0f * pad;
        const float halfW = (innerW - 4.0f) * 0.5f;
        float y = topY + 28.0f;

        colorLinkBtn_.setBounds(static_cast<int>(xCol + pad), static_cast<int>(y), static_cast<int>(innerW), 22);
        y += 25.0f;

        circuitEBtn_.setBounds(static_cast<int>(xCol + pad),              static_cast<int>(y), static_cast<int>(halfW), 22);
        circuitGBtn_.setBounds(static_cast<int>(xCol + pad + halfW + 4.0f), static_cast<int>(y), static_cast<int>(halfW), 22);
        y += 26.0f;

        const float availH = panelH - (y - topY) - 44.0f;
        const float rowH = availH / 3.0f;

        placeRotary(colorAmountCtrl_, xCol + pad, y, innerW, rowH * 1.12f);
        y += rowH * 1.12f + 2.0f;

        placeRotary(satToneCtrl_,   xCol + pad,              y, halfW, rowH * 0.94f);
        placeRotary(crosstalkCtrl_, xCol + pad + halfW + 4.0f, y, halfW, rowH * 0.94f);
        y += rowH * 0.94f + 2.0f;

        placeRotary(colorTrimCtrl_, xCol + pad, y, innerW, rowH * 0.94f);
    }

    // -------------------------------------------------------------------------
    // Module Central C : 4-BAND PARAMETRIC EQ
    // -------------------------------------------------------------------------
    {
        const float pad = 8.0f;
        const float innerW = wEq - 2.0f * pad;
        const float btnH = 22.0f;
        float y = topY + 28.0f;

        const float fourthW = (innerW - 15.0f) / 4.0f;
        eqEnableBtn_.setBounds(static_cast<int>(xEq + pad),                           static_cast<int>(y), static_cast<int>(fourthW), static_cast<int>(btnH));
        eqPreDynBtn_.setBounds(static_cast<int>(xEq + pad + 1.0f * (fourthW + 5.0f)), static_cast<int>(y), static_cast<int>(fourthW), static_cast<int>(btnH));
        lfBellBtn_.setBounds  (static_cast<int>(xEq + pad + 2.0f * (fourthW + 5.0f)), static_cast<int>(y), static_cast<int>(fourthW), static_cast<int>(btnH));
        hfBellBtn_.setBounds  (static_cast<int>(xEq + pad + 3.0f * (fourthW + 5.0f)), static_cast<int>(y), static_cast<int>(fourthW), static_cast<int>(btnH));
        y += btnH + 5.0f;

        const float curveH = 126.0f;
        eqCurveView_.setBounds(static_cast<int>(xEq + pad), static_cast<int>(y), static_cast<int>(innerW), static_cast<int>(curveH));
        y += curveH + 4.0f;

        const float colW = innerW / 4.0f;
        const float rowH = (panelH - (y - topY) - 8.0f) / 3.0f;

        placeRotary(lfGainCtrl_,  xEq + pad + 0.0f * colW, y,        colW, rowH);
        placeRotary(lfFreqCtrl_,  xEq + pad + 0.0f * colW, y + rowH, colW, rowH);

        placeRotary(lmfGainCtrl_, xEq + pad + 1.0f * colW, y,               colW, rowH);
        placeRotary(lmfFreqCtrl_, xEq + pad + 1.0f * colW, y + rowH,        colW, rowH);
        placeRotary(lmfQCtrl_,    xEq + pad + 1.0f * colW, y + 2.0f * rowH, colW, rowH);

        placeRotary(hmfGainCtrl_, xEq + pad + 2.0f * colW, y,               colW, rowH);
        placeRotary(hmfFreqCtrl_, xEq + pad + 2.0f * colW, y + rowH,        colW, rowH);
        placeRotary(hmfQCtrl_,    xEq + pad + 2.0f * colW, y + 2.0f * rowH, colW, rowH);

        placeRotary(hfGainCtrl_,  xEq + pad + 3.0f * colW, y,               colW, rowH);
        placeRotary(hfFreqCtrl_,  xEq + pad + 3.0f * colW, y + rowH,        colW, rowH);
        placeRotary(eqTrimCtrl_,  xEq + pad + 3.0f * colW, y + 2.0f * rowH, colW, rowH);
    }

    // -------------------------------------------------------------------------
    // 5. OUTPUT & METERS (Toujours à droite)
    // -------------------------------------------------------------------------
    {
        const float pad = 8.0f;
        const float usableW = w5 - 56.0f;
        float y = topY + 28.0f;

        autoGainBtn_.setBounds(static_cast<int>(x5 + pad), static_cast<int>(y), static_cast<int>(usableW), 22);
        y += 28.0f;

        const float knobH = (panelH - 68.0f) / 3.0f;
        placeRotary(analogTolCtrl_,  x5 + pad, y, usableW, knobH);
        y += knobH + 2.0f;
        placeRotary(mixCtrl_,        x5 + pad, y, usableW, knobH);
        y += knobH + 2.0f;
        placeRotary(outputGainCtrl_, x5 + pad, y, usableW, knobH);
    }
}

