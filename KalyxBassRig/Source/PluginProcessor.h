#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Core/BassRigParams.h"
#include "Core/PresetManager.h"
#include "DSP/TubeAmpModels.h"
#include "DSP/DIPathDSP.h"
#include "DSP/DualCabDSP.h"
#include "DSP/MasterAndTunerDSP.h"
#include "FX/PedalboardEngine.h"
#include <atomic>
#include <vector>

class KalyxBassRigProcessor final : public juce::AudioProcessor
{
public:
    KalyxBassRigProcessor();
    ~KalyxBassRigProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Kalyx Bass Rig"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts_; }
    kalyx::bass::core::PresetManager& getPresetManager() noexcept { return presetManager_; }
    kalyx::bass::dsp::DualCabEngine& getDualCab() noexcept { return dualCab_; }

    // Telemetry getters (lock-free)
    [[nodiscard]] float getVuInDb()    const noexcept { return vuInDb_.load(std::memory_order_relaxed); }
    [[nodiscard]] float getVuOutDb()   const noexcept { return vuOutDb_.load(std::memory_order_relaxed); }
    [[nodiscard]] float getVuGrDb()    const noexcept { return vuGrDb_.load(std::memory_order_relaxed); }
    [[nodiscard]] float getPhaseCorr() const noexcept { return phaseCorr_.load(std::memory_order_relaxed); }
    [[nodiscard]] float getAmpThd()    const noexcept { return ampEngine_.getThdHeatmap(); }
    [[nodiscard]] float getAmpSag()    const noexcept { return ampEngine_.getSagVoltsDrop(); }
    [[nodiscard]] float getDIGrDb()    const noexcept { return diEngine_.getOptoGrDb(); }
    [[nodiscard]] float getLimGrDb()   const noexcept { return limiter_.getGrDb(); }
    [[nodiscard]] float getPedalCompInDb() const noexcept { return pedalboard_.getCompInDb(); }
    [[nodiscard]] float getPedalCompGrDb() const noexcept { return pedalboard_.getCompGrDb(); }
    [[nodiscard]] float getGateGain()  const noexcept { return gate_.getGateGain(); }
    [[nodiscard]] float getKickDuck()  const noexcept { return kickRelief_.getDuckDb(); }

    [[nodiscard]] float getTunerFreq() const noexcept { return tuner_.getFrequencyHz(); }
    [[nodiscard]] int   getTunerMidi() const noexcept { return tuner_.getMidiNote(); }
    [[nodiscard]] float getTunerCents() const noexcept { return tuner_.getCents(); }
    [[nodiscard]] float getTunerConf() const noexcept { return tuner_.getConfidence(); }

    static constexpr int kFFTSize = 2048;

    [[nodiscard]] kalyx::bass::dsp::AmpControlState getAmpSnapshot() const noexcept;
    [[nodiscard]] double getCurrentSampleRate() const noexcept { return hostSampleRate_; }
    void copyPostAmpFFT(float* dest2048) noexcept;

    void triggerIRReload(int cabSlot, int irIndex);
    void loadCustomIR(int cabSlot, const juce::File& file);

private:
    juce::AudioProcessorValueTreeState apvts_;
    kalyx::bass::core::PresetManager presetManager_;

    kalyx::bass::dsp::NoiseGate64      gate_;
    kalyx::bass::dsp::StrobeTuner64    tuner_;
    kalyx::bass::fx::PedalboardEngine  pedalboard_;
    kalyx::bass::dsp::TubeAmpEngine    ampEngine_;
    kalyx::bass::dsp::DIPathEngine     diEngine_;
    kalyx::bass::dsp::DualCabEngine    dualCab_;
    kalyx::bass::dsp::SmartKickRelief64 kickRelief_;
    kalyx::bass::dsp::BrickwallLimiter64 limiter_;

    std::array<float, kFFTSize> fftRing_ {};
    std::atomic<int> fftWriteIdx_ { 0 };

    // Pre-allocated work buffers (zero allocation in processBlock)
    std::vector<double> preMonoBuf_;
    std::vector<float>  ampMonoBuf_;
    std::vector<float>  cabOutL_;
    std::vector<float>  cabOutR_;
    std::vector<float>  diMonoBuf_;
    std::vector<double> postBufL_;
    std::vector<double> postBufR_;

    double hostSampleRate_ { 48000.0 };
    double vuEnvIn_  { 0.0 };
    double vuEnvOut_ { 0.0 };
    double corrNum_  { 0.0 };
    double corrDenA_ { 1.0e-12 };
    double corrDenB_ { 1.0e-12 };

    std::atomic<float> vuInDb_    { -60.0f };
    std::atomic<float> vuOutDb_   { -60.0f };
    std::atomic<float> vuGrDb_    { 0.0f };
    std::atomic<float> phaseCorr_ { 1.0f };

    std::atomic<int> pendingIrA_ { -1 };
    std::atomic<int> pendingIrB_ { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KalyxBassRigProcessor)
};
