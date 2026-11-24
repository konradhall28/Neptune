#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include "StyleEngine.h"
#include "Crossover.h"

namespace nep
{
constexpr int kNumStyles = 28;
constexpr int kBandParams = 12;              // modulatable per-band parameters
constexpr int kNumTargets = kMaxBands * kBandParams + (kMaxBands - 1) + 3;
constexpr int kNumSlots = 12;
constexpr int kNumSources = 8;              // None, LFO 1, LFO 2, EF, EG, XY X, XY Y, MIDI CC1
constexpr int kSub = 32;                     // modulation / control rate block (samples)

const juce::StringArray& styleNames();
const juce::StringArray& styleGroups();     // group of each style (Tube, Tape, Amp, Saturation, Transformer, FX)
const juce::StringArray& sourceNames();
juce::String targetName (int t);
juce::String targetParamID (int t);
juce::String bandID (int band, const char* name);
}

class NeptuneProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    NeptuneProcessor();
    ~NeptuneProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Neptune MB"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- editor API ----------------------------------------------------------
    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;
    float param (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    void  setParam (const juce::String& id, float plain);
    int   numBands() const { return (int) param ("bands"); }
    void  splitBandAt (double freq);            // the '+' button in the display
    void  removeBand (int band);
    double crossover (int k) const;             // effective (sorted, clamped) crossover, Hz
    void  copyBand (int from, int to);

    int   addModulation (int source, int target, float amount);   // returns slot or -1
    void  loadFactoryPreset (int index);
    static juce::StringArray factoryPresetNames();

    int  getSlot() const { return abSlot; }
    void selectSlot (int s);
    void copyToOtherSlot();

    // UI feedback (audio thread -> UI)
    std::atomic<float> modNorm[nep::kNumTargets];     // final normalised value of each modulation target
    std::atomic<float> srcValue[nep::kNumSources];    // current value of each source
    std::atomic<float> bandPeak[nep::kMaxBands];
    std::atomic<float> inPeak { 0 }, outPeak { 0 };
    // analyzer FIFO
    static constexpr int kFifo = 1 << 13;
    float fifo[kFifo] {};
    std::atomic<int> fifoWrite { 0 };
    double getFs() const { return fs; }
    void applyLinearPhaseIRs();     // (re)designs the linear-phase crossover filters
    int  getOsLatency() const { return osLatency; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override;
    void rebuildOversampling();
    void updateLatency();
    void computeModulation (int numSamples, juce::AudioPlayHead::PositionInfo* pos, double ppqStart, bool playing, float inLevel);
    double fs = 48000.0;
    int maxBlock = 512;

    struct Band
    {
        nep::StyleChannel style[2];
        int styleIndex = -1;
        std::vector<float> fbBuf[2];
        int fbIdx = 0;
        double fbLp[2] {}, fbHp[2] {};
        float fbScale = 1.0f;
        nep::Biquad tone[2][4];
        float lastTone[4] { -999, -999, -999, -999 };
        double env = 0, envDb = -120;
        juce::AudioBuffer<float> buf, dry;
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 1 << 16 };
        juce::SmoothedValue<float> gainL, gainR, mixS, enabledS;
    } bands[nep::kMaxBands];

    // oversamplers per band: [band][0 = IIR, 1 = linear-phase FIR][0 = 8x, 1 = 32x]
    std::unique_ptr<juce::dsp::Oversampling<float>> os[nep::kMaxBands][2][2];
    int hqMode = -1, lpMode = -1;
    int osLatency = 0, lpLatency = 0, totalLatency = 0;

    nep::MinPhaseSplitter splitter;
    // linear phase
    int firLen = 4097;
    std::unique_ptr<juce::dsp::Convolution> lpConv[nep::kMaxBands - 1];
    double lpFreqLoaded[nep::kMaxBands - 1] {};
    int lpSlopeLoaded[nep::kMaxBands - 1] {};
    std::atomic<bool> lpDirty { true };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> lpDelay { 1 << 15 };
    juce::AudioBuffer<float> lpBufs[nep::kMaxBands - 1];

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 1 << 16 };
    juce::AudioBuffer<float> dryBuf, work;
    juce::SmoothedValue<float> inG[2], outG[2], gMix, bypassS;

    // modulation sources
    double lfoPhase[2] {}, lfoHeld[2] {}, lfoPrev[2] {}, lfoNext[2] {};
    double efEnv = 0, egLevel = 0; int egStage = 0; bool egGateWasOpen = false; int midiNotes = 0; float midiCC1 = 0;
    juce::Random rng { 42 };
    float modValues[nep::kNumTargets] {};          // plain values for this sub-block
    std::atomic<float>* raw[nep::kNumTargets] {};
    juce::RangedAudioParameter* tparam[nep::kNumTargets] {};
    float silentTime = 0;
    double ppqPerSample = 2.0 / 48000.0;
    std::vector<float> subMods;

    juce::ValueTree abSlots[2];
    int abSlot = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeptuneProcessor)
};
