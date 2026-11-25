#include "PresetManager.h"
#include <algorithm>

namespace kalyx::core
{

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvts)
    : apvts_(apvts)
{
    rescanPresets();
}

juce::File PresetManager::getUserPresetsDirectory() const
{
    const auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                         .getChildFile("KalyxAudio")
                         .getChildFile("Presets");
    if (!dir.exists())
        dir.createDirectory();
    return dir;
}

void PresetManager::rescanPresets()
{
    const juce::String prevName = getCurrentPresetName();
    presets_.clear();

    // 1. Tranches Complètes (Factory)
    presets_.push_back({ "Init (5-Slot Studio)",      "Tranches Completes", true, 0, {} });
    presets_.push_back({ "SSL 4000 E Vocal Strip",    "Tranches Completes", true, 1, {} });
    presets_.push_back({ "API 1608 / 2500 Drum Bus",  "Tranches Completes", true, 2, {} });
    presets_.push_back({ "Neve 1073 / 5254 Vintage",  "Tranches Completes", true, 3, {} });

    // 2. Hybrides (Factory)
    presets_.push_back({ "API Punch + SSL Glue",      "Hybrides",           true, 4, {} });
    presets_.push_back({ "Neve Warmth + 550B Air",    "Hybrides",           true, 5, {} });
    presets_.push_back({ "12-String Hybrid",          "Hybrides",           true, 6, {} });

    // 3. Mastering (Factory)
    presets_.push_back({ "SSL G Glue + Peak Ceiling", "Mastering",          true, 7, {} });
    presets_.push_back({ "Kalyx Heritage Clean Master","Mastering",         true, 8, {} });

    // 4. Utilisateur (Documents/KalyxAudio/Presets/*.xml)
    const auto userDir = getUserPresetsDirectory();
    const auto xmlFiles = userDir.findChildFiles(juce::File::findFiles, false, "*.xml");
    for (const auto& f : xmlFiles)
    {
        presets_.push_back({ f.getFileNameWithoutExtension(), "Utilisateur", false, -1, f });
    }

    // Restaure la sélection courante si trouvée
    currentIndex_ = 0;
    for (int i = 0; i < static_cast<int>(presets_.size()); ++i)
    {
        if (presets_[static_cast<std::size_t>(i)].name == prevName)
        {
            currentIndex_ = i;
            break;
        }
    }
}

juce::String PresetManager::getCurrentPresetName() const
{
    if (currentIndex_ >= 0 && currentIndex_ < static_cast<int>(presets_.size()))
        return presets_[static_cast<std::size_t>(currentIndex_)].name;
    return "Init (5-Slot Studio)";
}

bool PresetManager::isCurrentPresetUser() const noexcept
{
    if (currentIndex_ >= 0 && currentIndex_ < static_cast<int>(presets_.size()))
        return !presets_[static_cast<std::size_t>(currentIndex_)].isFactory;
    return false;
}

bool PresetManager::loadPresetByIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(presets_.size()))
        return false;

    currentIndex_ = index;
    const auto& entry = presets_[static_cast<std::size_t>(index)];

    if (entry.isFactory)
    {
        if (onLoadFactory_)
            onLoadFactory_(entry.factoryId);
        return true;
    }

    if (entry.file.existsAsFile())
    {
        if (auto xml = juce::XmlDocument::parse(entry.file))
        {
            if (xml->hasTagName(apvts_.state.getType()))
            {
                apvts_.replaceState(juce::ValueTree::fromXml(*xml));
                return true;
            }
        }
    }
    return false;
}

bool PresetManager::saveCurrentPreset()
{
    if (!isCurrentPresetUser())
        return false;

    const auto& entry = presets_[static_cast<std::size_t>(currentIndex_)];
    auto state = apvts_.copyState();
    if (auto xml = state.createXml())
        return xml->writeTo(entry.file);
    return false;
}

bool PresetManager::savePresetAs(const juce::String& newName)
{
    const auto cleanName = juce::File::createLegalFileName(newName.trim());
    if (cleanName.isEmpty())
        return false;

    const auto targetFile = getUserPresetsDirectory().getChildFile(cleanName + ".xml");
    auto state = apvts_.copyState();
    if (auto xml = state.createXml())
    {
        if (xml->writeTo(targetFile))
        {
            rescanPresets();
            for (int i = 0; i < static_cast<int>(presets_.size()); ++i)
            {
                if (!presets_[static_cast<std::size_t>(i)].isFactory &&
                    presets_[static_cast<std::size_t>(i)].name == cleanName)
                {
                    currentIndex_ = i;
                    break;
                }
            }
            return true;
        }
    }
    return false;
}

bool PresetManager::renameCurrentPreset(const juce::String& newName)
{
    if (!isCurrentPresetUser())
        return false;

    const auto cleanName = juce::File::createLegalFileName(newName.trim());
    if (cleanName.isEmpty())
        return false;

    const auto oldFile = presets_[static_cast<std::size_t>(currentIndex_)].file;
    const auto newFile = oldFile.getParentDirectory().getChildFile(cleanName + ".xml");
    if (oldFile.moveFileTo(newFile))
    {
        rescanPresets();
        for (int i = 0; i < static_cast<int>(presets_.size()); ++i)
        {
            if (!presets_[static_cast<std::size_t>(i)].isFactory &&
                presets_[static_cast<std::size_t>(i)].name == cleanName)
            {
                currentIndex_ = i;
                break;
            }
        }
        return true;
    }
    return false;
}

bool PresetManager::deleteCurrentPreset()
{
    if (!isCurrentPresetUser())
        return false;

    const auto file = presets_[static_cast<std::size_t>(currentIndex_)].file;
    if (file.deleteFile())
    {
        rescanPresets();
        currentIndex_ = 0;
        return true;
    }
    return false;
}

} // namespace kalyx::core
