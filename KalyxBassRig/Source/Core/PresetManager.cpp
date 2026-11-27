#include "PresetManager.h"

namespace kalyx::bass::core
{

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvts)
    : apvts_(apvts)
{
    factoryNames_ = {
        "Indie Pick (Classic Tube + Chorus)",
        "Modern Metal (4-PRO + B3K)",
        "Motown Thump (Heritage + Flatwound)",
        "Doom Wall (Tangerine + Fuzz)",
        "Synth Dub (Sub-Octave + Tape)"
    };

    userPresetDir_ = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                         .getChildFile("KalyxAudio")
                         .getChildFile("KalyxBassRig")
                         .getChildFile("Presets");
    userPresetDir_.createDirectory();
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
    const char* eqIds[9] = { "eq9_40", "eq9_90", "eq9_180", "eq9_300", "eq9_500", "eq9_1k", "eq9_2k", "eq9_4k", "eq9_10k" };
    for (auto* id : eqIds) setParam(id, 0.0f);

    // Reset all FX bypasses
    const char* fxEnIds[10] = {
        "fx_comp_en", "fx_oct_en", "fx_env_en", "fx_tdrv_en", "fx_rat_en",
        "fx_fuzz_en", "fx_cho_en", "fx_pha_en", "fx_tape_en", "fx_eq4_en"
    };
    for (auto* id : fxEnIds) setParam(id, 0.0f);

    setParam("gate_en", 0.0f);
    setParam("amp_en", 1.0f);
    setParam("cab_en", 1.0f);
    setParam("di_en", 1.0f);
    setParam("vu_ref", -18.0f);

    if (idx == 0) // Indie Pick (Classic Tube + Chorus)
    {
        setParam("amp_model", 1.0f); // Classic Tube
        setParam("amp_gain", 46.0f);
        setParam("amp_bass", 1.5f);
        setParam("amp_mid", 2.0f);
        setParam("amp_mid_freq", 2.0f);
        setParam("amp_treble", 3.0f);
        setParam("amp_drive", 24.0f);
        setParam("cab_a_ir", 0.0f); // 8x10
        setParam("fx_tdrv_en", 1.0f);
        setParam("fx_tdrv_drv", 28.0f);
        setParam("fx_tdrv_blend", 65.0f);
        setParam("fx_cho_en", 1.0f);
        setParam("fx_cho_rate", 0.85f);
        setParam("fx_cho_depth", 48.0f);
        setParam("fx_cho_mix", 38.0f);
        setParam("di_circuit", 1.0f);
        setParam("di_level", -7.0f);
        setParam("amp_level", -1.0f);
        setParam("out_fader", 0.0f);
    }
    else if (idx == 1) // Modern Metal (4-PRO + B3K)
    {
        setParam("amp_model", 2.0f); // Hybrid 4-PRO
        setParam("gate_en", 1.0f);
        setParam("gate_thresh", -44.0f);
        setParam("amp_gain", 52.0f);
        setParam("amp_bright", 1.0f);
        setParam("amp_ultra_lo", 4.0f);
        setParam("amp_ultra_hi", 1.0f);
        setParam("eq9_en", 1.0f);
        setParam("eq9_40", 3.0f);
        setParam("eq9_90", 2.5f);
        setParam("eq9_500", -6.0f);
        setParam("eq9_2k", 4.5f);
        setParam("eq9_4k", 3.5f);
        setParam("fx_tdrv_en", 1.0f);
        setParam("fx_tdrv_drv", 78.0f);
        setParam("fx_tdrv_tone", 68.0f);
        setParam("fx_tdrv_blend", 42.0f);
        setParam("cab_a_ir", 8.0f);
        setParam("di_circuit", 2.0f);
        setParam("di_level", -4.5f);
        setParam("amp_level", -1.5f);
        setParam("out_fader", -1.0f);
    }
    else if (idx == 2) // Motown Thump (Heritage + Flatwound)
    {
        setParam("amp_model", 0.0f); // Heritage 300
        setParam("amp_channel", 1.0f); // CH 2
        setParam("amp_gain", 48.0f);
        setParam("amp_ultra_lo", 3.0f);
        setParam("amp_ultra_hi", 0.0f);
        setParam("amp_bass", 6.5f);
        setParam("amp_mid", 1.5f);
        setParam("amp_treble", -8.0f);
        setParam("mic_type", 5.0f); // Ribbon 121
        setParam("mic_pos_y", 25.0f); // Far
        setParam("fx_comp_en", 1.0f);
        setParam("fx_comp_mode", 0.0f); // OPTO
        setParam("fx_comp_amt", 55.0f);
        setParam("fx_comp_blend", 80.0f);
        setParam("di_circuit", 1.0f);
        setParam("di_lpf_freq", 420.0f);
        setParam("di_level", -4.0f);
        setParam("amp_level", -1.0f);
        setParam("out_fader", 0.0f);
    }
    else if (idx == 3) // Doom Wall (Tangerine + Fuzz)
    {
        setParam("amp_model", 3.0f); // Tangerine 200
        setParam("amp_gain", 82.0f);
        setParam("amp_depth", 0.0f); // 100 nF
        setParam("amp_boost", 80.0f);
        setParam("amp_bass", 5.0f);
        setParam("amp_mid", 2.5f);
        setParam("amp_treble", 1.5f);
        setParam("amp_drive", 85.0f);
        setParam("amp_sag", 65.0f);
        setParam("fx_fuzz_en", 1.0f);
        setParam("fx_fuzz_sust", 100.0f);
        setParam("fx_fuzz_tilt", -15.0f);
        setParam("fx_fuzz_low", 65.0f);
        setParam("cab_a_ir", 0.0f);
        setParam("di_circuit", 3.0f);
        setParam("di_level", -10.0f);
        setParam("amp_level", -2.5f);
        setParam("out_fader", -2.0f);
    }
    else // Synth Dub (Sub-Octave + Tape)
    {
        setParam("amp_en", 0.0f);
        setParam("cab_en", 0.0f);
        setParam("di_circuit", 0.0f); // Transparent DI
        setParam("di_lpf_freq", 200.0f);
        setParam("di_level", 0.0f);
        setParam("amp_level", -60.0f);
        setParam("fx_oct_en", 1.0f);
        setParam("fx_oct_clean", 45.0f);
        setParam("fx_oct_sub1", 100.0f);
        setParam("fx_oct_sub2", 100.0f);
        setParam("fx_oct_up", 0.0f);
        setParam("fx_env_en", 1.0f);
        setParam("fx_env_sens", 65.0f);
        setParam("fx_env_q", 4.5f);
        setParam("fx_tape_en", 1.0f);
        setParam("fx_tape_sync", 1.0f);
        setParam("fx_tape_div", 2.0f); // 1/8D
        setParam("fx_tape_rep", 48.0f);
        setParam("fx_tape_mix", 34.0f);
        setParam("out_fader", -1.5f);
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

bool PresetManager::isUserPreset(int index) const
{
    return index >= factoryNames_.size() && index < getAllPresetNames().size();
}

bool PresetManager::saveCurrentPreset()
{
    if (isUserPreset(currentIndex_))
        return saveUserPreset(getCurrentPresetName());
    return false;
}

bool PresetManager::saveUserPreset(const juce::String& name)
{
    const auto trimmed = name.trim();
    if (trimmed.isEmpty())
        return false;
    userPresetDir_.createDirectory();
    const auto file = userPresetDir_.getChildFile(trimmed + ".xml");
    if (auto xml = apvts_.copyState().createXml())
    {
        if (xml->writeTo(file))
        {
            const auto all = getAllPresetNames();
            const int idx = all.indexOf(trimmed);
            if (idx >= 0) currentIndex_ = idx;
            return true;
        }
    }
    return false;
}

bool PresetManager::renameUserPreset(int index, const juce::String& newName)
{
    const auto trimmed = newName.trim();
    if (!isUserPreset(index) || trimmed.isEmpty())
        return false;
    const auto all = getAllPresetNames();
    const auto oldFile = userPresetDir_.getChildFile(all[index] + ".xml");
    const auto newFile = userPresetDir_.getChildFile(trimmed + ".xml");
    if (oldFile.existsAsFile() && oldFile.moveFileTo(newFile))
    {
        const auto updated = getAllPresetNames();
        const int idx = updated.indexOf(trimmed);
        if (idx >= 0) currentIndex_ = idx;
        return true;
    }
    return false;
}

bool PresetManager::deleteUserPreset(int index)
{
    if (!isUserPreset(index))
        return false;
    const auto all = getAllPresetNames();
    const auto file = userPresetDir_.getChildFile(all[index] + ".xml");
    if (file.existsAsFile() && file.deleteFile())
    {
        currentIndex_ = 0;
        loadPresetByIndex(0);
        return true;
    }
    return false;
}

} // namespace kalyx::bass::core
