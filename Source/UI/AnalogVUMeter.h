#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../DSPShared/StereoAndMeters.h"
#include "../Core/ConsoleModuleBase.h"

/**
 * VU-mètre Stéréo Analogique Compact rétroéclairé (IEC 60268-17, 300 ms RMS)
 * pour la colonne MASTER (façon ZenoMOD / API 2500 / Shelford) :
 * - Mode 0 (IN)  : niveau d'entrée stéréo calibré sur 0 VU = vuRefDb (-18 dBFS par défaut)
 * - Mode 1 (OUT) : niveau de sortie stéréo calibré sur 0 VU
 * - Mode 2 (GR)  : réduction de gain cumulée des modules actifs (aiguille partant de 0 VU vers la gauche).
 */
class AnalogVUMeter final : public juce::Component,
                            public juce::SettableTooltipClient
{
public:
    AnalogVUMeter() noexcept = default;

    void setChassisTheme(kalyx::core::ChassisTheme theme) noexcept { theme_ = theme; }
    void setUiScale(float s) noexcept { uiScale_ = std::clamp(s, 0.75f, 2.20f); }
    void updateTelemetry(const kalyx::dsp::StudioMeterSnapshot& snap, int vuMode, float vuRefDb) noexcept;

    void paint(juce::Graphics& g) override;

private:
    [[nodiscard]] static float vuDbToAngle(float vuDb) noexcept;

    kalyx::core::ChassisTheme theme_ { kalyx::core::ChassisTheme::Night };
    float uiScale_  { 1.0f };
    int   vuMode_   { 1 }; // 0=IN, 1=OUT, 2=GR
    float vuRefDb_  { 0.0f };
    float needleL_  { -20.0f };
    float needleR_  { -20.0f };
    float peakDb_   { -100.0f };
    float rawGrDb_  { 0.0f };
};
