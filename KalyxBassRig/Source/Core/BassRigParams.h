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
    addBool("cab_a_mute", "Cab A Mute", false);
    addBool("cab_a_solo", "Cab A Solo", false);
    addFloat("cab_a_pan", "Cab A Pan", -100.0f, 100.0f, 0.0f, 1.0f);
    addBool("cab_b_phase", "Cab B Phase", false);
    addBool("cab_b_mute", "Cab B Mute", false);
    addBool("cab_b_solo", "Cab B Solo", false);
    addFloat("cab_b_pan", "Cab B Pan", -100.0f, 100.0f, 0.0f, 1.0f);
    addFloat("cab_a_level", "Cab A Level", -48.0f, 12.0f, 0.0f, 0.1f, 2.0f);
    addFloat("cab_b_level", "Cab B Level", -48.0f, 12.0f, 0.0f, 0.1f, 2.0f);
    addBool("cab_auto_phase", "Cab Auto-Phase", true);
    addFloat("cab_breakup", "Speaker Breakup", 0.0f, 100.0f, 18.0f, 0.1f);
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

    // =========================================================================
    // Pass 2: Modular Pedalboard (6 Pre-Amp + 4 Post-Amp) & Routing
    // =========================================================================
    // Pedal 0: Opto / FET Bass Comp
    addBool("fx_comp_en", "Pedal Comp Enable", false);
    addChoice("fx_comp_mode", "Pedal Comp Mode", { "OPTO", "FET" }, 0);
    addFloat("fx_comp_in", "Pedal Comp Input", 0.0f, 100.0f, 50.0f, 0.1f);
    addFloat("fx_comp_amt", "Pedal Comp Amount", 0.0f, 100.0f, 40.0f, 0.1f);
    addFloat("fx_comp_blend", "Pedal Comp Blend", 0.0f, 100.0f, 75.0f, 0.1f);
    addFloat("fx_comp_makeup", "Pedal Comp Makeup", -24.0f, 24.0f, 0.0f, 0.1f);

    // Pedal 1: Octaver Poly/Sub
    addBool("fx_oct_en", "Octaver Enable", false);
    addFloat("fx_oct_clean", "Octaver Clean", 0.0f, 100.0f, 70.0f, 0.1f);
    addFloat("fx_oct_sub1", "Octaver Sub 1", 0.0f, 100.0f, 50.0f, 0.1f);
    addFloat("fx_oct_sub2", "Octaver Sub 2", 0.0f, 100.0f, 20.0f, 0.1f);
    addFloat("fx_oct_up", "Octaver Oct Up", 0.0f, 100.0f, 0.0f, 0.1f);

    // Pedal 2: Envelope Filter (Auto-Wah)
    addBool("fx_env_en", "Env Filter Enable", false);
    addFloat("fx_env_sens", "Env Filter Sens", 0.0f, 100.0f, 55.0f, 0.1f);
    addFloat("fx_env_att", "Env Filter Attack", 1.0f, 80.0f, 8.0f, 0.1f);
    addFloat("fx_env_dec", "Env Filter Decay", 30.0f, 600.0f, 180.0f, 1.0f);
    addFloat("fx_env_q", "Env Filter Q", 0.8f, 10.0f, 3.5f, 0.1f);

    // Pedal 3: Transparent Drive (B3K / Klon Bass)
    addBool("fx_tdrv_en", "Transparent Drive Enable", false);
    addFloat("fx_tdrv_drv", "Transparent Drive", 0.0f, 100.0f, 45.0f, 0.1f);
    addFloat("fx_tdrv_tone", "Transparent Tone", 0.0f, 100.0f, 55.0f, 0.1f);
    addFloat("fx_tdrv_blend", "Transparent Dry Blend", 0.0f, 100.0f, 50.0f, 0.1f);
    addFloat("fx_tdrv_lvl", "Transparent Level", -24.0f, 24.0f, 0.0f, 0.1f);

    // Pedal 4: Rodent Bass (ProCo Rat Modified)
    addBool("fx_rat_en", "Rodent Bass Enable", false);
    addFloat("fx_rat_dist", "Rodent Dist", 0.0f, 100.0f, 50.0f, 0.1f);
    addFloat("fx_rat_flt", "Rodent Filter", 0.0f, 100.0f, 40.0f, 0.1f);
    addFloat("fx_rat_blend", "Rodent Clean Blend", 0.0f, 100.0f, 35.0f, 0.1f);
    addFloat("fx_rat_lvl", "Rodent Level", -24.0f, 24.0f, 0.0f, 0.1f);

    // Pedal 5: Big Wall Fuzz (Russian Muff)
    addBool("fx_fuzz_en", "Big Wall Fuzz Enable", false);
    addFloat("fx_fuzz_sust", "Fuzz Sustain", 0.0f, 100.0f, 65.0f, 0.1f);
    addFloat("fx_fuzz_tilt", "Fuzz Tilt", -100.0f, 100.0f, 0.0f, 1.0f);
    addFloat("fx_fuzz_low", "Fuzz Low Recovery", 0.0f, 100.0f, 45.0f, 0.1f);
    addFloat("fx_fuzz_lvl", "Fuzz Level", -24.0f, 24.0f, 0.0f, 0.1f);

    // Pedal 6: Dimension Chorus
    addBool("fx_cho_en", "Chorus Enable", false);
    addFloat("fx_cho_rate", "Chorus Rate", 0.1f, 5.0f, 0.8f, 0.01f);
    addFloat("fx_cho_depth", "Chorus Depth", 0.0f, 100.0f, 45.0f, 0.1f);
    addFloat("fx_cho_mix", "Chorus Mix", 0.0f, 100.0f, 40.0f, 0.1f);

    // Pedal 7: Analog Phaser
    addBool("fx_pha_en", "Phaser Enable", false);
    addChoice("fx_pha_stages", "Phaser Stages", { "4-Stage", "8-Stage" }, 0);
    addBool("fx_pha_sync", "Phaser Host Sync", false);
    addChoice("fx_pha_div", "Phaser Sync Div", { "1/4", "1/8", "1/8D", "1/16" }, 0);
    addFloat("fx_pha_rate", "Phaser Rate", 0.05f, 8.0f, 0.6f, 0.01f);
    addFloat("fx_pha_depth", "Phaser Depth", 0.0f, 100.0f, 60.0f, 0.1f);
    addFloat("fx_pha_fb", "Phaser Feedback", 0.0f, 90.0f, 35.0f, 0.1f);

    // Pedal 8: Tape Echo
    addBool("fx_tape_en", "Tape Echo Enable", false);
    addBool("fx_tape_sync", "Tape Host Sync", true);
    addChoice("fx_tape_div", "Tape Sync Div", { "1/4", "1/8", "1/8D", "1/16" }, 2);
    addFloat("fx_tape_time", "Tape Time ms", 40.0f, 1200.0f, 320.0f, 1.0f);
    addFloat("fx_tape_rep", "Tape Repeats", 0.0f, 92.0f, 38.0f, 0.1f);
    addFloat("fx_tape_wow", "Tape Wow", 0.0f, 100.0f, 25.0f, 0.1f);
    addFloat("fx_tape_flutter", "Tape Flutter", 0.0f, 100.0f, 20.0f, 0.1f);
    addFloat("fx_tape_sat", "Tape Saturation", 0.0f, 100.0f, 35.0f, 0.1f);
    addFloat("fx_tape_mix", "Tape Mix", 0.0f, 100.0f, 28.0f, 0.1f);

    // Pedal 9: Studio Bass EQ + Mono Maker
    addBool("fx_eq4_en", "Studio EQ Enable", false);
    addFloat("fx_eq4_mono_freq", "Mono Maker Hz", 20.0f, 250.0f, 120.0f, 1.0f);
    addFloat("fx_eq4_b1", "EQ4 60 Hz", -12.0f, 12.0f, 0.0f, 0.1f);
    addFloat("fx_eq4_b2", "EQ4 250 Hz", -12.0f, 12.0f, 0.0f, 0.1f);
    addFloat("fx_eq4_b3", "EQ4 1.2 kHz", -12.0f, 12.0f, 0.0f, 0.1f);
    addFloat("fx_eq4_b4", "EQ4 4.5 kHz", -12.0f, 12.0f, 0.0f, 0.1f);

    // Routing order (10 slots: 0..5 default PRE, 6..9 default POST)
    for (int i = 0; i < 10; ++i)
    {
        const juce::String posId  = "fx_pos_" + juce::String(i);
        const juce::String postId = "fx_post_" + juce::String(i);
        p.push_back(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{ posId, 1 }, posId, 0, 9, i));
        p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ postId, 1 }, postId, i >= 6));
    }

    return { p.begin(), p.end() };
}

} // namespace kalyx::bass
