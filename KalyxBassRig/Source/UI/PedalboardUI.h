#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "KalyxTheme.h"
#include "DataWidgets.h"
#include "../FX/PedalboardEngine.h"
#include <array>
#include <memory>

namespace kalyx::bass::ui
{

class PedalCardComponent final : public juce::Component,
                                 public juce::SettableTooltipClient
{
public:
    PedalCardComponent(juce::AudioProcessorValueTreeState& apvts, int pedalId);

    [[nodiscard]] int getPedalId() const noexcept { return pedalId_; }
    void setUiScale(float s) noexcept;
    void setDarkText(bool dark) noexcept;
    void setTelemetryGr(float grDb) noexcept { compGrDb_ = grDb; if (pedalId_ == 0) repaint(); }
    void updateBypassVisuals();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

private:
    juce::AudioProcessorValueTreeState& apvts_;
    int pedalId_ { 0 };
    juce::String title_;
    juce::Colour accent_ { KalyxTheme::kalyxRoyalBlue };
    float uiScale_  { 1.0f };
    bool  darkText_ { false };
    bool  isHovered_ { false };
    float compGrDb_ { 0.0f };
    juce::Point<int> dragStart_;

    juce::ToggleButton bypassBtn_ { "ON" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt_;

    std::vector<std::unique_ptr<TactileFaderBar>> bars_;
    std::unique_ptr<KalyxSegmentedPill> pill_;
    std::unique_ptr<VectorPad2D> pad2d_;
    std::unique_ptr<juce::ToggleButton> syncBtn_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAtt_;
};

class PedalChainZone final : public juce::Component,
                             public juce::DragAndDropTarget
{
public:
    PedalChainZone(bool isPostZone, juce::String zoneTitle,
                   std::function<void(int pedalId, bool toPost, int targetIndex)> onDropCallback);

    void setUiScale(float s) noexcept { uiScale_ = std::clamp(s, 0.75f, 2.20f); resized(); repaint(); }
    void setDarkText(bool dark) noexcept { darkText_ = dark; repaint(); }
    void setCards(const std::vector<PedalCardComponent*>& cards);

    void paint(juce::Graphics& g) override;
    void resized() override;

    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDragEnter(const SourceDetails& details) override;
    void itemDragExit(const SourceDetails& details) override;
    void itemDropped(const SourceDetails& details) override;

private:
    bool isPost_ { false };
    juce::String title_;
    std::function<void(int, bool, int)> onDrop_;
    std::vector<PedalCardComponent*> cards_;
    bool dragHover_ { false };
    float uiScale_  { 1.0f };
    bool  darkText_ { false };
};

class PedalboardView final : public juce::Component,
                             public juce::DragAndDropContainer
{
public:
    explicit PedalboardView(juce::AudioProcessorValueTreeState& apvts);

    void setUiScale(float s) noexcept;
    void setDarkText(bool dark) noexcept;
    void setCompGrDb(float grDb) noexcept;
    void syncOrderFromAPVTS();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void handleDrop(int pedalId, bool toPost, int targetIndex);
    void resetDefaultOrder();

    juce::AudioProcessorValueTreeState& apvts_;
    float uiScale_  { 1.0f };
    bool  darkText_ { false };

    std::array<std::unique_ptr<PedalCardComponent>, fx::NumPedals> cards_;
    PedalChainZone preZone_;
    PedalChainZone postZone_;
    juce::TextButton resetOrderBtn_ { "RESET ROUTING" };
};

} // namespace kalyx::bass::ui
