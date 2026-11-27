#include "PedalboardUI.h"
#include <algorithm>

namespace kalyx::bass::ui
{

// ==============================================================================
// PedalCardComponent
// ==============================================================================
PedalCardComponent::PedalCardComponent(juce::AudioProcessorValueTreeState& apvts, int pedalId)
    : apvts_(apvts), pedalId_(pedalId)
{
    auto fmtPct = [](double v) { return juce::String(v, 0) + "%"; };
    auto fmtHz  = [](double v) { return juce::String(v, 2) + " Hz"; };
    auto fmtMs  = [](double v) { return juce::String(v, 0) + " ms"; };
    auto fmtDb  = [](double v) { return KalyxTheme::formatDbClean(v); };

    auto addBar = [&](const char* id, const char* lbl, bool bipolar, double defV, std::function<juce::String(double)> fmt, const juce::String& tip)
    {
        auto b = std::make_unique<TactileFaderBar>();
        b->init(*this, apvts_, id, lbl, accent_, bipolar, darkText_, defV, std::move(fmt), tip);
        bars_.push_back(std::move(b));
    };

    setWantsKeyboardFocus(false);

    switch (pedalId_)
    {
        case fx::Pedal_OptoFetComp:
            title_  = "OPTO / FET COMP";
            accent_ = KalyxTheme::classicAmber;
            setTooltip(juce::String::fromUTF8("Opto / FET Bass Comp : Compresseur dynamique à double topologie (OPTO doux / FET rapide)."));
            bypassAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts_, "fx_comp_en", bypassBtn_);
            pill_ = std::make_unique<KalyxSegmentedPill>(apvts_, "fx_comp_mode", "", juce::StringArray { "OPTO", "FET" }, accent_, darkText_);
            pill_->setWantsKeyboardFocus(false);
            addAndMakeVisible(*pill_);
            addBar("fx_comp_amt", "COMP", false, 40.0, fmtPct, juce::String::fromUTF8("Opto / FET Comp : Taux de compression appliqué au signal."));
            addBar("fx_comp_blend", "BLEND", false, 75.0, fmtPct, juce::String::fromUTF8("Opto / FET Comp : Dosage parallèle entre signal sec et compressé."));
            break;

        case fx::Pedal_Octaver:
            title_  = "OCTAVER POLY/SUB";
            accent_ = KalyxTheme::kalyxAubergine;
            setTooltip(juce::String::fromUTF8("Octaver Poly/Sub : Générateur d'octaves inférieures (-1/-2) et supérieure (+1)."));
            bypassAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts_, "fx_oct_en", bypassBtn_);
            addBar("fx_oct_clean", "CLEAN", false, 70.0, fmtPct, juce::String::fromUTF8("Octaver Poly/Sub : Niveau du signal direct non traité."));
            addBar("fx_oct_sub1", "-1 OCT", false, 50.0, fmtPct, juce::String::fromUTF8("Octaver Poly/Sub : Règle le niveau de l'octave inférieure (-1 Oct)."));
            addBar("fx_oct_sub2", "-2 OCT", false, 20.0, fmtPct, juce::String::fromUTF8("Octaver Poly/Sub : Règle le niveau de la double sous-octave (-2 Oct)."));
            addBar("fx_oct_up", "OCT UP", false, 0.0, fmtPct, juce::String::fromUTF8("Octaver Poly/Sub : Règle le niveau de l'octave supérieure (+1 Oct)."));
            break;

        case fx::Pedal_EnvFilter:
            title_  = "ENV FILTER WAH";
            accent_ = KalyxTheme::jadeGreen;
            setTooltip(juce::String::fromUTF8("Envelope Filter : Auto-Wah résonante pilotée par la dynamique de jeu."));
            bypassAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts_, "fx_env_en", bypassBtn_);
            addBar("fx_env_sens", "SENS", false, 55.0, fmtPct, juce::String::fromUTF8("Envelope Filter : Sensibilité du suiveur d'enveloppe."));
            addBar("fx_env_att", "ATT", false, 8.0, fmtMs, juce::String::fromUTF8("Envelope Filter : Temps d'ouverture du filtre."));
            addBar("fx_env_dec", "DEC", false, 180.0, fmtMs, juce::String::fromUTF8("Envelope Filter : Temps de fermeture du filtre."));
            addBar("fx_env_q", "Q", false, 3.5, [](double v) { return "Q " + juce::String(v, 1); }, juce::String::fromUTF8("Envelope Filter : Résonance (Q) du filtre passe-bande."));
            break;

        case fx::Pedal_TransDrive:
            title_  = "TRANSPARENT DRV";
            accent_ = KalyxTheme::hybridCyan;
            setTooltip(juce::String::fromUTF8("Transparent Drive : Overdrive type B3K/Klon à écrêtage asymétrique ADAA 64-bit."));
            bypassAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts_, "fx_tdrv_en", bypassBtn_);
            addBar("fx_tdrv_drv", "DRIVE", false, 45.0, fmtPct, juce::String::fromUTF8("Transparent Drive : Saturation harmonique asymétrique."));
            addBar("fx_tdrv_tone", "TONE", false, 55.0, fmtPct, juce::String::fromUTF8("Transparent Drive : Brillance de l'étage de saturation."));
            addBar("fx_tdrv_blend", "DRY BLEND", false, 50.0, fmtPct, juce::String::fromUTF8("Transparent Drive : Mélange avec la basse claire pour préserver l'attaque."));
            break;

        case fx::Pedal_RodentBass:
            title_  = "RODENT BASS";
            accent_ = KalyxTheme::tangerineOrange;
            setTooltip(juce::String::fromUTF8("Rodent Bass : Distorsion type Rat modifiée basse avec HPF interne et Clean Blend."));
            bypassAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts_, "fx_rat_en", bypassBtn_);
            addBar("fx_rat_dist", "DIST", false, 50.0, fmtPct, juce::String::fromUTF8("Rodent Bass : Taux de distorsion rocailleuse."));
            addBar("fx_rat_flt", "FILTER", false, 40.0, fmtPct, juce::String::fromUTF8("Rodent Bass : Coupe-haut inversé (plus élevé = plus sombre)."));
            addBar("fx_rat_blend", "CLEAN", false, 35.0, fmtPct, juce::String::fromUTF8("Rodent Bass : Réinjection du signal grave non saturé."));
            break;

        case fx::Pedal_BigWallFuzz:
            title_  = "BIG WALL FUZZ";
            accent_ = KalyxTheme::alertRed;
            setTooltip(juce::String::fromUTF8("Big Wall Fuzz : Fuzz massive type Russian Muff avec mid-scoop et récupération des graves."));
            bypassAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts_, "fx_fuzz_en", bypassBtn_);
            addBar("fx_fuzz_sust", "SUSTAIN", false, 65.0, fmtPct, juce::String::fromUTF8("Big Wall Fuzz : Saturation extrême et sustain infini."));
            addBar("fx_fuzz_tilt", "TILT", true, 0.0, [](double v) { return juce::String(v, 0); }, juce::String::fromUTF8("Big Wall Fuzz : Balance spectrale Tilt (graves vs aigus)."));
            addBar("fx_fuzz_low", "LOW REC", false, 45.0, fmtPct, juce::String::fromUTF8("Big Wall Fuzz : Récupération du bas du spectre sous 85 Hz."));
            break;

        case fx::Pedal_DimChorus:
            title_  = "DIM CHORUS (200Hz+)";
            accent_ = KalyxTheme::kalyxRoyalBlueGlow;
            setTooltip(juce::String::fromUTF8("Dimension Chorus : Chorus stéréo avec crossover Low Retain @ 200 Hz gardant les graves mono."));
            bypassAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts_, "fx_cho_en", bypassBtn_);
            addBar("fx_cho_rate", "RATE", false, 0.8, fmtHz, juce::String::fromUTF8("Dimension Chorus : Vitesse de modulation LFO."));
            addBar("fx_cho_depth", "DEPTH", false, 45.0, fmtPct, juce::String::fromUTF8("Dimension Chorus : Profondeur de désaccordage stéréo."));
            addBar("fx_cho_mix", "MIX", false, 40.0, fmtPct, juce::String::fromUTF8("Dimension Chorus : Dosage du chorus stéréo au-dessus de 200 Hz."));
            break;

        case fx::Pedal_AnalogPhaser:
            title_  = "ANALOG PHASER";
            accent_ = KalyxTheme::tangerineOrange;
            setTooltip(juce::String::fromUTF8("Analog Phaser : Phaser All-Pass 4/8 étages synchronisable au tempo du DAW."));
            bypassAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts_, "fx_pha_en", bypassBtn_);
            pill_ = std::make_unique<KalyxSegmentedPill>(apvts_, "fx_pha_div", "", juce::StringArray { "1/4", "1/8", "1/8D", "1/16" }, accent_, darkText_);
            pill_->setWantsKeyboardFocus(false);
            addAndMakeVisible(*pill_);
            syncBtn_ = std::make_unique<juce::ToggleButton>("SYNC");
            syncBtn_->setClickingTogglesState(true);
            syncBtn_->setWantsKeyboardFocus(false);
            syncBtn_->setTooltip(juce::String::fromUTF8("Analog Phaser : Synchronise la vitesse LFO sur le BPM du DAW."));
            syncAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts_, "fx_pha_sync", *syncBtn_);
            addAndMakeVisible(*syncBtn_);
            pad2d_ = std::make_unique<VectorPad2D>();
            pad2d_->init(*this, apvts_, "fx_pha_rate", "fx_pha_depth", "RATE", "DEPTH", accent_, darkText_, 0.1f, 0.6f);
            pad2d_->setWantsKeyboardFocus(false);
            addBar("fx_pha_fb", "FEEDBACK", false, 35.0, fmtPct, juce::String::fromUTF8("Analog Phaser : Réinjection de résonance du phaser."));
            break;

        case fx::Pedal_TapeEcho:
            title_  = "TAPE ECHO";
            accent_ = KalyxTheme::classicAmber;
            setTooltip(juce::String::fromUTF8("Tape Echo : Délai à bande magnétique avec saturation ADAA et wow & flutter."));
            bypassAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts_, "fx_tape_en", bypassBtn_);
            pill_ = std::make_unique<KalyxSegmentedPill>(apvts_, "fx_tape_div", "", juce::StringArray { "1/4", "1/8", "1/8D", "1/16" }, accent_, darkText_);
            pill_->setWantsKeyboardFocus(false);
            addAndMakeVisible(*pill_);
            syncBtn_ = std::make_unique<juce::ToggleButton>("SYNC");
            syncBtn_->setClickingTogglesState(true);
            syncBtn_->setWantsKeyboardFocus(false);
            syncBtn_->setTooltip(juce::String::fromUTF8("Tape Echo : Synchronise le temps de délai sur le BPM du DAW."));
            syncAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts_, "fx_tape_sync", *syncBtn_);
            addAndMakeVisible(*syncBtn_);
            pad2d_ = std::make_unique<VectorPad2D>();
            pad2d_->init(*this, apvts_, "fx_tape_time", "fx_tape_rep", "TIME", "REPEATS", accent_, darkText_, 0.25f, 0.4f);
            pad2d_->setWantsKeyboardFocus(false);
            addBar("fx_tape_wow", "WOW", false, 20.0, fmtPct, juce::String::fromUTF8("Tape Echo : Pleurage lent (Wow) du défilement de la bande."));
            addBar("fx_tape_flutter", "FLUTTER", false, 15.0, fmtPct, juce::String::fromUTF8("Tape Echo : Scintillement rapide (Flutter) de la bande."));
            addBar("fx_tape_sat", "TAPE SAT", false, 30.0, fmtPct, juce::String::fromUTF8("Tape Echo : Saturation magnétique ADAA de la bande."));
            addBar("fx_tape_mix", "MIX", false, 28.0, fmtPct, juce::String::fromUTF8("Tape Echo : Dosage des répétitions dans le mix final."));
            break;

        case fx::Pedal_StudioEQ:
        default:
            title_  = "STUDIO EQ + MONO";
            accent_ = KalyxTheme::jadeGreen;
            setTooltip(juce::String::fromUTF8("Studio Bass EQ : Égaliseur 4 bandes + Mono Maker recentrant les basses fréquences."));
            bypassAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts_, "fx_eq4_en", bypassBtn_);
            addBar("fx_eq4_mono_freq", "MONO <", false, 120.0, [](double v) { return juce::String(v, 0) + " Hz"; }, juce::String::fromUTF8("Mono Maker : Recentre en mono pur tout le spectre sous cette fréquence."));
            addBar("fx_eq4_b1", "60 Hz", true, 0.0, fmtDb, juce::String::fromUTF8("Studio EQ : Bande Low Shelf 60 Hz."));
            addBar("fx_eq4_b2", "250 Hz", true, 0.0, fmtDb, juce::String::fromUTF8("Studio EQ : Bande Bell 250 Hz."));
            addBar("fx_eq4_b3", "1.2 kHz", true, 0.0, fmtDb, juce::String::fromUTF8("Studio EQ : Bande Bell 1.2 kHz."));
            addBar("fx_eq4_b4", "4.5 kHz", true, 0.0, fmtDb, juce::String::fromUTF8("Studio EQ : Bande High Shelf 4.5 kHz."));
            break;
    }

    bypassBtn_.setClickingTogglesState(true);
    bypassBtn_.setWantsKeyboardFocus(false);
    bypassBtn_.setColour(juce::ToggleButton::tickColourId, accent_);
    bypassBtn_.setTooltip(juce::String::fromUTF8("Active ou désactive (Bypass) cette pédale."));
    bypassBtn_.onClick = [this]() { updateBypassVisuals(); };
    addAndMakeVisible(bypassBtn_);
    updateBypassVisuals();
}

void PedalCardComponent::updateBypassVisuals()
{
    const float childAlpha = bypassBtn_.getToggleState() ? 1.0f : 0.40f;
    for (auto& b : bars_) b->setAlpha(childAlpha);
    if (pill_) pill_->setAlpha(childAlpha);
    if (pad2d_) pad2d_->setAlpha(childAlpha);
    if (syncBtn_) syncBtn_->setAlpha(childAlpha);
    repaint();
}

void PedalCardComponent::setUiScale(float s) noexcept
{
    uiScale_ = std::clamp(s, 0.75f, 2.20f);
    for (auto& b : bars_) b->setUiScale(uiScale_);
    if (pill_) pill_->setUiScale(uiScale_);
    if (pad2d_) pad2d_->setUiScale(uiScale_);
    resized();
    repaint();
}

void PedalCardComponent::setDarkText(bool dark) noexcept
{
    darkText_ = dark;
    for (auto& b : bars_) b->setThemeColours(accent_, darkText_);
    if (pill_) pill_->setThemeColours(accent_, darkText_);
    if (pad2d_) pad2d_->setThemeColours(accent_, darkText_);
    repaint();
}

void PedalCardComponent::paint(juce::Graphics& g)
{
    const float sc = uiScale_;
    const auto r = getLocalBounds().toFloat().reduced(1.5f * sc);
    const auto bg = darkText_ ? KalyxTheme::dayStripBg : KalyxTheme::nightStripBg;
    const auto bd = darkText_ ? KalyxTheme::dayGridBorder : KalyxTheme::nightGridBorder;
    const auto tx = darkText_ ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary;
    const bool on = bypassBtn_.getToggleState();
    const float cardAlpha = on ? 1.0f : 0.40f;

    g.setColour(bg.withAlpha(cardAlpha));
    g.fillRoundedRectangle(r, 6.0f * sc);

    // Border highlights ONLY on hover or when active (no persistent focus outline!)
    const auto borderCol = isHovered_ ? accent_ : (on ? accent_.withAlpha(0.65f) : bd.withAlpha(0.45f));
    g.setColour(borderCol);
    g.drawRoundedRectangle(r, 6.0f * sc, isHovered_ ? 1.8f * sc : 1.1f * sc);

    // Top accent bar + drag grip
    g.setColour(accent_.withAlpha(on ? 0.90f : 0.28f));
    g.fillRoundedRectangle(r.getX(), r.getY(), r.getWidth(), 4.0f * sc, 2.0f * sc);

    g.setColour(tx.withAlpha(on ? 0.90f : 0.40f));
    g.setFont(KalyxTheme::makeFont(10.5f * sc, juce::Font::bold));
    g.drawFittedText(":: " + title_, r.reduced(6.0f * sc, 5.0f * sc).withHeight(18.0f * sc).toNearestInt(),
                     juce::Justification::centredLeft, 1);

    // Dedicated OPTO / FET Transfer Curve + Horizontal GR Bargraph
    if (pedalId_ == fx::Pedal_OptoFetComp)
    {
        auto vizR = r.reduced(8.0f * sc, 0.0f).withTrimmedTop(29.0f * sc).withHeight(64.0f * sc);
        auto curveR = vizR.withTrimmedBottom(16.0f * sc);
        auto grBarR = vizR.withTop(curveR.getBottom() + 3.0f * sc);

        g.setColour((darkText_ ? KalyxTheme::dayControlIdleBg : KalyxTheme::nightChassisBg).withAlpha(cardAlpha));
        g.fillRoundedRectangle(curveR, 4.0f * sc);
        g.setColour(bd.withAlpha(cardAlpha));
        g.drawRoundedRectangle(curveR, 4.0f * sc, 1.0f * sc);

        const bool isFet = apvts_.getRawParameterValue("fx_comp_mode")->load() > 0.5f;
        const float amt  = apvts_.getRawParameterValue("fx_comp_amt")->load() / 100.0f;
        const float threshNorm = 0.80f - 0.55f * amt;
        const float ratio = isFet ? 8.0f : 3.5f;
        const float knee  = isFet ? 0.04f : 0.16f;

        juce::Path tp;
        const auto plot = curveR.reduced(4.0f * sc);
        for (int i = 0; i <= 24; ++i)
        {
            const float xNorm = static_cast<float>(i) / 24.0f;
            float yNorm = xNorm;
            if (xNorm > threshNorm - knee)
            {
                const float over = xNorm - threshNorm;
                if (xNorm < threshNorm + knee)
                {
                    const float k = (over + knee) / (2.0f * knee);
                    yNorm = xNorm - (1.0f - 1.0f / ratio) * k * k * knee * 0.5f;
                }
                else
                {
                    yNorm = threshNorm + over / ratio;
                }
            }
            const float px = plot.getX() + xNorm * plot.getWidth();
            const float py = plot.getBottom() - std::clamp(yNorm, 0.0f, 1.0f) * plot.getHeight();
            if (i == 0) tp.startNewSubPath(px, py);
            else        tp.lineTo(px, py);
        }
        g.setColour(accent_.withAlpha(cardAlpha));
        g.strokePath(tp, juce::PathStrokeType(1.8f * sc));

        // Real-time luminous input/GR dot on transfer curve
        if (on && compInDb_ > -47.5f)
        {
            const float xDot = std::clamp((compInDb_ + 48.0f) / 48.0f, 0.0f, 1.0f);
            const float yDot = std::clamp((compInDb_ + compGrDb_ + 48.0f) / 48.0f, 0.0f, 1.0f);
            const float dx = plot.getX() + xDot * plot.getWidth();
            const float dy = plot.getBottom() - yDot * plot.getHeight();
            g.setColour(accent_.withAlpha(0.32f));
            g.fillEllipse(dx - 6.0f * sc, dy - 6.0f * sc, 12.0f * sc, 12.0f * sc);
            g.setColour(juce::Colours::white);
            g.fillEllipse(dx - 2.6f * sc, dy - 2.6f * sc, 5.2f * sc, 5.2f * sc);
        }

        // Horizontal GR Bargraph (-20 dB to 0 dB)
        g.setColour((darkText_ ? KalyxTheme::dayControlIdleBg : KalyxTheme::nightChassisBg).withAlpha(cardAlpha));
        g.fillRoundedRectangle(grBarR, 3.0f * sc);
        const float grFrac = std::clamp(-compGrDb_ / 20.0f, 0.0f, 1.0f);
        if (on && grFrac > 0.01f)
        {
            const float fw = grFrac * grBarR.getWidth();
            g.setColour(KalyxTheme::alertRed);
            g.fillRoundedRectangle(grBarR.getRight() - fw, grBarR.getY() + 1.0f, fw, grBarR.getHeight() - 2.0f, 2.0f * sc);
        }
        g.setColour(tx.withAlpha(0.75f * cardAlpha));
        g.setFont(KalyxTheme::makeMonoFont(8.5f * sc, juce::Font::bold));
        g.drawFittedText("GR " + juce::String(compGrDb_, 1) + " dB", grBarR.reduced(4.0f * sc, 0.0f).toNearestInt(),
                         juce::Justification::centredLeft, 1);
    }
}

void PedalCardComponent::resized()
{
    const float sc = uiScale_;
    auto r = getLocalBounds().reduced(static_cast<int>(6.0f * sc));
    auto top = r.removeFromTop(static_cast<int>(26.0f * sc));
    bypassBtn_.setBounds(top.removeFromRight(static_cast<int>(64.0f * sc)).reduced(0, 1));
    r.removeFromTop(static_cast<int>(3.0f * sc));

    if (pedalId_ == fx::Pedal_OptoFetComp)
        r.removeFromTop(static_cast<int>(66.0f * sc));

    // Equidistant FlexBox layout for all controls inside the pedal card
    juce::FlexBox fb;
    fb.flexDirection  = juce::FlexBox::Direction::column;
    fb.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
    fb.alignItems     = juce::FlexBox::AlignItems::stretch;

    if (syncBtn_ != nullptr && pill_ != nullptr)
    {
        auto syncRow = r.removeFromTop(static_cast<int>(24.0f * sc));
        syncBtn_->setBounds(syncRow.removeFromLeft(static_cast<int>(62.0f * sc)));
        syncRow.removeFromLeft(static_cast<int>(4.0f * sc));
        pill_->setBounds(syncRow);
        r.removeFromTop(static_cast<int>(4.0f * sc));
    }
    else if (pill_ != nullptr)
    {
        fb.items.add(juce::FlexItem(*pill_).withHeight(22.0f * sc));
    }

    if (pad2d_ != nullptr)
        fb.items.add(juce::FlexItem(*pad2d_).withFlex(1.3f).withMinHeight(44.0f * sc).withMargin({ 2.0f * sc, 0.0f, 2.0f * sc, 0.0f }));

    for (auto& b : bars_)
        fb.items.add(juce::FlexItem(*b).withFlex(1.0f).withMinHeight(24.0f * sc).withMaxHeight(32.0f * sc).withMargin({ 2.0f * sc, 0.0f, 2.0f * sc, 0.0f }));

    fb.performLayout(r);
}

void PedalCardComponent::mouseEnter(const juce::MouseEvent&)
{
    isHovered_ = true;
    repaint();
}

void PedalCardComponent::mouseExit(const juce::MouseEvent&)
{
    isHovered_ = false;
    repaint();
}

void PedalCardComponent::mouseDown(const juce::MouseEvent& e)
{
    dragStart_ = e.getPosition();
}

void PedalCardComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (dragStart_.getDistanceFrom(e.getPosition()) > 8)
    {
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
            container->startDragging("KALYX_PEDAL_" + juce::String(pedalId_), this);
    }
}

// ==============================================================================
// PedalChainZone
// ==============================================================================
PedalChainZone::PedalChainZone(bool isPostZone, juce::String zoneTitle,
                               std::function<void(int, bool, int)> onDropCallback)
    : isPost_(isPostZone), title_(std::move(zoneTitle)), onDrop_(std::move(onDropCallback))
{
}

void PedalChainZone::setCards(const std::vector<PedalCardComponent*>& cards)
{
    cards_ = cards;
    for (auto* c : cards_)
        addAndMakeVisible(c);
    resized();
    repaint();
}

void PedalChainZone::paint(juce::Graphics& g)
{
    const float sc = uiScale_;
    const auto r = getLocalBounds().toFloat().reduced(2.0f * sc);
    const auto bg = darkText_ ? KalyxTheme::dayControlIdleBg : KalyxTheme::nightControlIdleBg;
    const auto bd = dragHover_ ? KalyxTheme::kalyxRoyalBlueGlow
                               : (darkText_ ? KalyxTheme::dayGridBorder : KalyxTheme::nightGridBorder);
    const auto tx = darkText_ ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary;

    g.setColour(bg.withAlpha(0.65f));
    g.fillRoundedRectangle(r, 8.0f * sc);
    g.setColour(bd);
    g.drawRoundedRectangle(r, 8.0f * sc, dragHover_ ? 2.2f * sc : 1.2f * sc);

    g.setColour(tx.withAlpha(0.72f));
    g.setFont(KalyxTheme::makeFont(11.5f * sc, juce::Font::bold));
    g.drawFittedText(title_ + "  (DRAG & DROP TO REORDER OR MOVE PRE <-> POST)",
                     r.reduced(10.0f * sc, 6.0f * sc).withHeight(20.0f * sc).toNearestInt(),
                     juce::Justification::centredLeft, 1);
}

void PedalChainZone::resized()
{
    const float sc = uiScale_;
    auto area = getLocalBounds().reduced(static_cast<int>(10.0f * sc));
    area.removeFromTop(static_cast<int>(24.0f * sc));

    if (cards_.empty()) return;
    const int n = static_cast<int>(cards_.size());
    const int gap = static_cast<int>(8.0f * sc);
    const int cardW = std::min(static_cast<int>(228.0f * sc), (area.getWidth() - gap * (n - 1)) / std::max(1, n));
    int x = area.getX();
    for (auto* c : cards_)
    {
        c->setBounds(x, area.getY(), cardW, area.getHeight());
        x += cardW + gap;
    }
}

bool PedalChainZone::isInterestedInDragSource(const SourceDetails& details)
{
    return details.description.toString().startsWith("KALYX_PEDAL_");
}

void PedalChainZone::itemDragEnter(const SourceDetails&) { dragHover_ = true; repaint(); }
void PedalChainZone::itemDragExit(const SourceDetails&)  { dragHover_ = false; repaint(); }

void PedalChainZone::itemDropped(const SourceDetails& details)
{
    dragHover_ = false;
    repaint();
    const auto desc = details.description.toString();
    if (!desc.startsWith("KALYX_PEDAL_")) return;
    const int pedalId = desc.fromFirstOccurrenceOf("KALYX_PEDAL_", false, false).getIntValue();

    int targetIdx = static_cast<int>(cards_.size());
    for (int i = 0; i < static_cast<int>(cards_.size()); ++i)
    {
        if (details.localPosition.x < cards_[static_cast<size_t>(i)]->getBounds().getCentreX())
        {
            targetIdx = i;
            break;
        }
    }
    if (onDrop_)
        onDrop_(pedalId, isPost_, targetIdx);
}

// ==============================================================================
// PedalboardView
// ==============================================================================
PedalboardView::PedalboardView(juce::AudioProcessorValueTreeState& apvts)
    : apvts_(apvts),
      preZone_(false, "PRE-AMP FX CHAIN (BEFORE AMP & DI SPLIT)",
               [this](int pid, bool toPost, int idx) { handleDrop(pid, toPost, idx); }),
      postZone_(true, "POST-AMP FX CHAIN (AFTER AMP+CAB & DI MIX)",
                [this](int pid, bool toPost, int idx) { handleDrop(pid, toPost, idx); })
{
    for (int i = 0; i < fx::NumPedals; ++i)
        cards_[static_cast<size_t>(i)] = std::make_unique<PedalCardComponent>(apvts_, i);

    addAndMakeVisible(preZone_);
    addAndMakeVisible(postZone_);

    resetOrderBtn_.onClick = [this]() { resetDefaultOrder(); };
    addAndMakeVisible(resetOrderBtn_);

    syncOrderFromAPVTS();
}

void PedalboardView::setUiScale(float s) noexcept
{
    uiScale_ = std::clamp(s, 0.75f, 2.20f);
    for (auto& c : cards_) c->setUiScale(uiScale_);
    preZone_.setUiScale(uiScale_);
    postZone_.setUiScale(uiScale_);
    resized();
}

void PedalboardView::setDarkText(bool dark) noexcept
{
    darkText_ = dark;
    for (auto& c : cards_) c->setDarkText(dark);
    preZone_.setDarkText(dark);
    postZone_.setDarkText(dark);
    repaint();
}

void PedalboardView::setCompTelemetry(float inDb, float grDb) noexcept
{
    if (cards_[0]) cards_[0]->setTelemetryComp(inDb, grDb);
}

void PedalboardView::syncOrderFromAPVTS()
{
    std::vector<std::pair<int, int>> preList, postList;
    for (int i = 0; i < fx::NumPedals; ++i)
    {
        const int pos = static_cast<int>(apvts_.getRawParameterValue("fx_pos_" + juce::String(i))->load());
        const bool post = apvts_.getRawParameterValue("fx_post_" + juce::String(i))->load() > 0.5f;
        if (post) postList.push_back({ pos, i });
        else      preList.push_back({ pos, i });
    }
    std::sort(preList.begin(), preList.end());
    std::sort(postList.begin(), postList.end());

    std::vector<PedalCardComponent*> preCards, postCards;
    for (auto& pr : preList)  preCards.push_back(cards_[static_cast<size_t>(pr.second)].get());
    for (auto& pr : postList) postCards.push_back(cards_[static_cast<size_t>(pr.second)].get());

    preZone_.setCards(preCards);
    postZone_.setCards(postCards);
    for (auto& c : cards_)
        if (c) c->updateBypassVisuals();
}

void PedalboardView::handleDrop(int pedalId, bool toPost, int targetIndex)
{
    std::vector<int> preIds, postIds;
    for (int i = 0; i < fx::NumPedals; ++i)
    {
        const int pos = static_cast<int>(apvts_.getRawParameterValue("fx_pos_" + juce::String(i))->load());
        const bool post = apvts_.getRawParameterValue("fx_post_" + juce::String(i))->load() > 0.5f;
        if (i == pedalId) continue;
        if (post) postIds.push_back((pos << 8) | i);
        else      preIds.push_back((pos << 8) | i);
    }
    std::sort(preIds.begin(), preIds.end());
    std::sort(postIds.begin(), postIds.end());

    std::vector<int> preClean, postClean;
    for (int v : preIds) preClean.push_back(v & 0xFF);
    for (int v : postIds) postClean.push_back(v & 0xFF);

    auto& dest = toPost ? postClean : preClean;
    const int ins = std::clamp(targetIndex, 0, static_cast<int>(dest.size()));
    dest.insert(dest.begin() + ins, pedalId);

    int order = 0;
    for (int id : preClean)
    {
        if (auto* p = apvts_.getParameter("fx_post_" + juce::String(id))) p->setValueNotifyingHost(0.0f);
        if (auto* p = apvts_.getParameter("fx_pos_" + juce::String(id)))  p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(order++)));
    }
    for (int id : postClean)
    {
        if (auto* p = apvts_.getParameter("fx_post_" + juce::String(id))) p->setValueNotifyingHost(1.0f);
        if (auto* p = apvts_.getParameter("fx_pos_" + juce::String(id)))  p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(order++)));
    }
    syncOrderFromAPVTS();
}

void PedalboardView::resetDefaultOrder()
{
    for (int i = 0; i < fx::NumPedals; ++i)
    {
        if (auto* p = apvts_.getParameter("fx_post_" + juce::String(i))) p->setValueNotifyingHost(i >= 6 ? 1.0f : 0.0f);
        if (auto* p = apvts_.getParameter("fx_pos_" + juce::String(i)))  p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(i)));
    }
    syncOrderFromAPVTS();
}

void PedalboardView::paint(juce::Graphics& g)
{
    g.fillAll(darkText_ ? KalyxTheme::dayChassisBg : KalyxTheme::nightChassisBg);
}

void PedalboardView::resized()
{
    const float sc = uiScale_;
    auto r = getLocalBounds().reduced(static_cast<int>(6.0f * sc));
    auto topBar = r.removeFromBottom(static_cast<int>(28.0f * sc));
    resetOrderBtn_.setBounds(topBar.removeFromRight(static_cast<int>(130.0f * sc)));
    r.removeFromBottom(static_cast<int>(6.0f * sc));

    const int halfH = (r.getHeight() - static_cast<int>(10.0f * sc)) / 2;
    preZone_.setBounds(r.removeFromTop(halfH));
    r.removeFromTop(static_cast<int>(10.0f * sc));
    postZone_.setBounds(r);
}

} // namespace kalyx::bass::ui
