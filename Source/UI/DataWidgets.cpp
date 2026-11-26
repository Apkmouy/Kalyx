#include "DataWidgets.h"
#include <algorithm>
#include <cmath>

// =============================================================================
// 1. TactileFaderBar
// =============================================================================
void TactileFaderBar::init(juce::Component& parent,
                           juce::AudioProcessorValueTreeState& apvts,
                           const juce::String& paramID,
                           const juce::String& title,
                           juce::Colour accentColour,
                           bool bipolar,
                           bool darkText,
                           double defaultResetValue,
                           std::function<juce::String(double)> valueFormatter,
                           const juce::String& infoHint,
                           std::function<double(const juce::String&)> valueParser)
{
    title_             = title;
    accent_            = accentColour;
    bipolar_           = bipolar;
    darkText_          = darkText;
    defaultResetValue_ = defaultResetValue;
    formatter_         = std::move(valueFormatter);
    parser_            = std::move(valueParser);

    if (infoHint.isNotEmpty())
        setTooltip(infoHint);

    param_ = apvts.getParameter(paramID);
    att_.reset();
    if (param_ != nullptr)
    {
        currentValue_ = static_cast<double>(param_->convertFrom0to1(param_->getValue()));
        att_ = std::make_unique<juce::ParameterAttachment>(
            *param_,
            [this](float newVal)
            {
                currentValue_ = static_cast<double>(newVal);
                repaint();
            },
            apvts.undoManager);
        att_->sendInitialUpdate();
    }

    parent.addAndMakeVisible(*this);
}

void TactileFaderBar::setThemeColours(juce::Colour accentColour, bool darkText) noexcept
{
    accent_   = accentColour;
    darkText_ = darkText;
    repaint();
}

void TactileFaderBar::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced(0.5f);
    if (b.isEmpty()) return;

    const float sc     = uiScale_;
    const float radius = 3.0f * sc;
    const float alpha  = dimmed_ ? 0.35f : 1.0f;

    const auto bgCol     = darkText_ ? juce::Colour(0xffe1e5ee) : juce::Colour(0xff141820);
    const auto borderCol = darkText_ ? juce::Colour(0xffc2c9d8) : juce::Colour(0xff2b3140);
    const auto fillCol   = (dimmed_ ? juce::Colour(0xff6b7280) : accent_).withAlpha(0.55f * alpha);
    const auto barCol    = (dimmed_ ? juce::Colour(0xff6b7280) : accent_).withAlpha(0.95f * alpha);

    g.setColour(bgCol);
    g.fillRoundedRectangle(b, radius);

    // Normalisation [0..1] depuis param_
    double norm = 0.5;
    if (param_ != nullptr)
        norm = std::clamp(static_cast<double>(param_->convertTo0to1(static_cast<float>(currentValue_))), 0.0, 1.0);

    if (bipolar_)
    {
        const float midX = b.getCentreX();
        const float valX = b.getX() + static_cast<float>(norm) * b.getWidth();
        const float fx   = std::min(midX, valX);
        const float fw   = std::abs(valX - midX);
        if (fw > 0.5f)
        {
            g.setColour(fillCol);
            g.fillRoundedRectangle(fx, b.getY() + 1.0f, fw, b.getHeight() - 2.0f, 2.0f * sc);
        }
        g.setColour(borderCol);
        g.drawVerticalLine(static_cast<int>(midX), b.getY() + 2.0f, b.getBottom() - 2.0f);
        g.setColour(barCol);
        g.fillRect(valX - 1.5f * sc, b.getY() + 1.5f, 3.0f * sc, b.getHeight() - 3.0f);
    }
    else
    {
        const float fw = static_cast<float>(norm) * (b.getWidth() - 2.0f);
        if (fw > 0.5f)
        {
            g.setColour(fillCol);
            g.fillRoundedRectangle(b.getX() + 1.0f, b.getY() + 1.0f, fw, b.getHeight() - 2.0f, 2.0f * sc);
        }
        const float edgeX = b.getX() + 1.0f + fw;
        g.setColour(barCol);
        g.fillRect(std::clamp(edgeX - 1.5f * sc, b.getX() + 1.0f, b.getRight() - 3.0f * sc),
                   b.getY() + 1.5f, 2.5f * sc, b.getHeight() - 3.0f);
    }

    g.setColour(borderCol);
    g.drawRoundedRectangle(b, radius, 1.0f);

    const auto txtCol = (darkText_ ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary).withAlpha(alpha);
    g.setColour(txtCol);
    g.setFont(juce::Font(juce::FontOptions(10.0f * sc, juce::Font::bold)));
    g.drawText(title_, static_cast<int>(b.getX() + 6.0f * sc), static_cast<int>(b.getY()),
               static_cast<int>(b.getWidth() * 0.55f), static_cast<int>(b.getHeight()),
               juce::Justification::centredLeft, true);

    const juce::String valStr = formatter_ ? formatter_(currentValue_) : juce::String(currentValue_, 2);
    g.setFont(KalyxTheme::makeMonoFont(10.5f * sc, juce::Font::bold));
    g.drawText(valStr, static_cast<int>(b.getX() + b.getWidth() * 0.42f), static_cast<int>(b.getY()),
               static_cast<int>(b.getWidth() * 0.58f - 6.0f * sc), static_cast<int>(b.getHeight()),
               juce::Justification::centredRight, true);
}

void TactileFaderBar::resized()
{
    if (inlineEditor_)
        inlineEditor_->setBounds(getLocalBounds().reduced(2));
}

void TactileFaderBar::mouseDown(const juce::MouseEvent& e)
{
    if (dimmed_ || param_ == nullptr) return;
    dragStartX_ = e.position.x;
    dragStartValue_ = static_cast<double>(param_->convertTo0to1(static_cast<float>(currentValue_)));
    if (att_) att_->beginGesture();
}

void TactileFaderBar::mouseDrag(const juce::MouseEvent& e)
{
    if (dimmed_ || param_ == nullptr) return;
    const bool fine = e.mods.isShiftDown() || e.mods.isCommandDown() || e.mods.isCtrlDown();
    const float sens = fine ? 0.1f : 1.0f;
    const float dx = (e.position.x - dragStartX_) * sens;
    const double nextNorm = std::clamp(dragStartValue_ + static_cast<double>(dx) / std::max(40.0f, static_cast<float>(getWidth())), 0.0, 1.0);
    const float plain = param_->convertFrom0to1(static_cast<float>(nextNorm));
    if (att_) att_->setValueAsPartOfGesture(plain);
}

void TactileFaderBar::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (dimmed_ || param_ == nullptr) return;
    if (e.position.x >= static_cast<float>(getWidth()) * 0.52f && parser_)
    {
        openInlineEditor();
        return;
    }
    if (att_)
    {
        att_->beginGesture();
        att_->setValueAsPartOfGesture(static_cast<float>(defaultResetValue_));
        att_->endGesture();
    }
}

void TactileFaderBar::openInlineEditor()
{
    inlineEditor_ = std::make_unique<juce::TextEditor>();
    inlineEditor_->setFont(KalyxTheme::makeMonoFont(10.5f * uiScale_, juce::Font::bold));
    inlineEditor_->setJustification(juce::Justification::centredRight);
    inlineEditor_->setText(formatter_ ? formatter_(currentValue_) : juce::String(currentValue_, 2), false);
    inlineEditor_->selectAll();
    inlineEditor_->onReturnKey = [this]() { closeInlineEditor(true); };
    inlineEditor_->onEscapeKey = [this]() { closeInlineEditor(false); };
    inlineEditor_->onFocusLost = [this]() { closeInlineEditor(true); };
    addAndMakeVisible(*inlineEditor_);
    inlineEditor_->setBounds(getLocalBounds().reduced(2));
    inlineEditor_->grabKeyboardFocus();
}

void TactileFaderBar::closeInlineEditor(bool commit)
{
    if (!inlineEditor_) return;
    const juce::String txt = inlineEditor_->getText();
    inlineEditor_.reset();
    if (commit && parser_ && param_ != nullptr && att_)
    {
        const double parsed = parser_(txt);
        att_->beginGesture();
        att_->setValueAsPartOfGesture(static_cast<float>(parsed));
        att_->endGesture();
    }
    repaint();
}

// =============================================================================
// 2. VerticalConsoleFader
// =============================================================================
void VerticalConsoleFader::init(juce::Component& parent,
                                juce::AudioProcessorValueTreeState& apvts,
                                const juce::String& paramID,
                                const juce::String& title,
                                juce::Colour accentColour,
                                bool darkText,
                                double minDb,
                                double maxDb,
                                double defaultDb,
                                const juce::String& infoHint)
{
    title_     = title;
    accent_    = accentColour;
    darkText_  = darkText;
    minDb_     = minDb;
    maxDb_     = maxDb;
    defaultDb_ = defaultDb;

    if (infoHint.isNotEmpty())
        setTooltip(infoHint);

    param_ = apvts.getParameter(paramID);
    att_.reset();
    if (param_ != nullptr)
    {
        currentDb_ = static_cast<double>(param_->convertFrom0to1(param_->getValue()));
        att_ = std::make_unique<juce::ParameterAttachment>(
            *param_,
            [this](float newVal)
            {
                currentDb_ = static_cast<double>(newVal);
                repaint();
            },
            apvts.undoManager);
        att_->sendInitialUpdate();
    }

    parent.addAndMakeVisible(*this);
}

void VerticalConsoleFader::setThemeColours(juce::Colour accentColour, bool darkText) noexcept
{
    accent_   = accentColour;
    darkText_ = darkText;
    repaint();
}

float VerticalConsoleFader::dbToY(double db, const juce::Rectangle<float>& track) const noexcept
{
    // Courbe audio-taper : +12 en haut, 0.0 dB à ~74% de la hauteur, -60 en bas
    const double t = std::clamp((db - minDb_) / std::max(1.0, maxDb_ - minDb_), 0.0, 1.0);
    return track.getBottom() - static_cast<float>(t) * track.getHeight();
}

double VerticalConsoleFader::yToDb(float y, const juce::Rectangle<float>& track) const noexcept
{
    const double t = std::clamp(static_cast<double>(track.getBottom() - y) / std::max(1.0, static_cast<double>(track.getHeight())), 0.0, 1.0);
    return minDb_ + t * (maxDb_ - minDb_);
}

void VerticalConsoleFader::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    if (b.isEmpty()) return;

    const float sc = uiScale_;
    const auto txtCol = darkText_ ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary;
    const auto secCol = KalyxTheme::getTextSecondary(darkText_ ? kalyx::core::ChassisTheme::Day : kalyx::core::ChassisTheme::Night);

    g.setColour(txtCol);
    g.setFont(juce::Font(juce::FontOptions(10.5f * sc, juce::Font::bold)));
    g.drawText(title_, static_cast<int>(b.getX()), static_cast<int>(b.getY()),
               static_cast<int>(b.getWidth()), static_cast<int>(15.0f * sc), juce::Justification::centred);

    if (!trackRect_.isEmpty())
    {
        const float cx = trackRect_.getCentreX();
        const float slotW = 5.0f * sc;
        const juce::Rectangle<float> slot(cx - slotW * 0.5f, trackRect_.getY(), slotW, trackRect_.getHeight());

        g.setColour(darkText_ ? juce::Colour(0xffcbd2df) : juce::Colour(0xff10131a));
        g.fillRoundedRectangle(slot, 2.5f * sc);

        // Graduations dB (+12, +6, 0, -6, -12, -24, -48)
        static constexpr double kTicks[7] = { 12.0, 6.0, 0.0, -6.0, -12.0, -24.0, -48.0 };
        g.setFont(KalyxTheme::makeMonoFont(8.0f * sc, juce::Font::plain));
        for (double tk : kTicks)
        {
            if (tk < minDb_ || tk > maxDb_) continue;
            const float ty = dbToY(tk, trackRect_);
            const bool isZero = (std::abs(tk) < 0.01);
            g.setColour(isZero ? accent_ : secCol.withAlpha(0.5f));
            g.drawHorizontalLine(static_cast<int>(ty), cx - 16.0f * sc, cx - 5.0f * sc);
            g.drawHorizontalLine(static_cast<int>(ty), cx + 5.0f * sc, cx + 16.0f * sc);
            if (isZero || std::abs(tk - 12.0) < 0.01 || std::abs(tk + 12.0) < 0.01 || std::abs(tk + 48.0) < 0.01)
            {
                g.setColour(isZero ? accent_ : secCol);
                g.drawText(isZero ? "0" : juce::String(static_cast<int>(tk)),
                           static_cast<int>(cx + 18.0f * sc), static_cast<int>(ty - 5.5f * sc),
                           static_cast<int>(22.0f * sc), static_cast<int>(11.0f * sc),
                           juce::Justification::centredLeft);
            }
        }

        // Remplissage actif depuis 0 dB ou depuis le bas
        const float knobY = dbToY(currentDb_, trackRect_);
        const float fillY = std::min(knobY, trackRect_.getBottom());
        g.setColour(accent_.withAlpha(0.75f));
        g.fillRoundedRectangle(cx - 2.0f * sc, fillY, 4.0f * sc, trackRect_.getBottom() - fillY, 2.0f * sc);

        // Curseur tactile plat (cap)
        const float capW = 28.0f * sc;
        const float capH = 13.0f * sc;
        const juce::Rectangle<float> cap(cx - capW * 0.5f, knobY - capH * 0.5f, capW, capH);
        g.setColour(darkText_ ? juce::Colour(0xff2c3340) : juce::Colour(0xff262c38));
        g.fillRoundedRectangle(cap, 3.0f * sc);
        g.setColour(accent_);
        g.drawRoundedRectangle(cap, 3.0f * sc, 1.2f * sc);
        g.fillRect(cap.getX() + 3.0f * sc, knobY - 0.8f * sc, cap.getWidth() - 6.0f * sc, 1.6f * sc);
    }

    if (!valueRect_.isEmpty())
    {
        const juce::String valStr = (currentDb_ <= minDb_ + 0.2) ? "-inf dB" : KalyxValueParsers::formatDbClean(currentDb_);
        g.setColour(txtCol);
        g.setFont(KalyxTheme::makeMonoFont(11.0f * sc, juce::Font::bold));
        g.drawText(valStr, valueRect_.toNearestInt(), juce::Justification::centred);
    }
}

void VerticalConsoleFader::resized()
{
    const auto b = getLocalBounds().toFloat();
    const float topH = 16.0f * uiScale_;
    const float botH = 18.0f * uiScale_;
    trackRect_ = { b.getX(), b.getY() + topH + 6.0f * uiScale_,
                   b.getWidth(), std::max(24.0f, b.getHeight() - topH - botH - 12.0f * uiScale_) };
    valueRect_ = { b.getX(), b.getBottom() - botH, b.getWidth(), botH };
    if (inlineEditor_)
        inlineEditor_->setBounds(valueRect_.toNearestInt());
}

void VerticalConsoleFader::mouseDown(const juce::MouseEvent& e)
{
    if (param_ == nullptr) return;
    dragStartY_  = e.position.y;
    dragStartDb_ = currentDb_;
    if (att_) att_->beginGesture();
}

void VerticalConsoleFader::mouseDrag(const juce::MouseEvent& e)
{
    if (param_ == nullptr || trackRect_.isEmpty()) return;
    const bool fine = e.mods.isShiftDown() || e.mods.isCommandDown() || e.mods.isCtrlDown();
    const float sens = fine ? 0.1f : 1.0f;
    const double dbRange = maxDb_ - minDb_;
    const double deltaDb = -static_cast<double>((e.position.y - dragStartY_) * sens) / std::max(20.0f, trackRect_.getHeight()) * dbRange;
    double nextDb = std::clamp(dragStartDb_ + deltaDb, minDb_, maxDb_);
    // Repère 0.0 dB magnétique (±0.4 dB)
    if (!fine && std::abs(nextDb) < 0.4)
        nextDb = 0.0;
    if (att_) att_->setValueAsPartOfGesture(static_cast<float>(nextDb));
}

void VerticalConsoleFader::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (param_ == nullptr) return;
    if (valueRect_.contains(e.position))
    {
        openInlineEditor();
        return;
    }
    if (att_)
    {
        att_->beginGesture();
        att_->setValueAsPartOfGesture(static_cast<float>(defaultDb_));
        att_->endGesture();
    }
}

void VerticalConsoleFader::openInlineEditor()
{
    inlineEditor_ = std::make_unique<juce::TextEditor>();
    inlineEditor_->setFont(KalyxTheme::makeMonoFont(11.0f * uiScale_, juce::Font::bold));
    inlineEditor_->setJustification(juce::Justification::centred);
    inlineEditor_->setText(KalyxValueParsers::formatDbClean(currentDb_), false);
    inlineEditor_->selectAll();
    inlineEditor_->onReturnKey = [this]() { closeInlineEditor(true); };
    inlineEditor_->onEscapeKey = [this]() { closeInlineEditor(false); };
    inlineEditor_->onFocusLost = [this]() { closeInlineEditor(true); };
    addAndMakeVisible(*inlineEditor_);
    inlineEditor_->setBounds(valueRect_.toNearestInt());
    inlineEditor_->grabKeyboardFocus();
}

void VerticalConsoleFader::closeInlineEditor(bool commit)
{
    if (!inlineEditor_) return;
    const juce::String txt = inlineEditor_->getText();
    inlineEditor_.reset();
    if (commit && param_ != nullptr && att_)
    {
        juce::String suf;
        const double val = std::clamp(KalyxValueParsers::parseCleanNumber(txt, suf), minDb_, maxDb_);
        att_->beginGesture();
        att_->setValueAsPartOfGesture(static_cast<float>(val));
        att_->endGesture();
    }
    repaint();
}

// =============================================================================
// 3. InteractiveTransferCurve
// =============================================================================
void InteractiveTransferCurve::init(juce::Component& parent,
                                    juce::AudioProcessorValueTreeState& apvts,
                                    const juce::String& threshParamID,
                                    float minThreshDb,
                                    float maxThreshDb,
                                    float defaultThreshNorm,
                                    bool isOptoPeakReduction,
                                    juce::Colour accentColour,
                                    bool darkText,
                                    const juce::String& infoHint)
{
    minThreshDb_ = minThreshDb;
    maxThreshDb_ = maxThreshDb;
    defNorm_     = defaultThreshNorm;
    isOpto_      = isOptoPeakReduction;
    accent_      = accentColour;
    darkText_    = darkText;

    if (infoHint.isNotEmpty())
        setTooltip(infoHint);

    param_ = apvts.getParameter(threshParamID);
    att_.reset();
    if (param_ != nullptr)
    {
        curNorm_ = std::clamp(param_->getValue(), 0.0f, 1.0f);
        att_ = std::make_unique<juce::ParameterAttachment>(
            *param_,
            [this](float newVal)
            {
                if (param_ != nullptr)
                    curNorm_ = std::clamp(param_->convertTo0to1(newVal), 0.0f, 1.0f);
                repaint();
            },
            apvts.undoManager);
        att_->sendInitialUpdate();
    }

    parent.addAndMakeVisible(*this);
}

void InteractiveTransferCurve::setCurveConfig(CurveType type, float ratio, float kneeDb, bool active, bool steppedVintage) noexcept
{
    if (curveType_ == type && std::abs(ratio_ - ratio) < 0.01f && std::abs(kneeDb_ - kneeDb) < 0.01f
        && active_ == active && stepped_ == steppedVintage)
        return;

    curveType_ = type;
    ratio_     = ratio;
    kneeDb_    = kneeDb;
    active_    = active;
    stepped_   = steppedVintage;
    repaint();
}

void InteractiveTransferCurve::setTelemetry(float inputPeakDb, float grDb) noexcept
{
    if (std::abs(inPeakDb_ - inputPeakDb) > 0.25f || std::abs(grDb_ - grDb) > 0.1f)
    {
        inPeakDb_ = inputPeakDb;
        grDb_     = std::max(0.0f, grDb);
        repaint();
    }
}

void InteractiveTransferCurve::setThemeColours(juce::Colour accentColour, bool darkText) noexcept
{
    accent_   = accentColour;
    darkText_ = darkText;
    repaint();
}

float InteractiveTransferCurve::getCurrentThreshDb() const noexcept
{
    if (isOpto_)
    {
        // Peak Reduction 0..100 % -> Threshold effectif de -2 dB à -42 dB
        return -2.0f - curNorm_ * 40.0f;
    }
    return kalyx::core::mapLin(curNorm_, minThreshDb_, maxThreshDb_);
}

float InteractiveTransferCurve::computeOutputDb(float inDb) const noexcept
{
    const float thr = getCurrentThreshDb();
    const float over = inDb - thr;
    float gr = 0.0f;

    switch (curveType_)
    {
        case CurveType::Limiter:
        {
            if (over > 0.0f) gr = over;
            break;
        }
        case CurveType::Opto:
        case CurveType::VariMu:
        {
            const float softK = (curveType_ == CurveType::VariMu) ? 14.0f : 12.0f;
            const float slope = 1.0f - (1.0f / std::max(1.2f, ratio_));
            if (over > -softK * 0.5f && over < softK * 0.5f)
            {
                const float x = over + softK * 0.5f;
                gr = slope * (x * x) / (2.0f * softK);
            }
            else if (over >= softK * 0.5f)
            {
                gr = slope * over;
            }
            break;
        }
        case CurveType::FETSlam:
        case CurveType::DistressorBrit:
        {
            // Courbe All-Buttons / British légèrement repliée sur les fortes crêtes
            if (over > 0.0f)
                gr = over * 0.92f + 1.8f * std::tanh(over * 0.12f);
            break;
        }
        case CurveType::VCA:
        case CurveType::FET:
        case CurveType::Diode:
        case CurveType::Distressor:
        default:
        {
            const float k = std::max(0.5f, kneeDb_);
            const float slope = 1.0f - (1.0f / std::max(1.05f, ratio_));
            if (over > -k * 0.5f && over < k * 0.5f)
            {
                const float x = over + k * 0.5f;
                gr = slope * (x * x) / (2.0f * k);
            }
            else if (over >= k * 0.5f)
            {
                gr = slope * over;
            }
            break;
        }
    }

    return std::clamp(inDb - gr, -48.0f, 0.0f);
}

void InteractiveTransferCurve::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced(1.0f);
    if (b.isEmpty()) return;

    const float sc    = uiScale_;
    const float alpha = active_ ? 1.0f : 0.35f;

    const auto bgCol     = darkText_ ? juce::Colour(0xffe6eaf2) : juce::Colour(0xff10131a);
    const auto borderCol = darkText_ ? juce::Colour(0xffc4ccd9) : juce::Colour(0xff272d3b);
    const auto gridCol   = darkText_ ? juce::Colour(0xffd0d7e4) : juce::Colour(0xff1c212e);

    g.setColour(bgCol);
    g.fillRoundedRectangle(b, 4.0f * sc);
    g.setColour(borderCol);
    g.drawRoundedRectangle(b, 4.0f * sc, 1.0f);

    const auto area = b.reduced(3.0f);
    auto dbToX = [&](float db) { return area.getX() + std::clamp((db + 48.0f) / 48.0f, 0.0f, 1.0f) * area.getWidth(); };
    auto dbToY = [&](float db) { return area.getBottom() - std::clamp((db + 48.0f) / 48.0f, 0.0f, 1.0f) * area.getHeight(); };

    // Grille -12, -24, -36 dB
    g.setColour(gridCol);
    for (float db : { -12.0f, -24.0f, -36.0f })
    {
        g.drawHorizontalLine(static_cast<int>(dbToY(db)), area.getX(), area.getRight());
        g.drawVerticalLine(static_cast<int>(dbToX(db)), area.getY(), area.getBottom());
    }

    // Diagonale 1:1 de référence
    g.setColour(gridCol.brighter(0.25f));
    g.drawLine(area.getX(), area.getBottom(), area.getRight(), area.getY(), 1.0f);

    // Courbe de transfert dynamique
    juce::Path curve;
    constexpr int kPts = 48;
    for (int i = 0; i <= kPts; ++i)
    {
        const float inDb  = -48.0f + (static_cast<float>(i) / static_cast<float>(kPts)) * 48.0f;
        const float outDb = computeOutputDb(inDb);
        const float px = dbToX(inDb);
        const float py = dbToY(outDb);
        if (i == 0) curve.startNewSubPath(px, py);
        else        curve.lineTo(px, py);
    }

    const auto lineCol = (active_ ? accent_ : juce::Colour(0xff6b7280)).withAlpha(alpha);
    g.setColour(lineCol);
    g.strokePath(curve, juce::PathStrokeType(1.8f * sc));

    // Point de Threshold interactif
    const float thrDb = std::clamp(getCurrentThreshDb(), -48.0f, 0.0f);
    const float tx = dbToX(thrDb);
    const float ty = dbToY(computeOutputDb(thrDb));
    g.setColour(lineCol.withAlpha(0.35f * alpha));
    g.drawVerticalLine(static_cast<int>(tx), area.getY(), area.getBottom());
    g.setColour(lineCol);
    g.fillEllipse(tx - 3.5f * sc, ty - 3.5f * sc, 7.0f * sc, 7.0f * sc);

    // Bille lumineuse temps réel selon le niveau entrant / GR
    if (active_ && inPeakDb_ > -47.5f)
    {
        const float clIn = std::clamp(inPeakDb_, -48.0f, 0.0f);
        const float bx   = dbToX(clIn);
        const float by   = dbToY(std::clamp(clIn - grDb_, -48.0f, 0.0f));
        g.setColour(KalyxTheme::amberLed.withAlpha(0.35f));
        g.fillEllipse(bx - 6.0f * sc, by - 6.0f * sc, 12.0f * sc, 12.0f * sc);
        g.setColour(KalyxTheme::amberLed);
        g.fillEllipse(bx - 3.0f * sc, by - 3.0f * sc, 6.0f * sc, 6.0f * sc);
    }

    // Valeur numérique de GR intégrée dans le coin supérieur gauche
    g.setFont(KalyxTheme::makeMonoFont(9.5f * sc, juce::Font::bold));
    const auto secCol = KalyxTheme::getTextSecondary(darkText_ ? kalyx::core::ChassisTheme::Day : kalyx::core::ChassisTheme::Night);
    g.setColour(grDb_ > 0.2f ? KalyxTheme::amberLed : secCol);
    const juce::String grText = (grDb_ < 0.05f) ? "GR 0.0 dB" : ("GR -" + juce::String(grDb_, 1) + " dB");
    g.drawText(grText, static_cast<int>(area.getX() + 3.0f), static_cast<int>(area.getY() + 1.0f),
               static_cast<int>(area.getWidth() - 6.0f), static_cast<int>(13.0f * sc),
               juce::Justification::centredLeft);
}

void InteractiveTransferCurve::mouseDown(const juce::MouseEvent& e)
{
    if (param_ == nullptr) return;
    dragStartX_    = e.position.x;
    dragStartY_    = e.position.y;
    dragStartNorm_ = curNorm_;
    if (att_) att_->beginGesture();
}

void InteractiveTransferCurve::mouseDrag(const juce::MouseEvent& e)
{
    if (param_ == nullptr) return;
    const bool fine = e.mods.isShiftDown() || e.mods.isCommandDown() || e.mods.isCtrlDown();
    const float sens = fine ? 0.15f : 1.0f;
    const float delta = ((e.position.x - dragStartX_) - (e.position.y - dragStartY_)) * 0.5f * sens;
    float nextNorm = std::clamp(dragStartNorm_ + delta / std::max(40.0f, static_cast<float>(getWidth())), 0.0f, 1.0f);

    if (stepped_ && !isOpto_ && !fine)
    {
        float db = kalyx::core::mapLin(nextNorm, minThreshDb_, maxThreshDb_);
        db = std::round(db / 2.0f) * 2.0f;
        nextNorm = kalyx::core::unmapLin(db, minThreshDb_, maxThreshDb_);
    }

    if (att_) att_->setValueAsPartOfGesture(param_->convertFrom0to1(nextNorm));
}

void InteractiveTransferCurve::mouseDoubleClick(const juce::MouseEvent&)
{
    if (param_ == nullptr || !att_) return;
    att_->beginGesture();
    att_->setValueAsPartOfGesture(param_->convertFrom0to1(defNorm_));
    att_->endGesture();
}

// =============================================================================
// 4. VectorPad2D
// =============================================================================
void VectorPad2D::init(juce::Component& parent,
                       juce::AudioProcessorValueTreeState& apvts,
                       const juce::String& paramXId,
                       const juce::String& paramYId,
                       const juce::String& labelX,
                       const juce::String& labelY,
                       float defNormX,
                       float defNormY,
                       bool bipolar,
                       juce::Colour accentColour,
                       bool darkText,
                       std::function<juce::String(double)> fmtX,
                       std::function<juce::String(double)> fmtY,
                       const juce::String& infoHint)
{
    labelX_   = labelX;
    labelY_   = labelY;
    defX_     = defNormX;
    defY_     = defNormY;
    bipolar_  = bipolar;
    accent_   = accentColour;
    darkText_ = darkText;
    fmtX_     = std::move(fmtX);
    fmtY_     = std::move(fmtY);

    if (infoHint.isNotEmpty())
        setTooltip(infoHint);

    paramX_ = apvts.getParameter(paramXId);
    paramY_ = apvts.getParameter(paramYId);
    attX_.reset();
    attY_.reset();

    if (paramX_ != nullptr)
    {
        normX_ = std::clamp(paramX_->getValue(), 0.0f, 1.0f);
        attX_ = std::make_unique<juce::ParameterAttachment>(
            *paramX_, [this](float v) { if (paramX_) normX_ = paramX_->convertTo0to1(v); repaint(); }, apvts.undoManager);
        attX_->sendInitialUpdate();
    }
    if (paramY_ != nullptr)
    {
        normY_ = std::clamp(paramY_->getValue(), 0.0f, 1.0f);
        attY_ = std::make_unique<juce::ParameterAttachment>(
            *paramY_, [this](float v) { if (paramY_) normY_ = paramY_->convertTo0to1(v); repaint(); }, apvts.undoManager);
        attY_->sendInitialUpdate();
    }

    parent.addAndMakeVisible(*this);
}

void VectorPad2D::setThemeColours(juce::Colour accentColour, bool darkText) noexcept
{
    accent_   = accentColour;
    darkText_ = darkText;
    repaint();
}

void VectorPad2D::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced(1.0f);
    if (b.isEmpty()) return;

    const float sc    = uiScale_;
    const float alpha = dimmed_ ? 0.35f : 1.0f;

    const auto bgCol     = darkText_ ? juce::Colour(0xffe6eaf2) : juce::Colour(0xff10131a);
    const auto borderCol = darkText_ ? juce::Colour(0xffc4ccd9) : juce::Colour(0xff272d3b);
    const auto gridCol   = darkText_ ? juce::Colour(0xffd0d7e4) : juce::Colour(0xff1e2432);

    g.setColour(bgCol);
    g.fillRoundedRectangle(b, 4.0f * sc);
    g.setColour(borderCol);
    g.drawRoundedRectangle(b, 4.0f * sc, 1.0f);

    const auto area = b.reduced(4.0f);
    g.setColour(gridCol);
    g.drawHorizontalLine(static_cast<int>(area.getCentreY()), area.getX(), area.getRight());
    g.drawVerticalLine(static_cast<int>(area.getCentreX()), area.getY(), area.getBottom());

    const float px = area.getX() + std::clamp(normX_, 0.0f, 1.0f) * area.getWidth();
    const float py = area.getBottom() - std::clamp(normY_, 0.0f, 1.0f) * area.getHeight();
    const auto col = (dimmed_ ? juce::Colour(0xff6b7280) : accent_).withAlpha(alpha);

    g.setColour(col.withAlpha(0.35f * alpha));
    g.drawHorizontalLine(static_cast<int>(py), area.getX(), area.getRight());
    g.drawVerticalLine(static_cast<int>(px), area.getY(), area.getBottom());

    g.setColour(col.withAlpha(0.25f * alpha));
    g.fillEllipse(px - 8.0f * sc, py - 8.0f * sc, 16.0f * sc, 16.0f * sc);
    g.setColour(col);
    g.fillEllipse(px - 4.0f * sc, py - 4.0f * sc, 8.0f * sc, 8.0f * sc);

    const auto txtCol = (darkText_ ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary).withAlpha(0.85f * alpha);
    g.setColour(txtCol);
    g.setFont(KalyxTheme::makeMonoFont(9.0f * sc, juce::Font::bold));
    const juce::String sx = labelX_ + " " + (fmtX_ ? fmtX_(normX_) : juce::String(juce::roundToInt(normX_ * 100.0f)) + "%");
    const juce::String sy = labelY_ + " " + (fmtY_ ? fmtY_(normY_) : juce::String(juce::roundToInt(normY_ * 100.0f)) + "%");
    g.drawText(sx, static_cast<int>(area.getX() + 2.0f), static_cast<int>(area.getBottom() - 12.0f * sc),
               static_cast<int>(area.getWidth() * 0.55f), static_cast<int>(12.0f * sc), juce::Justification::centredLeft);
    g.drawText(sy, static_cast<int>(area.getX() + 2.0f), static_cast<int>(area.getY() + 1.0f),
               static_cast<int>(area.getWidth() * 0.65f), static_cast<int>(12.0f * sc), juce::Justification::centredLeft);
}

void VectorPad2D::mouseDown(const juce::MouseEvent& e)
{
    if (dimmed_) return;
    dragStartX_     = e.position.x;
    dragStartY_     = e.position.y;
    dragStartNormX_ = normX_;
    dragStartNormY_ = normY_;
    if (attX_) attX_->beginGesture();
    if (attY_) attY_->beginGesture();
}

void VectorPad2D::mouseDrag(const juce::MouseEvent& e)
{
    if (dimmed_) return;
    const bool fine = e.mods.isShiftDown() || e.mods.isCommandDown() || e.mods.isCtrlDown();
    const float sens = fine ? 0.15f : 1.0f;
    const float w = std::max(24.0f, static_cast<float>(getWidth()) - 8.0f);
    const float h = std::max(24.0f, static_cast<float>(getHeight()) - 8.0f);

    float nx = std::clamp(dragStartNormX_ + ((e.position.x - dragStartX_) * sens) / w, 0.0f, 1.0f);
    float ny = std::clamp(dragStartNormY_ - ((e.position.y - dragStartY_) * sens) / h, 0.0f, 1.0f);

    if (bipolar_ && !fine)
    {
        if (std::abs(nx - 0.5f) < 0.025f) nx = 0.5f;
        if (std::abs(ny - 0.5f) < 0.025f) ny = 0.5f;
    }
    if (stepped_ && !fine)
    {
        nx = std::round(nx * 10.0f) / 10.0f;
        ny = std::round(ny * 10.0f) / 10.0f;
    }

    if (paramX_ && attX_) attX_->setValueAsPartOfGesture(paramX_->convertFrom0to1(nx));
    if (paramY_ && attY_) attY_->setValueAsPartOfGesture(paramY_->convertFrom0to1(ny));
}

void VectorPad2D::mouseDoubleClick(const juce::MouseEvent&)
{
    if (dimmed_) return;
    if (paramX_ && attX_)
    {
        attX_->beginGesture();
        attX_->setValueAsPartOfGesture(paramX_->convertFrom0to1(defX_));
        attX_->endGesture();
    }
    if (paramY_ && attY_)
    {
        attY_->beginGesture();
        attY_->setValueAsPartOfGesture(paramY_->convertFrom0to1(defY_));
        attY_->endGesture();
    }
}

// =============================================================================
// 5. InputFilterGraph
// =============================================================================
void InputFilterGraph::init(juce::Component& parent,
                            juce::AudioProcessorValueTreeState& apvts,
                            const juce::String& hpfEnId,
                            const juce::String& hpfFreqId,
                            const juce::String& lpfEnId,
                            const juce::String& lpfFreqId,
                            juce::Colour accentColour,
                            bool darkText)
{
    accent_   = accentColour;
    darkText_ = darkText;
    setTooltip("Drag HPF / LPF handles to shape bandwidth (20 Hz - 20 kHz) | Double-Click handle to toggle ON/OFF");

    pHpfEn_   = apvts.getParameter(hpfEnId);
    pHpfFreq_ = apvts.getParameter(hpfFreqId);
    pLpfEn_   = apvts.getParameter(lpfEnId);
    pLpfFreq_ = apvts.getParameter(lpfFreqId);

    attHpfEn_.reset();
    attHpfFreq_.reset();
    attLpfEn_.reset();
    attLpfFreq_.reset();

    if (pHpfEn_)
    {
        hpfOn_ = (pHpfEn_->getValue() > 0.5f);
        attHpfEn_ = std::make_unique<juce::ParameterAttachment>(*pHpfEn_, [this](float v) { hpfOn_ = (v > 0.5f); repaint(); }, apvts.undoManager);
        attHpfEn_->sendInitialUpdate();
    }
    if (pHpfFreq_)
    {
        hpfFreq_ = pHpfFreq_->convertFrom0to1(pHpfFreq_->getValue());
        attHpfFreq_ = std::make_unique<juce::ParameterAttachment>(*pHpfFreq_, [this](float v) { hpfFreq_ = v; repaint(); }, apvts.undoManager);
        attHpfFreq_->sendInitialUpdate();
    }
    if (pLpfEn_)
    {
        lpfOn_ = (pLpfEn_->getValue() > 0.5f);
        attLpfEn_ = std::make_unique<juce::ParameterAttachment>(*pLpfEn_, [this](float v) { lpfOn_ = (v > 0.5f); repaint(); }, apvts.undoManager);
        attLpfEn_->sendInitialUpdate();
    }
    if (pLpfFreq_)
    {
        lpfFreq_ = pLpfFreq_->convertFrom0to1(pLpfFreq_->getValue());
        attLpfFreq_ = std::make_unique<juce::ParameterAttachment>(*pLpfFreq_, [this](float v) { lpfFreq_ = v; repaint(); }, apvts.undoManager);
        attLpfFreq_->sendInitialUpdate();
    }

    parent.addAndMakeVisible(*this);
}

void InputFilterGraph::setThemeColours(juce::Colour accentColour, bool darkText) noexcept
{
    accent_   = accentColour;
    darkText_ = darkText;
    repaint();
}

float InputFilterGraph::freqToX(float hz) const noexcept
{
    const auto area = getLocalBounds().toFloat().reduced(4.0f);
    const float norm = std::log(std::clamp(hz, 20.0f, 20000.0f) / 20.0f) / std::log(1000.0f);
    return area.getX() + norm * area.getWidth();
}

float InputFilterGraph::xToFreq(float x) const noexcept
{
    const auto area = getLocalBounds().toFloat().reduced(4.0f);
    const float norm = std::clamp((x - area.getX()) / std::max(1.0f, area.getWidth()), 0.0f, 1.0f);
    return 20.0f * std::pow(1000.0f, norm);
}

void InputFilterGraph::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced(1.0f);
    if (b.isEmpty()) return;

    const float sc = uiScale_;
    const auto bgCol     = darkText_ ? juce::Colour(0xffe6eaf2) : juce::Colour(0xff10131a);
    const auto borderCol = darkText_ ? juce::Colour(0xffc4ccd9) : juce::Colour(0xff272d3b);
    const auto gridCol   = darkText_ ? juce::Colour(0xffd0d7e4) : juce::Colour(0xff1e2432);

    g.setColour(bgCol);
    g.fillRoundedRectangle(b, 4.0f * sc);
    g.setColour(borderCol);
    g.drawRoundedRectangle(b, 4.0f * sc, 1.0f);

    const auto area = b.reduced(3.0f);
    g.setColour(gridCol);
    for (float f : { 100.0f, 1000.0f, 10000.0f })
        g.drawVerticalLine(static_cast<int>(freqToX(f)), area.getY(), area.getBottom());

    const float y0dB = area.getY() + area.getHeight() * 0.28f;
    g.drawHorizontalLine(static_cast<int>(y0dB), area.getX(), area.getRight());

    // Courbe combinée HPF (18 dB/oct) + LPF (12 dB/oct)
    juce::Path p;
    constexpr int kPts = 64;
    for (int i = 0; i <= kPts; ++i)
    {
        const float hz = 20.0f * std::pow(1000.0f, static_cast<float>(i) / static_cast<float>(kPts));
        double magDb = 0.0;
        if (hpfOn_)
        {
            const double r = std::max(0.01, static_cast<double>(hpfFreq_) / static_cast<double>(hz));
            magDb += -10.0 * std::log10(1.0 + std::pow(r, 6.0));
        }
        if (lpfOn_)
        {
            const double r = std::max(0.01, static_cast<double>(hz) / static_cast<double>(lpfFreq_));
            magDb += -10.0 * std::log10(1.0 + std::pow(r, 4.0));
        }
        const float px = freqToX(hz);
        const float py = std::clamp(y0dB - static_cast<float>(magDb) / 24.0f * (area.getBottom() - y0dB),
                                    area.getY(), area.getBottom());
        if (i == 0) p.startNewSubPath(px, py);
        else        p.lineTo(px, py);
    }

    g.setColour(accent_);
    g.strokePath(p, juce::PathStrokeType(1.6f * sc));

    // Poignées HPF & LPF
    auto drawHandle = [&](float freq, bool on, const juce::String& lbl, juce::Justification just)
    {
        const float hx = freqToX(freq);
        const auto hCol = on ? accent_ : (darkText_ ? juce::Colour(0xff8c96a8) : juce::Colour(0xff4c5568));
        g.setColour(hCol);
        g.fillEllipse(hx - 4.0f * sc, y0dB - 4.0f * sc, 8.0f * sc, 8.0f * sc);
        g.setFont(KalyxTheme::makeMonoFont(8.5f * sc, juce::Font::bold));
        const juce::String fTxt = (freq >= 1000.0f) ? (juce::String(freq * 0.001f, 1) + "k") : juce::String(juce::roundToInt(freq));
        g.drawText(lbl + (on ? (" " + fTxt) : " OFF"),
                   static_cast<int>(area.getX() + 2.0f), static_cast<int>(area.getBottom() - 12.0f * sc),
                   static_cast<int>(area.getWidth() - 4.0f), static_cast<int>(12.0f * sc), just);
    };

    drawHandle(hpfFreq_, hpfOn_, "HPF", juce::Justification::centredLeft);
    drawHandle(lpfFreq_, lpfOn_, "LPF", juce::Justification::centredRight);
}

void InputFilterGraph::mouseDown(const juce::MouseEvent& e)
{
    const float hx = freqToX(hpfFreq_);
    const float lx = freqToX(lpfFreq_);
    dragHandle_ = (std::abs(e.position.x - hx) <= std::abs(e.position.x - lx)) ? 1 : 2;
    if (dragHandle_ == 1 && attHpfFreq_) attHpfFreq_->beginGesture();
    if (dragHandle_ == 2 && attLpfFreq_) attLpfFreq_->beginGesture();
}

void InputFilterGraph::mouseDrag(const juce::MouseEvent& e)
{
    const float hz = xToFreq(e.position.x);
    if (dragHandle_ == 1 && attHpfFreq_)
    {
        if (!hpfOn_ && attHpfEn_)
        {
            attHpfEn_->beginGesture();
            attHpfEn_->setValueAsPartOfGesture(1.0f);
            attHpfEn_->endGesture();
        }
        attHpfFreq_->setValueAsPartOfGesture(std::clamp(hz, 20.0f, 500.0f));
    }
    else if (dragHandle_ == 2 && attLpfFreq_)
    {
        if (!lpfOn_ && attLpfEn_)
        {
            attLpfEn_->beginGesture();
            attLpfEn_->setValueAsPartOfGesture(1.0f);
            attLpfEn_->endGesture();
        }
        attLpfFreq_->setValueAsPartOfGesture(std::clamp(hz, 2000.0f, 20000.0f));
    }
}

void InputFilterGraph::mouseUp(const juce::MouseEvent&)
{
    if (dragHandle_ == 1 && attHpfFreq_) attHpfFreq_->endGesture();
    if (dragHandle_ == 2 && attLpfFreq_) attLpfFreq_->endGesture();
    dragHandle_ = 0;
}

void InputFilterGraph::mouseDoubleClick(const juce::MouseEvent& e)
{
    const float hx = freqToX(hpfFreq_);
    const float lx = freqToX(lpfFreq_);
    if (std::abs(e.position.x - hx) <= std::abs(e.position.x - lx))
    {
        if (attHpfEn_)
        {
            attHpfEn_->beginGesture();
            attHpfEn_->setValueAsPartOfGesture(hpfOn_ ? 0.0f : 1.0f);
            attHpfEn_->endGesture();
        }
    }
    else if (attLpfEn_)
    {
        attLpfEn_->beginGesture();
        attLpfEn_->setValueAsPartOfGesture(lpfOn_ ? 0.0f : 1.0f);
        attLpfEn_->endGesture();
    }
}
