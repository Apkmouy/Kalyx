#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace kalyx::bass::core
{

class PresetManager
{
public:
    explicit PresetManager(juce::AudioProcessorValueTreeState& apvts);

    [[nodiscard]] juce::StringArray getAllPresetNames() const;
    [[nodiscard]] int getCurrentPresetIndex() const noexcept { return currentIndex_; }
    [[nodiscard]] juce::String getCurrentPresetName() const;

    void loadPresetByIndex(int index);
    bool saveUserPreset(const juce::String& name);

    // A/B state comparison
    void switchSlotAB(int slot); // 0 = A, 1 = B
    void copyAToB();
    [[nodiscard]] int getActiveSlot() const noexcept { return activeSlot_; }

private:
    void applyFactoryPreset(int factoryIdx);
    void setParam(const char* id, float plainVal);

    juce::AudioProcessorValueTreeState& apvts_;
    juce::File userPresetDir_;
    juce::StringArray factoryNames_;
    mutable juce::StringArray userNames_;
    int currentIndex_ { 0 };

    int activeSlot_ { 0 };
    std::array<juce::ValueTree, 2> slotStates_;
};

} // namespace kalyx::bass::core
