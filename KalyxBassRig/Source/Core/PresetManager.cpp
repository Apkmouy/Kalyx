#include "PresetManager.h"

namespace kalyx::bass::core
{

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvts)
    : apvts_(apvts)
{
    factoryNames_ = {
        "Heritage 300 // Classic 8x10",
        "Classic Tube // Warm Motown",
        "Hybrid 4-PRO // Modern Punch",
        "Tangerine 200 // Heavy Fuzz",
        "Studio DI // Clean Sub + Cab"
    };

    userPresetDir_ = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                         .getChildFile("KalyxAudio")
                         .getChildFile("KalyxBassRig")
                         .getChildFile("Presets");
    userPresetDir_.createDirectory();

    slotStates_[0] = apvts_.copyState();
    slotStates_[1] = apvts_.copyState();
}

juce::StringArray PresetManager::getAllPresetNames() const
{
    juce::StringArray all = factoryNames_;
    userNames_.clear();
    for (const auto& f : userPresetDir_.findChildFiles(juce::File::findFiles, false, "*.xml"))
        userNames_.add(f.getFileNameWithoutExtension());
    userNames_.sort(true);
    all.addArray(userNames_);
    return all;
}

juce::String PresetManager::getCurrentPresetName() const
{
    auto all = getAllPresetNames();
    if (currentIndex_ >= 0 && currentIndex_ < all.size())
        return all[currentIndex_];
    return "Default";
}

void PresetManager::setParam(const char* id, float plainVal)
{
    if (auto* p = apvts_.getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(plainVal));
}

void PresetManager::applyFactoryPreset(int idx)
{
    // Reset common EQ9
    const char* eqIds[9] = { "eq9_40", "eq9_90", "eq9_180", "eq9_300", "eq9_500", "eq9_1k", "eq9_2k", "eq9_4k", "eq9_10k" };
    for (auto* id : eqIds) setParam(id, 0.0f);

    if (idx == 0) // Heritage 300
    {
        setParam("amp_model", 0.0f);
        setParam("amp_channel", 0.0f);
        setParam("amp_gain", 44.0f);
        setParam("amp_bright", 0.0f);
        setParam("amp_ultra_lo", 3.0f);
        setParam("amp_ultra_hi", 0.0f);
        setParam("amp_bass", 2.0f);
        setParam("amp_mid", -1.5f);
        setParam("amp_mid_freq", 2.0f);
        setParam("amp_treble", 1.5f);
        setParam("amp_drive", 25.0f);
        setParam("amp_sag", 35.0f);
        setParam("di_circuit", 1.0f);
        setParam("di_level", -6.0f);
        setParam("amp_level", 0.0f);
    }
    else if (idx == 1) // Classic Tube
    {
        setParam("amp_model", 1.0f);
        setParam("amp_gain", 56.0f);
        setParam("amp_ultra_lo", 3.0f);
        setParam("amp_ultra_hi", 0.0f);
        setParam("amp_bass", 4.0f);
        setParam("amp_mid", 2.0f);
        setParam("amp_mid_freq", 0.0f);
        setParam("amp_treble", -3.0f);
        setParam("amp_drive", 32.0f);
        setParam("amp_sag", 45.0f);
        setParam("di_circuit", 3.0f);
        setParam("di_level", -8.0f);
        setParam("amp_level", 0.0f);
    }
    else if (idx == 2) // Hybrid 4-PRO
    {
        setParam("amp_model", 2.0f);
        setParam("amp_gain", 50.0f);
        setParam("amp_bright", 1.0f);
        setParam("amp_ultra_lo", 4.0f);
        setParam("amp_ultra_hi", 1.0f);
        setParam("amp_bass", 2.5f);
        setParam("amp_mid", -2.5f);
        setParam("amp_mid_freq", 2.0f);
        setParam("amp_treble", 3.5f);
        setParam("amp_drive", 58.0f);
        setParam("amp_comp", 40.0f);
        setParam("eq9_en", 1.0f);
        setParam("eq9_40", 2.5f);
        setParam("eq9_90", 3.0f);
        setParam("eq9_500", -4.0f);
        setParam("eq9_2k", 3.5f);
        setParam("eq9_4k", 2.5f);
        setParam("di_circuit", 2.0f);
        setParam("di_level", -4.0f);
        setParam("amp_level", 0.0f);
    }
    else if (idx == 3) // Tangerine 200
    {
        setParam("amp_model", 3.0f);
        setParam("amp_gain", 74.0f);
        setParam("amp_depth", 0.0f); // 100nF
        setParam("amp_boost", 68.0f);
        setParam("amp_bass", 3.5f);
        setParam("amp_mid", 1.5f);
        setParam("amp_treble", 2.0f);
        setParam("amp_drive", 72.0f);
        setParam("amp_presence", 48.0f);
        setParam("amp_sag", 50.0f);
        setParam("di_circuit", 1.0f);
        setParam("di_level", -10.0f);
        setParam("amp_level", 0.0f);
    }
    else // Studio Clean DI
    {
        setParam("amp_model", 0.0f);
        setParam("amp_gain", 24.0f);
        setParam("amp_drive", 10.0f);
        setParam("di_circuit", 4.0f); // Sub-Harmonic
        setParam("di_sub_mix", 30.0f);
        setParam("di_comp", 40.0f);
        setParam("di_level", -1.5f);
        setParam("amp_level", -4.0f);
    }
}

void PresetManager::loadPresetByIndex(int index)
{
    auto all = getAllPresetNames();
    if (index < 0 || index >= all.size())
        return;

    currentIndex_ = index;
    if (index < factoryNames_.size())
    {
        applyFactoryPreset(index);
    }
    else
    {
        const auto file = userPresetDir_.getChildFile(all[index] + ".xml");
        if (auto xml = juce::XmlDocument::parse(file))
            apvts_.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

bool PresetManager::saveUserPreset(const juce::String& name)
{
    if (name.trim().isEmpty())
        return false;
    userPresetDir_.createDirectory();
    const auto file = userPresetDir_.getChildFile(name.trim() + ".xml");
    if (auto xml = apvts_.copyState().createXml())
        return xml->writeTo(file);
    return false;
}

void PresetManager::switchSlotAB(int slot)
{
    const int target = (slot == 0) ? 0 : 1;
    if (target == activeSlot_)
        return;
    slotStates_[static_cast<size_t>(activeSlot_)] = apvts_.copyState();
    activeSlot_ = target;
    apvts_.replaceState(slotStates_[static_cast<size_t>(activeSlot_)]);
}

void PresetManager::copyAToB()
{
    slotStates_[static_cast<size_t>(activeSlot_)] = apvts_.copyState();
    slotStates_[static_cast<size_t>(1 - activeSlot_)] = slotStates_[static_cast<size_t>(activeSlot_)].createCopy();
}

} // namespace kalyx::bass::core
