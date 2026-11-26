#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

KalyxHybridConsoleEditor::KalyxHybridConsoleEditor(KalyxHybridConsoleProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef_(p),
      settingsOverlay_(p)
{
    setLookAndFeel(&lookAndFeel_);
    auto& apvts = processorRef_.getAPVTS();

    lastTheme_ = static_cast<kalyx::core::ChassisTheme>(
        std::clamp(static_cast<int>(std::round(apvts.getRawParameterValue(ParamIDs::chassisTheme)->load())), 0, 2));
    const bool isDay = (lastTheme_ == kalyx::core::ChassisTheme::Day);

    // -------------------------------------------------------------------------
    // Bandeau supérieur : Presets catégorisés + SAVE / SAVE AS / REN / DEL + A/B + M/S + Finition + SETTINGS
    // -------------------------------------------------------------------------
    presetMenuBtn_.onClick = [this]() { showCategorizedPresetMenu(); };
    addAndMakeVisible(presetMenuBtn_);

    saveBtn_.onClick   = [this]() { processorRef_.getPresetManager().saveCurrentPreset(); };
    saveAsBtn_.onClick = [this]() { promptSavePresetAs(); };
    renameBtn_.onClick = [this]() { promptRenamePreset(); };
    deleteBtn_.onClick = [this]() { promptDeletePreset(); };
    addAndMakeVisible(saveBtn_);
    addAndMakeVisible(saveAsBtn_);
    addAndMakeVisible(renameBtn_);
    addAndMakeVisible(deleteBtn_);

    abToggleBtn_.onClick = [this]()
    {
        processorRef_.toggleABState();
        abToggleBtn_.setButtonText(processorRef_.isSlotBActive() ? "A/B:B" : "A/B:A");
    };
    abCopyBtn_.onClick = [this]() { processorRef_.copyActiveToInactiveAB(); };
    addAndMakeVisible(abToggleBtn_);
    addAndMakeVisible(abCopyBtn_);

    msModeBox_.addItem("LR STEREO",  1);
    msModeBox_.addItem("M/S (M+S)",  2);
    msModeBox_.addItem("M/S (MID)",  3);
    msModeBox_.addItem("M/S (SIDE)", 4);
    msModeBox_.setTooltip("Global Master Bus Routing: LR STEREO, Mid/Side (M+S), Mid-Only, or Side-Only");
    addAndMakeVisible(msModeBox_);
    msModeAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, ParamIDs::masterMsMode, msModeBox_);

    chassisThemeBox_.addItem("NIGHT",          1);
    chassisThemeBox_.addItem("DAY MODE",       2);
    chassisThemeBox_.addItem("KALYX HERITAGE", 3);
    chassisThemeBox_.setTooltip("Select External Studio Chassis Finish (Night, Day Mode, Kalyx Heritage)");
    addAndMakeVisible(chassisThemeBox_);
    chassisThemeAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, ParamIDs::chassisTheme, chassisThemeBox_);

    settingsBtn_.setTooltip("Open Studio Settings (0 VU Reference Calibration, Oversampling, EQ FFT Mode)");
    settingsBtn_.onClick = [this]()
    {
        settingsOverlay_.setChassisTheme(lastTheme_);
        settingsOverlay_.setVisible(!settingsOverlay_.isVisible());
        settingsOverlay_.toFront(true);
    };
    addAndMakeVisible(settingsBtn_);

    presetMenuBtn_.setTooltip("Open Categorized Preset Browser (Tranches Completes, Hybrides, Mastering, Utilisateur)");
    saveBtn_.setTooltip("Overwrite active user preset");
    saveAsBtn_.setTooltip("Save current console state as a new user preset (always available)");
    renameBtn_.setTooltip("Rename active user preset");
    deleteBtn_.setTooltip("Delete active user preset");
    abToggleBtn_.setTooltip("Compare A/B console snapshots");
    abCopyBtn_.setTooltip("Copy active A/B snapshot to the inactive slot");

    // -------------------------------------------------------------------------
    // Tranche Fixe 1 : INPUT (adopte la finition historique du préampli sélectionné)
    // -------------------------------------------------------------------------
    inPreampBox_.addItem("British 4K-E", 1);
    inPreampBox_.addItem("British 4K-G", 2);
    inPreampBox_.addItem("American 25",  3);
    inPreampBox_.addItem("Class-A 73",   4);
    inPreampBox_.addItem("Kalyx Clean",  5);
    inPreampBox_.setTooltip("Select Input Preamp Circuit & Historical Faceplate Finish");
    addAndMakeVisible(inPreampBox_);
    inPreampAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, ParamIDs::inPreampModel, inPreampBox_);

    const auto accent = KalyxTheme::getAccent(lastTheme_);
    auto fmtRawDb  = [](double v) { return KalyxValueParsers::formatDbClean(v); };
    auto fmtRawPct = [](double v) { return juce::String(juce::roundToInt(v)) + " %"; };

    inTrimBar_.init (*this, apvts, ParamIDs::inTrim,  "IN TRIM", accent, true,  isDay, 0.0, fmtRawDb,  "Input Preamp Trim Gain (-24 to +24 dB)",
                     KalyxValueParsers::parseRawClamped(-24.0, 24.0));
    inDriveBar_.init(*this, apvts, ParamIDs::inDrive, "DRIVE",   KalyxTheme::kalyxRoseParme, false, isDay, 0.0, fmtRawPct, "Input Transformer & Preamp ADAA Saturation Drive (0 - 100 %)",
                     KalyxValueParsers::parseRawClamped(0.0, 100.0));
    inFilterGraph_.init(*this, apvts, ParamIDs::inHpfEn, ParamIDs::inHpfFreq, ParamIDs::inLpfEn, ParamIDs::inLpfFreq, accent, isDay);

    setupKalyxToggle(*this, inPhaseBtn_,    inPhaseAtt_,    apvts, ParamIDs::inPhase,    "PHASE",    KalyxTheme::alertRed, isDay);
    setupKalyxToggle(*this, inFiltToScBtn_, inFiltToScAtt_, apvts, ParamIDs::inFiltToSc, "FILT>SC",  accent, isDay);
    setupKalyxToggle(*this, inExtScBtn_,    inExtScAtt_,    apvts, ParamIDs::inExtSc,    "EXT SC",   accent, isDay);
    setupKalyxToggle(*this, inScListenBtn_, inScListenAtt_, apvts, ParamIDs::inScListen, "SC LSTN",  KalyxTheme::amberLed, isDay);
    inPhaseBtn_.setTooltip("Invert Input Polarity by 180 degrees");
    inFiltToScBtn_.setTooltip("Route Input HPF/LPF to the Dynamics Sidechain instead of main audio");
    inExtScBtn_.setTooltip("Use External Sidechain Bus (Inputs 3-4) for Dynamics modules");
    inScListenBtn_.setTooltip("Audition the Sidechain signal directly on Main Output");

    // -------------------------------------------------------------------------
    // 5 Emplacements Série 500 Interchangeables (Slot A..E)
    // -------------------------------------------------------------------------
    for (int s = 0; s < 5; ++s)
    {
        slotViews_[static_cast<std::size_t>(s)] = std::make_unique<ModuleSlotView>(processorRef_, s);
        addAndMakeVisible(*slotViews_[static_cast<std::size_t>(s)]);
    }

    // -------------------------------------------------------------------------
    // Tranche Fixe 7 : MASTER (VU-mètre 300 ms + [IN|OUT|GR] + Tol, Mix, Out Fader)
    // -------------------------------------------------------------------------
    vuMeter_.setTooltip("300 ms IEC 60268-17 Analog VU Meter (0 VU = -18 dBFS default, adjustable in SETTINGS)");
    addAndMakeVisible(vuMeter_);

    vuModePill_ = std::make_unique<KalyxSegmentedPill>(
        apvts, ParamIDs::masterVuMode, "",
        juce::StringArray { "IN", "OUT", "GR" }, accent, isDay,
        "Monitor IN, OUT, or Cumulative Gain Reduction (GR) on the Master VU Meter");
    addAndMakeVisible(*vuModePill_);

    analogTolBar_.init(*this, apvts, ParamIDs::masterAnalogTol, "ANALOG TOL", KalyxTheme::kalyxAubergine, false, isDay, 25.0, fmtRawPct,
                       "Analog Component Tolerance L/R Micro-Mismatch (0 - 100 %)", KalyxValueParsers::parseRawClamped(0.0, 100.0));
    mixBar_.init      (*this, apvts, ParamIDs::masterMix,       "MIX",        accent, false, isDay, 100.0, fmtRawPct,
                       "Global Parallel Dry/Wet Mix (0 - 100 %)", KalyxValueParsers::parseRawClamped(0.0, 100.0));
    outFader_.init    (*this, apvts, ParamIDs::masterOutGain,   "OUT FADER",  accent, isDay, -24.0, 12.0, 0.0,
                       "Master Output Fader (-24 to +12 dB, magnetic 0.0 dB detent)");

    addChildComponent(settingsOverlay_);

    lastSlotOrder_ = processorRef_.getSlotOrder();
    refreshPresetButtonText();
    applyChassisTheme(lastTheme_);
    const int initPre = std::clamp(static_cast<int>(std::round(apvts.getRawParameterValue(ParamIDs::inPreampModel)->load())), 0, 4);
    applyInputPreampTheme(initPre);
    updateSlotOrderVisuals();

    // Redimensionnement libre proportionnel (gabarit de référence 1480 x 820, uiScale 0.75x .. 2.20x)
    setResizable(true, true);
    getConstrainer()->setFixedAspectRatio(1480.0 / 820.0);
    setResizeLimits(1110, 615, 2960, 1640);
    setSize(1480, 820);

    startTimerHz(30);
}

KalyxHybridConsoleEditor::~KalyxHybridConsoleEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void KalyxHybridConsoleEditor::applyInputPreampTheme(int preampModel)
{
    lastInPreampModel_ = std::clamp(preampModel, 0, 4);

    const bool isDay = (lastTheme_ == kalyx::core::ChassisTheme::Day);
    const bool darkTxt = isDay;
    const juce::Colour txtCol = darkTxt ? KalyxTheme::dayTextPrimary : KalyxTheme::nightTextPrimary;
    const juce::Colour capCol = KalyxTheme::getControlIdleBg(lastTheme_);

    juce::Colour arcCol;
    switch (lastInPreampModel_)
    {
        case 0: arcCol = KalyxTheme::eqLfWineRed;        break; // SSL 4000 E
        case 1: arcCol = KalyxTheme::sslGPinkMidCap;     break; // SSL 4000 G
        case 2: arcCol = KalyxTheme::apiCyanLight;       break; // API 2520
        case 3: arcCol = KalyxTheme::neveBrickRed;       break; // Neve 1073
        default:arcCol = KalyxTheme::kalyxRoyalBlueGlow; break; // Kalyx Clean
    }

    inTrimBar_.setThemeColours(arcCol, darkTxt);
    inDriveBar_.setThemeColours(KalyxTheme::kalyxAubergineHover, darkTxt);
    inFilterGraph_.setThemeColours(arcCol, darkTxt);

    juce::ToggleButton* inTogs[] = { &inPhaseBtn_, &inFiltToScBtn_, &inExtScBtn_, &inScListenBtn_ };
    for (auto* t : inTogs) t->setColour(juce::ToggleButton::textColourId, txtCol);
    repaint();
}

void KalyxHybridConsoleEditor::applyChassisTheme(kalyx::core::ChassisTheme theme)
{
    lastTheme_ = theme;
    lookAndFeel_.setChassisTheme(theme);
    settingsOverlay_.setChassisTheme(theme);
    vuMeter_.setChassisTheme(theme);

    const bool isDay   = (theme == kalyx::core::ChassisTheme::Day);
    const auto txtCol  = KalyxTheme::getTextPrimary(theme);
    const auto idleBg  = KalyxTheme::getControlIdleBg(theme);
    const auto accent  = KalyxTheme::getAccent(theme);
    const auto primary = KalyxTheme::getPrimary(theme);

    analogTolBar_.setThemeColours(primary, isDay);
    mixBar_.setThemeColours(accent, isDay);
    outFader_.setThemeColours(accent, isDay);

    juce::TextButton* hdrBtns[] = { &presetMenuBtn_, &saveBtn_, &saveAsBtn_, &renameBtn_, &deleteBtn_, &abCopyBtn_, &settingsBtn_ };
    for (auto* b : hdrBtns)
    {
        b->setColour(juce::TextButton::buttonColourId, idleBg);
        b->setColour(juce::TextButton::textColourOffId, txtCol);
    }
    abToggleBtn_.setColour(juce::TextButton::buttonColourId, primary);
    abToggleBtn_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);

    if (vuModePill_)
        vuModePill_->setThemeColours(accent, isDay);

    msModeBox_.lookAndFeelChanged();
    chassisThemeBox_.lookAndFeelChanged();
    inPreampBox_.lookAndFeelChanged();
    if (lastInPreampModel_ >= 0)
        applyInputPreampTheme(lastInPreampModel_);

    for (auto& sv : slotViews_)
        if (sv) sv->setChassisTheme(theme);

    repaint();
}

void KalyxHybridConsoleEditor::updateSlotOrderVisuals()
{
    const auto order = processorRef_.getSlotOrder();
    for (int pos = 0; pos < 5; ++pos)
    {
        const int sIdx = order[static_cast<std::size_t>(pos)];
        auto& view = *slotViews_[static_cast<std::size_t>(sIdx)];
        const bool is2U = kalyx::core::isModuleId2U(processorRef_.getSlotModuleId(sIdx)) && (pos < 4);

        if (is2U)
        {
            view.setVisualPosition(pos, pos > 0, pos + 2 <= 4);
            view.setReorderCallbacks(
                [this, pos]()
                {
                    processorRef_.move2USlot(pos, -1);
                    lastSlotOrder_ = processorRef_.getSlotOrder();
                    updateSlotOrderVisuals();
                    resized();
                    repaint();
                },
                [this, pos]()
                {
                    processorRef_.move2USlot(pos, +1);
                    lastSlotOrder_ = processorRef_.getSlotOrder();
                    updateSlotOrderVisuals();
                    resized();
                    repaint();
                });
            ++pos; // Skip linked partner slot
        }
        else
        {
            view.setVisualPosition(pos, pos > 0, pos < 4);
            view.setReorderCallbacks(
                [this, pos]()
                {
                    const auto curOrder = processorRef_.getSlotOrder();
                    if (pos >= 2 && processorRef_.isSlotLinkedTo2U(curOrder[static_cast<std::size_t>(pos - 1)]))
                        processorRef_.move2USlot(pos - 2, +1);
                    else
                        processorRef_.swapVisualSlots(pos, pos - 1);
                    lastSlotOrder_ = processorRef_.getSlotOrder();
                    updateSlotOrderVisuals();
                    resized();
                    repaint();
                },
                [this, pos]()
                {
                    const auto curOrder = processorRef_.getSlotOrder();
                    if (pos < 4 && kalyx::core::isModuleId2U(processorRef_.getSlotModuleId(curOrder[static_cast<std::size_t>(pos + 1)])))
                        processorRef_.move2USlot(pos + 1, -1);
                    else
                        processorRef_.swapVisualSlots(pos, pos + 1);
                    lastSlotOrder_ = processorRef_.getSlotOrder();
                    updateSlotOrderVisuals();
                    resized();
                    repaint();
                });
        }
    }
}

void KalyxHybridConsoleEditor::refreshPresetButtonText()
{
    presetMenuBtn_.setButtonText(processorRef_.getPresetManager().getCurrentPresetName());
    const bool isUser = processorRef_.getPresetManager().isCurrentPresetUser();
    saveBtn_.setEnabled(isUser);
    saveAsBtn_.setEnabled(true); // Toujours actif, même sur les presets d'usine
    renameBtn_.setEnabled(isUser);
    deleteBtn_.setEnabled(isUser);
}

void KalyxHybridConsoleEditor::showCategorizedPresetMenu()
{
    auto& pm = processorRef_.getPresetManager();
    pm.rescanPresets();
    const auto& all = pm.getPresets();

    juce::PopupMenu root;
    const char* cats[4] = { "Tranches Completes", "Hybrides", "Mastering", "Utilisateur" };
    for (const char* cat : cats)
    {
        juce::PopupMenu sub;
        for (int i = 0; i < static_cast<int>(all.size()); ++i)
        {
            if (all[static_cast<std::size_t>(i)].category == cat)
                sub.addItem(i + 1, all[static_cast<std::size_t>(i)].name, true, i == pm.getCurrentPresetIndex());
        }
        if (sub.getNumItems() == 0)
            sub.addItem(-1, "(Aucun preset)", false, false);
        root.addSubMenu(cat, sub);
    }

    root.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(presetMenuBtn_),
                       [this](int result)
                       {
                           if (result > 0)
                           {
                               processorRef_.getPresetManager().loadPresetByIndex(result - 1);
                               refreshPresetButtonText();
                               updateSlotOrderVisuals();
                               resized();
                           }
                       });
}

void KalyxHybridConsoleEditor::promptSavePresetAs()
{
    auto* dlg = new juce::AlertWindow("Save Preset As", "Nom du nouveau preset (.xml) :", juce::MessageBoxIconType::NoIcon);
    dlg->addTextEditor("name", processorRef_.getPresetManager().getCurrentPresetName() + " Copy", "Nom :");
    dlg->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dlg->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dlg->enterModalState(true, juce::ModalCallbackFunction::create([this, dlg](int res)
    {
        if (res == 1)
        {
            processorRef_.getPresetManager().savePresetAs(dlg->getTextEditorContents("name"));
            refreshPresetButtonText();
        }
    }), true);
}

void KalyxHybridConsoleEditor::promptRenamePreset()
{
    if (!processorRef_.getPresetManager().isCurrentPresetUser()) return;
    auto* dlg = new juce::AlertWindow("Rename Preset", "Nouveau nom du preset :", juce::MessageBoxIconType::NoIcon);
    dlg->addTextEditor("name", processorRef_.getPresetManager().getCurrentPresetName(), "Nom :");
    dlg->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dlg->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dlg->enterModalState(true, juce::ModalCallbackFunction::create([this, dlg](int res)
    {
        if (res == 1)
        {
            processorRef_.getPresetManager().renameCurrentPreset(dlg->getTextEditorContents("name"));
            refreshPresetButtonText();
        }
    }), true);
}

void KalyxHybridConsoleEditor::promptDeletePreset()
{
    if (!processorRef_.getPresetManager().isCurrentPresetUser()) return;
    juce::AlertWindow::showOkCancelBox(
        juce::MessageBoxIconType::WarningIcon,
        "Delete Preset",
        "Supprimer definitivement le preset '" + processorRef_.getPresetManager().getCurrentPresetName() + "' ?",
        "Delete", "Cancel", this,
        juce::ModalCallbackFunction::create([this](int res)
        {
            if (res == 1)
            {
                processorRef_.getPresetManager().deleteCurrentPreset();
                refreshPresetButtonText();
            }
        }));
}

void KalyxHybridConsoleEditor::timerCallback()
{
    auto& apvts = processorRef_.getAPVTS();
    meterSnap_ = processorRef_.getMeterSnapshot();

    const auto curTheme = static_cast<kalyx::core::ChassisTheme>(
        std::clamp(static_cast<int>(std::round(apvts.getRawParameterValue(ParamIDs::chassisTheme)->load(std::memory_order_relaxed))), 0, 2));
    const int curVuMode = std::clamp(
        static_cast<int>(std::round(apvts.getRawParameterValue(ParamIDs::masterVuMode)->load(std::memory_order_relaxed))), 0, 2);
    const int curPreModel = std::clamp(
        static_cast<int>(std::round(apvts.getRawParameterValue(ParamIDs::inPreampModel)->load(std::memory_order_relaxed))), 0, 4);

    if (curTheme != lastTheme_)
        applyChassisTheme(curTheme);
    if (curVuMode != lastVuMode_)
        lastVuMode_ = curVuMode;
    if (curPreModel != lastInPreampModel_)
        applyInputPreampTheme(curPreModel);

    const auto curOrder = processorRef_.getSlotOrder();
    static std::array<bool, 5> lastIs2U { false, false, false, false, false };
    std::array<bool, 5> curIs2U {};
    for (int s = 0; s < 5; ++s)
        curIs2U[static_cast<std::size_t>(s)] = kalyx::core::isModuleId2U(processorRef_.getSlotModuleId(s));

    if (curOrder != lastSlotOrder_ || curIs2U != lastIs2U)
    {
        lastSlotOrder_ = curOrder;
        lastIs2U = curIs2U;
        updateSlotOrderVisuals();
        resized();
        repaint();
    }

    for (auto& sv : slotViews_)
        sv->updateFFTAndRepaint();

    const float vuRef = apvts.getRawParameterValue(ParamIDs::vuRefDb)->load(std::memory_order_relaxed);
    vuMeter_.updateTelemetry(meterSnap_, curVuMode, vuRef);
    vuMeter_.repaint();

    // Mini-LEDs SIG (verte) et PEAK (rouge) sur la tranche INPUT
    const float maxInPk = std::max(meterSnap_.inputPeakDbL, meterSnap_.inputPeakDbR);
    const bool  sigNow  = (maxInPk > -48.0f);
    const bool  pkNow   = (maxInPk >= -3.0f);
    if (sigNow != inSigLit_ || pkNow != inPeakLit_)
    {
        inSigLit_  = sigNow;
        inPeakLit_ = pkNow;
        if (!inLedsRect_.isEmpty())
            repaint(inLedsRect_.expanded(2.0f).toNearestInt());
    }

    // Barre d'info contextuelle discrète au survol d'un contrôle
    juce::String newTip = "Shift/Ctrl + Drag: Fine Mode (0.1x) | Double-Click: Reset | Click Lock: Protect Strip";
    if (auto* under = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse())
    {
        auto* c = under;
        for (int depth = 0; depth < 3 && c != nullptr; ++depth, c = c->getParentComponent())
        {
            if (auto* tc = dynamic_cast<juce::TooltipClient*>(c))
            {
                const auto t = tc->getTooltip();
                if (t.isNotEmpty()) { newTip = t; break; }
            }
        }
    }
    if (newTip != infoBarText_)
    {
        infoBarText_ = newTip;
        repaint(8, getHeight() - 22, getWidth() - 16, 20);
    }

    repaint(getWidth() - 220, 8, 212, 36);
}

void KalyxHybridConsoleEditor::drawLockIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool isLocked) const
{
    const auto box = bounds.reduced(1.5f);
    g.setColour(isLocked ? KalyxTheme::amberLed : juce::Colours::white.withAlpha(0.18f));
    g.fillRoundedRectangle(box, 2.5f);
    g.setColour(isLocked ? juce::Colours::white : juce::Colours::white.withAlpha(0.55f));
    g.drawRoundedRectangle(box, 2.5f, 1.0f);

    const float cx = box.getCentreX(), cy = box.getCentreY();
    g.setColour(isLocked ? KalyxTheme::dayTextPrimary : juce::Colours::white);
    g.fillRoundedRectangle(cx - 3.0f, cy - 0.5f, 6.0f, 5.0f, 1.0f);

    juce::Path sh;
    if (isLocked) sh.addCentredArc(cx, cy - 1.0f, 2.2f, 2.8f, 0.0f, -2.8f, 2.8f, true);
    else          sh.addCentredArc(cx + 1.2f, cy - 2.0f, 2.2f, 2.8f, 0.0f, -2.5f, 1.8f, true);
    g.strokePath(sh, juce::PathStrokeType(1.3f));
}

void KalyxHybridConsoleEditor::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto txtCol = KalyxTheme::getTextPrimary(lastTheme_);
    const auto accent = KalyxTheme::getAccent(lastTheme_);
    const float margin = 8.0f * uiScale_;
    const float hdrH   = 36.0f * uiScale_;
    const float radius = 4.0f * uiScale_;

    // 1. Châssis externe (Anthracite Nuit #111318 ou Day Mode #E6E8EC)
    g.fillAll(KalyxTheme::getChassisBg(lastTheme_));

    // 2. Bandeau supérieur sur une seule ligne non tronquée
    const juce::Rectangle<float> headerRect(margin, margin, bounds.getWidth() - 2.0f * margin, hdrH);
    g.setColour(KalyxTheme::getStripBg(lastTheme_));
    g.fillRoundedRectangle(headerRect, radius);
    g.setColour(KalyxTheme::getGridBorder(lastTheme_));
    g.drawRoundedRectangle(headerRect, radius, std::max(0.8f, 1.0f * uiScale_));

    g.setColour(txtCol);
    g.setFont(juce::Font(juce::FontOptions(13.5f * uiScale_, juce::Font::bold)));
    g.drawText("KALYX // HYBRID CONSOLE",
               static_cast<int>(14.0f * uiScale_), static_cast<int>(margin),
               static_cast<int>(184.0f * uiScale_), static_cast<int>(hdrH),
               juce::Justification::centredLeft);

    g.setColour(KalyxTheme::getTextSecondary(lastTheme_));
    g.setFont(juce::Font(juce::FontOptions(10.5f * uiScale_, juce::Font::plain)));
    g.drawText(juce::String("v") + JucePlugin_VersionString,
               static_cast<int>(196.0f * uiScale_), static_cast<int>(margin + 1.0f * uiScale_),
               static_cast<int>(48.0f * uiScale_), static_cast<int>(hdrH),
               juce::Justification::centredLeft);

    const juce::String stat = juce::String::formatted("GR: -%4.1f dB", meterSnap_.cumulativeGrDb);
    g.setColour(lastTheme_ == kalyx::core::ChassisTheme::Day ? KalyxTheme::dayTextPrimary : accent);
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.0f * uiScale_, juce::Font::bold)));
    g.drawText(stat,
               static_cast<int>(bounds.getWidth() - 220.0f * uiScale_), static_cast<int>(margin),
               static_cast<int>(204.0f * uiScale_), static_cast<int>(hdrH),
               juce::Justification::centredRight);

    // 3. Tranches Fixes INPUT et MASTER (surface unifiée #181B22 ou Day #F4F5F7)
    const float barH = 26.0f * uiScale_;
    auto drawFixedPanel = [&](const juce::Rectangle<float>& rect, const char* title,
                              juce::Colour bgCol, juce::Colour barAccent,
                              int lockIdx, juce::Rectangle<float>& lockOut)
    {
        if (rect.isEmpty()) return;
        g.setColour(bgCol);
        g.fillRoundedRectangle(rect, radius);

        const juce::Rectangle<float> bar(rect.getX(), rect.getY(), rect.getWidth(), barH);
        g.setColour(KalyxTheme::nightChassisBg);
        g.fillRoundedRectangle(bar, radius);
        g.setColour(barAccent);
        g.fillRect(rect.getX(), rect.getY() + barH - 2.0f * uiScale_, rect.getWidth(), 2.0f * uiScale_);

        g.setColour(KalyxTheme::getGridBorder(lastTheme_));
        g.drawRoundedRectangle(rect.reduced(0.5f), radius, std::max(0.8f, 1.0f * uiScale_));

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(11.0f * uiScale_, juce::Font::bold)));
        g.drawText(title, bar.withTrimmedLeft(6.0f * uiScale_).withTrimmedRight(22.0f * uiScale_).toNearestInt(),
                   juce::Justification::centredLeft);

        const float lockSz = 17.0f * uiScale_;
        lockOut = { rect.getRight() - lockSz - 3.0f * uiScale_, rect.getY() + 4.0f * uiScale_, lockSz, lockSz };
        drawLockIcon(g, lockOut, processorRef_.isSectionLocked(lockIdx));
    };

    juce::Colour inAcc = KalyxTheme::eqLfWineRed;
    switch (lastInPreampModel_)
    {
        case 0: inAcc = KalyxTheme::eqLfWineRed;     break;
        case 1: inAcc = KalyxTheme::sslGPinkMidCap;  break;
        case 2: inAcc = KalyxTheme::apiCobaltBlue;   break;
        case 3: inAcc = KalyxTheme::neveBrickRed;    break;
        default:inAcc = KalyxTheme::kalyxRoyalBlue;  break;
    }

    const juce::Colour stripBg = KalyxTheme::getStripBg(lastTheme_);
    drawFixedPanel(inputPanelRect_,  "INPUT",  stripBg, inAcc,  0, inputLockRect_);
    drawFixedPanel(masterPanelRect_, "MASTER", stripBg, accent, 6, masterLockRect_);

    // Mini-LEDs SIG (verte) & PEAK (rouge) à côté du sélecteur de préampli INPUT
    if (!inLedsRect_.isEmpty())
    {
        const juce::Colour lblCol = KalyxTheme::getTextPrimary(lastTheme_);
        const float halfH = inLedsRect_.getHeight() * 0.5f;
        const float ledX  = inLedsRect_.getX() + 5.0f * uiScale_;
        const float r     = 3.0f * uiScale_;

        auto drawMiniLed = [&](float cy, bool lit, juce::Colour onCol, const char* lbl)
        {
            if (lit)
            {
                g.setColour(onCol.withAlpha(0.35f));
                g.fillEllipse(ledX - r - 1.5f * uiScale_, cy - r - 1.5f * uiScale_,
                              (r + 1.5f * uiScale_) * 2.0f, (r + 1.5f * uiScale_) * 2.0f);
                g.setColour(onCol);
            }
            else
            {
                g.setColour(KalyxTheme::knobTrackBg);
            }
            g.fillEllipse(ledX - r, cy - r, r * 2.0f, r * 2.0f);
            g.setColour(lit ? lblCol : lblCol.withAlpha(0.65f));
            g.setFont(juce::Font(juce::FontOptions(8.0f * uiScale_, juce::Font::bold)));
            g.drawText(lbl, static_cast<int>(ledX + r + 2.0f * uiScale_), static_cast<int>(cy - halfH * 0.5f),
                       static_cast<int>(24.0f * uiScale_), static_cast<int>(halfH), juce::Justification::centredLeft);
        };

        drawMiniLed(inLedsRect_.getY() + halfH * 0.5f, inSigLit_,  juce::Colour(0xff22c55e), "SIG");
        drawMiniLed(inLedsRect_.getY() + halfH * 1.5f, inPeakLit_, KalyxTheme::alertRed,     "PK");
    }

    // 4. Barre d'info contextuelle discrète en bas de l'interface
    const float infoH = 18.0f * uiScale_;
    const juce::Rectangle<float> infoRect(margin, bounds.getHeight() - infoH - 4.0f * uiScale_,
                                          bounds.getWidth() - 2.0f * margin, infoH);
    g.setColour(stripBg);
    g.fillRoundedRectangle(infoRect, 3.0f * uiScale_);
    g.setColour(KalyxTheme::getGridBorder(lastTheme_));
    g.drawRoundedRectangle(infoRect, 3.0f * uiScale_, std::max(0.8f, 1.0f * uiScale_));
    g.setColour(txtCol.withAlpha(0.82f));
    g.setFont(juce::Font(juce::FontOptions(10.5f * uiScale_, juce::Font::plain)));
    g.drawText(infoBarText_, infoRect.reduced(8.0f * uiScale_, 0.0f).toNearestInt(), juce::Justification::centredLeft, true);
}

void KalyxHybridConsoleEditor::resized()
{
    const auto bounds = getLocalBounds().toFloat();
    const float uiScale = std::clamp(std::min(bounds.getWidth() / 1480.0f, bounds.getHeight() / 820.0f), 0.75f, 2.20f);
    uiScale_ = uiScale;

    lookAndFeel_.setUiScale(uiScale);
    vuMeter_.setUiScale(uiScale);
    if (vuModePill_) vuModePill_->setUiScale(uiScale);

    inTrimBar_.setUiScale(uiScale);
    inDriveBar_.setUiScale(uiScale);
    inFilterGraph_.setUiScale(uiScale);
    analogTolBar_.setUiScale(uiScale);
    mixBar_.setUiScale(uiScale);
    outFader_.setUiScale(uiScale);

    settingsOverlay_.setBounds(getLocalBounds());

    const float margin = 8.0f * uiScale;
    const float hdrH   = 36.0f * uiScale;

    // 1. Header Flexbox proportionnel
    {
        juce::FlexBox fb;
        fb.flexDirection  = juce::FlexBox::Direction::row;
        fb.justifyContent = juce::FlexBox::JustifyContent::flexStart;
        fb.alignItems     = juce::FlexBox::AlignItems::center;

        const float m  = 2.0f * uiScale;
        const float bh = 23.0f * uiScale;
        fb.items.add(juce::FlexItem(presetMenuBtn_).withWidth(160.0f * uiScale).withHeight(bh).withMargin({ 0, m, 0, m }));
        fb.items.add(juce::FlexItem(saveBtn_).withWidth(40.0f * uiScale).withHeight(bh).withMargin({ 0, m, 0, m }));
        fb.items.add(juce::FlexItem(saveAsBtn_).withWidth(54.0f * uiScale).withHeight(bh).withMargin({ 0, m, 0, m }));
        fb.items.add(juce::FlexItem(renameBtn_).withWidth(34.0f * uiScale).withHeight(bh).withMargin({ 0, m, 0, m }));
        fb.items.add(juce::FlexItem(deleteBtn_).withWidth(34.0f * uiScale).withHeight(bh).withMargin({ 0, m, 0, m }));
        fb.items.add(juce::FlexItem(abToggleBtn_).withWidth(46.0f * uiScale).withHeight(bh).withMargin({ 0, m, 0, 4.0f * uiScale }));
        fb.items.add(juce::FlexItem(abCopyBtn_).withWidth(42.0f * uiScale).withHeight(bh).withMargin({ 0, m, 0, m }));
        fb.items.add(juce::FlexItem(msModeBox_).withWidth(110.0f * uiScale).withHeight(bh).withMargin({ 0, m, 0, 4.0f * uiScale }));
        fb.items.add(juce::FlexItem(chassisThemeBox_).withWidth(130.0f * uiScale).withHeight(bh).withMargin({ 0, m, 0, m }));
        fb.items.add(juce::FlexItem(settingsBtn_).withWidth(66.0f * uiScale).withHeight(bh).withMargin({ 0, m, 0, m }));

        const float leftX  = 244.0f * uiScale;
        const float rightW = 200.0f * uiScale;
        fb.performLayout(juce::Rectangle<float>(leftX, margin, std::max(100.0f, bounds.getWidth() - leftX - rightW), hdrH));
    }

    // 2. 7 Colonnes : [ INPUT | SLOT A | SLOT B | SLOT C | SLOT D | SLOT E | MASTER ]
    const float topY   = margin + hdrH + 6.0f * uiScale;
    const float botGap = 26.0f * uiScale;
    const float panelH = bounds.getHeight() - topY - botGap;
    const float totalW = bounds.getWidth() - 2.0f * margin;
    const float gap    = 5.0f * uiScale;
    const float availW = totalW - 6.0f * gap;

    const float wIn   = std::floor(availW * 0.115f);
    const float wMast = std::floor(availW * 0.135f);
    const float wSlot = std::floor((availW - wIn - wMast) / 5.0f);

    float curX = margin;

    // Tranche Fixe 1 : INPUT
    inputPanelRect_ = { curX, topY, wIn, panelH };
    {
        const float pad = 5.0f * uiScale, iw = wIn - 2.0f * pad, hw = (iw - 3.0f * uiScale) * 0.5f;
        float y = topY + 30.0f * uiScale;
        const float ledsW = 30.0f * uiScale;
        const float pillH = 22.0f * uiScale;
        inPreampBox_.setBounds(static_cast<int>(curX + pad), static_cast<int>(y), static_cast<int>(iw - ledsW - 2.0f * uiScale), static_cast<int>(pillH));
        inLedsRect_ = { curX + pad + iw - ledsW, y, ledsW, pillH };
        y += pillH + 5.0f * uiScale;

        inPhaseBtn_.setBounds(static_cast<int>(curX + pad), static_cast<int>(y), static_cast<int>(iw), static_cast<int>(21.0f * uiScale));
        y += 26.0f * uiScale;

        const float barH = 24.0f * uiScale;
        inTrimBar_.setBounds (static_cast<int>(curX + pad), static_cast<int>(y), static_cast<int>(iw), static_cast<int>(barH)); y += barH + 5.0f * uiScale;
        inDriveBar_.setBounds(static_cast<int>(curX + pad), static_cast<int>(y), static_cast<int>(iw), static_cast<int>(barH)); y += barH + 8.0f * uiScale;

        const float botBtnsH = 48.0f * uiScale;
        const float graphH   = std::max(72.0f * uiScale, (topY + panelH - 8.0f * uiScale - botBtnsH) - y);
        inFilterGraph_.setBounds(static_cast<int>(curX + pad), static_cast<int>(y), static_cast<int>(iw), static_cast<int>(graphH));
        y += graphH + 8.0f * uiScale;

        inFiltToScBtn_.setBounds(static_cast<int>(curX + pad), static_cast<int>(y), static_cast<int>(iw), static_cast<int>(20.0f * uiScale));
        y += 23.0f * uiScale;
        inExtScBtn_.setBounds   (static_cast<int>(curX + pad),                 static_cast<int>(y), static_cast<int>(hw), static_cast<int>(20.0f * uiScale));
        inScListenBtn_.setBounds(static_cast<int>(curX + pad + hw + 3.0f * uiScale), static_cast<int>(y), static_cast<int>(hw), static_cast<int>(20.0f * uiScale));
    }
    curX += wIn + gap;

    // 5 Slots Série 500 (A..E) avec propagation de uiScale et support 2U Double-Largeur
    const auto order = processorRef_.getSlotOrder();
    for (int pos = 0; pos < 5; ++pos)
    {
        const int sIdx = order[static_cast<std::size_t>(pos)];
        auto& sv = *slotViews_[static_cast<std::size_t>(sIdx)];
        const bool is2U = kalyx::core::isModuleId2U(processorRef_.getSlotModuleId(sIdx)) && (pos < 4);

        if (is2U)
        {
            const int partnerIdx = order[static_cast<std::size_t>(pos + 1)];
            slotViews_[static_cast<std::size_t>(partnerIdx)]->setVisible(false);

            sv.setVisible(true);
            sv.setBounds(static_cast<int>(curX), static_cast<int>(topY),
                         static_cast<int>(2.0f * wSlot + gap), static_cast<int>(panelH));
            sv.setUiScale(uiScale);
            curX += 2.0f * wSlot + 2.0f * gap;
            ++pos; // Skip linked partner slot
        }
        else
        {
            sv.setVisible(true);
            sv.setBounds(static_cast<int>(curX), static_cast<int>(topY),
                         static_cast<int>(wSlot), static_cast<int>(panelH));
            sv.setUiScale(uiScale);
            curX += wSlot + gap;
        }
    }

    // Tranche Fixe 7 : MASTER (Compact 300 ms Analog VU Meter + [IN|OUT|GR] + TactileFaderBar Tol/Mix + VerticalConsoleFader)
    const float finalMastW = bounds.getWidth() - margin - curX;
    masterPanelRect_ = { curX, topY, finalMastW, panelH };
    {
        const float pad = 6.0f * uiScale, iw = finalMastW - 2.0f * pad;
        float y = topY + 30.0f * uiScale;

        const float vuH = 128.0f * uiScale;
        vuMeter_.setBounds(static_cast<int>(curX + pad), static_cast<int>(y), static_cast<int>(iw), static_cast<int>(vuH));
        y += vuH + 4.0f * uiScale;

        const float pillH = 22.0f * uiScale;
        if (vuModePill_)
            vuModePill_->setBounds(static_cast<int>(curX + pad), static_cast<int>(y), static_cast<int>(iw), static_cast<int>(pillH));
        y += pillH + 8.0f * uiScale;

        const float barH = 24.0f * uiScale;
        analogTolBar_.setBounds(static_cast<int>(curX + pad), static_cast<int>(y), static_cast<int>(iw), static_cast<int>(barH)); y += barH + 5.0f * uiScale;
        mixBar_.setBounds      (static_cast<int>(curX + pad), static_cast<int>(y), static_cast<int>(iw), static_cast<int>(barH)); y += barH + 10.0f * uiScale;

        const float faderH = std::max(80.0f * uiScale, (topY + panelH - 6.0f * uiScale) - y);
        outFader_.setBounds(static_cast<int>(curX + pad), static_cast<int>(y), static_cast<int>(iw), static_cast<int>(faderH));
    }
}

void KalyxHybridConsoleEditor::mouseDown(const juce::MouseEvent& e)
{
    if (inputLockRect_.expanded(3.0f).contains(e.position))
    {
        processorRef_.setSectionLocked(0, !processorRef_.isSectionLocked(0));
        repaint();
        return;
    }
    if (masterLockRect_.expanded(3.0f).contains(e.position))
    {
        processorRef_.setSectionLocked(6, !processorRef_.isSectionLocked(6));
        repaint();
    }
}
