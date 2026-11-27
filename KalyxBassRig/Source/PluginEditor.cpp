#include "PluginEditor.h"

KalyxBassRigEditor::KalyxBassRigEditor(KalyxBassRigProcessor& proc)
    : AudioProcessorEditor(&proc),
      proc_(proc),
      tunerBar_(proc),
      pedalboardView_(proc.getAPVTS()),
      ampGraph_(proc),
      settingsOverlay_(proc)
{
    setLookAndFeel(&lnf_);

    auto& apvts = proc_.getAPVTS();
    const bool dark = false;

    // Header
    tunerMuteBtn_.setClickingTogglesState(true);
    tunerMuteBtn_.setWantsKeyboardFocus(false);
    tunerMuteBtn_.setTooltip("Coupe la sortie audio pendant l'accordage.");
    tunerMuteAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "tuner_mute", tunerMuteBtn_);
    addAndMakeVisible(tunerMuteBtn_);
    tunerBar_.setTooltip("Accordeur chromatique haute précision (détection YIN temps réel).");
    addAndMakeVisible(tunerBar_);

    refreshPresetList();
    presetBox_.setWantsKeyboardFocus(false);
    presetBox_.setTooltip("Sélecteur de presets usine et utilisateur.");
    presetBox_.onChange = [this]()
    {
        const int idx = presetBox_.getSelectedItemIndex();
        if (idx >= 0)
        {
            proc_.getPresetManager().loadPresetByIndex(idx);
            pedalboardView_.syncOrderFromAPVTS();
        }
    };
    addAndMakeVisible(presetBox_);

    savePresetBtn_.setWantsKeyboardFocus(false);
    savePresetBtn_.setTooltip("Enregistre les modifications dans le preset utilisateur actif.");
    savePresetBtn_.onClick = [this]()
    {
        if (proc_.getPresetManager().isUserPreset(presetBox_.getSelectedItemIndex()))
            proc_.getPresetManager().saveCurrentPreset();
        else
            promptPresetName(false);
    };
    addAndMakeVisible(savePresetBtn_);

    saveAsPresetBtn_.setWantsKeyboardFocus(false);
    saveAsPresetBtn_.setTooltip("Enregistre la configuration actuelle sous un nouveau nom de preset.");
    saveAsPresetBtn_.onClick = [this]() { promptPresetName(false); };
    addAndMakeVisible(saveAsPresetBtn_);

    renamePresetBtn_.setWantsKeyboardFocus(false);
    renamePresetBtn_.setTooltip("Renomme le preset utilisateur sélectionné.");
    renamePresetBtn_.onClick = [this]() { promptPresetName(true); };
    addAndMakeVisible(renamePresetBtn_);

    deletePresetBtn_.setWantsKeyboardFocus(false);
    deletePresetBtn_.setTooltip("Supprime le preset utilisateur sélectionné.");
    deletePresetBtn_.onClick = [this]()
    {
        const int idx = presetBox_.getSelectedItemIndex();
        if (proc_.getPresetManager().deleteUserPreset(idx))
            refreshPresetList();
    };
    addAndMakeVisible(deletePresetBtn_);

    themeBtn_.setWantsKeyboardFocus(false);
    themeBtn_.setTooltip("Bascule entre le mode Nuit (#111318) et le mode Jour (#E6E8EC).");
    themeBtn_.onClick = [this]()
    {
        const int cur = static_cast<int>(proc_.getAPVTS().getRawParameterValue("ui_theme")->load(std::memory_order_relaxed));
        if (auto* p = proc_.getAPVTS().getParameter("ui_theme"))
            p->setValueNotifyingHost(cur == 0 ? 1.0f : 0.0f);
    };
    addAndMakeVisible(themeBtn_);

    settingsBtn_.setWantsKeyboardFocus(false);
    settingsBtn_.setTooltip("Ouvre les paramètres (Calibration 0 VU, Oversampling).");
    settingsBtn_.onClick = [this]() { settingsOverlay_.setVisible(true); settingsOverlay_.toFront(true); };
    addAndMakeVisible(settingsBtn_);

    addMouseListener(this, true);

    rigTabBtn_.setClickingTogglesState(true);
    pedalsTabBtn_.setClickingTogglesState(true);
    rigTabBtn_.setToggleState(true, juce::dontSendNotification);
    rigTabBtn_.onClick = [this]()
    {
        showPedalboard_ = false;
        rigTabBtn_.setToggleState(true, juce::dontSendNotification);
        pedalsTabBtn_.setToggleState(false, juce::dontSendNotification);
        pedalboardView_.setVisible(false);
        repaint();
    };
    pedalsTabBtn_.onClick = [this]()
    {
        showPedalboard_ = true;
        rigTabBtn_.setToggleState(false, juce::dontSendNotification);
        pedalsTabBtn_.setToggleState(true, juce::dontSendNotification);
        pedalboardView_.syncOrderFromAPVTS();
        pedalboardView_.setVisible(true);
        pedalboardView_.toFront(false);
        repaint();
    };
    addAndMakeVisible(rigTabBtn_);
    addAndMakeVisible(pedalsTabBtn_);

    // Left Column: Input & DI
    inTrimKnob_.init(*this, apvts, "in_trim", "IN TRIM", KalyxTheme::kalyxRoyalBlue, true, dark, 0.0,
                     [](double v) { return KalyxTheme::formatDbClean(v); });

    padBtn_.setClickingTogglesState(true);
    gateBtn_.setClickingTogglesState(true);
    padAtt_  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "in_pad", padBtn_);
    gateAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "gate_en", gateBtn_);
    addAndMakeVisible(padBtn_);
    addAndMakeVisible(gateBtn_);

    gateCurve_.init(apvts);
    addAndMakeVisible(gateCurve_);

    diEnBtn_.setClickingTogglesState(true);
    diEnAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "di_en", diEnBtn_);
    addAndMakeVisible(diEnBtn_);

    diCircuitBox_.addItemList({ "Transparent", "Tube DI", "American 2503", "Class-A 73", "Sub-Harmonic" }, 1);
    diCircuitAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "di_circuit", diCircuitBox_);
    addAndMakeVisible(diCircuitBox_);

    diSubMixBar_.init(*this, apvts, "di_sub_mix", "SUB MIX", KalyxTheme::kalyxAubergine, false, dark, 35.0,
                      [](double v) { return juce::String(v, 0) + "%"; });

    diFilterGraph_.init(apvts);
    addAndMakeVisible(diFilterGraph_);

    diCompBar_.init(*this, apvts, "di_comp", "DI OPTO COMP", KalyxTheme::kalyxRoyalBlue, false, dark, 25.0,
                    [](double v) { return juce::String(v, 0) + "%"; });

    diAutoPhaseBtn_.setClickingTogglesState(true);
    diAutoPhaseAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "di_auto_phase", diAutoPhaseBtn_);
    addAndMakeVisible(diAutoPhaseBtn_);

    // Center Top: Amp Head
    ampEnBtn_.setClickingTogglesState(true);
    ampEnAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "amp_en", ampEnBtn_);
    addAndMakeVisible(ampEnBtn_);

    ampModelPill_ = std::make_unique<kalyx::bass::ui::KalyxSegmentedPill>(
        apvts, "amp_model", "", juce::StringArray { "HERITAGE 300", "CLASSIC TUBE", "HYBRID 4-PRO", "TANGERINE 200" },
        KalyxTheme::heritageBlue, dark);
    addAndMakeVisible(*ampModelPill_);
    addAndMakeVisible(ampGraph_);

    ampChannelPill_ = std::make_unique<kalyx::bass::ui::KalyxSegmentedPill>(
        apvts, "amp_channel", "IN", juce::StringArray { "CH 1 (12DW7)", "CH 2 (12AX7)" },
        KalyxTheme::heritageBlue, dark);
    addAndMakeVisible(*ampChannelPill_);

    brightBtn_.setClickingTogglesState(true);
    ultraHiBtn_.setClickingTogglesState(true);
    brightAtt_  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "amp_bright", brightBtn_);
    ultraHiAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "amp_ultra_hi", ultraHiBtn_);
    addAndMakeVisible(brightBtn_);
    addAndMakeVisible(ultraHiBtn_);

    ultraLoBox_.addItemList({ "U-LO: OFF", "SUB CUT", "LOW CUT", "ULTRA LO", "MODERN" }, 1);
    ultraLoAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "amp_ultra_lo", ultraLoBox_);
    addAndMakeVisible(ultraLoBox_);

    midFreqBox_.addItemList({ "220 Hz", "450 Hz", "800 Hz", "1.6 kHz", "3.0 kHz" }, 1);
    midFreqAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "amp_mid_freq", midFreqBox_);
    addAndMakeVisible(midFreqBox_);

    depthBox_.addItemList({ "FAC 100nF", "FAC 47nF", "FAC 10nF", "FAC 4.7nF", "FAC 2.2nF", "FAC 1.0nF" }, 1);
    depthAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "amp_depth", depthBox_);
    addAndMakeVisible(depthBox_);

    auto fmtPct = [](double v) { return juce::String(v, 0) + "%"; };
    auto fmtDb  = [](double v) { return KalyxTheme::formatDbClean(v); };

    gainKnob_.init(*this, apvts, "amp_gain", "GAIN", KalyxTheme::heritageBlue, false, dark, 42.0, fmtPct);
    bassKnob_.init(*this, apvts, "amp_bass", "BASS", KalyxTheme::heritageBlue, true, dark, 0.0, fmtDb);
    midKnob_.init(*this, apvts, "amp_mid", "MID", KalyxTheme::heritageBlue, true, dark, 0.0, fmtDb);
    trebleKnob_.init(*this, apvts, "amp_treble", "TREBLE", KalyxTheme::heritageBlue, true, dark, 0.0, fmtDb);
    driveKnob_.init(*this, apvts, "amp_drive", "DRIVE", KalyxTheme::classicAmber, false, dark, 25.0, fmtPct);
    compBoostKnob_.init(*this, apvts, "amp_comp", "COMP", KalyxTheme::hybridCyan, false, dark, 20.0, fmtPct);
    presenceKnob_.init(*this, apvts, "amp_presence", "PRESENCE", KalyxTheme::tangerineOrange, false, dark, 35.0, fmtPct);
    masterKnob_.init(*this, apvts, "amp_master", "MASTER", KalyxTheme::heritageBlue, false, dark, 65.0, fmtPct);

    sagBar_.init(*this, apvts, "amp_sag", "POWER SAG (660V)", KalyxTheme::classicAmber, false, dark, 30.0, fmtPct);
    biasBar_.init(*this, apvts, "amp_bias", "TUBE BIAS", KalyxTheme::kalyxAubergine, true, dark, 0.0,
                 [](double v) { return (v >= 0.5 ? "+" : "") + juce::String(v, 0) + "%"; });

    // Center Bottom: Dual Cabinet & Mic 2D
    cabEnBtn_.setClickingTogglesState(true);
    cabEnAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "cab_en", cabEnBtn_);
    addAndMakeVisible(cabEnBtn_);

    const auto& irs = proc_.getDualCab().getBuiltinIRs();
    for (int i = 0; i < static_cast<int>(irs.size()); ++i)
    {
        cabABox_.addItem(irs[static_cast<size_t>(i)].displayName, i + 1);
        cabBBox_.addItem(irs[static_cast<size_t>(i)].displayName, i + 1);
    }
    if (irs.empty())
    {
        cabABox_.addItem("Default 8x10", 1);
        cabBBox_.addItem("Default SVT", 2);
    }
    cabAAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "cab_a_ir", cabABox_);
    cabBAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "cab_b_ir", cabBBox_);
    addAndMakeVisible(cabABox_);
    addAndMakeVisible(cabBBox_);

    phaseABtn_.setClickingTogglesState(true);
    phaseBBtn_.setClickingTogglesState(true);
    cabAutoPhaseBtn_.setClickingTogglesState(true);
    phaseAAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "cab_a_phase", phaseABtn_);
    phaseBAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "cab_b_phase", phaseBBtn_);
    cabAutoPhaseAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "cab_auto_phase", cabAutoPhaseBtn_);
    addAndMakeVisible(phaseABtn_);
    addAndMakeVisible(phaseBBtn_);
    addAndMakeVisible(cabAutoPhaseBtn_);

    loadIrABtn_.onClick = [this]() { openCustomIRChooser(0); };
    loadIrBBtn_.onClick = [this]() { openCustomIRChooser(1); };
    addAndMakeVisible(loadIrABtn_);
    addAndMakeVisible(loadIrBBtn_);

    cabMixBar_.init(*this, apvts, "cab_mix", "CAB A <-> B", KalyxTheme::kalyxRoyalBlue, false, dark, 35.0,
                    [](double v) { return juce::String(100.0 - v, 0) + "/" + juce::String(v, 0); });
    cabBreakupBar_.init(*this, apvts, "cab_breakup", "CONE BREAKUP", KalyxTheme::classicAmber, false, dark, 18.0, fmtPct);
    cabRoomBar_.init(*this, apvts, "cab_room", "ROOM", KalyxTheme::kalyxAubergine, false, dark, 15.0, fmtPct);

    // Right Column: Master & Mixer (Official Kalyx Palette)
    addAndMakeVisible(vuMeter_);
    vuModePill_ = std::make_unique<kalyx::bass::ui::KalyxSegmentedPill>(
        apvts, "vu_mode", "", juce::StringArray { "IN", "OUT", "GR" }, KalyxTheme::kalyxRoyalBlue, dark);
    addAndMakeVisible(*vuModePill_);
    addAndMakeVisible(phaseCorrBar_);

    ampLevelFader_.init(*this, apvts, "amp_level", "AMP", KalyxTheme::kalyxRoyalBlue, dark, -60.0, 12.0, 0.0);
    diLevelFader_.init(*this, apvts, "di_level", "DI", KalyxTheme::kalyxAubergine, dark, -60.0, 12.0, -6.0);
    outFader_.init(*this, apvts, "out_fader", "OUT", KalyxTheme::classicAmber, dark, -60.0, 12.0, 0.0);

    kickEnBtn_.setClickingTogglesState(true);
    limEnBtn_.setClickingTogglesState(true);
    kickEnAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "kick_relief_en", kickEnBtn_);
    limEnAtt_  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "lim_en", limEnBtn_);
    addAndMakeVisible(kickEnBtn_);
    addAndMakeVisible(limEnBtn_);

    kickAmtBar_.init(*this, apvts, "kick_relief_amt", "KICK DUCK", KalyxTheme::kalyxAubergine, false, dark, 45.0, fmtPct);
    limCeilBar_.init(*this, apvts, "lim_ceiling", "CEILING", KalyxTheme::alertRed, false, dark, -0.3, fmtDb);

    addChildComponent(pedalboardView_);
    addChildComponent(settingsOverlay_);

    setResizable(true, true);
    setResizeLimits(1060, 620, 2600, 1500);
    setSize(1480, 820);

    applyTheme();
    updateAmpModelVisibility();
    startTimerHz(30);
}

KalyxBassRigEditor::~KalyxBassRigEditor()
{
    removeMouseListener(this);
    stopTimer();
    setLookAndFeel(nullptr);
}

void KalyxBassRigEditor::refreshPresetList()
{
    presetBox_.clear(juce::dontSendNotification);
    presetBox_.addItemList(proc_.getPresetManager().getAllPresetNames(), 1);
    const int idx = std::clamp(proc_.getPresetManager().getCurrentPresetIndex(), 0, std::max(0, presetBox_.getNumItems() - 1));
    presetBox_.setSelectedItemIndex(idx, juce::dontSendNotification);
}

void KalyxBassRigEditor::promptPresetName(bool isRename)
{
    const int curIdx = presetBox_.getSelectedItemIndex();
    if (isRename && !proc_.getPresetManager().isUserPreset(curIdx))
    {
        infoBarText_ = "Info : Seuls les presets utilisateur peuvent être renommés.";
        repaint();
        return;
    }

    auto* aw = new juce::AlertWindow(isRename ? "Renommer le Preset" : "Enregistrer sous...",
                                     "Nom du preset :", juce::MessageBoxIconType::NoIcon, this);
    aw->addTextEditor("name", isRename ? proc_.getPresetManager().getCurrentPresetName() : "My Bass Rig", "Nom :");
    aw->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Annuler", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    aw->enterModalState(true, juce::ModalCallbackFunction::create([this, aw, isRename, curIdx](int res)
    {
        if (res == 1)
        {
            const auto name = aw->getTextEditorContents("name").trim();
            if (name.isNotEmpty())
            {
                if (isRename) proc_.getPresetManager().renameUserPreset(curIdx, name);
                else          proc_.getPresetManager().saveUserPreset(name);
                refreshPresetList();
            }
        }
    }), true);
}

void KalyxBassRigEditor::mouseEnter(const juce::MouseEvent& e)
{
    updateInfoFromComponent(e.eventComponent);
}

void KalyxBassRigEditor::mouseMove(const juce::MouseEvent& e)
{
    updateInfoFromComponent(e.eventComponent);
}

void KalyxBassRigEditor::updateInfoFromComponent(juce::Component* comp)
{
    juce::String nextText;
    for (auto* c = comp; c != nullptr && c != this; c = c->getParentComponent())
    {
        if (auto* ttc = dynamic_cast<juce::TooltipClient*>(c))
        {
            const auto tip = ttc->getTooltip();
            if (tip.isNotEmpty())
            {
                nextText = tip;
                if (auto* bar = dynamic_cast<kalyx::bass::ui::TactileFaderBar*>(c))
                    nextText += " [" + bar->getFormattedValue() + "]";
                else if (auto* sl = dynamic_cast<juce::Slider*>(c))
                    nextText += " [" + juce::String(sl->getValue(), 1) + "]";
                break;
            }
        }
    }

    if (nextText.isNotEmpty() && nextText != infoBarText_)
    {
        infoBarText_ = nextText;
        repaint(0, getHeight() - static_cast<int>(28.0f * uiScale_), getWidth(), static_cast<int>(28.0f * uiScale_));
    }
}

void KalyxBassRigEditor::openCustomIRChooser(int cabSlot)
{
    fileChooser_ = std::make_unique<juce::FileChooser>("Select Custom Cabinet IR (.wav)", juce::File{}, "*.wav");
    fileChooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this, cabSlot](const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile())
                                      proc_.loadCustomIR(cabSlot, f);
                              });
}

void KalyxBassRigEditor::timerCallback()
{
    tunerBar_.updateFromProcessor();
    gateCurve_.setGateGain(proc_.getGateGain());
    ampGraph_.setThdHeatmap(proc_.getAmpThd());
    phaseCorrBar_.setCorrelation(proc_.getPhaseCorr());
    pedalboardView_.setCompGrDb(proc_.getPedalCompGrDb());

    if (auto* p = proc_.getAPVTS().getRawParameterValue("vu_mode"))
        vuMeter_.setMode(static_cast<int>(p->load(std::memory_order_relaxed)));
    if (auto* p = proc_.getAPVTS().getRawParameterValue("vu_ref"))
        vuMeter_.setRefDb(p->load(std::memory_order_relaxed));
    vuMeter_.setReadings(proc_.getVuInDb(), proc_.getVuOutDb(), proc_.getVuGrDb());

    const auto nextTheme = (proc_.getAPVTS().getRawParameterValue("ui_theme")->load(std::memory_order_relaxed) > 0.5f)
                               ? kalyx::bass::ChassisTheme::Day : kalyx::bass::ChassisTheme::Night;
    if (nextTheme != theme_)
    {
        theme_ = nextTheme;
        applyTheme();
    }

    const int curModel = static_cast<int>(proc_.getAPVTS().getRawParameterValue("amp_model")->load(std::memory_order_relaxed));
    if (curModel != lastModelIdx_)
    {
        lastModelIdx_ = curModel;
        updateAmpModelVisibility();
    }
}

void KalyxBassRigEditor::applyTheme()
{
    const bool dark = (theme_ == kalyx::bass::ChassisTheme::Day);
    lnf_.setChassisTheme(theme_);
    themeBtn_.setButtonText(dark ? "NIGHT" : "DAY");

    tunerBar_.setDarkText(dark);
    gateCurve_.setDarkText(dark);
    diFilterGraph_.setDarkText(dark);
    ampGraph_.setDarkText(dark);
    phaseCorrBar_.setDarkText(dark);
    vuMeter_.setDarkText(dark);
    pedalboardView_.setDarkText(dark);
    settingsOverlay_.setTheme(theme_);

    inTrimKnob_.setThemeColours(KalyxTheme::getAccent(theme_), dark);
    diSubMixBar_.setThemeColours(KalyxTheme::getPrimary(theme_), dark);
    diCompBar_.setThemeColours(KalyxTheme::getAccent(theme_), dark);
    sagBar_.setThemeColours(KalyxTheme::classicAmber, dark);
    biasBar_.setThemeColours(KalyxTheme::getPrimary(theme_), dark);
    cabMixBar_.setThemeColours(KalyxTheme::getAccent(theme_), dark);
    cabBreakupBar_.setThemeColours(KalyxTheme::classicAmber, dark);
    cabRoomBar_.setThemeColours(KalyxTheme::getPrimary(theme_), dark);
    ampLevelFader_.setThemeColours(KalyxTheme::getAccent(theme_), dark);
    diLevelFader_.setThemeColours(KalyxTheme::getPrimary(theme_), dark);
    outFader_.setThemeColours(KalyxTheme::classicAmber, dark);
    kickAmtBar_.setThemeColours(KalyxTheme::getPrimary(theme_), dark);
    limCeilBar_.setThemeColours(KalyxTheme::alertRed, dark);
    if (vuModePill_) vuModePill_->setThemeColours(KalyxTheme::getAccent(theme_), dark);

    repaint();
}

void KalyxBassRigEditor::updateAmpModelVisibility()
{
    const int m = std::clamp(lastModelIdx_, 0, 3);
    const bool dark = (theme_ == kalyx::bass::ChassisTheme::Day);
    juce::Colour acc = KalyxTheme::heritageBlue;
    if (m == 1) acc = KalyxTheme::classicAmber;
    else if (m == 2) acc = KalyxTheme::hybridCyan;
    else if (m == 3) acc = KalyxTheme::tangerineOrange;

    if (ampModelPill_)   ampModelPill_->setThemeColours(acc, dark);
    if (ampChannelPill_) ampChannelPill_->setThemeColours(acc, dark);
    ampGraph_.setAccentColour(acc);
    gainKnob_.setThemeColours(acc, dark);
    bassKnob_.setThemeColours(acc, dark);
    midKnob_.setThemeColours(acc, dark);
    trebleKnob_.setThemeColours(acc, dark);
    masterKnob_.setThemeColours(acc, dark);

    // Model-specific controls
    ampChannelPill_->setVisible(m == 0);
    depthBox_.setVisible(m == 3);
    midFreqBox_.setVisible(m != 3);
    compBoostKnob_.setVisible(m == 2 || m == 3);
    compBoostKnob_.titleLabel.setText(m == 3 ? "BOOST" : "COMP", juce::dontSendNotification);
    presenceKnob_.setVisible(m == 3 || m == 0);

    resized();
    repaint();
}

void KalyxBassRigEditor::paint(juce::Graphics& g)
{
    const float sc = uiScale_;
    g.fillAll(KalyxTheme::getChassisBg(theme_));

    const auto stripBg = KalyxTheme::getStripBg(theme_);
    const auto border  = KalyxTheme::getGridBorder(theme_);
    const auto textCol = KalyxTheme::getTextPrimary(theme_);

    const int pad = static_cast<int>(10.0f * sc);
    const int hdrH = static_cast<int>(46.0f * sc);

    // Header bar
    auto hdrR = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(getWidth()), static_cast<float>(hdrH));
    g.setColour(stripBg);
    g.fillRect(hdrR);
    g.setColour(border);
    g.drawLine(0.0f, hdrR.getBottom(), hdrR.getRight(), hdrR.getBottom(), 1.2f * sc);

    // Brand + v1.0.0 badge
    g.setColour(textCol);
    g.setFont(KalyxTheme::makeFont(16.0f * sc, juce::Font::bold));
    g.drawFittedText("KALYX // BASS RIG", static_cast<int>(14.0f * sc), 0, static_cast<int>(190.0f * sc), hdrH,
                     juce::Justification::centredLeft, 1);

    auto badgeR = juce::Rectangle<float>(196.0f * sc, 13.0f * sc, 52.0f * sc, 20.0f * sc);
    g.setColour(KalyxTheme::getPrimary(theme_));
    g.fillRoundedRectangle(badgeR, 4.0f * sc);
    g.setColour(KalyxTheme::nightTextPrimary);
    g.setFont(KalyxTheme::makeMonoFont(10.5f * sc, juce::Font::bold));
    g.drawFittedText("v1.0.0", badgeR.toNearestInt(), juce::Justification::centred, 1);

    // 4 Main Panels (above bottom InfoBar)
    const int infoH = static_cast<int>(25.0f * sc);
    if (!showPedalboard_)
    {
        auto body = getLocalBounds().withTrimmedTop(hdrH + pad).withTrimmedBottom(infoH + pad).reduced(pad, 0);
        const int leftW  = static_cast<int>(260.0f * sc);
        const int rightW = static_cast<int>(255.0f * sc);
        const auto leftR  = body.removeFromLeft(leftW);
        body.removeFromLeft(pad);
        const auto rightR = body.removeFromRight(rightW);
        body.removeFromRight(pad);
        auto ampR = body.removeFromTop(static_cast<int>(body.getHeight() * 0.56f));
        body.removeFromTop(pad);
        const auto cabR = body;

        auto drawCard = [&](juce::Rectangle<int> area, const juce::String& title)
        {
            auto rf = area.toFloat();
            g.setColour(stripBg);
            g.fillRoundedRectangle(rf, 6.0f * sc);
            g.setColour(border);
            g.drawRoundedRectangle(rf, 6.0f * sc, 1.2f * sc);
            g.setColour(textCol.withAlpha(0.70f));
            g.setFont(KalyxTheme::makeFont(11.0f * sc, juce::Font::bold));
            g.drawFittedText(title, area.reduced(static_cast<int>(10.0f * sc), static_cast<int>(6.0f * sc))
                                        .withHeight(static_cast<int>(18.0f * sc)),
                             juce::Justification::centredLeft, 1);
        };

        drawCard(leftR, "INPUT & DI PATH");
        drawCard(ampR, "TUBE AMP HEAD (64-BIT)");
        drawCard(cabR, "DUAL CABINET IR");
        drawCard(rightR, "MASTER & MIXER");
    }

    // Bottom Contextual InfoBar (25px * uiScale)
    auto infoR = juce::Rectangle<float>(0.0f, static_cast<float>(getHeight() - infoH),
                                        static_cast<float>(getWidth()), static_cast<float>(infoH));
    g.setColour(stripBg);
    g.fillRect(infoR);
    g.setColour(border);
    g.drawLine(0.0f, infoR.getY(), infoR.getRight(), infoR.getY(), 1.0f * sc);
    g.setColour(textCol.withAlpha(0.82f));
    g.setFont(KalyxTheme::makeMonoFont(10.5f * sc, juce::Font::plain));
    g.drawFittedText(infoBarText_, infoR.reduced(12.0f * sc, 2.0f * sc).toNearestInt(),
                     juce::Justification::centredLeft, 1);
}

void KalyxBassRigEditor::resized()
{
    uiScale_ = std::clamp(std::min(static_cast<float>(getWidth()) / 1480.0f,
                                   static_cast<float>(getHeight()) / 820.0f), 0.75f, 2.20f);
    const float sc = uiScale_;
    lnf_.setUiScale(sc);

    tunerBar_.setUiScale(sc);
    inTrimKnob_.setUiScale(sc);
    gateCurve_.setUiScale(sc);
    diSubMixBar_.setUiScale(sc);
    diFilterGraph_.setUiScale(sc);
    diCompBar_.setUiScale(sc);
    if (ampModelPill_) ampModelPill_->setUiScale(sc);
    if (ampChannelPill_) ampChannelPill_->setUiScale(sc);
    ampGraph_.setUiScale(sc);
    gainKnob_.setUiScale(sc);
    bassKnob_.setUiScale(sc);
    midKnob_.setUiScale(sc);
    trebleKnob_.setUiScale(sc);
    driveKnob_.setUiScale(sc);
    compBoostKnob_.setUiScale(sc);
    presenceKnob_.setUiScale(sc);
    masterKnob_.setUiScale(sc);
    sagBar_.setUiScale(sc);
    biasBar_.setUiScale(sc);
    cabMixBar_.setUiScale(sc);
    cabBreakupBar_.setUiScale(sc);
    cabRoomBar_.setUiScale(sc);
    vuMeter_.setUiScale(sc);
    if (vuModePill_) vuModePill_->setUiScale(sc);
    phaseCorrBar_.setUiScale(sc);
    ampLevelFader_.setUiScale(sc);
    diLevelFader_.setUiScale(sc);
    outFader_.setUiScale(sc);
    kickAmtBar_.setUiScale(sc);
    limCeilBar_.setUiScale(sc);
    pedalboardView_.setUiScale(sc);
    settingsOverlay_.setUiScale(sc);
    settingsOverlay_.setBounds(getLocalBounds());

    const int pad   = static_cast<int>(10.0f * sc);
    const int hdrH  = static_cast<int>(46.0f * sc);
    const int infoH = static_cast<int>(25.0f * sc);
    pedalboardView_.setBounds(0, hdrH, getWidth(), getHeight() - hdrH - infoH);

    // Header layout
    int hx = static_cast<int>(256.0f * sc);
    const int tunerW = static_cast<int>(220.0f * sc);
    tunerBar_.setBounds(hx, static_cast<int>(8.0f * sc), tunerW, static_cast<int>(30.0f * sc));
    hx += tunerW + static_cast<int>(3.0f * sc);
    tunerMuteBtn_.setBounds(hx, static_cast<int>(10.0f * sc), static_cast<int>(84.0f * sc), static_cast<int>(26.0f * sc));
    hx += static_cast<int>(92.0f * sc);
    rigTabBtn_.setBounds(hx, static_cast<int>(10.0f * sc), static_cast<int>(46.0f * sc), static_cast<int>(26.0f * sc));
    hx += static_cast<int>(50.0f * sc);
    pedalsTabBtn_.setBounds(hx, static_cast<int>(10.0f * sc), static_cast<int>(62.0f * sc), static_cast<int>(26.0f * sc));

    int rx = getWidth() - pad;
    auto placeRight = [&](juce::Component& c, int w)
    {
        rx -= w;
        c.setBounds(rx, static_cast<int>(10.0f * sc), w, static_cast<int>(26.0f * sc));
        rx -= static_cast<int>(4.0f * sc);
    };
    placeRight(settingsBtn_, static_cast<int>(72.0f * sc));
    placeRight(themeBtn_, static_cast<int>(52.0f * sc));
    placeRight(deletePresetBtn_, static_cast<int>(40.0f * sc));
    placeRight(renamePresetBtn_, static_cast<int>(40.0f * sc));
    placeRight(saveAsPresetBtn_, static_cast<int>(60.0f * sc));
    placeRight(savePresetBtn_, static_cast<int>(46.0f * sc));
    placeRight(presetBox_, static_cast<int>(200.0f * sc));

    // Main body
    auto body = getLocalBounds().withTrimmedTop(hdrH + pad).withTrimmedBottom(infoH + pad).reduced(pad, 0);
    const int leftW  = static_cast<int>(260.0f * sc);
    const int rightW = static_cast<int>(255.0f * sc);
    auto leftR  = body.removeFromLeft(leftW).reduced(static_cast<int>(10.0f * sc));
    body.removeFromLeft(pad);
    auto rightR = body.removeFromRight(rightW).reduced(static_cast<int>(10.0f * sc));
    body.removeFromRight(pad);
    auto ampR = body.removeFromTop(static_cast<int>(body.getHeight() * 0.56f)).reduced(static_cast<int>(10.0f * sc));
    body.removeFromTop(pad);
    auto cabR = body.reduced(static_cast<int>(10.0f * sc));

    const int topTitleSkip = static_cast<int>(24.0f * sc);
    leftR.removeFromTop(topTitleSkip);
    ampR.removeFromTop(topTitleSkip);
    cabR.removeFromTop(topTitleSkip);
    rightR.removeFromTop(topTitleSkip);

    // Left Column
    auto inRow = leftR.removeFromTop(static_cast<int>(86.0f * sc));
    inTrimKnob_.setBoundsCompact(static_cast<float>(inRow.getX()), static_cast<float>(inRow.getY()),
                                 static_cast<float>(inRow.getWidth() * 0.45f), static_cast<float>(inRow.getHeight()));
    auto inBtns = inRow.withTrimmedLeft(static_cast<int>(inRow.getWidth() * 0.48f));
    padBtn_.setBounds(inBtns.removeFromTop(static_cast<int>(26.0f * sc)));
    inBtns.removeFromTop(static_cast<int>(6.0f * sc));
    gateBtn_.setBounds(inBtns.removeFromTop(static_cast<int>(26.0f * sc)));

    leftR.removeFromTop(static_cast<int>(6.0f * sc));
    gateCurve_.setBounds(leftR.removeFromTop(static_cast<int>(62.0f * sc)));
    leftR.removeFromTop(static_cast<int>(10.0f * sc));

    auto diTop = leftR.removeFromTop(static_cast<int>(26.0f * sc));
    diEnBtn_.setBounds(diTop.removeFromLeft(static_cast<int>(88.0f * sc)));
    diTop.removeFromLeft(static_cast<int>(6.0f * sc));
    diCircuitBox_.setBounds(diTop);
    leftR.removeFromTop(static_cast<int>(6.0f * sc));
    diSubMixBar_.setBounds(leftR.removeFromTop(static_cast<int>(24.0f * sc)));
    leftR.removeFromTop(static_cast<int>(6.0f * sc));
    diFilterGraph_.setBounds(leftR.removeFromTop(static_cast<int>(110.0f * sc)));
    leftR.removeFromTop(static_cast<int>(6.0f * sc));
    diCompBar_.setBounds(leftR.removeFromTop(static_cast<int>(24.0f * sc)));
    leftR.removeFromTop(static_cast<int>(6.0f * sc));
    diAutoPhaseBtn_.setBounds(leftR.removeFromTop(static_cast<int>(26.0f * sc)));

    // Amp Head
    auto ampHeaderRow = ampR.removeFromTop(static_cast<int>(28.0f * sc));
    ampEnBtn_.setBounds(ampHeaderRow.removeFromLeft(static_cast<int>(66.0f * sc)));
    ampHeaderRow.removeFromLeft(static_cast<int>(8.0f * sc));
    if (ampModelPill_) ampModelPill_->setBounds(ampHeaderRow);

    ampR.removeFromTop(static_cast<int>(6.0f * sc));
    ampGraph_.setBounds(ampR.removeFromTop(static_cast<int>(138.0f * sc)));
    ampR.removeFromTop(static_cast<int>(6.0f * sc));

    auto switchRow = ampR.removeFromTop(static_cast<int>(24.0f * sc));
    if (ampChannelPill_->isVisible())
    {
        ampChannelPill_->setBounds(switchRow.removeFromLeft(static_cast<int>(190.0f * sc)));
        switchRow.removeFromLeft(static_cast<int>(6.0f * sc));
    }
    if (depthBox_.isVisible())
    {
        depthBox_.setBounds(switchRow.removeFromLeft(static_cast<int>(110.0f * sc)));
        switchRow.removeFromLeft(static_cast<int>(6.0f * sc));
    }
    brightBtn_.setBounds(switchRow.removeFromLeft(static_cast<int>(82.0f * sc)));
    switchRow.removeFromLeft(static_cast<int>(6.0f * sc));
    ultraLoBox_.setBounds(switchRow.removeFromLeft(static_cast<int>(108.0f * sc)));
    switchRow.removeFromLeft(static_cast<int>(6.0f * sc));
    ultraHiBtn_.setBounds(switchRow.removeFromLeft(static_cast<int>(88.0f * sc)));
    switchRow.removeFromLeft(static_cast<int>(6.0f * sc));
    if (midFreqBox_.isVisible())
        midFreqBox_.setBounds(switchRow.removeFromLeft(static_cast<int>(92.0f * sc)));

    ampR.removeFromTop(static_cast<int>(6.0f * sc));
    auto sagBiasRow = ampR.removeFromBottom(static_cast<int>(24.0f * sc));
    sagBar_.setBounds(sagBiasRow.removeFromLeft(sagBiasRow.getWidth() / 2 - 4));
    sagBiasRow.removeFromLeft(8);
    biasBar_.setBounds(sagBiasRow);
    ampR.removeFromBottom(static_cast<int>(4.0f * sc));

    // Knobs row
    std::vector<kalyx::bass::ui::KalyxRotaryControl*> knobs = {
        &gainKnob_, &bassKnob_, &midKnob_, &trebleKnob_, &driveKnob_
    };
    if (compBoostKnob_.slider.isVisible()) knobs.push_back(&compBoostKnob_);
    if (presenceKnob_.slider.isVisible()) knobs.push_back(&presenceKnob_);
    knobs.push_back(&masterKnob_);

    const float kw = static_cast<float>(ampR.getWidth()) / static_cast<float>(knobs.size());
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i]->setBoundsCompact(static_cast<float>(ampR.getX()) + static_cast<float>(i) * kw,
                                   static_cast<float>(ampR.getY()), kw, static_cast<float>(ampR.getHeight()));

    // Dual Cabinet IR (Full-width clean vertical layout)
    auto cabTop = cabR.removeFromTop(static_cast<int>(26.0f * sc));
    cabEnBtn_.setBounds(cabTop.removeFromLeft(static_cast<int>(94.0f * sc)));
    cabTop.removeFromLeft(static_cast<int>(8.0f * sc));
    cabAutoPhaseBtn_.setBounds(cabTop.removeFromLeft(static_cast<int>(120.0f * sc)));

    cabR.removeFromTop(static_cast<int>(10.0f * sc));
    auto cabARow = cabR.removeFromTop(static_cast<int>(28.0f * sc));
    const int btnPairW = static_cast<int>(140.0f * sc);
    cabABox_.setBounds(cabARow.removeFromLeft(cabARow.getWidth() - btnPairW));
    cabARow.removeFromLeft(static_cast<int>(6.0f * sc));
    phaseABtn_.setBounds(cabARow.removeFromLeft(static_cast<int>(48.0f * sc)));
    cabARow.removeFromLeft(static_cast<int>(6.0f * sc));
    loadIrABtn_.setBounds(cabARow);

    cabR.removeFromTop(static_cast<int>(8.0f * sc));
    auto cabBRow = cabR.removeFromTop(static_cast<int>(28.0f * sc));
    cabBBox_.setBounds(cabBRow.removeFromLeft(cabBRow.getWidth() - btnPairW));
    cabBRow.removeFromLeft(static_cast<int>(6.0f * sc));
    phaseBBtn_.setBounds(cabBRow.removeFromLeft(static_cast<int>(48.0f * sc)));
    cabBRow.removeFromLeft(static_cast<int>(6.0f * sc));
    loadIrBBtn_.setBounds(cabBRow);

    cabR.removeFromTop(static_cast<int>(14.0f * sc));
    const int cabBarH = static_cast<int>(26.0f * sc);
    cabMixBar_.setBounds(cabR.removeFromTop(cabBarH));
    cabR.removeFromTop(static_cast<int>(8.0f * sc));
    auto cabBottomRow = cabR.removeFromTop(cabBarH);
    cabBreakupBar_.setBounds(cabBottomRow.removeFromLeft(cabBottomRow.getWidth() / 2 - static_cast<int>(4.0f * sc)));
    cabBottomRow.removeFromLeft(static_cast<int>(8.0f * sc));
    cabRoomBar_.setBounds(cabBottomRow);

    // Right Column: Master & Mixer
    vuMeter_.setBounds(rightR.removeFromTop(static_cast<int>(82.0f * sc)));
    rightR.removeFromTop(static_cast<int>(4.0f * sc));
    if (vuModePill_) vuModePill_->setBounds(rightR.removeFromTop(static_cast<int>(22.0f * sc)));
    rightR.removeFromTop(static_cast<int>(4.0f * sc));
    phaseCorrBar_.setBounds(rightR.removeFromTop(static_cast<int>(20.0f * sc)));
    rightR.removeFromTop(static_cast<int>(8.0f * sc));

    auto botMaster = rightR.removeFromBottom(static_cast<int>(88.0f * sc));
    auto kickRow = botMaster.removeFromTop(static_cast<int>(24.0f * sc));
    kickEnBtn_.setBounds(kickRow.removeFromLeft(static_cast<int>(82.0f * sc)));
    kickRow.removeFromLeft(4);
    kickAmtBar_.setBounds(kickRow);
    botMaster.removeFromTop(static_cast<int>(6.0f * sc));
    auto limRow = botMaster.removeFromTop(static_cast<int>(24.0f * sc));
    limEnBtn_.setBounds(limRow.removeFromLeft(static_cast<int>(82.0f * sc)));
    limRow.removeFromLeft(4);
    limCeilBar_.setBounds(limRow);

    rightR.removeFromBottom(static_cast<int>(6.0f * sc));
    const int fw = rightR.getWidth() / 3;
    ampLevelFader_.setBounds(rightR.removeFromLeft(fw));
    diLevelFader_.setBounds(rightR.removeFromLeft(fw));
    outFader_.setBounds(rightR);
}
