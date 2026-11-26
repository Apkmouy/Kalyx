#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "UI/KalyxLookAndFeel.h"
#include "UI/DataWidgets.h"
#include "UI/ModuleSlotView.h"
#include "UI/AnalogVUMeter.h"
#include <array>
#include <memory>

/**
 * Éditeur principal « Châssis Studio 5-Slots + Entrée / Master » de Kalyx Hybrid Console v3 :
 *   [ INPUT (Fixe) | SLOT A | SLOT B | SLOT C | SLOT D | SLOT E | MASTER (Fixe) ]
 * - Rendu vectoriel logiciel pur juce::Graphics (Zéro OpenGL, Zéro WebView)
 * - VU-mètre analogique compact 300 ms IEC 60268-17 (IN / OUT / GR) dans MASTER
 * - Gestionnaire de presets classé par catégories + SAVE / SAVE AS / RENAME / DELETE
 * - Finitions de châssis externe : NIGHT / DAY (#DCDFE4) / KALYX HERITAGE (#143832 & #C88A9E).
 */
class KalyxHybridConsoleEditor final : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit KalyxHybridConsoleEditor(KalyxHybridConsoleProcessor&);
    ~KalyxHybridConsoleEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    void applyChassisTheme(kalyx::core::ChassisTheme theme);
    void applyInputPreampTheme(int preampModel);
    void updateSlotOrderVisuals();
    void showCategorizedPresetMenu();
    void promptSavePresetAs();
    void promptRenamePreset();
    void promptDeletePreset();
    void refreshPresetButtonText();
    void drawLockIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool isLocked) const;

    KalyxHybridConsoleProcessor& processorRef_;
    KalyxLookAndFeel lookAndFeel_;
    KalyxSettingsOverlayComponent settingsOverlay_;

    // Bandeau supérieur (Header) : Presets catégorisés + 4 actions + A/B + M/S + Finition + Settings
    juce::TextButton presetMenuBtn_  { "Init (5-Slot Studio)" };
    juce::TextButton saveBtn_        { "SAVE" };
    juce::TextButton saveAsBtn_      { "SAVE AS" };
    juce::TextButton renameBtn_      { "REN" };
    juce::TextButton deleteBtn_      { "DEL" };
    juce::TextButton abToggleBtn_    { "A/B:A" };
    juce::TextButton abCopyBtn_      { "COPY" };
    juce::ComboBox   msModeBox_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> msModeAtt_;
    juce::ComboBox   chassisThemeBox_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> chassisThemeAtt_;
    juce::TextButton settingsBtn_    { "SETTINGS" };

    // Tranche Fixe 1 : INPUT (adopte la finition historique du préampli actif)
    juce::ComboBox   inPreampBox_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> inPreampAtt_;
    TactileFaderBar  inTrimBar_, inDriveBar_;
    InputFilterGraph inFilterGraph_;
    juce::ToggleButton inPhaseBtn_, inFiltToScBtn_, inExtScBtn_, inScListenBtn_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> inPhaseAtt_, inFiltToScAtt_, inExtScAtt_, inScListenAtt_;
    juce::Rectangle<float> inputPanelRect_, inputLockRect_, inLedsRect_;

    // 5 Emplacements Série 500 Interchangeables (Slot A..E)
    std::array<std::unique_ptr<ModuleSlotView>, 5> slotViews_;

    // Tranche Fixe 7 : MASTER (VU-mètre 300 ms + sélecteur segmenté [IN|OUT|GR] + Tol, Mix, Out Fader)
    AnalogVUMeter vuMeter_;
    std::unique_ptr<KalyxSegmentedPill> vuModePill_;
    TactileFaderBar analogTolBar_, mixBar_;
    VerticalConsoleFader outFader_;
    juce::Rectangle<float> masterPanelRect_, masterLockRect_;

    kalyx::dsp::StudioMeterSnapshot meterSnap_ {};
    kalyx::core::ChassisTheme lastTheme_ { kalyx::core::ChassisTheme::Night };
    std::array<int, 5> lastSlotOrder_ {{ 0, 1, 2, 3, 4 }};
    float uiScale_          { 1.0f };
    int  lastVuMode_        { 1 };
    int  lastInPreampModel_ { -1 };
    bool inSigLit_          { false };
    bool inPeakLit_         { false };
    juce::String infoBarText_ { "Shift/Ctrl + Drag: Fine Mode (0.1x) | Double-Click: Reset | Click Lock: Protect Strip" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KalyxHybridConsoleEditor)
};
