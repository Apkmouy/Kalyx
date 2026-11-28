#include "ModuleSlotView.h"
#include "../Core/SlotRackDSP.h"
#include <cmath>
#include <algorithm>

namespace
{
    using namespace kalyx::core;

    inline std::function<juce::String(double)> fmtDb(float minDb, float maxDb)
    {
        return [minDb, maxDb](double v)
        {
            return KalyxValueParsers::formatDbClean(mapLin(static_cast<float>(v), minDb, maxDb));
        };
    }

    inline std::function<double(const juce::String&)> parseDb(float minDb, float maxDb)
    {
        return KalyxValueParsers::parseLinDb(minDb, maxDb);
    }

    inline std::function<juce::String(double)> fmtPct(float minP = 0.0f, float maxP = 100.0f)
    {
        return [minP, maxP](double v)
        {
            return juce::String(juce::roundToInt(mapLin(static_cast<float>(v), minP, maxP))) + " %";
        };
    }

    inline std::function<double(const juce::String&)> parsePct(float minP = 0.0f, float maxP = 100.0f)
    {
        return KalyxValueParsers::parseLinPct(minP, maxP);
    }

    inline std::function<juce::String(double)> fmtHzLog(float minHz, float maxHz)
    {
        return [minHz, maxHz](double v)
        {
            const float f = mapLog(static_cast<float>(v), minHz, maxHz);
            if (f >= 1000.0f) return juce::String(f * 0.001f, 1) + " kHz";
            return juce::String(juce::roundToInt(f)) + " Hz";
        };
    }

    inline std::function<double(const juce::String&)> parseHzLog(float minHz, float maxHz)
    {
        return KalyxValueParsers::parseHzLog(minHz, maxHz);
    }

    inline std::function<juce::String(double)> fmtMsLog(float minMs, float maxMs)
    {
        return [minMs, maxMs](double v)
        {
            const float m = mapLog(static_cast<float>(v), minMs, maxMs);
            return (m < 10.0f) ? (juce::String(m, 1) + " ms") : (juce::String(juce::roundToInt(m)) + " ms");
        };
    }

    inline std::function<double(const juce::String&)> parseMsLog(float minMs, float maxMs)
    {
        return KalyxValueParsers::parseMsLog(minMs, maxMs);
    }

    inline std::function<juce::String(double)> fmtRatioLog(float minR, float maxR)
    {
        return [minR, maxR](double v)
        {
            return juce::String(mapLog(static_cast<float>(v), minR, maxR), 1) + ":1";
        };
    }

    inline std::function<double(const juce::String&)> parseRatioLog(float minR, float maxR)
    {
        return KalyxValueParsers::parseRatioLog(minR, maxR);
    }
} // namespace

ModuleSlotView::ModuleSlotView(KalyxHybridConsoleProcessor& proc, int slotIndex)
    : processor_(proc), slotIndex_(slotIndex)
{
    moduleMenuBtn_.setTooltip("Click to select a 500-Series module from 5 categories (Preamplis, Compression, EQ, Coloration, Utilitaire)");
    moduleMenuBtn_.onClick = [this]() { showCategorizedModuleMenu(); };
    addAndMakeVisible(moduleMenuBtn_);

    setupKalyxToggle(*this, agBtn_, agAtt_, processor_.getAPVTS(), ParamIDs::slotAutogainId(slotIndex_),
                     "AG", KalyxTheme::getAccent(theme_), theme_ == kalyx::core::ChassisTheme::Day,
                     "Per-Slot Auto-Gain (+/-18 dB max, freezes below -60 dBFS, returns to 0 dB after 2s silence)");

    setupKalyxToggle(*this, extScBtn_, extScAtt_, processor_.getAPVTS(), ParamIDs::inExtSc,
                     "EXT SC", KalyxTheme::kalyxRoyalBlueGlow, theme_ == kalyx::core::ChassisTheme::Day,
                     "Use External Sidechain Bus (Inputs 3-4) for Dynamics detection");
    setupKalyxToggle(*this, scListenBtn_, scListenAtt_, processor_.getAPVTS(), ParamIDs::inScListen,
                     "SC LISTEN", KalyxTheme::kalyxAubergine, theme_ == kalyx::core::ChassisTheme::Day,
                     "Audition External Sidechain Bus (Inputs 3-4) on Main Output");

    syncModuleFromAPVTS();
}

void ModuleSlotView::showCategorizedModuleMenu()
{
    juce::PopupMenu root;
    const auto& all = kalyx::core::ModuleRegistry::getAll();
    const char* categories[4] = {
        "Dynamics",
        "Equalizers",
        "Color & Preamp",
        "Utility"
    };

    for (const char* cat : categories)
    {
        juce::PopupMenu sub;
        for (int i = 0; i < static_cast<int>(all.size()); ++i)
        {
            const auto& d = all[static_cast<std::size_t>(i)];
            if (juce::String(d.category) == cat)
                sub.addItem(i + 1, d.displayName, true, i == currentModuleId_);
        }
        root.addSubMenu(cat, sub);
    }

    root.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(moduleMenuBtn_),
                       [this](int result)
                       {
                           const int chosen = result - 1;
                           if (chosen >= 0 && chosen < static_cast<int>(kalyx::core::SlotModuleId::Count)
                               && chosen != currentModuleId_)
                           {
                               processor_.loadModuleIntoSlotWithDefaults(slotIndex_, static_cast<kalyx::core::SlotModuleId>(chosen));
                               rebuildControlsForModule(chosen);
                           }
                       });
}

void ModuleSlotView::setUiScale(float s)
{
    uiScale_ = std::clamp(s, 0.75f, 2.20f);
    agBtn_.getProperties().set("kalyxScale", uiScale_);
    extScBtn_.getProperties().set("kalyxScale", uiScale_);
    scListenBtn_.getProperties().set("kalyxScale", uiScale_);
    for (auto& r : rotaries_)       r->setUiScale(uiScale_);
    for (auto& sp : segmentedPills_) sp->setUiScale(uiScale_);
    for (auto& rs : rotarySteps_)   rs->setUiScale(uiScale_);
    if (topArrowSelector_)    topArrowSelector_->setUiScale(uiScale_);
    if (bottomArrowSelector_) bottomArrowSelector_->setUiScale(uiScale_);
    if (ua1176UI_)            ua1176UI_->setUiScale(uiScale_);
    if (la2aUI_)              la2aUI_->setUiScale(uiScale_);
    if (distressorUI_)        distressorUI_->setUiScale(uiScale_);
    if (fairchildUI_)         fairchildUI_->setUiScale(uiScale_);
    if (surgicalEq2uUI_)      surgicalEq2uUI_->setUiScale(uiScale_);
    if (precisionDyn2uUI_)    precisionDyn2uUI_->setUiScale(uiScale_);
    if (harmonicSculptorUI_)  harmonicSculptorUI_->setUiScale(uiScale_);
    if (stereoPhaseUI_)       stereoPhaseUI_->setUiScale(uiScale_);
    if (transferCurve_)       transferCurve_->setUiScale(uiScale_);
    resized();
    repaint();
}

void ModuleSlotView::setChassisTheme(kalyx::core::ChassisTheme t)
{
    if (theme_ == t)
        return;
    theme_ = t;

    const bool darkTxt = (theme_ == kalyx::core::ChassisTheme::Day);
    const auto capCol  = KalyxTheme::getControlIdleBg(theme_);
    moduleMenuBtn_.setColour(juce::TextButton::buttonColourId, capCol);
    moduleMenuBtn_.setColour(juce::TextButton::textColourOffId, KalyxTheme::getTextPrimary(theme_));
    agBtn_.setColour(juce::ToggleButton::tickColourId, KalyxTheme::getAccent(theme_));
    agBtn_.setColour(juce::ToggleButton::textColourId, KalyxTheme::getTextPrimary(theme_));
    agBtn_.getProperties().set("kalyxDarkText", darkTxt);
    extScBtn_.getProperties().set("kalyxDarkText", darkTxt);
    scListenBtn_.getProperties().set("kalyxDarkText", darkTxt);

    for (auto& r : rotaries_)
        r->setThemeColours(r->arcColour, capCol, darkTxt);
    for (auto& sp : segmentedPills_)
        sp->setThemeColours(KalyxTheme::getAccent(theme_), darkTxt);
    for (auto& rs : rotarySteps_)
        rs->setThemeColours(KalyxTheme::getAccent(theme_), darkTxt);
    if (topArrowSelector_)
        topArrowSelector_->setThemeColours(KalyxTheme::getAccent(theme_), darkTxt);
    if (bottomArrowSelector_)
        bottomArrowSelector_->setThemeColours(KalyxTheme::getAccent(theme_), darkTxt);
    for (auto& tog : toggles_)
        tog->getProperties().set("kalyxDarkText", darkTxt);
    if (eqCurve_)
    {
        eqCurve_->setDarkText(darkTxt);
        eqCurve_->repaint();
    }
    if (transferCurve_)
        transferCurve_->setThemeColours(KalyxTheme::getAccent(theme_), darkTxt);
    if (ua1176UI_)       ua1176UI_->setChassisTheme(theme_);
    if (la2aUI_)         la2aUI_->setChassisTheme(theme_);
    if (distressorUI_)   distressorUI_->setChassisTheme(theme_);
    if (fairchildUI_)    fairchildUI_->setChassisTheme(theme_);
    if (surgicalEq2uUI_)     surgicalEq2uUI_->setChassisTheme(theme_);
    if (precisionDyn2uUI_)   precisionDyn2uUI_->setChassisTheme(theme_);
    if (harmonicSculptorUI_) harmonicSculptorUI_->setChassisTheme(theme_);
    if (stereoPhaseUI_)      stereoPhaseUI_->setChassisTheme(theme_);

    if (static_cast<kalyx::core::SlotModuleId>(currentModuleId_) == kalyx::core::SlotModuleId::SSL4000EQ)
        applySslEqCapColours(isSslGActive_);

    repaint();
}

void ModuleSlotView::setVisualPosition(int visualPos, bool canMoveLeft, bool canMoveRight)
{
    visualPos_    = visualPos;
    canMoveLeft_  = canMoveLeft;
    canMoveRight_ = canMoveRight;
    repaint();
}

void ModuleSlotView::setReorderCallbacks(std::function<void()> onMoveLeft, std::function<void()> onMoveRight)
{
    onMoveLeft_  = std::move(onMoveLeft);
    onMoveRight_ = std::move(onMoveRight);
}

void ModuleSlotView::syncModuleFromAPVTS()
{
    const int modId = processor_.getSlotModuleId(slotIndex_);
    const int partner = kalyx::core::isModuleId2U(modId) ? processor_.getPartnerSlotFor2U(slotIndex_) : -1;
    if (modId != currentModuleId_ || partner != lastPartnerSlot_)
        rebuildControlsForModule(modId);
}

void ModuleSlotView::updateFFTAndRepaint()
{
    syncModuleFromAPVTS();
    updateDependentControlStates();

    // Télémétrie lock-free (30 Hz) pour les échelles de LEDs GR, Harmonic Drive et LED PEAK
    const float rawGr    = processor_.getSlotGrDb(slotIndex_);
    const float rawDrv   = processor_.getSlotDriveDb(slotIndex_);
    const float rawPkDb  = processor_.getSlotPeakDb(slotIndex_);

    const float prevGr   = smoothedGrDb_;
    const float prevDrv  = smoothedDriveDb_;
    const bool  prevPeak = peakLedLit_;

    smoothedGrDb_    = (rawGr > smoothedGrDb_)   ? rawGr  : (smoothedGrDb_ * 0.72f + rawGr * 0.28f);
    if (smoothedGrDb_ < 0.05f) smoothedGrDb_ = 0.0f;
    smoothedDriveDb_ = (rawDrv > smoothedDriveDb_) ? rawDrv : (smoothedDriveDb_ * 0.70f + rawDrv * 0.30f);
    peakLedLit_      = (rawPkDb >= -3.0f);

    if (!ledMeterRect_.isEmpty() && (std::abs(smoothedGrDb_ - prevGr) > 0.05f || std::abs(smoothedDriveDb_ - prevDrv) > 0.2f))
        repaint(ledMeterRect_.expanded(2.0f).toNearestInt());
    if (!peakLedRect_.isEmpty() && peakLedLit_ != prevPeak)
        repaint(peakLedRect_.expanded(3.0f).toNearestInt());

    const float pkDb = processor_.getSlotPeakDb(slotIndex_);

    if (ua1176UI_)
    {
        const int prevRev = ua1176UI_->getCurrentRevision();
        ua1176UI_->syncFromAPVTS();
        ua1176UI_->setGainReductionDb(smoothedGrDb_, pkDb);
        if (ua1176UI_->getCurrentRevision() != prevRev)
            repaint(0, 0, getWidth(), static_cast<int>(26.0f * uiScale_));
    }

    if (la2aUI_)
    {
        la2aUI_->syncFromAPVTS();
        la2aUI_->setGainReductionDb(smoothedGrDb_, pkDb);
    }

    if (distressorUI_)
    {
        distressorUI_->syncFromAPVTS();
        distressorUI_->setTelemetry(smoothedGrDb_, processor_.getSlotThdPct(slotIndex_), pkDb);
    }

    if (fairchildUI_)
    {
        fairchildUI_->syncFromAPVTS();
        fairchildUI_->setTelemetry(smoothedGrDb_, pkDb);
    }

    if (transferCurve_)
    {
        using Mod = kalyx::core::SlotModuleId;
        const auto mod = static_cast<Mod>(currentModuleId_);
        const auto p   = processor_.getSlotParamsForGUI(slotIndex_);
        if (mod == Mod::SSL4000Comp)
        {
            const float ratio = kalyx::core::mapLog(p[2], 1.0f, 20.0f);
            transferCurve_->setCurveConfig(InteractiveTransferCurve::CurveType::VCA, ratio, isSslGActive_ ? 4.0f : 2.0f, lastModuleOn_, false);
        }
        else if (mod == Mod::API2500Comp)
        {
            const float ratio = kalyx::core::mapLog(p[2], 1.5f, 20.0f);
            const int kneeIdx = kalyx::core::mapChoice(p[5], 3);
            const float knee  = (kneeIdx == 0) ? 1.0f : ((kneeIdx == 1) ? 6.0f : 12.0f);
            transferCurve_->setCurveConfig(InteractiveTransferCurve::CurveType::VCA, ratio, knee, lastModuleOn_, false);
        }
        else if (mod == Mod::Neve5254Comp)
        {
            static constexpr float kNeveR[5] = { 1.5f, 2.0f, 3.0f, 4.0f, 6.0f };
            const int rIdx = kalyx::core::mapChoice(p[2], 5);
            transferCurve_->setCurveConfig(InteractiveTransferCurve::CurveType::Diode, kNeveR[rIdx], 6.0f, lastModuleOn_, lastVintageMode_);
        }
        else if (mod == Mod::PrecisionLimiter)
        {
            transferCurve_->setCurveConfig(InteractiveTransferCurve::CurveType::Limiter, 100.0f, 0.5f, lastModuleOn_, false);
        }
        transferCurve_->setTelemetry(pkDb, smoothedGrDb_);
    }

    if (surgicalEq2uUI_)
    {
        surgicalEq2uUI_->updateFFTAndRepaint();
    }
    if (precisionDyn2uUI_)
        precisionDyn2uUI_->updateScopeAndRepaint();
    if (harmonicSculptorUI_)
        harmonicSculptorUI_->updateTelemetryAndRepaint();
    if (stereoPhaseUI_)
        stereoPhaseUI_->updateTelemetryAndRepaint();

    if (eqCurve_)
    {
        eqCurve_->updateFFTFromProcessor();
        eqCurve_->repaint();
    }
}

void ModuleSlotView::applySslEqCapColours(bool isG)
{
    if (rotaries_.size() < 11) return;
    const auto cLf  = isG ? KalyxTheme::eqLfWineRed     : KalyxTheme::eqLfWineRed;
    const auto cLmf = isG ? KalyxTheme::sslGPinkMidCap  : KalyxTheme::eqLmfSlateBlue;
    const auto cHmf = isG ? KalyxTheme::sslGRedMidCap   : KalyxTheme::eqHmfJadeGreen;
    const auto cHf  = isG ? KalyxTheme::eqHfPearlSilver : KalyxTheme::eqHfPearlSilver;
    const auto arc  = isG ? KalyxTheme::sslGPinkMidCap  : KalyxTheme::eqLfWineRed;
    const auto cap  = KalyxTheme::getControlIdleBg(theme_);

    rotaries_[0]->setCapAndArcColour(cLf,  cap);  // LF GAIN
    rotaries_[1]->setCapAndArcColour(cLf,  cap);  // LF FRQ
    rotaries_[2]->setCapAndArcColour(cLmf, cap);  // LMF GN
    rotaries_[3]->setCapAndArcColour(cLmf, cap);  // LMF FRQ
    rotaries_[4]->setCapAndArcColour(cLmf, cap);  // LMF Q
    rotaries_[5]->setCapAndArcColour(cHmf, cap);  // HMF GN
    rotaries_[6]->setCapAndArcColour(cHmf, cap);  // HMF FRQ
    rotaries_[7]->setCapAndArcColour(cHmf, cap);  // HMF Q
    rotaries_[8]->setCapAndArcColour(cHf,  cap);  // HF GAIN
    rotaries_[9]->setCapAndArcColour(cHf,  cap);  // HF FRQ
    rotaries_[10]->setCapAndArcColour(arc, cap);  // EQ TRIM
}

void ModuleSlotView::updateDependentControlStates()
{
    const auto p = processor_.getSlotParamsForGUI(slotIndex_);
    using Mod = kalyx::core::SlotModuleId;
    const auto mod = static_cast<Mod>(currentModuleId_);

    bool modOn = true;
    if (mod == Mod::EmptyBypass)
        modOn = false;
    else if (mod == Mod::UtilityGateDeEsser)
        modOn = (p[0] > 0.5f) || (p[5] > 0.5f);
    else
        modOn = (p[0] > 0.5f);

    // AUTO REL -> grisement & désactivation de RELEASE (35 % d'opacité + arc gris neutre)
    bool autoRelActive = false;
    if (mod == Mod::SSL4000Comp)
        autoRelActive = (p[5] > 0.5f);
    else if (mod == Mod::PrecisionLimiter)
        autoRelActive = (p[3] > 0.5f);

    if (releaseKnob_ != nullptr)
        releaseKnob_->setDimmedDisabled(autoRelActive);

    if (modOn != lastModuleOn_)
    {
        lastModuleOn_ = modOn;
        const float alpha = modOn ? 1.0f : 0.35f;
        for (auto& r : rotaries_)
        {
            if (r.get() == releaseKnob_ && autoRelActive) continue;
            r->slider.setAlpha(alpha);
            r->titleLabel.setAlpha(alpha);
        }
        for (auto& sp : segmentedPills_) sp->setAlpha(alpha);
        for (auto& rs : rotarySteps_)    rs->setAlpha(alpha);
        if (topArrowSelector_) topArrowSelector_->setAlpha(alpha);
        if (bottomArrowSelector_) bottomArrowSelector_->setAlpha(alpha);
        if (eqCurve_) eqCurve_->setAlpha(alpha);
    }

    // Commutateur SSL E / G dynamique
    if (mod == Mod::SSL4000Preamp || mod == Mod::SSL4000Comp || mod == Mod::SSL4000EQ)
    {
        const int egIdx = (mod == Mod::SSL4000Preamp) ? 8 : ((mod == Mod::SSL4000Comp) ? 10 : 14);
        const bool isG  = (p[static_cast<std::size_t>(egIdx)] > 0.5f);
        if (isG != isSslGActive_)
        {
            isSslGActive_ = isG;
            if (sslEGButton_ != nullptr)
            {
                sslEGButton_->setButtonText(isG ? "SSL G" : "SSL E");
                sslEGButton_->setColour(juce::ToggleButton::tickColourId,
                                        isG ? KalyxTheme::sslGPinkMidCap : KalyxTheme::eqLfWineRed);
            }
            if (mod == Mod::SSL4000EQ)
                applySslEqCapColours(isG);
            repaint();
        }
    }

    // API 2500 REL TYPE (FIXED / VAR)
    if (mod == Mod::API2500Comp && apiRelPill_ != nullptr)
    {
        const bool relFixed = (p[13] <= 0.5f);
        if (relFixed != lastApiRelFixed_)
        {
            lastApiRelFixed_ = relFixed;
            if (releaseKnob_ != nullptr)
                releaseKnob_->slider.updateText();
        }
    }

    // API 550B VINTAGE / MODERN
    if (mod == Mod::API550BEQ)
    {
        const bool stepped = (p[14] > 0.5f);
        if (stepped != lastApiStepped_)
        {
            lastApiStepped_ = stepped;
            for (auto& r : rotaries_)
                r->slider.updateText();
        }
    }

    // Universel VINTAGE / MOD (SSL4000EQ p[15], Neve1073EQ p[10], Neve5254Comp p[10])
    if (mod == Mod::SSL4000EQ || mod == Mod::Neve1073EQ || mod == Mod::Neve5254Comp)
    {
        const int vintIdx = (mod == Mod::SSL4000EQ) ? 15 : 10;
        const bool vint = (p[static_cast<std::size_t>(vintIdx)] > 0.5f);
        if (vint != lastVintageMode_)
        {
            lastVintageMode_ = vint;
            if (mod == Mod::Neve5254Comp)
            {
                if (threshKnob_ != nullptr) threshKnob_->setSteppedMode(vint, 21);
                if (gainKnob_ != nullptr)   gainKnob_->setSteppedMode(vint, 19);
            }
            for (auto& r : rotaries_)
                r->slider.updateText();
        }
    }
}

void ModuleSlotView::rebuildControlsForModule(int modId)
{
    currentModuleId_ = modId;
    releaseKnob_     = nullptr;
    threshKnob_      = nullptr;
    gainKnob_        = nullptr;
    sslEGButton_     = nullptr;
    apiRelPill_      = nullptr;
    neveLinkBtn_     = nullptr;
    lastModuleOn_    = true;
    smoothedGrDb_    = 0.0f;
    smoothedDriveDb_ = -100.0f;
    peakLedLit_      = false;
    ledMeterRect_    = {};
    peakLedRect_     = {};

    // Nettoyage propre des anciens contrôles de la façade
    for (auto& r : rotaries_)        { removeChildComponent(&r->slider); removeChildComponent(&r->titleLabel); }
    for (auto& sp : segmentedPills_) { removeChildComponent(sp.get()); }
    for (auto& rs : rotarySteps_)    { removeChildComponent(rs.get()); }
    if (topArrowSelector_)           { removeChildComponent(topArrowSelector_.get()); }
    if (bottomArrowSelector_)        { removeChildComponent(bottomArrowSelector_.get()); }
    for (auto& t : toggles_)         { removeChildComponent(t.get()); }
    if (eqCurve_)                    { removeChildComponent(eqCurve_.get()); }
    if (ua1176UI_)                   { removeChildComponent(ua1176UI_.get()); }
    if (la2aUI_)                     { removeChildComponent(la2aUI_.get()); }
    if (distressorUI_)               { removeChildComponent(distressorUI_.get()); }
    if (fairchildUI_)                { removeChildComponent(fairchildUI_.get()); }
    if (surgicalEq2uUI_)             { removeChildComponent(surgicalEq2uUI_.get()); }
    if (precisionDyn2uUI_)           { removeChildComponent(precisionDyn2uUI_.get()); }
    if (harmonicSculptorUI_)         { removeChildComponent(harmonicSculptorUI_.get()); }
    if (stereoPhaseUI_)              { removeChildComponent(stereoPhaseUI_.get()); }
    if (transferCurve_)              { removeChildComponent(transferCurve_.get()); }

    lastPartnerSlot_ = kalyx::core::isModuleId2U(modId) ? processor_.getPartnerSlotFor2U(slotIndex_) : -1;

    rotaries_.clear();
    segmentedPills_.clear();
    rotarySteps_.clear();
    topArrowSelector_.reset();
    bottomArrowSelector_.reset();
    toggles_.clear();
    toggleAtts_.clear();
    eqCurve_.reset();
    transferCurve_.reset();
    ua1176UI_.reset();
    la2aUI_.reset();
    distressorUI_.reset();
    fairchildUI_.reset();
    surgicalEq2uUI_.reset();
    precisionDyn2uUI_.reset();
    harmonicSculptorUI_.reset();
    stereoPhaseUI_.reset();

    auto& apvts = processor_.getAPVTS();
    const auto& desc = kalyx::core::ModuleRegistry::getById(modId);
    const auto defs  = kalyx::core::ModuleRegistry::getDefaultParamsForModule(static_cast<kalyx::core::SlotModuleId>(modId));
    const bool darkTxt = (theme_ == kalyx::core::ChassisTheme::Day);

    moduleMenuBtn_.setButtonText(desc.shortName);
    moduleMenuBtn_.setColour(juce::TextButton::buttonColourId, KalyxTheme::getControlIdleBg(theme_));
    moduleMenuBtn_.setColour(juce::TextButton::textColourOffId, KalyxTheme::getTextPrimary(theme_));

    auto addKnob = [&](int pOneBased, const juce::String& title,
                       juce::Colour arcCol, juce::Colour capCol,
                       std::function<juce::String(double)> fmt,
                       const juce::String& hint = {},
                       int notchedDots = 0,
                       bool marconi = false,
                       std::function<double(const juce::String&)> parser = nullptr) -> KalyxRotaryControl*
    {
        auto rc = std::make_unique<KalyxRotaryControl>();
        rc->init(*this, apvts, ParamIDs::slotParamId(slotIndex_, pOneBased),
                 title, arcCol, capCol, darkTxt,
                 defs[static_cast<std::size_t>(pOneBased - 1)], std::move(fmt), hint, std::move(parser));
        if (notchedDots > 0)
            rc->slider.getProperties().set("kalyxNotchedDots", notchedDots);
        if (marconi)
            rc->slider.getProperties().set("kalyxMarconiKnob", true);
        auto* raw = rc.get();
        rotaries_.push_back(std::move(rc));
        return raw;
    };

    auto addTog = [&](int pOneBased, const juce::String& text, juce::Colour activeCol,
                      const juce::String& hint = {}) -> juce::ToggleButton*
    {
        auto btn = std::make_unique<juce::ToggleButton>();
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
        setupKalyxToggle(*this, *btn, att, apvts, ParamIDs::slotParamId(slotIndex_, pOneBased), text, activeCol, darkTxt, hint);
        auto* raw = btn.get();
        toggles_.push_back(std::move(btn));
        toggleAtts_.push_back(std::move(att));
        return raw;
    };

    using Mod = kalyx::core::SlotModuleId;
    const juce::StringArray routingLabels { "MONO", "STEREO", "DUAL M", "M/S", "INV" };

    switch (static_cast<Mod>(modId))
    {
        case Mod::SSL4000Preamp:
        case Mod::API2520Preamp:
        case Mod::Neve1073Preamp:
        case Mod::KalyxCleanPreamp:
        {
            const bool isSsl  = (modId == static_cast<int>(Mod::SSL4000Preamp));
            const bool isApi  = (modId == static_cast<int>(Mod::API2520Preamp));
            const bool isNeve = (modId == static_cast<int>(Mod::Neve1073Preamp));

            const auto accent  = isSsl ? KalyxTheme::eqLfWineRed
                               : isApi ? KalyxTheme::apiCobaltBlue
                               : isNeve ? KalyxTheme::neveBrickRed
                               : KalyxTheme::kalyxRoseParme;
            const auto gainCap = isSsl ? KalyxTheme::sslEBrownCap
                               : isApi ? KalyxTheme::apiCobaltBlue
                               : isNeve ? KalyxTheme::neveBrickRed
                               : KalyxTheme::kalyxControlBg;
            const auto auxCap  = isSsl ? KalyxTheme::sslEBlackCap
                               : isApi ? KalyxTheme::apiCobaltBlue
                               : isNeve ? juce::Colour(0xff52677a)
                               : KalyxTheme::kalyxControlBg;
            const auto arc     = isApi ? juce::Colour(0xff60a5fa)
                               : isNeve ? KalyxTheme::eqHfPearlSilver
                               : accent;

            addTog(1, "PRE IN", accent, "Engage or bypass this 500-series preamp module");
            addTog(4, "PHASE",  KalyxTheme::alertRed, "Invert audio polarity by 180 degrees");
            addTog(5, "HPF",    accent, "Engage 18 dB/oct ZDF High-Pass Filter");
            addTog(7, "LPF",    accent, "Engage 12 dB/oct ZDF Low-Pass Filter");
            if (isSsl)
            {
                const auto p = processor_.getSlotParamsForGUI(slotIndex_);
                isSslGActive_ = (p[8] > 0.5f);
                sslEGButton_ = addTog(9, isSslGActive_ ? "SSL G" : "SSL E",
                                      isSslGActive_ ? KalyxTheme::eqLmfSlateBlue : KalyxTheme::eqLfWineRed,
                                      "Switch SSL 4000 Preamp topology between E-Series and G-Series");
            }

            addKnob(2, "GAIN",  arc, gainCap, fmtDb(-24.f, 24.f),        "Preamp Input Gain (-24 to +24 dB)", isNeve ? 11 : 0, isNeve, parseDb(-24.f, 24.f));
            addKnob(3, "DRIVE", arc, auxCap,  fmtPct(),                  "Analog Transformer & Op-Amp ADAA Saturation Drive", 13, false, parsePct());
            addKnob(6, "HPF",   arc, auxCap,  fmtHzLog(20.f, 500.f),     "Preamp High-Pass Filter Cutoff (20 Hz - 500 Hz)", 0, false, parseHzLog(20.f, 500.f));
            addKnob(8, "LPF",   arc, auxCap,  fmtHzLog(2000.f, 20000.f), "Preamp Low-Pass Filter Cutoff (2 kHz - 20 kHz)", 0, false, parseHzLog(2000.f, 20000.f));
            break;
        }

        case Mod::UtilityGateDeEsser: // Kalyx Heritage (#143832 / #8A4F6E)
        {
            const auto plum = KalyxTheme::kalyxRoseParme;
            const auto cap  = KalyxTheme::kalyxControlBg;
            addTog(1, "GATE IN", plum, "Engage Noise Gate / Expander");
            addTog(6, "DE-ESS",  plum, "Engage Split-Band Sibilance De-Esser");
            addKnob(2, "GATE THR", plum, cap, fmtDb(-70.f, 0.f),       "Gate Threshold (-70 to 0 dB)", 0, false, parseDb(-70.f, 0.f));
            addKnob(3, "RANGE",    plum, cap, fmtDb(0.f, 40.f),        "Maximum Gate Attenuation Range (0 to 40 dB)", 0, false, parseDb(0.f, 40.f));
            addKnob(4, "HOLD",     plum, cap, [](double v){ return juce::String(juce::roundToInt(kalyx::core::mapLin(static_cast<float>(v), 5.f, 300.f))) + " ms"; }, "Gate Hold Time", 0, false, KalyxValueParsers::parseLinPct(5.f, 300.f));
            addKnob(5, "RELEASE",  plum, cap, [](double v){ return juce::String(juce::roundToInt(kalyx::core::mapLin(static_cast<float>(v), 20.f, 800.f))) + " ms"; }, "Gate Release Time", 0, false, KalyxValueParsers::parseLinPct(20.f, 800.f));
            addKnob(7, "DS FREQ",  plum, cap, fmtHzLog(3000.f, 12000.f), "De-Esser Sibilance Center Frequency (3 - 12 kHz)", 0, false, parseHzLog(3000.f, 12000.f));
            addKnob(8, "DS THR",   plum, cap, fmtDb(-40.f, 0.f),       "De-Esser Threshold (-40 to 0 dB)", 0, false, parseDb(-40.f, 0.f));
            addKnob(9, "DS RNG",   plum, cap, fmtDb(0.f, 18.f),        "De-Esser Maximum Reduction Range (0 to 18 dB)", 0, false, parseDb(0.f, 18.f));
            break;
        }

        case Mod::SSL4000Comp: // SSL matte grey + 6-LED GR + < ROUTING >
        {
            const auto p = processor_.getSlotParamsForGUI(slotIndex_);
            isSslGActive_ = (p[10] > 0.5f);
            const auto accent = isSslGActive_ ? KalyxTheme::eqLmfSlateBlue : KalyxTheme::eqLfWineRed;
            const auto cap    = isSslGActive_ ? juce::Colour(0xff2c333d) : KalyxTheme::sslEBrownCap;

            addTog(1, "COMP IN", accent, "Engage SSL 4000 VCA Compressor");
            addTog(6, "AUTO RC", KalyxTheme::eqLfWineRed, "Dual-stage analog RC Auto-Release (disables manual RELEASE)");
            sslEGButton_ = addTog(11, isSslGActive_ ? "SSL G" : "SSL E", accent,
                                  "Switch SSL VCA circuit between 4000 E (feedback RMS) and 4000 G (feedforward Bus)");

            addKnob(2,  "THRESH",  accent, cap, fmtDb(-40.f, 0.f),     "VCA Compression Threshold (-40 to 0 dB)", 0, false, parseDb(-40.f, 0.f));
            transferCurve_ = std::make_unique<InteractiveTransferCurve>();
            transferCurve_->init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 2), -40.0f, 0.0f, defs[1], false, accent, darkTxt,
                                 "Dynamic VCA Transfer Curve (Drag to adjust Threshold)");
            addKnob(3,  "RATIO",   accent, cap, fmtRatioLog(1.f, 20.f),"Compression Ratio (1.0:1 to 20.0:1)", 7, false, parseRatioLog(1.f, 20.f));
            addKnob(4,  "ATTACK",  accent, cap, fmtMsLog(0.5f, 50.f),  "VCA Attack Time (0.5 to 50 ms)", 0, false, parseMsLog(0.5f, 50.f));
            releaseKnob_ = addKnob(5, "RELEASE", accent, cap, fmtMsLog(20.f, 1000.f), "Manual Release Time (disabled when AUTO RC is active)", 0, false, parseMsLog(20.f, 1000.f));
            addKnob(7,  "MAKEUP",  accent, cap, fmtDb(-12.f, 24.f),    "Output Makeup Gain (-12 to +24 dB)", 0, false, parseDb(-12.f, 24.f));
            addKnob(8,  "BLEND",   accent, cap, fmtPct(),              "Parallel Dry/Wet Compression Blend (0 - 100 %)", 0, false, parsePct());
            addKnob(10, "SC HPF",  accent, cap, fmtHzLog(20.f, 300.f), "Internal Sidechain High-Pass Filter (20 - 300 Hz)", 0, false, parseHzLog(20.f, 300.f));

            bottomArrowSelector_ = std::make_unique<KalyxArrowSelector>(
                apvts, ParamIDs::slotParamId(slotIndex_, 9), "ROUTING :", routingLabels, accent, darkTxt,
                "Channel Routing Mode: MONO, STEREO, DUAL MONO, M/S, INVERSE");
            addAndMakeVisible(*bottomArrowSelector_);
            break;
        }

        case Mod::API2500Comp: // API 2500 : 3 pilules segmentées nettes + [FIXED|VAR] + RATIO cranté Bleu Cobalt #1E64B4
        {
            const auto cobalt = KalyxTheme::apiCobaltBlue; // #1E64B4
            const auto cyan   = KalyxTheme::apiCyanLight;  // #4DA3FF
            const auto cap    = KalyxTheme::getControlIdleBg(theme_);
            const auto p      = processor_.getSlotParamsForGUI(slotIndex_);
            lastApiRelFixed_  = (p[13] <= 0.5f);

            addTog(1, "2500 IN", cobalt, "Engage API 2500 Stereo Bus Compressor");
            addTog(8, "OLD FB",  KalyxTheme::amberLed, "OLD (Feedback vintage) vs NEW (Feedforward modern) detector topology");

            addKnob(2,  "THRESH",   cyan,   cap, fmtDb(-40.f, 0.f),      "API 2500 Threshold (-40 to 0 dB)", 0, false, parseDb(-40.f, 0.f));
            transferCurve_ = std::make_unique<InteractiveTransferCurve>();
            transferCurve_->init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 2), -40.0f, 0.0f, defs[1], false, cobalt, darkTxt,
                                 "Dynamic VCA Transfer Curve (Drag to adjust Threshold)");
            addKnob(3,  "RATIO",    cobalt, cap, fmtRatioLog(1.5f, 20.f),"Stepped API 2500 Compression Ratio (1.5:1 to 20:1)", 7, false, parseRatioLog(1.5f, 20.f));
            addKnob(4,  "ATTACK",   cyan,   cap, fmtMsLog(0.3f, 30.f),   "Attack Time (0.3 to 30 ms)", 0, false, parseMsLog(0.3f, 30.f));

            auto fmtApiRel = [this](double v) -> juce::String
            {
                const bool fixed = processor_.getSlotParamsForGUI(slotIndex_)[13] <= 0.5f;
                const float m = kalyx::core::SlotUnitDSP::quantizeApi2500RelMs(kalyx::core::mapLog(static_cast<float>(v), 50.f, 1500.f), fixed);
                return juce::String(juce::roundToInt(m)) + " ms";
            };
            releaseKnob_ = addKnob(5, "RELEASE", cyan, cap, fmtApiRel, "Release Time (FIXED stepped 50ms-2s or VAR continuous)", 0, false, parseMsLog(50.f, 1500.f));

            // Sélecteur segmenté [ FIXED | VAR ] sous RELEASE
            auto relPill = std::make_unique<KalyxSegmentedPill>(
                apvts, ParamIDs::slotParamId(slotIndex_, 14), "",
                juce::StringArray { "FIXED", "VAR" }, cobalt, darkTxt,
                "Toggle API 2500 Release Type between FIXED (stepped) and VAR (variable)");
            addAndMakeVisible(*relPill);
            apiRelPill_ = relPill.get();
            segmentedPills_.push_back(std::move(relPill));

            addKnob(9,  "L/R LINK", cyan, cap, fmtPct(50.f, 100.f),    "Stereo L/R Detector Link Percentage (50 - 100 %)", 0, false, parsePct(50.f, 100.f));
            addKnob(11, "MAKEUP",   cyan, cap, fmtDb(-12.f, 24.f),     "Makeup Gain (-12 to +24 dB)", 0, false, parseDb(-12.f, 24.f));
            addKnob(12, "BLEND",    cyan, cap, fmtPct(),               "Parallel Dry/Wet Blend (0 - 100 %)", 0, false, parsePct());

            // 3 pilules segmentées nettes : KNEE, THRUST, LINK FLT
            auto addPill = [&](int pOneBased, const juce::String& title, const juce::StringArray& labels,
                               juce::Colour activeCol, const juce::String& hint)
            {
                auto sp = std::make_unique<KalyxSegmentedPill>(apvts, ParamIDs::slotParamId(slotIndex_, pOneBased),
                                                               title, labels, activeCol, darkTxt, hint);
                addAndMakeVisible(*sp);
                segmentedPills_.push_back(std::move(sp));
            };
            addPill(6,  "KNEE",     { "HARD", "MED", "SOFT" }, cobalt, "API 2500 Knee Curve: HARD, MED, or SOFT");
            addPill(7,  "THRUST",   { "LOUD", "MED", "NORM" }, KalyxTheme::amberLed, "Patented API THRUST Sidechain Filter: LOUD, MED, NORM");
            addPill(10, "LINK FLT", { "HP",   "BP",  "LP"   }, cyan,   "L/R Link Sidechain Filter: HP, BP, LP");

            bottomArrowSelector_ = std::make_unique<KalyxArrowSelector>(
                apvts, ParamIDs::slotParamId(slotIndex_, 13), "ROUTING :", routingLabels, cobalt, darkTxt,
                "Channel Routing Mode: MONO, STEREO, DUAL MONO, M/S, INV");
            addAndMakeVisible(*bottomArrowSelector_);
            break;
        }

        case Mod::Neve5254Comp: // Neve 5254 : Rouge Brique #A62C3B & Bleu-Gris #467294
        {
            const auto brick = KalyxTheme::neveBrickRed;    // #A62C3B
            const auto steel = KalyxTheme::neveRafGreyBlue; // #467294
            const auto cap   = KalyxTheme::getControlIdleBg(theme_);
            lastVintageMode_ = (processor_.getSlotParamsForGUI(slotIndex_)[10] > 0.5f);
            addTog(1, "5254 IN", brick, "Engage Neve 5254 Diode-Bridge Compressor");
            addTog(5, "FAST",    KalyxTheme::amberLed, "Fast Attack Mode (+30% faster transient grab)");

            // Sélecteurs rotatifs crantés (RATIO & TIMING)
            auto addRotStep = [&](int pOneBased, const juce::String& title, const juce::StringArray& labels,
                                  juce::Colour capCol, const juce::String& hint)
            {
                auto rs = std::make_unique<KalyxRotaryStepSelector>(apvts, ParamIDs::slotParamId(slotIndex_, pOneBased),
                                                                    title, labels, capCol, darkTxt, hint);
                addAndMakeVisible(*rs);
                rotarySteps_.push_back(std::move(rs));
            };
            addRotStep(3, "RATIO",  { "1.5:1", "2:1", "3:1", "4:1", "6:1", "8:1" }, brick, "Neve 5254 Compression Ratio (Stepped in VINTAGE, Continuous in MOD)");
            addRotStep(4, "TIMING", { "FAST", "MF", "MED", "MS", "SLOW", "AUTO" },  steel, "Neve 5254 Timing Preset (Stepped in VINTAGE, Continuous in MOD)");

            auto fmtNeve5254Db = [this](float minDb, float maxDb) -> std::function<juce::String(double)>
            {
                return [this, minDb, maxDb](double v) -> juce::String
                {
                    float db = kalyx::core::mapLin(static_cast<float>(v), minDb, maxDb);
                    if (processor_.getSlotParamsForGUI(slotIndex_)[10] > 0.5f)
                        db = std::round(db * 0.5f) * 2.0f;
                    return KalyxValueParsers::formatDbClean(db);
                };
            };

            threshKnob_ = addKnob(2, "THRESH", brick, cap, fmtNeve5254Db(-40.f, 0.f),  "Diode-Bridge Threshold (-40 to 0 dB, 2 dB steps in VINTAGE)", 21, false, parseDb(-40.f, 0.f));
            threshKnob_->setSteppedMode(lastVintageMode_, 21);
            transferCurve_ = std::make_unique<InteractiveTransferCurve>();
            transferCurve_->init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 2), -40.0f, 0.0f, defs[1], false, brick, darkTxt,
                                 "Dynamic Diode-Bridge Transfer Curve (Drag to adjust Threshold)");
            addKnob(6, "SC HPF", steel, cap, fmtHzLog(20.f, 250.f), "Sidechain High-Pass Filter (20 - 250 Hz)", 0, false, parseHzLog(20.f, 250.f));
            gainKnob_   = addKnob(7, "GAIN",   brick, cap, fmtNeve5254Db(-12.f, 24.f), "Class-A Output Makeup Gain (-12 to +24 dB, 2 dB steps in VINTAGE)", 19, false, parseDb(-12.f, 24.f));
            gainKnob_->setSteppedMode(lastVintageMode_, 19);
            addKnob(8, "BLEND",  steel, cap, fmtPct(),              "Parallel Dry/Wet Blend (0 - 100 %)", 0, false, parsePct());

            neveLinkBtn_ = addTog(10, "LINK", brick, "Stereo Link L/R diode-bridge sidechain detectors");

            auto vintPill = std::make_unique<KalyxSegmentedPill>(
                apvts, ParamIDs::slotParamId(slotIndex_, 11), "",
                juce::StringArray { "MOD", "VINTAGE" }, brick, darkTxt,
                "VINTAGE: Stepped Ratio/Timing detents / MOD: Continuous Ratio/Timing");
            addAndMakeVisible(*vintPill);
            segmentedPills_.push_back(std::move(vintPill));

            bottomArrowSelector_ = std::make_unique<KalyxArrowSelector>(
                apvts, ParamIDs::slotParamId(slotIndex_, 9), "ROUTING :", routingLabels, steel, darkTxt,
                "Channel Routing Mode: MONO, STEREO, DUAL MONO, M/S, INV");
            addAndMakeVisible(*bottomArrowSelector_);
            break;
        }

        case Mod::SSL4000EQ:
        case Mod::API550BEQ:
        {
            const bool isApi = (modId == static_cast<int>(Mod::API550BEQ));
            const auto p = processor_.getSlotParamsForGUI(slotIndex_);
            if (!isApi)
            {
                isSslGActive_    = (p[14] > 0.5f);
                lastVintageMode_ = (p[15] > 0.5f);
            }
            else
            {
                lastApiStepped_ = (p[14] > 0.5f);
            }

            eqCurve_ = std::make_unique<KalyxEQCurveComponent>(processor_, slotIndex_, darkTxt);
            eqCurve_->setTooltip(isApi
                ? "API 550B 3-Band Proportional-Q EQ + HPF/LPF Curve: Drag nodes | Double-Click to reset"
                : "Combined 4-Band SSL EQ + INPUT HPF/LPF Curve: Drag nodes | Double-Click to reset");
            addAndMakeVisible(*eqCurve_);

            const auto sslAcc = isSslGActive_ ? KalyxTheme::eqLmfSlateBlue : KalyxTheme::eqLfWineRed;
            const auto colBtn = isApi ? KalyxTheme::apiCobaltBlue : sslAcc;
            addTog(1,  "EQ IN",   colBtn, isApi ? "Engage or bypass API 550B 3-Band EQ" : "Engage or bypass SSL 4-Band EQ");
            addTog(13, "LF BELL", isApi ? KalyxTheme::apiCobaltBlue : KalyxTheme::eqLfWineRed,   "Switch Low Band between Shelf and Bell");
            addTog(14, "HF BELL", isApi ? KalyxTheme::apiCobaltBlue : KalyxTheme::eqLmfSlateBlue,"Switch High Band between Shelf and Bell");
            if (!isApi)
            {
                sslEGButton_ = addTog(15, isSslGActive_ ? "SSL G" : "SSL E", sslAcc,
                                      "Switch SSL EQ between 4000 E (Brown/Black Knob) and 4000 G (Pink/Red Mids)");
            }

            // Pilule lisible [ MOD | VINTAGE ] (API 550B p[14], SSL 4000 EQ p[15])
            auto vintPill = std::make_unique<KalyxSegmentedPill>(
                apvts, ParamIDs::slotParamId(slotIndex_, isApi ? 15 : 16), "",
                juce::StringArray { "MOD", "VINTAGE" }, colBtn, darkTxt,
                isApi ? "VINTAGE: Classic API 550B stepped +/-2 dB & fixed freqs / MOD: Continuous"
                      : "VINTAGE: Stepped SSL hardware freq detents / MOD: Continuous sweep");
            addAndMakeVisible(*vintPill);
            segmentedPills_.push_back(std::move(vintPill));

            const auto cLf  = isApi ? KalyxTheme::apiCobaltBlue : (isSslGActive_ ? KalyxTheme::eqLfWineRed     : KalyxTheme::sslEBrownCap);
            const auto cLmf = isApi ? KalyxTheme::apiCobaltBlue : (isSslGActive_ ? KalyxTheme::sslGPinkMidCap  : KalyxTheme::sslEBlackCap);
            const auto cHmf = isApi ? KalyxTheme::apiCobaltBlue : (isSslGActive_ ? KalyxTheme::sslGRedMidCap   : KalyxTheme::sslEBrownCap);
            const auto cHf  = isApi ? KalyxTheme::apiCobaltBlue : (isSslGActive_ ? KalyxTheme::eqHfPearlSilver : KalyxTheme::sslEBlackCap);
            const auto arc  = isApi ? KalyxTheme::apiCyanLight  : sslAcc;

            auto fmtEqGain = [this, isApi](double v) -> juce::String
            {
                float db = kalyx::core::mapLin(static_cast<float>(v), -15.f, 15.f);
                if (isApi)
                {
                    const bool st = processor_.getSlotParamsForGUI(slotIndex_)[14] > 0.5f;
                    db = kalyx::core::SlotUnitDSP::quantizeApi550BGain(db, st);
                }
                return KalyxValueParsers::formatDbClean(db);
            };

            auto fmtEqFreq = [this, isApi](int bandIdx, float minHz, float maxHz) -> std::function<juce::String(double)>
            {
                return [this, isApi, bandIdx, minHz, maxHz](double v) -> juce::String
                {
                    float f = kalyx::core::mapLog(static_cast<float>(v), minHz, maxHz);
                    const auto curP = processor_.getSlotParamsForGUI(slotIndex_);
                    if (isApi)
                        f = kalyx::core::SlotUnitDSP::quantizeApi550BFreq(bandIdx, f, curP[14] > 0.5f);
                    else if (curP[15] > 0.5f)
                        f = kalyx::core::SlotUnitDSP::quantizeApi550BFreq(bandIdx, f, true);
                    if (f >= 1000.0f) return juce::String(f * 0.001f, 1) + " kHz";
                    return juce::String(juce::roundToInt(f)) + " Hz";
                };
            };

            auto fmtQ = [](double v) { return "Q " + juce::String(kalyx::core::mapLog(static_cast<float>(v), 0.4f, 4.f), 2); };
            auto parseQ = KalyxValueParsers::parseQLog(0.4f, 4.f);
            auto parseEqG = parseDb(-15.f, 15.f);

            if (isApi)
            {
                // API 550B authentique : 3 bandes Proportional-Q (LF, MF, HF) + EQ TRIM
                addKnob(3,  "LF GAIN", cLf,  cLf,  fmtEqGain,                    "Low Band Gain (-15 to +15 dB)", 0, false, parseEqG);
                addKnob(2,  "LF FRQ",  cLf,  cLf,  fmtEqFreq(0, 30.f, 400.f),    "Low Band Frequency (30 / 40 / 50 / 100 / 200 / 300 / 400 Hz)", 0, false, parseHzLog(30.f, 400.f));
                addKnob(5,  "MF GAIN", cLmf, cLmf, fmtEqGain,                    "Mid Band Proportional-Q Gain (-15 to +15 dB)", 0, false, parseEqG);
                addKnob(4,  "MF FRQ",  cLmf, cLmf, fmtEqFreq(1, 200.f, 5000.f),  "Mid Band Frequency (200 Hz - 5 kHz)", 0, false, parseHzLog(200.f, 5000.f));
                addKnob(6,  "MF Q",    cLmf, cLmf, fmtQ,                         "Mid Band Bandwidth (Proportional-Q)", 0, false, parseQ);
                addKnob(11, "HF GAIN", cHf,  cHf,  fmtEqGain,                    "High Band Gain (-15 to +15 dB)", 0, false, parseEqG);
                addKnob(10, "HF FRQ",  cHf,  cHf,  fmtEqFreq(3, 2500.f, 20000.f),"High Band Frequency (2.5 - 20 kHz)", 0, false, parseHzLog(2500.f, 20000.f));
                addKnob(12, "EQ TRIM", arc,  cLmf, fmtDb(-12.f, 12.f),           "EQ Stage Output Trim (-12 to +12 dB)", 0, false, parseDb(-12.f, 12.f));
            }
            else
            {
                addKnob(3,  "LF GAIN",  cLf,  cLf,  fmtEqGain,                     "Low Band Gain (-15 to +15 dB)", 0, false, parseEqG);
                addKnob(2,  "LF FRQ",   cLf,  cLf,  fmtEqFreq(0, 30.f, 450.f),     "Low Band Frequency (30 - 450 Hz)", 0, false, parseHzLog(30.f, 450.f));
                addKnob(5,  "LMF GN",   cLmf, cLmf, fmtEqGain,                     "Low-Mid Band Gain (-15 to +15 dB)", 0, false, parseEqG);
                addKnob(4,  "LMF FRQ",  cLmf, cLmf, fmtEqFreq(1, 200.f, 2500.f),   "Low-Mid Band Frequency (200 Hz - 2.5 kHz)", 0, false, parseHzLog(200.f, 2500.f));
                addKnob(6,  "LMF Q",    cLmf, cLmf, fmtQ,                          "Low-Mid Bandwidth (Q)", 0, false, parseQ);
                addKnob(8,  "HMF GN",   cHmf, cHmf, fmtEqGain,                     "High-Mid Band Gain (-15 to +15 dB)", 0, false, parseEqG);
                addKnob(7,  "HMF FRQ",  cHmf, cHmf, fmtEqFreq(2, 600.f, 7000.f),   "High-Mid Band Frequency (600 Hz - 7 kHz)", 0, false, parseHzLog(600.f, 7000.f));
                addKnob(9,  "HMF Q",    cHmf, cHmf, fmtQ,                          "High-Mid Bandwidth (Q)", 0, false, parseQ);
                addKnob(11, "HF GAIN",  cHf,  cHf,  fmtEqGain,                     "High Band Gain (-15 to +15 dB)", 0, false, parseEqG);
                addKnob(10, "HF FRQ",   cHf,  cHf,  fmtEqFreq(3, 1500.f, 18000.f), "High Band Frequency (1.5 - 18 kHz)", 0, false, parseHzLog(1500.f, 18000.f));
                addKnob(12, "EQ TRIM",  arc,  cLmf, fmtDb(-12.f, 12.f),            "EQ Stage Output Trim (-12 to +12 dB)", 0, false, parseDb(-12.f, 12.f));
            }
            break;
        }

        case Mod::Neve1073EQ: // Neve 1073 3-Band Inductive EQ + Ind HPF
        {
            const auto brick = KalyxTheme::neveBrickRed;
            const auto steel = juce::Colour(0xff52677a);
            const auto arc   = KalyxTheme::eqHfPearlSilver;
            lastVintageMode_ = (processor_.getSlotParamsForGUI(slotIndex_)[10] > 0.5f);

            eqCurve_ = std::make_unique<KalyxEQCurveComponent>(processor_, slotIndex_, darkTxt);
            eqCurve_->setTooltip("Interactive Neve 1073 EQ + HPF/LPF Curve: Drag band nodes | Double-Click to flatten");
            addAndMakeVisible(*eqCurve_);

            addTog(1, "1073 IN", brick, "Engage Neve 1073 3-Band Inductive EQ");
            addTog(8, "IND HPF", steel, "Engage 18 dB/oct Inductive High-Pass Filter");

            auto vintPill = std::make_unique<KalyxSegmentedPill>(
                apvts, ParamIDs::slotParamId(slotIndex_, 11), "",
                juce::StringArray { "MOD", "VINTAGE" }, brick, darkTxt,
                "VINTAGE: Historical Neve 1073 stepped freqs (35/60/110/220 Hz, 360-7.2k) / MOD: Continuous");
            addAndMakeVisible(*vintPill);
            segmentedPills_.push_back(std::move(vintPill));

            auto fmtNeveLow = [this](double v) -> juce::String
            {
                const float raw = kalyx::core::mapLog(static_cast<float>(v), 35.f, 220.f);
                const bool vint = (processor_.getSlotParamsForGUI(slotIndex_)[10] > 0.5f);
                const float f   = kalyx::core::SlotUnitDSP::quantizeNeve1073LowFreq(raw, vint);
                return juce::String(juce::roundToInt(f)) + " Hz";
            };
            auto fmtNeveMid = [this](double v) -> juce::String
            {
                const float raw = kalyx::core::mapLog(static_cast<float>(v), 360.f, 7200.f);
                const bool vint = (processor_.getSlotParamsForGUI(slotIndex_)[10] > 0.5f);
                const float f   = kalyx::core::SlotUnitDSP::quantizeNeve1073MidFreq(raw, vint);
                if (f >= 1000.0f) return juce::String(f * 0.001f, 1) + " kHz";
                return juce::String(juce::roundToInt(f)) + " Hz";
            };
            auto fmtNeveHpf = [this](double v) -> juce::String
            {
                const float raw = kalyx::core::mapLog(static_cast<float>(v), 35.f, 300.f);
                const bool vint = (processor_.getSlotParamsForGUI(slotIndex_)[10] > 0.5f);
                const float f   = kalyx::core::SlotUnitDSP::quantizeNeve1073HpfFreq(raw, vint);
                return juce::String(juce::roundToInt(f)) + " Hz";
            };

            addKnob(3,  "LOW GN",   arc, brick, fmtDb(-15.f, 15.f), "Low Shelf Gain (-15 to +15 dB)", 0, false, parseDb(-15.f, 15.f));
            addKnob(2,  "LOW FRQ",  arc, steel, fmtNeveLow,         "Low Shelf Frequency (Stepped 35/60/110/220 Hz in VINTAGE)", 0, false, parseHzLog(35.f, 220.f));
            addKnob(5,  "MID GN",   arc, brick, fmtDb(-15.f, 15.f), "Inductive Mid Band Gain (-15 to +15 dB)", 0, false, parseDb(-15.f, 15.f));
            addKnob(4,  "MID FRQ",  arc, steel, fmtNeveMid,         "Inductive Mid Frequency (Stepped 360 Hz - 7.2 kHz in VINTAGE)", 0, false, parseHzLog(360.f, 7200.f));
            addKnob(6,  "MID Q",    arc, steel, [](double v){ return "Q " + juce::String(kalyx::core::mapLog(static_cast<float>(v), 0.4f, 3.5f), 2); }, "Mid Band Resonance (Q)", 0, false, KalyxValueParsers::parseQLog(0.4f, 3.5f));
            addKnob(7,  "HIGH 12k", arc, brick, fmtDb(-15.f, 15.f), "Fixed 12 kHz High Shelf Gain (-15 to +15 dB)", 0, false, parseDb(-15.f, 15.f));
            addKnob(9,  "HPF FRQ",  arc, steel, fmtNeveHpf,         "Inductive HPF Cutoff (Stepped 50/80/160/300 Hz in VINTAGE)", 0, false, parseHzLog(35.f, 300.f));
            addKnob(10, "1073 OUT", arc, brick, fmtDb(-12.f, 12.f), "Neve 1073 Output Trim (-12 to +12 dB)", 0, false, parseDb(-12.f, 12.f));
            break;
        }

        case Mod::ConsoleColor: // Kalyx Original : Aubergine #5B2344 & Bleu Roi #2348B8
        {
            const auto aub = KalyxTheme::kalyxAubergine;
            const auto blu = KalyxTheme::kalyxRoyalBlueGlow;
            const auto cap = KalyxTheme::getControlIdleBg(theme_);
            addTog(1, "COLOR IN", aub, "Engage Console Transformer & Bus Saturation Stage");

            topArrowSelector_ = std::make_unique<KalyxArrowSelector>(
                apvts, ParamIDs::slotParamId(slotIndex_, 2), "",
                juce::StringArray { "SSL 4000 E", "SSL 4000 G", "API 2503", "NEVE SILK", "KALYX" },
                blu, darkTxt, "Select Analog Console Transformer & Harmonic Topology");
            addAndMakeVisible(*topArrowSelector_);

            addKnob(3, "DRIVE",     aub, cap, fmtPct(),              "Transformer Hysteresis & ADAA Harmonic Drive (0 - 100 %)", 15, false, parsePct());
            addKnob(4, "SAT TONE",  blu, cap, fmtPct(-100.f, 100.f), "Harmonic Tilt: Negative = Warm/Dark, Positive = Airy/Present", 15, false, parsePct(-100.f, 100.f));
            addKnob(5, "CROSSTALK", blu, cap, fmtPct(),              "Analog Stereo Bus L/R Crosstalk Bleed (0 - 100 %)", 15, false, parsePct());
            addKnob(6, "TRIM",      aub, cap, fmtDb(-12.f, 12.f),    "Color Stage Output Trim (-12 to +12 dB)", 15, false, parseDb(-12.f, 12.f));
            break;
        }

        case Mod::PrecisionLimiter: // Kalyx Original : Aubergine #5B2344 & Bleu Roi #2348B8
        {
            const auto aub = KalyxTheme::kalyxAubergine;
            const auto blu = KalyxTheme::kalyxRoyalBlueGlow;
            const auto cap = KalyxTheme::getControlIdleBg(theme_);
            addTog(1, "LIMIT IN", KalyxTheme::alertRed, "Engage Precision Peak Limiter");
            addTog(4, "AUTO REL", aub, "Program-dependent Auto-Release (disables manual LIMIT REL)");
            addKnob(2, "LIMIT THR", blu, cap, fmtDb(-30.f, 0.f),    "Peak Limiter Threshold (-30 to 0 dB)", 13, false, parseDb(-30.f, 0.f));
            transferCurve_ = std::make_unique<InteractiveTransferCurve>();
            transferCurve_->init(*this, apvts, ParamIDs::slotParamId(slotIndex_, 2), -30.0f, 0.0f, defs[1], false, blu, darkTxt,
                                 "Brickwall Limiter Transfer Curve (Drag to adjust Threshold)");
            releaseKnob_ = addKnob(3, "LIMIT REL", blu, cap, fmtMsLog(10.f, 500.f), "Manual Limiter Release Time (disabled when AUTO REL is active)", 13, false, parseMsLog(10.f, 500.f));
            addKnob(5, "CEILING",   aub, cap, fmtDb(-24.f, 0.f),    "True Output Brickwall Ceiling (-24 to 0 dBFS)", 13, false, parseDb(-24.f, 0.f));
            addKnob(6, "BLEND",     blu, cap, fmtPct(),             "Parallel Dry/Wet Blend (0 - 100 %)", 13, false, parsePct());
            break;
        }

        case Mod::UA1176Comp:
        {
            ua1176UI_ = std::make_unique<kalyx::consoles::UA1176UI>(processor_, slotIndex_);
            addAndMakeVisible(*ua1176UI_);
            break;
        }

        case Mod::LA2AComp:
        {
            la2aUI_ = std::make_unique<kalyx::consoles::LA2AUI>(processor_, slotIndex_);
            addAndMakeVisible(*la2aUI_);
            break;
        }

        case Mod::DistressorComp:
        {
            distressorUI_ = std::make_unique<kalyx::consoles::DistressorUI>(processor_, slotIndex_);
            addAndMakeVisible(*distressorUI_);
            break;
        }

        case Mod::FairchildComp:
        {
            fairchildUI_ = std::make_unique<kalyx::consoles::FairchildUI>(processor_, slotIndex_);
            addAndMakeVisible(*fairchildUI_);
            break;
        }

        case Mod::SurgicalEQ2U:
        {
            const int partner = std::max(0, lastPartnerSlot_);
            surgicalEq2uUI_ = std::make_unique<kalyx::consoles::SurgicalEQ2UUI>(processor_, slotIndex_, partner);
            addAndMakeVisible(*surgicalEq2uUI_);
            break;
        }

        case Mod::PrecisionDyn2U:
        {
            const int partner = std::max(0, lastPartnerSlot_);
            precisionDyn2uUI_ = std::make_unique<kalyx::consoles::PrecisionDyn2UUI>(processor_, slotIndex_, partner);
            addAndMakeVisible(*precisionDyn2uUI_);
            break;
        }

        case Mod::HarmonicSculptor:
        {
            harmonicSculptorUI_ = std::make_unique<kalyx::consoles::HarmonicSculptorUI>(processor_, slotIndex_);
            addAndMakeVisible(*harmonicSculptorUI_);
            break;
        }

        case Mod::StereoPhase:
        {
            stereoPhaseUI_ = std::make_unique<kalyx::consoles::StereoPhaseUI>(processor_, slotIndex_);
            addAndMakeVisible(*stereoPhaseUI_);
            break;
        }

        case Mod::EmptyBypass:
        case Mod::Count:
        default:
            break;
    }

    for (auto& r : rotaries_)        r->setUiScale(uiScale_);
    for (auto& sp : segmentedPills_) sp->setUiScale(uiScale_);
    for (auto& rs : rotarySteps_)    rs->setUiScale(uiScale_);
    if (topArrowSelector_)    topArrowSelector_->setUiScale(uiScale_);
    if (bottomArrowSelector_) bottomArrowSelector_->setUiScale(uiScale_);
    if (ua1176UI_)            { ua1176UI_->setUiScale(uiScale_);           ua1176UI_->setChassisTheme(theme_); }
    if (la2aUI_)              { la2aUI_->setUiScale(uiScale_);             la2aUI_->setChassisTheme(theme_); }
    if (distressorUI_)        { distressorUI_->setUiScale(uiScale_);       distressorUI_->setChassisTheme(theme_); }
    if (fairchildUI_)         { fairchildUI_->setUiScale(uiScale_);        fairchildUI_->setChassisTheme(theme_); }
    if (surgicalEq2uUI_)      { surgicalEq2uUI_->setUiScale(uiScale_);     surgicalEq2uUI_->setChassisTheme(theme_); }
    if (precisionDyn2uUI_)    { precisionDyn2uUI_->setUiScale(uiScale_);   precisionDyn2uUI_->setChassisTheme(theme_); }
    if (harmonicSculptorUI_)  { harmonicSculptorUI_->setUiScale(uiScale_); harmonicSculptorUI_->setChassisTheme(theme_); }
    if (stereoPhaseUI_)       { stereoPhaseUI_->setUiScale(uiScale_);      stereoPhaseUI_->setChassisTheme(theme_); }

    updateDependentControlStates();
    resized();
    repaint();
}

void ModuleSlotView::drawArrowButton(juce::Graphics& g, juce::Rectangle<float> bounds, const char* symbol, bool enabled) const
{
    const auto box = bounds.reduced(1.0f);
    const auto txt = KalyxTheme::getTextPrimary(theme_);
    g.setColour(enabled ? txt.withAlpha(0.16f) : txt.withAlpha(0.05f));
    g.fillRoundedRectangle(box, 2.0f);
    g.setColour(enabled ? txt.withAlpha(0.75f) : txt.withAlpha(0.20f));
    g.drawRoundedRectangle(box, 2.0f, 1.0f);
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::bold)));
    g.drawText(symbol, box.toNearestInt(), juce::Justification::centred);
}

void ModuleSlotView::drawLockIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool isLocked) const
{
    const auto box = bounds.reduced(1.5f);
    const auto txt = KalyxTheme::getTextPrimary(theme_);
    g.setColour(isLocked ? KalyxTheme::amberLed : txt.withAlpha(0.12f));
    g.fillRoundedRectangle(box, 2.5f);
    g.setColour(isLocked ? txt : txt.withAlpha(0.45f));
    g.drawRoundedRectangle(box, 2.5f, 1.0f);

    const float cx = box.getCentreX(), cy = box.getCentreY();
    g.setColour(isLocked ? KalyxTheme::dayTextPrimary : txt);
    g.fillRoundedRectangle(cx - 3.0f, cy - 0.5f, 6.0f, 5.0f, 1.0f);

    juce::Path sh;
    if (isLocked) sh.addCentredArc(cx, cy - 1.0f, 2.2f, 2.8f, 0.0f, -2.8f, 2.8f, true);
    else          sh.addCentredArc(cx + 1.2f, cy - 2.0f, 2.2f, 2.8f, 0.0f, -2.5f, 1.8f, true);
    g.strokePath(sh, juce::PathStrokeType(1.3f));
}

void ModuleSlotView::drawLedMeterStrip(juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    if (bounds.isEmpty()) return;
    using Mod = kalyx::core::SlotModuleId;
    const auto mod = static_cast<Mod>(currentModuleId_);

    const bool isDriveMeter = (mod == Mod::ConsoleColor);
    const bool isGrMeter    = (mod == Mod::API2500Comp || mod == Mod::Neve5254Comp
                            || mod == Mod::PrecisionLimiter || mod == Mod::SSL4000Comp);
    if (!isDriveMeter && !isGrMeter) return;

    const auto offTrack = (theme_ == kalyx::core::ChassisTheme::Day) ? KalyxTheme::dayKnobTrackBg : KalyxTheme::knobTrackBg;

    // Boîtier encastré pour la barre de LEDs
    g.setColour(KalyxTheme::getControlIdleBg(theme_));
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(KalyxTheme::getGridBorder(theme_));
    g.drawRoundedRectangle(bounds, 3.0f, 0.8f);

    g.setColour(KalyxTheme::getTextSecondary(theme_));
    g.setFont(juce::Font(juce::FontOptions(7.8f, juce::Font::bold)));
    g.drawText(isDriveMeter ? "HARMONIC DRIVE" : "GAIN REDUCTION (dB)",
               bounds.withHeight(10.0f).toNearestInt(), juce::Justification::centred);

    const auto row = bounds.withTrimmedTop(10.0f).reduced(4.0f, 1.0f);
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 7.5f, juce::Font::bold)));

    if (isDriveMeter)
    {
        static constexpr float kThresh[5] = { -12.0f, -6.0f, -3.0f, 0.0f, 2.0f };
        static constexpr const char* kLbls[5] = { "-12", "-6", "-3", "0", "PK" };
        const float cellW = row.getWidth() / 5.0f;
        for (int i = 0; i < 5; ++i)
        {
            const float cx = row.getX() + (static_cast<float>(i) + 0.5f) * cellW;
            const float cy = row.getY() + 5.5f;
            const bool lit = (smoothedDriveDb_ >= kThresh[i]);
            const juce::Colour onCol = (i < 2) ? juce::Colour(0xff22c55e)
                                     : (i < 4) ? KalyxTheme::amberLed : KalyxTheme::alertRed;
            const float r = 3.0f;
            if (lit)
            {
                g.setColour(onCol.withAlpha(0.35f));
                g.fillEllipse(cx - r - 1.8f, cy - r - 1.8f, (r + 1.8f) * 2.0f, (r + 1.8f) * 2.0f);
                g.setColour(onCol);
            }
            else
            {
                g.setColour(offTrack);
            }
            g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);

            g.setColour(lit ? KalyxTheme::getTextPrimary(theme_) : KalyxTheme::getTextSecondary(theme_));
            g.drawText(kLbls[i], static_cast<int>(cx - cellW * 0.5f), static_cast<int>(cy + 3.5f),
                       static_cast<int>(cellW), 9, juce::Justification::centred);
        }
    }
    else
    {
        static constexpr float kGrThresh[6] = { 1.0f, 2.0f, 4.0f, 6.0f, 10.0f, 20.0f };
        static constexpr const char* kGrLbls[6] = { "-1", "-2", "-4", "-6", "-10", "-20" };
        const float cellW = row.getWidth() / 6.0f;
        for (int i = 0; i < 6; ++i)
        {
            const float cx = row.getX() + (static_cast<float>(i) + 0.5f) * cellW;
            const float cy = row.getY() + 5.5f;
            const bool lit = (smoothedGrDb_ >= kGrThresh[i]);
            const juce::Colour onCol = (i < 2) ? juce::Colour(0xff22c55e)
                                     : (i < 4) ? KalyxTheme::amberLed : KalyxTheme::alertRed;
            const float r = 3.0f;
            if (lit)
            {
                g.setColour(onCol.withAlpha(0.35f));
                g.fillEllipse(cx - r - 1.8f, cy - r - 1.8f, (r + 1.8f) * 2.0f, (r + 1.8f) * 2.0f);
                g.setColour(onCol);
            }
            else
            {
                g.setColour(offTrack);
            }
            g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);

            g.setColour(lit ? KalyxTheme::getTextPrimary(theme_) : KalyxTheme::getTextSecondary(theme_));
            g.drawText(kGrLbls[i], static_cast<int>(cx - cellW * 0.5f), static_cast<int>(cy + 3.5f),
                       static_cast<int>(cellW), 9, juce::Justification::centred);
        }
    }
}

void ModuleSlotView::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const auto& desc = kalyx::core::ModuleRegistry::getById(currentModuleId_);

    using Mod = kalyx::core::SlotModuleId;
    const auto mod = static_cast<Mod>(currentModuleId_);
    const bool isSslMod = (mod == Mod::SSL4000Preamp || mod == Mod::SSL4000Comp || mod == Mod::SSL4000EQ);

    // Surface unifiée Tactile Flat (#181B22 Night / #F4F5F7 Day) + code couleur référencé par machine
    const juce::Colour bgCol = KalyxTheme::getStripBg(theme_);
    juce::Colour accentCol   = juce::Colour(desc.accentArgb);
    if (isSslMod)
        accentCol = isSslGActive_ ? KalyxTheme::sslGPinkMidCap : KalyxTheme::eqLfWineRed;
    else if (mod == Mod::UA1176Comp && ua1176UI_)
        accentCol = ua1176UI_->getCurrentAccentColour();

    const float cardRadius = 4.0f * uiScale_;
    g.setColour(bgCol);
    g.fillRoundedRectangle(r, cardRadius);

    // En-tête de slot avec liseré d'accent
    const float headerH = 26.0f * uiScale_;
    const juce::Rectangle<float> headerBar(0.0f, 0.0f, r.getWidth(), headerH);
    g.setColour(KalyxTheme::getChassisBg(theme_));
    g.fillRoundedRectangle(headerBar, cardRadius);
    g.setColour(accentCol);
    g.fillRect(0.0f, headerH - 2.0f * uiScale_, r.getWidth(), 2.0f * uiScale_);

    g.setColour(KalyxTheme::getGridBorder(theme_));
    g.drawRoundedRectangle(r.reduced(0.5f), cardRadius, std::max(0.8f, 1.0f * uiScale_));

    const float arrowW = 15.0f * uiScale_;
    const float arrowH = 17.0f * uiScale_;
    const float arrowY = 4.0f * uiScale_;
    leftArrowRect_  = { 3.0f * uiScale_,  arrowY, arrowW, arrowH };
    rightArrowRect_ = { 3.0f * uiScale_ + arrowW + 1.0f * uiScale_, arrowY, arrowW, arrowH };
    drawArrowButton(g, leftArrowRect_,  "<", canMoveLeft_);
    drawArrowButton(g, rightArrowRect_, ">", canMoveRight_);

    const float lockW = 17.0f * uiScale_;
    lockIconRect_ = { r.getWidth() - lockW - 3.0f * uiScale_, arrowY, lockW, arrowH };
    drawLockIcon(g, lockIconRect_, processor_.isSectionLocked(slotIndex_ + 1));

    // Badge lettre du Slot (A..E)
    static constexpr char kLetters[5] = { 'A', 'B', 'C', 'D', 'E' };
    g.setColour(accentCol);
    g.setFont(juce::Font(juce::FontOptions(11.0f * uiScale_, juce::Font::bold)));
    g.drawText(juce::String::charToString(static_cast<juce::juce_wchar>(kLetters[visualPos_])),
               static_cast<int>(rightArrowRect_.getRight() + 1.0f * uiScale_), 0,
               static_cast<int>(14.0f * uiScale_), static_cast<int>(headerH - 2.0f * uiScale_),
               juce::Justification::centred);

    // LED PEAK rouge en haut de la façade Neve 5254 (s'allume à -3 dBFS du clipping)
    if (mod == Mod::Neve5254Comp && !peakLedRect_.isEmpty())
    {
        const float cx = peakLedRect_.getX() + 7.0f * uiScale_;
        const float cy = peakLedRect_.getCentreY();
        const float rad = 3.5f * uiScale_;
        if (peakLedLit_)
        {
            g.setColour(KalyxTheme::alertRed.withAlpha(0.35f));
            g.fillEllipse(cx - rad - 2.0f * uiScale_, cy - rad - 2.0f * uiScale_,
                          (rad + 2.0f * uiScale_) * 2.0f, (rad + 2.0f * uiScale_) * 2.0f);
            g.setColour(KalyxTheme::alertRed);
        }
        else
        {
            g.setColour((theme_ == kalyx::core::ChassisTheme::Day) ? KalyxTheme::dayKnobTrackBg : KalyxTheme::knobTrackBg);
        }
        g.fillEllipse(cx - rad, cy - rad, rad * 2.0f, rad * 2.0f);
        g.setColour(peakLedLit_ ? KalyxTheme::getTextPrimary(theme_) : KalyxTheme::getTextSecondary(theme_));
        g.setFont(juce::Font(juce::FontOptions(9.0f * uiScale_, juce::Font::bold)));
        g.drawText("PEAK", static_cast<int>(cx + rad + 3.0f * uiScale_), static_cast<int>(peakLedRect_.getY()),
                   static_cast<int>(34.0f * uiScale_), static_cast<int>(peakLedRect_.getHeight()), juce::Justification::centredLeft);
    }

    // Mini-échelle de LEDs GR (6 LEDs) ou Harmonic Drive (5 LEDs)
    drawLedMeterStrip(g, ledMeterRect_);

    if (mod == Mod::EmptyBypass)
    {
        g.setColour(KalyxTheme::getTextSecondary(theme_).withAlpha(0.65f));
        g.setFont(juce::Font(juce::FontOptions(11.0f * uiScale_, juce::Font::bold)));
        g.drawText("500-SERIES EMPTY SLOT", r.withTrimmedTop(headerH).toNearestInt(), juce::Justification::centred);
    }
}

void ModuleSlotView::resized()
{
    const auto b = getLocalBounds().toFloat();
    const float headerH = 26.0f * uiScale_;
    const float menuX   = 50.0f * uiScale_;
    const float lockW   = 17.0f * uiScale_;
    const float agW     = 26.0f * uiScale_;
    const float menuW   = std::max(34.0f, b.getWidth() - menuX - lockW - agW - 10.0f * uiScale_);
    moduleMenuBtn_.setBounds(static_cast<int>(menuX), static_cast<int>(3.0f * uiScale_),
                             static_cast<int>(menuW), static_cast<int>(19.0f * uiScale_));
    agBtn_.setBounds(static_cast<int>(b.getWidth() - lockW - agW - 6.0f * uiScale_), static_cast<int>(3.0f * uiScale_),
                     static_cast<int>(agW), static_cast<int>(19.0f * uiScale_));

    const float pad = 5.0f * uiScale_;
    const float innerW = b.getWidth() - 2.0f * pad;
    float y = headerH + 4.0f * uiScale_;

    using Mod = kalyx::core::SlotModuleId;
    const auto mod = static_cast<Mod>(currentModuleId_);
    ledMeterRect_ = {};
    peakLedRect_  = {};

    const bool isDynWithExtSc = (mod == Mod::SSL4000Comp || mod == Mod::API2500Comp || mod == Mod::Neve5254Comp
                              || mod == Mod::PrecisionLimiter || mod == Mod::UA1176Comp || mod == Mod::LA2AComp
                              || mod == Mod::DistressorComp || mod == Mod::FairchildComp);
    extScBtn_.setVisible(isDynWithExtSc);
    scListenBtn_.setVisible(isDynWithExtSc);
    int contentTop = static_cast<int>(headerH);
    if (isDynWithExtSc)
    {
        const float hw  = (innerW - 4.0f * uiScale_) * 0.5f;
        const float scH = 19.0f * uiScale_;
        extScBtn_.setBounds(static_cast<int>(pad), static_cast<int>(y), static_cast<int>(hw), static_cast<int>(scH));
        scListenBtn_.setBounds(static_cast<int>(pad + hw + 4.0f * uiScale_), static_cast<int>(y), static_cast<int>(hw), static_cast<int>(scH));
        y += scH + 3.0f * uiScale_;
        contentTop = static_cast<int>(y);
    }

    if (mod == Mod::UA1176Comp && ua1176UI_)
    {
        ua1176UI_->setBounds(0, contentTop, getWidth(), std::max(0, getHeight() - contentTop));
        return;
    }

    if (mod == Mod::LA2AComp && la2aUI_)
    {
        la2aUI_->setBounds(0, contentTop, getWidth(), std::max(0, getHeight() - contentTop));
        return;
    }

    if (mod == Mod::DistressorComp && distressorUI_)
    {
        distressorUI_->setBounds(0, contentTop, getWidth(), std::max(0, getHeight() - contentTop));
        return;
    }

    if (mod == Mod::FairchildComp && fairchildUI_)
    {
        fairchildUI_->setBounds(0, contentTop, getWidth(), std::max(0, getHeight() - contentTop));
        return;
    }

    if (mod == Mod::SurgicalEQ2U && surgicalEq2uUI_)
    {
        surgicalEq2uUI_->setBounds(0, static_cast<int>(headerH), getWidth(), std::max(0, getHeight() - static_cast<int>(headerH)));
        return;
    }

    if (mod == Mod::PrecisionDyn2U && precisionDyn2uUI_)
    {
        precisionDyn2uUI_->setBounds(0, static_cast<int>(headerH), getWidth(), std::max(0, getHeight() - static_cast<int>(headerH)));
        return;
    }

    if (mod == Mod::HarmonicSculptor && harmonicSculptorUI_)
    {
        harmonicSculptorUI_->setBounds(0, static_cast<int>(headerH), getWidth(), std::max(0, getHeight() - static_cast<int>(headerH)));
        return;
    }

    if (mod == Mod::StereoPhase && stereoPhaseUI_)
    {
        stereoPhaseUI_->setBounds(0, static_cast<int>(headerH), getWidth(), std::max(0, getHeight() - static_cast<int>(headerH)));
        return;
    }

    const float pillH = 22.0f * uiScale_;

    // =========================================================================
    // Layout matériel dédié : API 2500 (3 pilules segmentées + [FIXED|VAR] + 6-LED GR)
    // =========================================================================
    if (mod == Mod::API2500Comp)
    {
        const float hw = (innerW - 3.0f * uiScale_) * 0.5f;
        if (toggles_.size() >= 2)
        {
            toggles_[0]->setBounds(static_cast<int>(pad),                        static_cast<int>(y), static_cast<int>(hw), static_cast<int>(20.0f * uiScale_));
            toggles_[1]->setBounds(static_cast<int>(pad + hw + 3.0f * uiScale_), static_cast<int>(y), static_cast<int>(hw), static_cast<int>(20.0f * uiScale_));
            y += 23.0f * uiScale_;
        }

        // Courbe de transfert dynamique (remplace la barre LED passive)
        if (transferCurve_)
        {
            const float cH = 62.0f * uiScale_;
            transferCurve_->setBounds(static_cast<int>(pad), static_cast<int>(y), static_cast<int>(innerW), static_cast<int>(cH));
            ledMeterRect_ = {};
            y += cH + 4.0f * uiScale_;
        }
        else
        {
            ledMeterRect_ = { pad, y, innerW, 27.0f * uiScale_ };
            y += 30.0f * uiScale_;
        }

        // Zone basse réservée : 3 pilules segmentées (KNEE, THRUST, LINK FLT) + ROUTING
        const float pillGap = 3.0f * uiScale_;
        const float bottomH = 4.0f * pillH + 3.0f * pillGap + 6.0f * uiScale_;
        const float knobAreaH = std::max(115.0f * uiScale_, b.getHeight() - y - bottomH - 4.0f * uiScale_);

        const float colW = innerW * 0.5f;
        const float rH   = (knobAreaH - pillH - 2.0f * uiScale_) / 4.0f;
        if (rotaries_.size() >= 7)
        {
            rotaries_[0]->setBoundsTight(pad,        y, colW, rH); // THRESH
            rotaries_[1]->setBoundsTight(pad + colW, y, colW, rH); // RATIO (cranté)
            y += rH;

            rotaries_[2]->setBoundsTight(pad,        y, colW, rH); // ATTACK
            rotaries_[3]->setBoundsTight(pad + colW, y, colW, rH); // RELEASE
            y += rH;

            if (apiRelPill_ != nullptr)
                apiRelPill_->setBounds(static_cast<int>(pad + colW), static_cast<int>(y),
                                       static_cast<int>(colW), static_cast<int>(pillH));
            y += pillH + 2.0f * uiScale_;

            rotaries_[4]->setBoundsTight(pad,        y, colW, rH); // L/R LINK
            rotaries_[5]->setBoundsTight(pad + colW, y, colW, rH); // MAKEUP
            y += rH;

            rotaries_[6]->setBoundsTight(pad + colW * 0.5f, y, colW, rH); // BLEND centré
        }

        // 3 pilules segmentées empilées proprement : KNEE, THRUST, LINK FLT + ROUTING
        float py = b.getHeight() - bottomH;
        for (auto& sp : segmentedPills_)
        {
            if (sp.get() == apiRelPill_) continue;
            sp->setBounds(static_cast<int>(pad), static_cast<int>(py), static_cast<int>(innerW), static_cast<int>(pillH));
            py += pillH + pillGap;
        }
        if (bottomArrowSelector_)
            bottomArrowSelector_->setBounds(static_cast<int>(pad), static_cast<int>(py),
                                            static_cast<int>(innerW), static_cast<int>(pillH));
        return;
    }

    // =========================================================================
    // Layout matériel dédié : Class-A 54 Diode (PEAK LED + Transfer Curve + 2 Rotatifs crantés + LINK + < ROUTING >)
    // =========================================================================
    if (mod == Mod::Neve5254Comp)
    {
        const float peakW = 48.0f * uiScale_;
        const float bw = (innerW - peakW - 6.0f * uiScale_) * 0.5f;
        if (toggles_.size() >= 2)
        {
            toggles_[0]->setBounds(static_cast<int>(pad),                        static_cast<int>(y), static_cast<int>(bw), static_cast<int>(20.0f * uiScale_));
            toggles_[1]->setBounds(static_cast<int>(pad + bw + 3.0f * uiScale_), static_cast<int>(y), static_cast<int>(bw), static_cast<int>(20.0f * uiScale_));
        }
        peakLedRect_ = { pad + 2.0f * bw + 6.0f * uiScale_, y, peakW, 20.0f * uiScale_ };
        y += 23.0f * uiScale_;

        if (transferCurve_)
        {
            const float cH = 66.0f * uiScale_;
            transferCurve_->setBounds(static_cast<int>(pad), static_cast<int>(y), static_cast<int>(innerW), static_cast<int>(cH));
            ledMeterRect_ = {};
            y += cH + 4.0f * uiScale_;
        }
        else
        {
            ledMeterRect_ = { pad, y, innerW, 27.0f * uiScale_ };
            y += 30.0f * uiScale_;
        }

        const float bottomH = 2.0f * pillH + 8.0f * uiScale_;
        const float bodyH   = std::max(120.0f * uiScale_, b.getHeight() - y - bottomH - 4.0f * uiScale_);
        const float colW    = innerW * 0.5f;
        const float rowH    = bodyH / 3.0f;

        if (rotaries_.size() >= 4)
        {
            rotaries_[0]->setBoundsTight(pad,        y, colW, rowH); // THRESH
            rotaries_[1]->setBoundsTight(pad + colW, y, colW, rowH); // SC HPF
            y += rowH;
            rotaries_[2]->setBoundsTight(pad,        y, colW, rowH); // GAIN
            rotaries_[3]->setBoundsTight(pad + colW, y, colW, rowH); // BLEND
            y += rowH;
        }
        if (rotarySteps_.size() >= 2)
        {
            rotarySteps_[0]->setBounds(static_cast<int>(pad),        static_cast<int>(y), static_cast<int>(colW), static_cast<int>(rowH)); // RATIO
            rotarySteps_[1]->setBounds(static_cast<int>(pad + colW), static_cast<int>(y), static_cast<int>(colW), static_cast<int>(rowH)); // TIMING
        }

        if (neveLinkBtn_ != nullptr && !segmentedPills_.empty())
        {
            const float linkW = innerW * 0.38f;
            const float gap   = 4.0f * uiScale_;
            const int rowY    = static_cast<int>(b.getHeight() - bottomH);
            neveLinkBtn_->setBounds(static_cast<int>(pad), rowY, static_cast<int>(linkW), static_cast<int>(pillH));
            segmentedPills_[0]->setBounds(static_cast<int>(pad + linkW + gap), rowY,
                                          static_cast<int>(innerW - linkW - gap), static_cast<int>(pillH));
        }
        else if (neveLinkBtn_ != nullptr)
        {
            neveLinkBtn_->setBounds(static_cast<int>(pad + innerW * 0.20f), static_cast<int>(b.getHeight() - bottomH),
                                    static_cast<int>(innerW * 0.60f), static_cast<int>(pillH));
        }
        if (bottomArrowSelector_)
            bottomArrowSelector_->setBounds(static_cast<int>(pad), static_cast<int>(b.getHeight() - pillH - 3.0f * uiScale_),
                                            static_cast<int>(innerW), static_cast<int>(pillH));
        return;
    }

    // =========================================================================
    // Layout générique haute densité (Console Color, Peak Limiter, SSL Comp, EQ, Preamps, Gate)
    // =========================================================================
    if (!toggles_.empty())
    {
        const int nT = static_cast<int>(toggles_.size());
        if (nT <= 4)
        {
            const float tw = (innerW - static_cast<float>(nT - 1) * 3.0f * uiScale_) / static_cast<float>(nT);
            for (int i = 0; i < nT; ++i)
                toggles_[static_cast<std::size_t>(i)]->setBounds(
                    static_cast<int>(pad + static_cast<float>(i) * (tw + 3.0f * uiScale_)),
                    static_cast<int>(y), static_cast<int>(tw), static_cast<int>(20.0f * uiScale_));
            y += 23.0f * uiScale_;
        }
        else
        {
            const int r1 = (nT + 1) / 2;
            const int r2 = nT - r1;
            const float tw1 = (innerW - static_cast<float>(r1 - 1) * 3.0f * uiScale_) / static_cast<float>(r1);
            for (int i = 0; i < r1; ++i)
                toggles_[static_cast<std::size_t>(i)]->setBounds(
                    static_cast<int>(pad + static_cast<float>(i) * (tw1 + 3.0f * uiScale_)),
                    static_cast<int>(y), static_cast<int>(tw1), static_cast<int>(19.0f * uiScale_));
            y += 22.0f * uiScale_;
            const float tw2 = (innerW - static_cast<float>(r2 - 1) * 3.0f * uiScale_) / static_cast<float>(r2);
            for (int i = 0; i < r2; ++i)
                toggles_[static_cast<std::size_t>(r1 + i)]->setBounds(
                    static_cast<int>(pad + static_cast<float>(i) * (tw2 + 3.0f * uiScale_)),
                    static_cast<int>(y), static_cast<int>(tw2), static_cast<int>(19.0f * uiScale_));
            y += 22.0f * uiScale_;
        }
    }

    if (!segmentedPills_.empty())
    {
        segmentedPills_[0]->setBounds(static_cast<int>(pad), static_cast<int>(y),
                                      static_cast<int>(innerW), static_cast<int>(pillH));
        y += pillH + 3.0f * uiScale_;
    }

    if (topArrowSelector_)
    {
        topArrowSelector_->setBounds(static_cast<int>(pad), static_cast<int>(y), static_cast<int>(innerW), static_cast<int>(pillH));
        y += pillH + 4.0f * uiScale_;
    }

    if (transferCurve_)
    {
        const float cH = 74.0f * uiScale_;
        transferCurve_->setBounds(static_cast<int>(pad), static_cast<int>(y), static_cast<int>(innerW), static_cast<int>(cH));
        ledMeterRect_ = {};
        y += cH + 4.0f * uiScale_;
    }
    else if (mod == Mod::ConsoleColor)
    {
        ledMeterRect_ = { pad, y, innerW, 27.0f * uiScale_ };
        y += 30.0f * uiScale_;
    }

    if (eqCurve_)
    {
        const float curveH = 102.0f * uiScale_;
        eqCurve_->setBounds(static_cast<int>(pad), static_cast<int>(y), static_cast<int>(innerW), static_cast<int>(curveH));
        y += curveH + 3.0f * uiScale_;
    }

    const float botArrowH = bottomArrowSelector_ ? (pillH + 4.0f * uiScale_) : 0.0f;
    if (bottomArrowSelector_)
        bottomArrowSelector_->setBounds(static_cast<int>(pad), static_cast<int>(b.getHeight() - pillH - 3.0f * uiScale_),
                                        static_cast<int>(innerW), static_cast<int>(pillH));

    const int nK = static_cast<int>(rotaries_.size());
    const float availH = std::max(40.0f * uiScale_, b.getHeight() - y - botArrowH - 4.0f * uiScale_);
    if (nK > 0)
    {
        const int cols = (nK <= 4) ? 1 : 2;
        const int rows = (nK + cols - 1) / cols;
        const float maxSingleColRowH = 92.0f * uiScale_;
        const float rawRowH = availH / static_cast<float>(rows);
        const float rowH    = (cols == 1) ? std::min(rawRowH, maxSingleColRowH) : rawRowH;
        const float totalKH = rowH * static_cast<float>(rows);
        const float startY  = (cols == 1 && totalKH < availH) ? (y + (availH - totalKH) * 0.35f) : y;
        const float colW    = innerW / static_cast<float>(cols);

        for (int i = 0; i < nK; ++i)
        {
            const int r = i / cols;
            const int c = i % cols;
            rotaries_[static_cast<std::size_t>(i)]->setBoundsTight(
                pad + static_cast<float>(c) * colW,
                startY + static_cast<float>(r) * rowH,
                colW, rowH);
        }
    }
}

void ModuleSlotView::mouseDown(const juce::MouseEvent& e)
{
    if (canMoveLeft_ && leftArrowRect_.expanded(2.0f).contains(e.position) && onMoveLeft_)
    {
        onMoveLeft_();
        return;
    }
    if (canMoveRight_ && rightArrowRect_.expanded(2.0f).contains(e.position) && onMoveRight_)
    {
        onMoveRight_();
        return;
    }
    if (lockIconRect_.expanded(3.0f).contains(e.position))
    {
        processor_.setSectionLocked(slotIndex_ + 1, !processor_.isSectionLocked(slotIndex_ + 1));
        repaint();
    }
}
