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
    [[nodiscard]] bool isUserPreset(int index) const;

    void loadPresetByIndex(int index);
    bool saveCurrentPreset();
    bool saveUserPreset(const juce::String& name);
    bool renameUserPreset(int index, const juce::String& newName);
    bool deleteUserPreset(int index);

private:
    void applyFactoryPreset(int factoryIdx);
    void setParam(const char* id, float plainVal);

    juce::AudioProcessorValueTreeState& apvts_;
    juce::File userPresetDir_;
    juce::StringArray factoryNames_;
    mutable juce::StringArray userNames_;
    int currentIndex_ { 0 };
};

} // namespace kalyx::bass::core
