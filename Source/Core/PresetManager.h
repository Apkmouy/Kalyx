#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <vector>

namespace kalyx::core
{

struct PresetEntry
{
    juce::String name;
    juce::String category; // "Tranches Completes", "Hybrides", "Mastering", "Utilisateur"
    bool isFactory { false };
    int  factoryId { -1 };
    juce::File file;
};

/**
 * Gestionnaire de Presets complet (Documents/KalyxAudio/Presets/, fichiers .xml) :
 * - Catégories : Tranches Complètes, Hybrides, Mastering, Utilisateur
 * - Sauvegarde/Restauration complète de l'APVTS (ordre des 5 slots A-E, modules, finition, M/S)
 * - Actions directes : SAVE, SAVE AS..., RENAME, DELETE.
 */
class PresetManager
{
public:
    explicit PresetManager(juce::AudioProcessorValueTreeState& apvts);

    void rescanPresets();
    [[nodiscard]] const std::vector<PresetEntry>& getPresets() const noexcept { return presets_; }
    [[nodiscard]] int getCurrentPresetIndex() const noexcept { return currentIndex_; }
    [[nodiscard]] juce::String getCurrentPresetName() const;
    [[nodiscard]] bool isCurrentPresetUser() const noexcept;

    void setFactoryPresetCallback(std::function<void(int)> cb) { onLoadFactory_ = std::move(cb); }

    bool loadPresetByIndex(int index);
    bool saveCurrentPreset();
    bool savePresetAs(const juce::String& newName);
    bool renameCurrentPreset(const juce::String& newName);
    bool deleteCurrentPreset();

    [[nodiscard]] juce::File getUserPresetsDirectory() const;

private:
    juce::AudioProcessorValueTreeState& apvts_;
    std::vector<PresetEntry> presets_;
    int currentIndex_ { 0 };
    std::function<void(int)> onLoadFactory_;
};

} // namespace kalyx::core
