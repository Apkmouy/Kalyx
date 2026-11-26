#pragma once

#include "ConsoleModuleBase.h"
#include <array>

namespace kalyx::core
{

/**
 * Registre central du Catalogue Série 500 (ModuleRegistry) classé en 4 catégories :
 * - Dynamics : SSL 4000 VCA (E/G), API 2500, Neve 5254, UA 1176 (FET), Teletronix LA-2A (Opto), Gate / De-Esser, Peak Limiter
 * - Equalizers : SSL 4000 EQ (E/G), API 550B EQ, Neve 1073 EQ
 * - Color & Preamp : SSL 4000 Preamp (E/G), API 2520 Preamp, Neve 1073 Marinair, Kalyx Clean Preamp, Color Stage
 * - Utility : Empty / Bypass
 */
class ModuleRegistry
{
public:
    static const std::array<SlotModuleDescriptor, static_cast<std::size_t>(SlotModuleId::Count)>& getAll() noexcept
    {
        static constexpr std::array<SlotModuleDescriptor, static_cast<std::size_t>(SlotModuleId::Count)> kModules {{
            // Color & Preamp
            { SlotModuleId::SSL4000Preamp,      "British 4K Pre",    "British 4K Preamp",            "Color & Preamp", 0xff181b22u, 0xff9e2a3bu, false },
            { SlotModuleId::API2520Preamp,      "American 25 Pre",   "American 25 Preamp",           "Color & Preamp", 0xff181b22u, 0xff1e64b4u, false },
            { SlotModuleId::Neve1073Preamp,     "Class-A 73 Pre",    "Class-A 73 Preamp",            "Color & Preamp", 0xff181b22u, 0xffa62c3bu, false },
            { SlotModuleId::KalyxCleanPreamp,   "Kalyx Clean",       "Kalyx Clean Preamp",           "Color & Preamp", 0xff181b22u, 0xff2348b8u, false },

            // Dynamics
            { SlotModuleId::SSL4000Comp,        "British 4K VCA",    "British 4K VCA",               "Dynamics",       0xff181b22u, 0xff9e2a3bu, false },
            { SlotModuleId::API2500Comp,        "American 2500",     "American 2500 VCA",            "Dynamics",       0xff181b22u, 0xff1e64b4u, false },
            { SlotModuleId::Neve5254Comp,       "Class-A 54",        "Class-A 54 Diode",             "Dynamics",       0xff181b22u, 0xffa62c3bu, false },

            // Equalizers
            { SlotModuleId::SSL4000EQ,          "British 4K EQ",     "British 4K EQ",                "Equalizers",     0xff181b22u, 0xff9e2a3bu, false },
            { SlotModuleId::API550BEQ,          "American 550 EQ",   "American 550 EQ",              "Equalizers",     0xff181b22u, 0xff1e64b4u, false },
            { SlotModuleId::Neve1073EQ,         "Class-A 73 EQ",     "Class-A 73 EQ",                "Equalizers",     0xff181b22u, 0xffa62c3bu, false },

            // Color & Preamp (Analog Color)
            { SlotModuleId::ConsoleColor,       "Analog Color",      "Analog Color",                 "Color & Preamp", 0xff181b22u, 0xff5b2344u, false },

            // Dynamics / Utility
            { SlotModuleId::UtilityGateDeEsser, "Gate & De-Esser",   "Gate & De-Esser",              "Dynamics",       0xff181b22u, 0xff2348b8u, false },
            { SlotModuleId::PrecisionLimiter,   "Brickwall Lim",     "Brickwall Limiter",            "Dynamics",       0xff181b22u, 0xff5b2344u, false },
            { SlotModuleId::EmptyBypass,        "Empty",             "Empty / Bypass",               "Utility",        0xff181b22u, 0xff262a35u, false },

            // Dynamics (FET / Opto / VCA Hybride / Vari-Mu)
            { SlotModuleId::UA1176Comp,         "FET-76 Comp",       "FET-76 Compressor",            "Dynamics",       0xff181b22u, 0xffd64545u, false },
            { SlotModuleId::LA2AComp,           "Opto-2A Lev",       "Opto-2A Leveler",              "Dynamics",       0xff181b22u, 0xff7e96adu, false },
            { SlotModuleId::DistressorComp,     "Stress-VCA",        "Stress-VCA",                   "Dynamics",       0xff181b22u, 0xffc93030u, false },
            { SlotModuleId::FairchildComp,      "Vari-Mu 670",       "Vari-Mu 670",                  "Dynamics",       0xff181b22u, 0xffc59b38u, false },

            // Gamme Moderne Kalyx Audio (2U Double-Largeur & 1U)
            { SlotModuleId::SurgicalEQ2U,       "Spectral Split-EQ", "Spectral Split-EQ (2U)",       "Equalizers",     0xff181b22u, 0xff2348b8u, false },
            { SlotModuleId::PrecisionDyn2U,     "Kinetic Dynamics",  "Kinetic Dynamics (2U)",        "Dynamics",       0xff181b22u, 0xff2348b8u, false },
            { SlotModuleId::HarmonicSculptor,   "Harmonic Matrix",   "Harmonic Matrix (1U)",         "Color & Preamp", 0xff181b22u, 0xff5b2344u, false },
            { SlotModuleId::StereoPhase,        "Spatial Vector",    "Spatial Vector (1U)",          "Utility",        0xff181b22u, 0xff2348b8u, false }
        }};
        return kModules;
    }

    [[nodiscard]] static const SlotModuleDescriptor& getById(int id) noexcept
    {
        const auto& all = getAll();
        const int idx = std::clamp(id, 0, static_cast<int>(SlotModuleId::Count) - 1);
        return all[static_cast<std::size_t>(idx)];
    }

    [[nodiscard]] static juce::StringArray getDisplayNames()
    {
        juce::StringArray arr;
        for (const auto& d : getAll())
            arr.add(d.displayName);
        return arr;
    }

    [[nodiscard]] static SlotParamArray getDefaultParamsForModule(SlotModuleId id) noexcept
    {
        SlotParamArray p {};
        p.fill(0.5f);

        switch (id)
        {
            case SlotModuleId::SSL4000Preamp:
            case SlotModuleId::API2520Preamp:
            case SlotModuleId::Neve1073Preamp:
            case SlotModuleId::KalyxCleanPreamp:
                p[0] = 1.0f; // PRE IN
                p[1] = unmapLin(0.0f, -24.0f, 24.0f);   // GAIN
                p[2] = 0.0f;                             // DRIVE (0%)
                p[3] = 0.0f;                             // PHASE
                p[4] = 0.0f;                             // HPF IN
                p[5] = unmapLog(40.0f, 20.0f, 500.0f);   // HPF FREQ
                p[6] = 0.0f;                             // LPF IN
                p[7] = unmapLog(18000.0f, 2000.0f, 20000.0f); // LPF FREQ
                p[8] = 0.0f;                             // SSL E (0) / G (1)
                break;

            case SlotModuleId::UtilityGateDeEsser:
                p[0] = 1.0f; // Gate ON
                p[1] = unmapLin(-55.0f, -70.0f, 0.0f);       // Gate THR
                p[2] = 0.0f;                                  // Gate RANGE (0 dB)
                p[3] = unmapLin(40.0f, 5.0f, 300.0f);         // Gate HOLD
                p[4] = unmapLin(120.0f, 20.0f, 800.0f);       // Gate REL
                p[5] = 0.0f;                                  // De-Esser ON
                p[6] = unmapLog(6500.0f, 3000.0f, 12000.0f);  // DS FREQ
                p[7] = unmapLin(-18.0f, -40.0f, 0.0f);        // DS THR
                p[8] = unmapLin(10.0f, 0.0f, 18.0f);          // DS RANGE
                break;

            case SlotModuleId::SSL4000Comp:
                p[0]  = 1.0f; // IN
                p[1]  = unmapLin(-14.0f, -40.0f, 0.0f);   // THR
                p[2]  = unmapLog(4.0f, 1.0f, 20.0f);      // RATIO
                p[3]  = unmapLog(10.0f, 0.5f, 50.0f);     // ATTACK
                p[4]  = unmapLog(120.0f, 20.0f, 1000.0f); // RELEASE
                p[5]  = 0.0f;                             // AUTO-REL (RC)
                p[6]  = unmapLin(0.0f, -12.0f, 24.0f);    // MAKEUP
                p[7]  = 1.0f;                             // BLEND (100%)
                p[8]  = choiceToNorm(1, 5);               // ROUTING (STEREO)
                p[9]  = 0.0f;                             // SC HPF (20 Hz)
                p[10] = 0.0f;                             // SSL E (0) / G (1)
                break;

            case SlotModuleId::API2500Comp:
                p[0]  = 1.0f; // IN
                p[1]  = unmapLin(-14.0f, -40.0f, 0.0f);   // THR
                p[2]  = unmapLog(4.0f, 1.5f, 20.0f);      // RATIO
                p[3]  = unmapLog(10.0f, 0.3f, 30.0f);     // ATTACK
                p[4]  = unmapLog(150.0f, 50.0f, 1500.0f); // RELEASE
                p[5]  = choiceToNorm(1, 3);               // KNEE (MED)
                p[6]  = choiceToNorm(1, 3);               // THRUST (MED)
                p[7]  = 0.0f;                             // TYPE (NEW FF = 0, OLD FB = 1)
                p[8]  = 1.0f;                             // L/R LINK (100%)
                p[9]  = choiceToNorm(0, 3);               // LINK FLT (HP / BP / LP)
                p[10] = unmapLin(0.0f, -12.0f, 24.0f);    // MAKEUP
                p[11] = 1.0f;                             // BLEND (100%)
                p[12] = choiceToNorm(1, 5);               // ROUTING (STEREO)
                p[13] = 1.0f;                             // REL TYPE (0 = FIXED, 1 = VAR)
                break;

            case SlotModuleId::Neve5254Comp:
                p[0] = 1.0f; // IN
                p[1] = unmapLin(-14.0f, -40.0f, 0.0f);   // THRESHOLD
                p[2] = choiceToNorm(2, 6);               // RATIO (0=1.5, 1=2, 2=3, 3=4, 4=6, 5=8)
                p[3] = choiceToNorm(2, 6);               // TIMING (0=FAST..5=AUTO)
                p[4] = 0.0f;                             // FAST (+30%)
                p[5] = 0.0f;                             // S/C HPF
                p[6] = unmapLin(0.0f, -12.0f, 24.0f);    // GAIN
                p[7] = 1.0f;                             // BLEND (100%)
                p[8] = choiceToNorm(1, 5);               // ROUTING (STEREO)
                p[9] = 1.0f;                             // LINK (1 = Stereo Linked, 0 = Dual)
                p[10] = 1.0f;                            // VINTAGE (1 = Stepped Ratio/Timing, 0 = Modern Continuous)
                break;

            case SlotModuleId::SSL4000EQ:
            case SlotModuleId::API550BEQ:
                p[0]  = 1.0f; // EQ IN
                p[1]  = unmapLog(80.0f, 30.0f, 450.0f);       // LF FREQ
                p[2]  = 0.5f;                                 // LF GAIN (0 dB)
                p[3]  = unmapLog(450.0f, 200.0f, 2500.0f);    // LMF / MF FREQ
                p[4]  = 0.5f;                                 // LMF / MF GAIN (0 dB)
                p[5]  = unmapLog(1.2f, 0.4f, 4.0f);           // LMF / MF Q
                p[6]  = unmapLog(2500.0f, 600.0f, 7000.0f);   // HMF FREQ (SSL only)
                p[7]  = 0.5f;                                 // HMF GAIN (SSL only)
                p[8]  = unmapLog(1.2f, 0.4f, 4.0f);           // HMF Q (SSL only)
                p[9]  = unmapLog(10000.0f, 1500.0f, 18000.0f);// HF FREQ
                p[10] = 0.5f;                                 // HF GAIN (0 dB)
                p[11] = 0.5f;                                 // EQ TRIM (0 dB)
                p[12] = 0.0f; // LF BELL
                p[13] = 0.0f; // HF BELL
                p[14] = (id == SlotModuleId::API550BEQ) ? 1.0f : 0.0f; // VINTAGE/STEPPED (API) / E-G (SSL)
                p[15] = 0.0f; // SSL VINTAGE (1 = Stepped, 0 = Modern Continuous)
                break;

            case SlotModuleId::Neve1073EQ:
                p[0] = 1.0f; // EQ IN
                p[1] = unmapLog(80.0f, 35.0f, 220.0f);    // LOW FREQ
                p[2] = 0.5f;                              // LOW GAIN (0 dB)
                p[3] = unmapLog(700.0f, 360.0f, 7200.0f); // MID FREQ
                p[4] = 0.5f;                              // MID GAIN (0 dB)
                p[5] = unmapLog(1.0f, 0.4f, 3.5f);        // MID Q
                p[6] = 0.5f;                              // HIGH 12k GAIN (0 dB)
                p[7] = 0.0f;                              // IND HPF IN
                p[8] = unmapLog(80.0f, 35.0f, 360.0f);    // IND HPF FREQ
                p[9] = 0.5f;                              // EQ TRIM (0 dB)
                p[10] = 1.0f;                             // VINTAGE (1 = Stepped Freqs, 0 = Modern Continuous)
                break;

            case SlotModuleId::ConsoleColor:
                p[0] = 1.0f;               // COLOR IN
                p[1] = choiceToNorm(0, 5); // CIRCUIT (SSL 4000 E)
                p[2] = 0.25f;              // DRIVE (25%)
                p[3] = 0.5f;               // SAT TONE (0%)
                p[4] = 0.0f;               // CROSSTALK (0%)
                p[5] = 0.5f;               // COLOR TRIM (0 dB)
                break;

            case SlotModuleId::PrecisionLimiter:
                p[0] = 0.0f;                            // LIMIT IN
                p[1] = unmapLin(-3.0f, -30.0f, 0.0f);   // LIMIT THR (-3 dB)
                p[2] = unmapLog(60.0f, 10.0f, 500.0f);  // LIMIT REL (60 ms)
                p[3] = 1.0f;                            // AUTO REL
                p[4] = unmapLin(-0.3f, -24.0f, 0.0f);   // CEILING (-0.3 dBFS)
                p[5] = 1.0f;                            // BLEND (100%)
                break;

            case SlotModuleId::UA1176Comp:
                p[0]  = 1.0f;                                                    // 1176 IN
                p[1]  = choiceToNorm(1, 3);                                      // REVISION (Rev D Blackface)
                p[2]  = unmapLin(0.0f, -24.0f, 24.0f);                           // INPUT (0.0 dB)
                p[3]  = unmapLin(0.0f, -24.0f, 24.0f);                           // OUTPUT (0.0 dB)
                p[4]  = std::log(250.0f / 800.0f) / std::log(20.0f / 800.0f);    // ATTACK (250 us, CW = faster)
                p[5]  = std::log(250.0f / 1100.0f) / std::log(50.0f / 1100.0f);  // RELEASE (250 ms, CW = faster)
                p[6]  = 0.5f;                                                    // FET BIAS (50 %)
                p[7]  = 1.0f;                                                    // BLEND (100 %)
                p[8]  = choiceToNorm(0, 4);                                      // RATIO (4:1)
                p[9]  = 0.0f;                                                    // ALL-BUTTONS IN (SLAM)
                p[10] = 0.0f;                                                    // SUPER SLOW ATTACK (SLOW)
                p[11] = choiceToNorm(1, 5);                                      // ROUTING (STEREO)
                break;

            case SlotModuleId::LA2AComp:
                p[0] = 1.0f;                                                     // LA-2A IN
                p[1] = 0.0f;                                                     // COMP (0) / LIMIT (1)
                p[2] = unmapLin(0.0f, -24.0f, 24.0f);                            // GAIN (0.0 dB)
                p[3] = 0.45f;                                                    // PEAK REDUCTION (45 %)
                p[4] = 0.25f;                                                    // R37 EMPHASIS (25 %)
                p[5] = 0.50f;                                                    // CELL AGE (50 %)
                p[6] = 1.0f;                                                     // BLEND (100 %)
                p[7] = choiceToNorm(1, 5);                                       // ROUTING (STEREO)
                break;

            case SlotModuleId::DistressorComp:
                p[0]  = 1.0f;                                                    // DIST IN
                p[1]  = unmapLin(0.0f, -24.0f, 24.0f);                           // INPUT (0.0 dB)
                p[2]  = unmapLin(0.0f, -24.0f, 24.0f);                           // OUTPUT (0.0 dB)
                p[3]  = unmapLog(2.5f, 0.05f, 30.0f);                            // ATTACK (2.5 ms)
                p[4]  = unmapLog(250.0f, 50.0f, 3500.0f);                        // RELEASE (250 ms)
                p[5]  = choiceToNorm(4, 8);                                      // RATIO (6:1)
                p[6]  = 0.0f;                                                    // BRITISH MODE
                p[7]  = 0.0f;                                                    // AUDIO HPF (80 Hz)
                p[8]  = 0.0f;                                                    // DET HPF (80 Hz)
                p[9]  = 0.0f;                                                    // BAND EMPH (6 kHz)
                p[10] = choiceToNorm(0, 3);                                      // DIST MODE (CLEAN)
                p[11] = 1.0f;                                                    // BLEND (100 %)
                p[12] = choiceToNorm(1, 5);                                      // ROUTING (STEREO)
                break;

            case SlotModuleId::FairchildComp:
                p[0]  = 1.0f;                                                    // 670 IN
                p[1]  = unmapLin(0.0f, -24.0f, 24.0f);                           // INPUT GAIN (0.0 dB)
                p[2]  = 0.45f;                                                   // AC THRESHOLD (45 %)
                p[3]  = choiceToNorm(1, 6);                                      // TIME CONST (TC 2)
                p[4]  = unmapLin(0.0f, -24.0f, 24.0f);                           // OUTPUT (0.0 dB)
                p[5]  = 1.0f;                                                    // MATCHING L/R (100 %)
                p[6]  = 1.0f;                                                    // MATCHING P/P (100 %)
                p[7]  = 1.0f;                                                    // BLEND (100 %)
                p[8]  = choiceToNorm(3, 7);                                      // NOS TUBE (RCA 6386 Black Plate)
                p[9]  = choiceToNorm(1, 5);                                      // ROUTING (STEREO)
                p[10] = 1.0f;                                                    // VINTAGE (1 = Stepped TC 1-6, 0 = Modern)
                break;

            case SlotModuleId::SurgicalEQ2U:
            case SlotModuleId::PrecisionDyn2U:
            {
                const auto p2u = getDefaultParams2UForModule(id);
                for (std::size_t i = 0; i < kParamsPerSlot; ++i)
                    p[i] = p2u[i];
                break;
            }

            case SlotModuleId::HarmonicSculptor:
                p[0] = 1.0f;                                   // IN
                p[1] = 0.0f;                                   // TILT LNK (Off)
                p[2] = 0.35f;                                  // EVEN 2nd (35%)
                p[3] = 0.20f;                                  // ODD 3rd (20%)
                p[4] = unmapLin(0.0f, -6.0f, 6.0f);            // TILT EQ (0.0 dB)
                p[5] = 0.5f;                                   // DYNAMICS (0%)
                p[6] = unmapLin(0.0f, -12.0f, 24.0f);          // DRIVE (0.0 dB)
                p[7] = 1.0f;                                   // BLEND (100%)
                p[8] = 0.0f;                                   // BAND (FULL)
                p[9] = 0.0f;                                   // ROUTING (STEREO)
                break;

            case SlotModuleId::StereoPhase:
                p[0]  = 1.0f;                                  // IN
                p[1]  = 0.0f;                                  // MODE (L/R)
                p[2]  = 0.5f;                                  // STEREO WIDTH (100%)
                p[3]  = unmapLog(20.0f, 20.0f, 500.0f);        // MONO MAKER (20 Hz)
                p[4]  = 0.5f;                                  // PHASE ROTATE (0 deg)
                p[5]  = 0.5f;                                  // SIDE TILT (0.0 dB)
                p[6]  = 0.5f;                                  // MID GAIN (0.0 dB)
                p[7]  = 0.5f;                                  // SIDE GAIN (0.0 dB)
                p[8]  = 0.0f;                                  // Ø L (Off)
                p[9]  = 0.0f;                                  // Ø R (Off)
                p[10] = 0.0f;                                  // L<>R (Off)
                p[11] = 0.0f;                                  // MONITOR (STEREO)
                break;

            case SlotModuleId::EmptyBypass:
            case SlotModuleId::Count:
            default:
                break;
        }
        return p;
    }

    static SlotParamArray2U getDefaultParams2UForModule(SlotModuleId id) noexcept
    {
        SlotParamArray2U p {};
        p.fill(0.0f);
        if (id == SlotModuleId::SurgicalEQ2U)
        {
            p[0] = 1.0f; // EQ IN
            p[1] = 0.0f; // PHASE MODE (0 = ZERO LAT, 1 = NATURAL)
            p[2] = 1.0f; // FFT MODE (0 = PRE, 1 = POST)
            p[3] = 0.0f; // SOLO BAND (0 = Off, 1..6 = Band 1..6)
            p[4] = 0.0f; // ACTIVE BAND (0..5)
            p[5] = 0.5f; // OUT TRIM (0.0 dB)

            static constexpr float kDefFreqs[6] = { 60.0f, 220.0f, 1000.0f, 3500.0f, 8000.0f, 14000.0f };
            for (int b = 0; b < 6; ++b)
            {
                const int base = 6 + b * 5;
                p[static_cast<std::size_t>(base + 0)] = unmapLog(kDefFreqs[b], 20.0f, 20000.0f); // FREQ
                p[static_cast<std::size_t>(base + 1)] = 0.5f;                                    // TONAL GAIN (0.0 dB)
                p[static_cast<std::size_t>(base + 2)] = 0.5f;                                    // TRANS GAIN (0.0 dB)
                p[static_cast<std::size_t>(base + 3)] = unmapLog(1.0f, 0.2f, 18.0f);             // Q (1.0)
                p[static_cast<std::size_t>(base + 4)] = 0.5f;                                    // DYN RANGE (0.0 dB)
            }
        }
        else if (id == SlotModuleId::PrecisionDyn2U)
        {
            p[0]  = 1.0f;                                       // IN
            p[1]  = 0.0f;                                       // ORDER (PRE-TRANS)
            p[2]  = 0.4f;                                       // LOOKAHEAD (2.0 ms)
            p[3]  = 0.0f;                                       // AUTO REL (Off)
            p[4]  = unmapLin(-16.0f, -50.0f, 0.0f);             // THRESH (-16.0 dB)
            p[5]  = 0.45f;                                      // RATIO (~3.2:1)
            p[6]  = unmapLin(8.0f, 0.0f, 24.0f);                // KNEE (8.0 dB)
            p[7]  = unmapLog(4.5f, 0.01f, 250.0f);              // ATTACK (4.5 ms)
            p[8]  = unmapLog(140.0f, 5.0f, 2500.0f);            // RELEASE (140 ms)
            p[9]  = unmapLin(0.0f, -12.0f, 24.0f);              // MAKEUP (0.0 dB)
            p[10] = 0.5f;                                       // PUNCH (0%)
            p[11] = 0.5f;                                       // SUSTAIN (0%)
            p[12] = unmapLog(250.0f, 100.0f, 8000.0f);          // TRANS FRQ (250 Hz)
            p[13] = unmapLog(60.0f, 20.0f, 500.0f);             // SC HPF (60 Hz)
            p[14] = 1.0f;                                       // BLEND (100%)
            p[15] = 0.0f;                                       // ROUTING (STEREO)
        }
        return p;
    }
};

} // namespace kalyx::core
