#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <algorithm>
#include <cmath>

namespace kalyx::bass
{

enum class ChassisTheme
{
    Night = 0,
    Day   = 1
};

enum class AmpModel
{
    Heritage300  = 0,
    ClassicTube  = 1,
    Hybrid4Pro   = 2,
    Tangerine200 = 3
};

enum class DICircuit
{
    Transparent  = 0,
    TubeDI       = 1,
    American2503 = 2,
    ClassA73     = 3,
    SubHarmonic  = 4
};

enum class MicType
{
    RE20      = 0,
    D112      = 1,
    MD421     = 2,
    U47FET    = 3,
    SM57      = 4,
    Ribbon121 = 5
};

[[nodiscard]] inline float mapLog(float norm01, float minV, float maxV) noexcept
{
    const float t = std::clamp(norm01, 0.0f, 1.0f);
    return minV * std::pow(maxV / std::max(minV, 1e-6f), t);
}

[[nodiscard]] inline float unmapLog(float val, float minV, float maxV) noexcept
{
    const float clamped = std::clamp(val, minV, maxV);
    return std::log(clamped / std::max(minV, 1e-6f)) / std::log(maxV / std::max(minV, 1e-6f));
}

[[nodiscard]] inline float mapLin(float norm01, float minV, float maxV) noexcept
{
    return minV + std::clamp(norm01, 0.0f, 1.0f) * (maxV - minV);
}

[[nodiscard]] inline float unmapLin(float val, float minV, float maxV) noexcept
{
    return std::clamp((val - minV) / std::max(maxV - minV, 1e-6f), 0.0f, 1.0f);
}

[[nodiscard]] inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto addBool = [&](const char* id, const char* name, bool def)
    {
        p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id, 1 }, name, def));
    };
    auto addFloat = [&](const char* id, const char* name, float minV, float maxV, float defV, float step = 0.0f, float skew = 1.0f)
    {
        juce::NormalisableRange<float> r(minV, maxV, step, skew);
        p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id, 1 }, name, r, defV));
    };
    auto addChoice = [&](const char* id, const char* name, const juce::StringArray& choices, int defIdx)
    {
        p.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ id, 1 }, name, choices, defIdx));
    };

    // Input, Gate & Tuner
    addFloat("in_trim", "In Trim", -24.0f, 24.0f, 0.0f, 0.1f);
    addBool("in_pad", "Pad -15 dB", false);
    addBool("gate_en", "Noise Gate", false);
    addFloat("gate_thresh", "Gate Threshold", -80.0f, -10.0f, -55.0f, 0.1f);
    addBool("tuner_mute", "Mute When Tuning", false);

    // Parallel DI Path
    addBool("di_en", "DI Enable", true);
    addChoice("di_circuit", "DI Circuit", { "Transparent", "Tube DI", "American 2503", "Class-A 73", "Sub-Harmonic" }, 1);
    addFloat("di_sub_mix", "Sub-Harmonic Mix", 0.0f, 100.0f, 35.0f, 0.1f);
    addFloat("di_hpf_freq", "DI HPF", 20.0f, 120.0f, 30.0f, 0.1f, 0.5f);
    addFloat("di_lpf_freq", "DI LPF", 60.0f, 5000.0f, 800.0f, 1.0f, 0.35f);
    addFloat("di_comp", "DI Opto Comp", 0.0f, 100.0f, 25.0f, 0.1f);
    addBool("di_auto_phase", "DI Auto-Phase", true);
    addFloat("di_level", "DI Level", -60.0f, 12.0f, -6.0f, 0.1f, 2.0f);

    // Amp Head
    addBool("amp_en", "Amp Enable", true);
    addChoice("amp_model", "Amp Model", { "Heritage 300", "Classic Tube", "Hybrid 4-PRO", "Tangerine 200" }, 0);
    addChoice("amp_channel", "Input Channel", { "CH 1 (12DW7)", "CH 2 (12AX7)" }, 0);
    addFloat("amp_gain", "Amp Gain", 0.0f, 100.0f, 42.0f, 0.1f);
    addBool("amp_bright", "Bright", false);
    addChoice("amp_ultra_lo", "Ultra Low", { "OFF", "SUB CUT", "LOW CUT", "ULTRA LO", "MODERN" }, 0);
    addBool("amp_ultra_hi", "Ultra High", false);
    addFloat("amp_bass", "Bass", -15.0f, 15.0f, 0.0f, 0.1f);
    addFloat("amp_mid", "Mid", -15.0f, 15.0f, 0.0f, 0.1f);
    addChoice("amp_mid_freq", "Mid Freq", { "220 Hz", "450 Hz", "800 Hz", "1.6 kHz", "3.0 kHz" }, 2);
    addFloat("amp_treble", "Treble", -15.0f, 15.0f, 0.0f, 0.1f);
    addFloat("amp_drive", "Drive", 0.0f, 100.0f, 25.0f, 0.1f);
    addFloat("amp_comp", "Amp Opto Comp", 0.0f, 100.0f, 20.0f, 0.1f);
    addChoice("amp_depth", "Depth / F.A.C.", { "100 nF", "47 nF", "10 nF", "4.7 nF", "2.2 nF", "1.0 nF" }, 1);
    addFloat("amp_boost", "Boost", 0.0f, 100.0f, 25.0f, 0.1f);
    addFloat("amp_presence", "Presence", 0.0f, 100.0f, 35.0f, 0.1f);
    addFloat("amp_sag", "Power Sag", 0.0f, 100.0f, 30.0f, 0.1f);
    addFloat("amp_bias", "Tube Bias", -50.0f, 50.0f, 0.0f, 0.5f);
    addFloat("amp_master", "Amp Master", 0.0f, 100.0f, 65.0f, 0.1f);

    // 9-Band Graphic EQ (Hybrid 4-PRO 07S728-02)
    addBool("eq9_en", "9-Band EQ Enable", true);
    addFloat("eq9_40",  "EQ 40 Hz",   -15.0f, 15.0f, 0.0f, 0.1f);
    addFloat("eq9_90",  "EQ 90 Hz",   -15.0f, 15.0f, 0.0f, 0.1f);
    addFloat("eq9_180", "EQ 180 Hz",  -15.0f, 15.0f, 0.0f, 0.1f);
    addFloat("eq9_300", "EQ 300 Hz",  -15.0f, 15.0f, 0.0f, 0.1f);
    addFloat("eq9_500", "EQ 500 Hz",  -15.0f, 15.0f, 0.0f, 0.1f);
    addFloat("eq9_1k",  "EQ 1 kHz",   -15.0f, 15.0f, 0.0f, 0.1f);
    addFloat("eq9_2k",  "EQ 2 kHz",   -15.0f, 15.0f, 0.0f, 0.1f);
    addFloat("eq9_4k",  "EQ 4 kHz",   -15.0f, 15.0f, 0.0f, 0.1f);
    addFloat("eq9_10k", "EQ 10 kHz",  -15.0f, 15.0f, 0.0f, 0.1f);

    // Dual Cabinet IR & Mic Studio
    addBool("cab_en", "Cabinet Enable", true);
    p.push_back(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{ "cab_a_ir", 1 }, "Cab A IR", 0, 63, 0));
    p.push_back(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{ "cab_b_ir", 1 }, "Cab B IR", 0, 63, 8));
    addBool("cab_a_phase", "Cab A Phase", false);
    addBool("cab_b_phase", "Cab B Phase", false);
    addFloat("cab_mix", "Cab A/B Mix", 0.0f, 100.0f, 35.0f, 0.1f);
    addBool("cab_auto_phase", "Cab Auto-Phase", true);
    addFloat("cab_breakup", "Speaker Breakup", 0.0f, 100.0f, 18.0f, 0.1f);
    addChoice("mic_type", "Mic Type", { "RE20", "D112", "MD421", "U47 FET", "SM57", "Ribbon 121" }, 0);
    addFloat("mic_pos_x", "Mic Cap-Edge", 0.0f, 100.0f, 25.0f, 0.1f);
    addFloat("mic_pos_y", "Mic Proximity", 0.0f, 100.0f, 75.0f, 0.1f);
    addFloat("cab_room", "Room Ambience", 0.0f, 100.0f, 15.0f, 0.1f);

    // Master, Kick-Relief Sidechain, Limiter & UI
    addFloat("amp_level", "Amp+Cab Level", -60.0f, 12.0f, 0.0f, 0.1f, 2.0f);
    addBool("kick_relief_en", "Kick-Relief SC", false);
    addFloat("kick_relief_amt", "Kick-Relief Amount", 0.0f, 100.0f, 45.0f, 0.1f);
    addBool("lim_en", "Brickwall Limiter", true);
    addFloat("lim_ceiling", "Limiter Ceiling", -24.0f, 0.0f, -0.3f, 0.1f);
    addFloat("out_fader", "Out Fader", -60.0f, 12.0f, 0.0f, 0.1f, 2.0f);
    addChoice("vu_mode", "VU Mode", { "IN", "OUT", "GR" }, 1);
    addFloat("vu_ref", "VU Reference", -20.0f, 0.0f, -18.0f, 0.5f);
    addChoice("oversampling", "Oversampling", { "1x (Off)", "2x IIR", "4x IIR" }, 0);
    addChoice("ui_theme", "UI Theme", { "Night", "Day" }, 0);

    return { p.begin(), p.end() };
}

} // namespace kalyx::bass
