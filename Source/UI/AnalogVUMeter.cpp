#include "AnalogVUMeter.h"
#include <cmath>
#include <algorithm>

void AnalogVUMeter::updateTelemetry(const kalyx::dsp::StudioMeterSnapshot& snap, int vuMode, float vuRefDb) noexcept
{
    vuMode_  = std::clamp(vuMode, 0, 2);
    vuRefDb_ = vuRefDb;
    rawGrDb_ = snap.cumulativeGrDb;

    if (vuMode_ == 0) // IN
    {
        needleL_ = std::clamp(snap.inputVu300DbL - vuRefDb_, -20.0f, 3.0f);
        needleR_ = std::clamp(snap.inputVu300DbR - vuRefDb_, -20.0f, 3.0f);
        peakDb_  = std::max(snap.inputPeakDbL, snap.inputPeakDbR);
    }
    else if (vuMode_ == 1) // OUT
    {
        needleL_ = std::clamp(snap.outputVu300DbL - vuRefDb_, -20.0f, 3.0f);
        needleR_ = std::clamp(snap.outputVu300DbR - vuRefDb_, -20.0f, 3.0f);
        peakDb_  = std::max(snap.outputPeakDbL, snap.outputPeakDbR);
    }
    else // GR (aiguille au repos sur 0 VU, dévie vers la gauche jusqu'à -20 dB)
    {
        const float grVal = std::clamp(-snap.cumulativeGrVuDb, -20.0f, 0.0f);
        needleL_ = grVal;
        needleR_ = grVal;
        peakDb_  = std::max(snap.outputPeakDbL, snap.outputPeakDbR);
    }
}

float AnalogVUMeter::vuDbToAngle(float vuDb) noexcept
{
    // Échelle VU logarithmique authentique (-20 VU -> -42°, 0 VU -> +26°, +3 VU -> +42°)
    const float clamped = std::clamp(vuDb, -20.0f, 3.0f);
    float norm = 0.0f;
    if (clamped <= 0.0f)
        norm = 0.78f * std::pow((clamped + 20.0f) / 20.0f, 1.35f);
    else
        norm = 0.78f + 0.22f * (clamped / 3.0f);

    constexpr float startDeg = -42.0f;
    constexpr float endDeg   =  42.0f;
    const float deg = startDeg + norm * (endDeg - startDeg);
    return deg * (3.14159265f / 180.0f);
}

void AnalogVUMeter::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto bezel = b.reduced(2.0f * uiScale_);

    // Cadre Tactile Flat encastré
    g.setColour(juce::Colour(0xff111318));
    g.fillRoundedRectangle(bezel, 4.0f * uiScale_);
    g.setColour(juce::Colour(0xff262a35));
    g.drawRoundedRectangle(bezel, 4.0f * uiScale_, std::max(0.8f, 1.0f * uiScale_));

    // Cadran rétroéclairé chaud (Ambre crème vintage)
    const auto dial = bezel.reduced(4.0f * uiScale_).withTrimmedBottom(18.0f * uiScale_);
    const juce::Colour dialBg = (theme_ == kalyx::core::ChassisTheme::Day)
                                    ? juce::Colour(0xfff5ebd6)
                                    : juce::Colour(0xffefe0be);
    g.setColour(dialBg);
    g.fillRoundedRectangle(dial, 2.5f * uiScale_);
    g.setColour(juce::Colour(0xff2a241b));
    g.drawRoundedRectangle(dial, 2.5f * uiScale_, std::max(0.8f, 1.0f * uiScale_));

    const float cx = dial.getCentreX();
    const float pivotY = dial.getBottom() + dial.getHeight() * 0.42f;
    const float arcR   = dial.getHeight() * 1.18f;

    // Arc d'échelle : noir (-20..0 VU) et rouge (0..+3 VU)
    const float aMin  = vuDbToAngle(-20.0f);
    const float aZero = vuDbToAngle(0.0f);
    const float aMax  = vuDbToAngle(3.0f);

    juce::Path blackArc;
    blackArc.addCentredArc(cx, pivotY, arcR, arcR, 0.0f, aMin, aZero, true);
    g.setColour(juce::Colour(0xff1b1d22));
    g.strokePath(blackArc, juce::PathStrokeType(1.6f * uiScale_));

    juce::Path redArc;
    redArc.addCentredArc(cx, pivotY, arcR, arcR, 0.0f, aZero, aMax, true);
    g.setColour(juce::Colour(0xffc93030));
    g.strokePath(redArc, juce::PathStrokeType(2.4f * uiScale_));

    // Graduations (-20, -10, -7, -5, -3, 0, +3)
    const float marks[] = { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, 0.0f, 3.0f };
    g.setFont(juce::Font(juce::FontOptions(8.5f * uiScale_, juce::Font::bold)));
    for (float m : marks)
    {
        const float ang  = vuDbToAngle(m);
        const float sinA = std::sin(ang);
        const float cosA = -std::cos(ang);
        const bool isRed = (m > 0.1f);
        g.setColour(isRed ? juce::Colour(0xffc93030) : juce::Colour(0xff1b1d22));
        g.drawLine(cx + (arcR - 5.0f * uiScale_) * sinA, pivotY + (arcR - 5.0f * uiScale_) * cosA,
                   cx + (arcR + 3.0f * uiScale_) * sinA, pivotY + (arcR + 3.0f * uiScale_) * cosA,
                   ((std::abs(m) < 0.1f) ? 1.8f : 1.1f) * uiScale_);

        const float lblR = arcR - 13.0f * uiScale_;
        const juce::String txt = (m > 0.1f) ? "+3" : juce::String(static_cast<int>(m));
        g.drawText(txt,
                   static_cast<int>(cx + lblR * sinA - 10.0f * uiScale_),
                   static_cast<int>(pivotY + lblR * cosA - 5.0f * uiScale_),
                   static_cast<int>(20.0f * uiScale_), static_cast<int>(10.0f * uiScale_),
                   juce::Justification::centred);
    }

    // Étiquette centrale du mode (IN / OUT / GR)
    g.setColour(juce::Colour(0xff12151c));
    g.setFont(juce::Font(juce::FontOptions(10.0f * uiScale_, juce::Font::bold)));
    const char* modeLbl = (vuMode_ == 0) ? "IN (VU)" : (vuMode_ == 1 ? "OUT (VU)" : "GR (dB)");
    g.drawText(modeLbl, dial.toNearestInt().withTrimmedTop(static_cast<int>(dial.getHeight() * 0.48f)),
               juce::Justification::centred);

    // Clip LED en haut à droite du cadran
    const bool isClip = (peakDb_ >= -0.2f);
    const float clipSz = 6.0f * uiScale_;
    g.setColour(isClip ? juce::Colour(0xffc93030) : juce::Colour(0xff4a1f1f));
    g.fillEllipse(dial.getRight() - 10.0f * uiScale_, dial.getY() + 4.0f * uiScale_, clipSz, clipSz);

    // Aiguilles stéréo (Clipées dans le cadran)
    g.saveState();
    g.reduceClipRegion(dial.toNearestInt());

    auto drawNeedle = [&](float vuVal, juce::Colour col, float thickness)
    {
        const float ang = vuDbToAngle(vuVal);
        const float tipR = arcR + 4.0f * uiScale_;
        g.setColour(col);
        g.drawLine(cx, pivotY, cx + tipR * std::sin(ang), pivotY - tipR * std::cos(ang), thickness * uiScale_);
    };

    if (vuMode_ != 2)
        drawNeedle(needleR_, juce::Colour(0xffa62c3b).withAlpha(0.75f), 1.4f);
    drawNeedle(needleL_, juce::Colour(0xff12151c), 1.8f);
    g.restoreState();

    // Lecture numérique compacte en bas du boîtier (tabular-nums)
    const auto footer = bezel.reduced(4.0f * uiScale_).removeFromBottom(16.0f * uiScale_);
    g.setColour(juce::Colours::white.withAlpha(0.92f));
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f * uiScale_, juce::Font::bold)));
    juce::String readout;
    if (vuMode_ == 2)
        readout = juce::String::formatted("GR: -%4.1f dB", rawGrDb_);
    else
        readout = juce::String::formatted("%+4.1f dB (0=%.0f)", std::max(needleL_, needleR_), vuRefDb_);
    g.drawText(readout, footer.toNearestInt(), juce::Justification::centred);
}
