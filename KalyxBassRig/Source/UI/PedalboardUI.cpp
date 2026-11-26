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

    auto addBar = [&](const char* id, const char* lbl, bool bipolar, double defV, std::function<juce::String(double)> fmt)
    {
        auto b = std::make_unique<TactileFaderBar>();
        b->init(*this, apvts_, id, lbl, accent_, bipolar, darkText_, defV, std::move(fmt));
        bars_.push_back(std::move(b));
    };

    switch (pedalId_)
    {
        case fx::Pedal_OptoFetComp:
            title_  = "OPTO / FET COMP";
            accent_ = KalyxTheme::classicAmber;
            bypassAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts_, "fx_comp_en", bypassBtn_);
            pill_ = std::make_unique<KalyxSegmentedPill>(apvts_, "fx_comp_mode", "", juce::StringArray { "OPTO", "FET" }, accent_, darkText_);
            addAndMakeVisible(*pill_);
            addBar("fx_comp_amt", "COMP", false, 40.0, fmtPct);
            addBar("fx_comp_blend", "BLEND", false, 75.0, fmtPct);
            break;

        case fx::Pedal_Octaver:
            title_  = "OCTAVER POLY/SUB";
            accent_ = KalyxTheme::kalyxAubergine;
            bypassAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts_, "fx_oct_en", bypassBtn_);
            addBar("fx_oct_clean", "CLEAN", false, 70.0, fmtPct);
            addBar("fx_oct_sub1", "SUB 1", false, 50.0, fmtPct);
            addBar("fx_oct_sub2", "SUB 2", false, 20.0, fmtPct);
            addBar("fx_oct_up", "OCT UP", false, 0.0, fmtPct);
            break;

        case fx::Pedal_EnvFilter:
            title_  = "ENV FILTER WAH";
            accent_ = KalyxTheme::jadeGreen;
            bypassAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts_, "fx_env_en", bypassBtn_);
            addBar("fx_env_sens", "SENS", false, 55.0, fmtPct);
            addBar("fx_env_att", "ATT", false, 8.0, fmtMs);
            addBar("fx_env_dec", "DEC", false, 180.0, fmtMs);
            addBar("fx_env_q", "Q", false, 3.5, [](double v) { return "Q " + juce::String(v, 1); });
            break;

        case fx::Pedal_TransDrive:
            title_  = "TRANSPARENT DRV";
            accent_ = KalyxTheme::hybridCyan;
            bypassAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts_, "fx_tdrv_en", bypassBtn_);
            addBar("fx_tdrv_drv", "DRIVE", false, 45.0, fmtPct);
            addBar("fx_tdrv_tone", "TONE", false, 55.0, fmtPct);
            addBar("fx_tdrv_blend", "DRY BLEND", false, 50.0, fmtPct);
            break;

        case fx::Pedal_RodentBass:
            title_  = "RODENT BASS";
            accent_ = KalyxTheme::tangerineOrange;
            bypassAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts_, "fx_rat_en", bypassBtn_);
            addBar("fx_rat_dist", "DIST", false, 50.0, fmtPct);
            addBar("fx_rat_flt", "FILTER", false, 40.0, fmtPct);
            addBar("fx_rat_blend", "CLEAN", false, 35.0, fmtPct);
            break;

        case fx::Pedal_BigWallFuzz:
            title_  = "BIG WALL FUZZ";
            accent_ = KalyxTheme::alertRed;
            bypassAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts_, "fx_fuzz_en", bypassBtn_);
            addBar("fx_fuzz_sust", "SUSTAIN", false, 65.0, fmtPct);
            addBar("fx_fuzz_tilt", "TILT", true, 0.0, [](double v) { return juce::String(v, 0); });
            addBar("fx_fuzz_low", "LOW REC", false, 45.0, fmtPct);
            break;

        case fx::Pedal_DimChorus:
            title_  = "DIM CHORUS (200Hz+)";
            accent_ = KalyxTheme::kalyxRoyalBlueGlow;
            bypassAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts_, "fx_cho_en", bypassBtn_);
            addBar("fx_cho_rate", "RATE", false, 0.8, fmtHz);
            addBar("fx_cho_depth", "DEPTH", false, 45.0, fmtPct);
            addBar("fx_cho_mix", "MIX", false, 40.0, fmtPct);
            break;

        case fx::Pedal_AnalogPhaser:
            title_  = "ANALOG PHASER";
            accent_ = KalyxTheme::tangerineOrange;
            bypassAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts_, "fx_pha_en", bypassBtn_);
            pill_ = std::make_unique<KalyxSegmentedPill>(apvts_, "fx_pha_div", "", juce::StringArray { "1/4", "1/8", "1/8D", "1/16" }, accent_, darkText_);
            addAndMakeVisible(*pill_);
            syncBtn_ = std::make_unique<juce::ToggleButton>("SYNC");
            syncBtn_->setClickingTogglesState(true);
            syncAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts_, "fx_pha_sync", *syncBtn_);
            addAndMakeVisible(*syncBtn_);
            pad2d_ = std::make_unique<VectorPad2D>();
            pad2d_->init(*this, apvts_, "fx_pha_rate", "fx_pha_depth", "RATE", "DEPTH", accent_, darkText_, 0.1f, 0.6f);
            addBar("fx_pha_fb", "FEEDBACK", false, 35.0, fmtPct);
            break;

        case fx::Pedal_TapeEcho:
            title_  = "TAPE ECHO";
            accent_ = KalyxTheme::classicAmber;
            bypassAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts_, "fx_tape_en", bypassBtn_);
            pill_ = std::make_unique<KalyxSegmentedPill>(apvts_, "fx_tape_div", "", juce::StringArray { "1/4", "1/8", "1/8D", "1/16" }, accent_, darkText_);
            addAndMakeVisible(*pill_);
            syncBtn_ = std::make_unique<juce::ToggleButton>("SYNC");
            syncBtn_->setClickingTogglesState(true);
            syncAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts_, "fx_tape_sync", *syncBtn_);
            addAndMakeVisible(*syncBtn_);
            pad2d_ = std::make_unique<VectorPad2D>();
            pad2d_->init(*this, apvts_, "fx_tape_time", "fx_tape_rep", "TIME", "REPEATS", accent_, darkText_, 0.25f, 0.4f);
            addBar("fx_tape_mix", "MIX", false, 28.0, fmtPct);
            break;

        case fx::Pedal_StudioEQ:
        default:
            title_  = "STUDIO EQ + MONO";
            accent_ = KalyxTheme::jadeGreen;
            bypassAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts_, "fx_eq4_en", bypassBtn_);
            addBar("fx_eq4_mono_freq", "MONO <", false, 120.0, [](double v) { return juce::String(v, 0) + " Hz"; });
            addBar("fx_eq4_b1", "60 Hz", true, 0.0, fmtDb);
            addBar("fx_eq4_b2", "250 Hz", true, 0.0, fmtDb);
            addBar("fx_eq4_b3", "1.2 kHz", true, 0.0, fmtDb);
            addBar("fx_eq4_b4", "4.5 kHz", true, 0.0, fmtDb);
            break;
    }

    bypassBtn_.setClickingTogglesState(true);
    bypassBtn_.setColour(juce::ToggleButton::tickColourId, accent_);
    addAndMakeVisible(bypassBtn_);
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

    g.setColour(bg);
    g.fillRoundedRectangle(r, 6.0f * sc);
    g.setColour(on ? accent_ : bd);
    g.drawRoundedRectangle(r, 6.0f * sc, on ? 1.6f * sc : 1.0f * sc);

    // Top accent bar + drag grip
    g.setColour(accent_.withAlpha(on ? 0.90f : 0.30f));
    g.fillRoundedRectangle(r.getX(), r.getY(), r.getWidth(), 4.0f * sc, 2.0f * sc);

    g.setColour(tx.withAlpha(0.88f));
    g.setFont(KalyxTheme::makeFont(10.5f * sc, juce::Font::bold));
    g.drawFittedText(":: " + title_, r.reduced(6.0f * sc, 5.0f * sc).withHeight(18.0f * sc).toNearestInt(),
                     juce::Justification::centredLeft, 1);

    if (pedalId_ == fx::Pedal_OptoFetComp && on)
    {
        g.setColour(KalyxTheme::classicAmber);
        g.setFont(KalyxTheme::makeMonoFont(9.0f * sc, juce::Font::bold));
        g.drawFittedText("GR " + juce::String(compGrDb_, 1),
                         r.reduced(6.0f * sc, 24.0f * sc).withHeight(14.0f * sc).toNearestInt(),
                         juce::Justification::centredRight, 1);
    }
}

void PedalCardComponent::resized()
{
    const float sc = uiScale_;
    auto r = getLocalBounds().reduced(static_cast<int>(6.0f * sc));
    auto top = r.removeFromTop(static_cast<int>(22.0f * sc));
    bypassBtn_.setBounds(top.removeFromRight(static_cast<int>(48.0f * sc)).reduced(0, 1));
    r.removeFromTop(static_cast<int>(4.0f * sc));

    if (syncBtn_ != nullptr && pill_ != nullptr)
    {
        auto row = r.removeFromTop(static_cast<int>(20.0f * sc));
        syncBtn_->setBounds(row.removeFromLeft(static_cast<int>(54.0f * sc)));
        row.removeFromLeft(4);
        pill_->setBounds(row);
        r.removeFromTop(static_cast<int>(4.0f * sc));
    }
    else if (pill_ != nullptr)
    {
        pill_->setBounds(r.removeFromTop(static_cast<int>(20.0f * sc)));
        r.removeFromTop(static_cast<int>(4.0f * sc));
    }

    if (pad2d_ != nullptr)
    {
        const int padH = std::max(44, r.getHeight() - static_cast<int>(bars_.size()) * static_cast<int>(24.0f * sc));
        pad2d_->setBounds(r.removeFromTop(padH));
        r.removeFromTop(static_cast<int>(3.0f * sc));
    }

    const int barH = static_cast<int>(21.0f * sc);
    for (auto& b : bars_)
    {
        b->setBounds(r.removeFromTop(barH));
        r.removeFromTop(static_cast<int>(3.0f * sc));
    }
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

void PedalboardView::setCompGrDb(float grDb) noexcept
{
    if (cards_[0]) cards_[0]->setTelemetryGr(grDb);
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
