#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "StyleTable.h"

using namespace nep;

// ============================================================================ names
const juce::StringArray& nep::styleNames()
{
    static juce::StringArray s { "Subtle Tube", "Clean Tube", "Warm Tube", "Broken Tube",
        "Subtle Tape", "Clean Tape", "Warm Tape", "Old Tape",
        "American Tweed Amp", "American Plexi Amp", "British Rock Amp", "British Pop Amp",
        "Smooth Amp", "Crunchy Amp", "Lead Amp", "Screaming Amp", "Power Amp",
        "Subtle Saturation", "Gentle Saturation", "Heavy Saturation",
        "Subtle Transformer", "Gentle Transformer", "Warm Transformer",
        "Smudge", "Breakdown", "Foldback", "Rectify", "Destroy" };
    return s;
}
const juce::StringArray& nep::styleGroups()
{
    static juce::StringArray g { "Tube", "Tube", "Tube", "Tube", "Tape", "Tape", "Tape", "Tape",
        "Amp", "Amp", "Amp", "Amp", "Amp", "Amp", "Amp", "Amp", "Amp",
        "Saturation", "Saturation", "Saturation", "Transformer", "Transformer", "Transformer",
        "FX", "FX", "FX", "FX", "FX" };
    return g;
}
const juce::StringArray& nep::sourceNames()
{
    static juce::StringArray s { "None", "XLFO 1", "XLFO 2", "Env Follower", "Envelope Gen", "XY  X", "XY  Y", "MIDI CC 1" };
    return s;
}
static const char* kBandKeys[kBandParams] = { "drive", "mix", "fb", "fbfreq", "dyn", "bass", "mid", "treble", "pres", "level", "dpan", "lpan" };
static const char* kBandNames[kBandParams] = { "Drive", "Mix", "Feedback", "FB Freq", "Dynamics", "Bass", "Mid", "Treble", "Presence", "Level", "Drive Pan", "Level Pan" };
juce::String nep::bandID (int band, const char* name) { return "b" + juce::String (band + 1) + "_" + name; }
juce::String nep::targetParamID (int t)
{
    if (t < kMaxBands * kBandParams) return bandID (t / kBandParams, kBandKeys[t % kBandParams]);
    t -= kMaxBands * kBandParams;
    if (t < kMaxBands - 1) return "x" + juce::String (t + 1);
    t -= kMaxBands - 1;
    return t == 0 ? "input" : t == 1 ? "output" : "gmix";
}
juce::String nep::targetName (int t)
{
    if (t < kMaxBands * kBandParams) return "Band " + juce::String (t / kBandParams + 1) + " " + kBandNames[t % kBandParams];
    t -= kMaxBands * kBandParams;
    if (t < kMaxBands - 1) return "Crossover " + juce::String (t + 1);
    t -= kMaxBands - 1;
    return t == 0 ? "Input Level" : t == 1 ? "Output Level" : "Global Mix";
}

// ============================================================================ layout
static juce::NormalisableRange<float> logRange (float lo, float hi)
{
    return { lo, hi,
             [] (float s, float e, float v) { return s * std::pow (e / s, v); },
             [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
             [] (float s, float e, float v) { return juce::jlimit (s, e, v); } };
}
static juce::String hzText (float v, int) { return v >= 1000 ? juce::String (v / 1000.0f, v >= 10000 ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; }
static juce::String dbText (float v, int) { return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; }
static juce::String pctText (float v, int) { return juce::String (juce::roundToInt (v)) + "%"; }
static juce::String panText (float v, int) { const int p = juce::roundToInt (v); return p == 0 ? juce::String ("C") : (p < 0 ? juce::String (-p) + "% L" : juce::String (p) + "% R"); }
static juce::String levelText (float v, int) { return v <= -47.9f ? juce::String ("-inf dB") : dbText (v, 0); }

juce::AudioProcessorValueTreeState::ParameterLayout NeptuneProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout L;
    auto F = [&] (const String& id, const String& name, NormalisableRange<float> r, float def, std::function<String (float, int)> txt)
    {
        L.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                   AudioParameterFloatAttributes().withStringFromValueFunction (txt)));
    };
    auto C = [&] (const String& id, const String& name, const StringArray& items, int def)
    { L.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };
    auto B = [&] (const String& id, const String& name, bool def)
    { L.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };

    L.add (std::make_unique<AudioParameterInt> (ParameterID { "bands", 1 }, "Bands", 1, kMaxBands, 1));
    const float xdef[5] = { 150, 700, 2500, 6000, 12000 };
    for (int k = 0; k < kMaxBands - 1; ++k)
    {
        F ("x" + String (k + 1), "Crossover " + String (k + 1), logRange (40, 18000), xdef[k], hzText);
        C ("slope" + String (k + 1), "Slope " + String (k + 1), { "6 dB/oct", "12 dB/oct", "24 dB/oct", "48 dB/oct" }, 2);
    }
    for (int b = 0; b < kMaxBands; ++b)
    {
        const String n = "Band " + String (b + 1) + " ";
        B (bandID (b, "on"), n + "Enabled", true);
        B (bandID (b, "solo"), n + "Solo", false);
        B (bandID (b, "mute"), n + "Mute", false);
        C (bandID (b, "style"), n + "Style", styleNames(), 6);       // Warm Tape
        F (bandID (b, "drive"), n + "Drive", { 0, 100, 0.01f }, 0, pctText);
        F (bandID (b, "mix"), n + "Mix", { 0, 100, 0.01f }, 100, pctText);
        F (bandID (b, "fb"), n + "Feedback", { 0, 100, 0.01f }, 0, pctText);
        F (bandID (b, "fbfreq"), n + "Feedback Freq", logRange (20, 10000), 400, hzText);
        F (bandID (b, "dyn"), n + "Dynamics", { -100, 100, 0.01f }, 0, pctText);
        F (bandID (b, "bass"), n + "Bass", { -24, 24, 0.01f }, 0, dbText);
        F (bandID (b, "mid"), n + "Mid", { -24, 24, 0.01f }, 0, dbText);
        F (bandID (b, "treble"), n + "Treble", { -24, 24, 0.01f }, 0, dbText);
        F (bandID (b, "pres"), n + "Presence", { -24, 24, 0.01f }, 0, dbText);
        F (bandID (b, "level"), n + "Level", { -48, 36, 0.01f }, 0, levelText);
        F (bandID (b, "dpan"), n + "Drive Pan", { -100, 100, 0.01f }, 0, panText);
        F (bandID (b, "lpan"), n + "Level Pan", { -100, 100, 0.01f }, 0, panText);
    }
    F ("input", "Input Level", { -36, 36, 0.01f }, 0, dbText);
    F ("inpan", "Input Pan", { -100, 100, 0.01f }, 0, panText);
    F ("output", "Output Level", { -36, 36, 0.01f }, 0, dbText);
    F ("outpan", "Output Pan", { -100, 100, 0.01f }, 0, panText);
    F ("gmix", "Global Mix", { 0, 100, 0.01f }, 100, pctText);
    C ("hq", "High Quality", { "Off", "Good", "Superb" }, 0);
    B ("linear", "Linear Phase", false);
    C ("chmode", "Channel Mode", { "Stereo", "Mid/Side" }, 0);
    B ("unrestricted", "Unrestricted Feedback", false);
    B ("bypass", "Bypass", false);

    const StringArray shapes { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Random Steps", "Smooth Random" };
    const StringArray syncs { "Free", "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4 T", "1/8 T", "1/16 T" };
    for (int n = 1; n <= 2; ++n)
    {
        F ("lfo" + String (n) + "_rate", "XLFO " + String (n) + " Rate", logRange (0.02f, 40.0f), n == 1 ? 1.0f : 0.25f,
           [] (float v, int) { return String (v, v < 1 ? 2 : 1) + " Hz"; });
        C ("lfo" + String (n) + "_shape", "XLFO " + String (n) + " Shape", shapes, 0);
        C ("lfo" + String (n) + "_sync", "XLFO " + String (n) + " Sync", syncs, 0);
    }
    F ("ef_att", "EF Attack", logRange (0.1f, 500), 5, [] (float v, int) { return String (v, v < 10 ? 1 : 0) + " ms"; });
    F ("ef_rel", "EF Release", logRange (5, 3000), 150, [] (float v, int) { return String (v, 0) + " ms"; });
    F ("ef_gain", "EF Gain", { -24, 24, 0.01f }, 0, dbText);
    C ("eg_trig", "EG Trigger", { "Audio", "MIDI" }, 0);
    F ("eg_thr", "EG Threshold", { -60, 0, 0.01f }, -24, dbText);
    F ("eg_a", "EG Attack", logRange (0.1f, 2000), 5, [] (float v, int) { return String (v, v < 10 ? 1 : 0) + " ms"; });
    F ("eg_d", "EG Decay", logRange (1, 5000), 200, [] (float v, int) { return String (v, 0) + " ms"; });
    F ("eg_s", "EG Sustain", { 0, 100, 0.01f }, 50, pctText);
    F ("eg_r", "EG Release", logRange (1, 5000), 300, [] (float v, int) { return String (v, 0) + " ms"; });
    F ("xy_x", "XY X", { 0, 100, 0.01f }, 50, pctText);
    F ("xy_y", "XY Y", { 0, 100, 0.01f }, 50, pctText);

    StringArray targets { "None" };
    for (int t = 0; t < kNumTargets; ++t) targets.add (targetName (t));
    for (int s = 1; s <= kNumSlots; ++s)
    {
        C ("m" + String (s) + "_src", "Mod " + String (s) + " Source", sourceNames(), 0);
        C ("m" + String (s) + "_dst", "Mod " + String (s) + " Target", targets, 0);
        F ("m" + String (s) + "_amt", "Mod " + String (s) + " Amount", { -100, 100, 0.01f }, 50, pctText);
    }
    return L;
}

// ============================================================================ ctor
NeptuneProcessor::NeptuneProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "NEPTUNE", createLayout())
{
    for (int t = 0; t < kNumTargets; ++t)
    {
        raw[t] = apvts.getRawParameterValue (targetParamID (t));
        tparam[t] = apvts.getParameter (targetParamID (t));
        modNorm[t] = tparam[t]->getValue();
    }
    for (auto& s : srcValue) s = 0;
    for (auto& p : bandPeak) p = 0;
    abSlots[0] = apvts.copyState(); abSlots[1] = apvts.copyState();
    startTimerHz (15);
}
NeptuneProcessor::~NeptuneProcessor() { stopTimer(); }

bool NeptuneProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto& o = l.getMainOutputChannelSet();
    if (o != juce::AudioChannelSet::stereo() && o != juce::AudioChannelSet::mono()) return false;
    return o == l.getMainInputChannelSet();
}

void NeptuneProcessor::setParam (const juce::String& id, float plain)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
        p->endChangeGesture();
    }
}

// ============================================================================ prepare
void NeptuneProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate; maxBlock = juce::jmax (kSub, samplesPerBlock);
    const juce::dsp::ProcessSpec spec { fs, (juce::uint32) maxBlock, 2 };
    const juce::dsp::ProcessSpec subSpec { fs, (juce::uint32) kSub, 2 };

    for (int b = 0; b < kMaxBands; ++b)
    {
        for (int t = 0; t < 2; ++t)
            for (int q = 0; q < 2; ++q)
            {
                os[b][t][q] = std::make_unique<juce::dsp::Oversampling<float>> (2, q == 0 ? 3 : 5,
                    t == 0 ? juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR : juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
                    true, true);
                os[b][t][q]->initProcessing ((size_t) kSub);
            }
        auto& bd = bands[b];
        bd.buf.setSize (2, maxBlock); bd.dry.setSize (2, kSub);
        bd.dryDelay.prepare (subSpec); bd.dryDelay.setMaximumDelayInSamples (1 << 15);
        for (int c = 0; c < 2; ++c) { bd.fbBuf[c].assign ((size_t) (1 << 18), 0.0f); bd.fbLp[c] = bd.fbHp[c] = 0; }
        bd.styleIndex = -1;
        for (auto& v : bd.lastTone) v = -999;
        for (auto* s : { &bd.gainL, &bd.gainR, &bd.mixS, &bd.enabledS }) s->reset (fs, 0.02);
        bd.env = 0; bd.envDb = -120;
    }
    splitter.prepare (fs);
    firLen = (int) (fs * 0.085) | 1;
    for (int k = 0; k < kMaxBands - 1; ++k)
    {
        lpConv[k] = std::make_unique<juce::dsp::Convolution> (juce::dsp::Convolution::NonUniform { 512 });
        lpConv[k]->prepare (spec);
        lpBufs[k].setSize (2, maxBlock);
        lpFreqLoaded[k] = -1; lpSlopeLoaded[k] = -1;
    }
    lpDelay.prepare (spec); lpDelay.setMaximumDelayInSamples (1 << 14);
    dryDelay.prepare (spec); dryDelay.setMaximumDelayInSamples (1 << 15);
    dryBuf.setSize (2, maxBlock); work.setSize (2, maxBlock);
    for (auto* s : { &inG[0], &inG[1], &outG[0], &outG[1], &gMix, &bypassS }) s->reset (fs, 0.02);
    hqMode = lpMode = -1;
    rebuildOversampling();
    applyLinearPhaseIRs();
}

void NeptuneProcessor::rebuildOversampling()
{
    const int hq = (int) param ("hq"), lp = param ("linear") > 0.5f ? 1 : 0;
    if (hq == hqMode && lp == lpMode) return;
    hqMode = hq; lpMode = lp;
    osLatency = hq == 0 ? 0 : (int) std::round (os[0][lp][hq - 1]->getLatencyInSamples());
    const double osFs = fs * (hq == 0 ? 1 : hq == 1 ? 8 : 32);
    for (int b = 0; b < kMaxBands; ++b)
    {
        if (hq > 0) os[b][lp][hq - 1]->reset();
        for (int c = 0; c < 2; ++c) { bands[b].style[c].prepare (osFs); }
        bands[b].styleIndex = -1;
        bands[b].dryDelay.reset();
        bands[b].dryDelay.setDelay ((float) osLatency);
    }
    updateLatency();
}

void NeptuneProcessor::updateLatency()
{
    lpLatency = lpMode == 1 ? (firLen - 1) / 2 : 0;
    totalLatency = osLatency + lpLatency;
    lpDelay.setDelay ((float) lpLatency);
    dryDelay.setDelay ((float) totalLatency);
    setLatencySamples (totalLatency);
}

void NeptuneProcessor::applyLinearPhaseIRs()
{
    const int nb = numBands();
    for (int k = 0; k < nb - 1; ++k)
    {
        const double f = crossover (k);
        const int slope = 6 << (int) param ("slope" + juce::String (k + 1));
        if (lpConv[k] == nullptr) continue;
        if (std::abs (f / juce::jmax (1.0, lpFreqLoaded[k]) - 1.0) < 0.003 && slope == lpSlopeLoaded[k]) continue;
        lpFreqLoaded[k] = f; lpSlopeLoaded[k] = slope;
        std::vector<float> h; designLinearPhaseLP (h, firLen, fs, f, slope);
        juce::AudioBuffer<float> ir (1, firLen);
        std::copy (h.begin(), h.end(), ir.getWritePointer (0));
        lpConv[k]->loadImpulseResponse (std::move (ir), fs, juce::dsp::Convolution::Stereo::no,
                                        juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
    }
}

void NeptuneProcessor::timerCallback()
{
    if (param ("linear") > 0.5f) applyLinearPhaseIRs();
}

double NeptuneProcessor::crossover (int k) const
{
    // the modulated value, kept ascending
    double prev = 30.0, f = 0;
    for (int i = 0; i <= k; ++i)
    {
        const int t = kMaxBands * kBandParams + i;
        f = tparam[t]->convertFrom0to1 (modNorm[t].load());
        f = juce::jlimit (prev * 1.06, 18000.0, f);
        prev = f;
    }
    return f;
}

// ============================================================================ modulation
void NeptuneProcessor::computeModulation (int n, juce::AudioPlayHead::PositionInfo*, double ppq, bool playing, float inLevel)
{
    const double dt = n / fs;
    static const double beats[12] = { 0, 16, 8, 4, 2, 1, 0.5, 0.25, 0.125, 2.0 / 3, 1.0 / 3, 1.0 / 6 };
    double sv[kNumSources] {};
    for (int l = 0; l < 2; ++l)
    {
        const juce::String pre = "lfo" + juce::String (l + 1);
        const int sync = (int) param (pre + "_sync");
        double ph = lfoPhase[l];
        const double before = ph;
        if (sync > 0 && playing) ph = std::fmod (ppq / beats[sync], 1.0);
        else if (sync > 0)        ph += dt * (120.0 / 60.0) / beats[sync];
        else                      ph += dt * param (pre + "_rate");
        ph -= std::floor (ph);
        if (ph < before) { lfoPrev[l] = lfoNext[l]; lfoNext[l] = rng.nextDouble() * 2 - 1; lfoHeld[l] = lfoNext[l]; }
        lfoPhase[l] = ph;
        double v = 0;
        switch ((int) param (pre + "_shape"))
        {
            case 0: v = std::sin (2 * M_PI * ph); break;
            case 1: v = ph < 0.25 ? ph * 4 : ph < 0.75 ? 2 - ph * 4 : ph * 4 - 4; break;
            case 2: v = ph * 2 - 1; break;
            case 3: v = 1 - ph * 2; break;
            case 4: v = ph < 0.5 ? 1 : -1; break;
            case 5: v = lfoHeld[l]; break;
            default: { const double s = 0.5 - 0.5 * std::cos (M_PI * ph); v = lfoPrev[l] + (lfoNext[l] - lfoPrev[l]) * s; } break;
        }
        sv[1 + l] = v;
    }
    // envelope follower (on the input)
    {
        const double a = std::exp (-dt / (param ("ef_att") * 0.001)), r = std::exp (-dt / (param ("ef_rel") * 0.001));
        efEnv = inLevel > efEnv ? a * efEnv + (1 - a) * inLevel : r * efEnv + (1 - r) * inLevel;
        const double db = juce::Decibels::gainToDecibels (efEnv, -90.0) + param ("ef_gain");
        sv[3] = juce::jlimit (0.0, 1.0, (db + 60.0) / 60.0);
    }
    // envelope generator
    {
        bool gate;
        if ((int) param ("eg_trig") == 0) gate = juce::Decibels::gainToDecibels (inLevel, -90.0f) > param ("eg_thr");
        else gate = midiNotes > 0;
        if (gate && ! egGateWasOpen) egStage = 1;
        if (! gate && egGateWasOpen) egStage = 4;
        egGateWasOpen = gate;
        const double s = param ("eg_s") * 0.01;
        switch (egStage)
        {
            case 1: egLevel += dt / (param ("eg_a") * 0.001); if (egLevel >= 1) { egLevel = 1; egStage = 2; } break;
            case 2: egLevel -= dt / (param ("eg_d") * 0.001) * (1 - s); if (egLevel <= s) { egLevel = s; egStage = 3; } break;
            case 3: egLevel = s; break;
            case 4: egLevel -= dt / (param ("eg_r") * 0.001); if (egLevel <= 0) { egLevel = 0; egStage = 0; } break;
            default: break;
        }
        sv[4] = egLevel;
    }
    sv[5] = param ("xy_x") * 0.01; sv[6] = param ("xy_y") * 0.01; sv[7] = midiCC1;
    for (int i = 0; i < kNumSources; ++i) srcValue[i] = (float) sv[i];

    float add[kNumTargets] {};
    for (int s = 1; s <= kNumSlots; ++s)
    {
        const juce::String pre = "m" + juce::String (s);
        const int src = (int) param (pre + "_src"), dst = (int) param (pre + "_dst") - 1;
        if (src <= 0 || dst < 0 || dst >= kNumTargets) continue;
        add[dst] += (float) (param (pre + "_amt") * 0.01 * sv[src]);
    }
    for (int t = 0; t < kNumTargets; ++t)
    {
        const float base = tparam[t]->convertTo0to1 (raw[t]->load());
        const float v = juce::jlimit (0.0f, 1.0f, base + add[t]);
        modNorm[t] = v;
        modValues[t] = add[t] == 0 ? raw[t]->load() : tparam[t]->convertFrom0to1 (v);
    }
}

// ============================================================================ process
void NeptuneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int N = buffer.getNumSamples();
    const int nch = juce::jmin (2, buffer.getNumChannels());
    if (N > maxBlock) { prepareToPlay (fs, N); }
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        if (msg.isNoteOn()) ++midiNotes; else if (msg.isNoteOff()) midiNotes = juce::jmax (0, midiNotes - 1);
        else if (msg.isControllerOfType (1)) midiCC1 = msg.getControllerValue() / 127.0f;
        else if (msg.isAllNotesOff()) midiNotes = 0;
    }
    rebuildOversampling();
    const int hq = hqMode, lp = lpMode;
    const int factor = hq == 0 ? 1 : hq == 1 ? 8 : 32;

    double ppq = 0; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) ppq = *p;
            if (auto bpm = pos->getBpm()) ppqPerSample = *bpm / 60.0 / fs;
        }

    // mono -> use the same signal on both channels internally
    for (int c = 0; c < 2; ++c)
        dryBuf.copyFrom (c, 0, buffer, juce::jmin (c, nch - 1), 0, N);

    float inPk = 0;
    for (int c = 0; c < 2; ++c) inPk = juce::jmax (inPk, dryBuf.getMagnitude (c, 0, N));
    inPeak = juce::jmax (inPk, inPeak.load() * 0.9f);

    const bool ms = (int) param ("chmode") == 1;
    const bool unrestricted = param ("unrestricted") > 0.5f;
    const int nb = numBands();

    // ---- pass 1: modulation + input stage, per control block
    const int nSub = (N + kSub - 1) / kSub;
    if ((int) subMods.size() < nSub * kNumTargets) subMods.resize ((size_t) (nSub * kNumTargets));
    for (int s = 0; s < nSub; ++s)
    {
        const int s0 = s * kSub, n = juce::jmin (kSub, N - s0);
        float lvl = 0;
        for (int c = 0; c < 2; ++c) lvl = juce::jmax (lvl, dryBuf.getMagnitude (c, s0, n));
        computeModulation (n, nullptr, ppq + s0 * ppqPerSample, playing, lvl);
        std::copy (modValues, modValues + kNumTargets, subMods.begin() + s * kNumTargets);
        const float* mv = modValues;
        const float ig = juce::Decibels::decibelsToGain (mv[kMaxBands * kBandParams + kMaxBands - 1]);
        const float ip = param ("inpan") * 0.01f;
        inG[0].setTargetValue (ig * juce::jmin (1.0f, 1.0f - ip));
        inG[1].setTargetValue (ig * juce::jmin (1.0f, 1.0f + ip));
        for (int i = s0; i < s0 + n; ++i)
        {
            float l = dryBuf.getSample (0, i), r = dryBuf.getSample (1, i);
            if (ms) { const float m = (l + r) * 0.70710678f, sd = (l - r) * 0.70710678f; l = m; r = sd; }
            work.setSample (0, i, l * inG[0].getNextValue());
            work.setSample (1, i, r * inG[1].getNextValue());
        }
        // silence detection for the restricted feedback mode
        if (lvl < 1.0e-4f) silentTime += n / (float) fs; else silentTime = 0;
    }

    // ---- pass 2: band split
    if (lp == 1 && nb > 1)
    {
        for (int k = 0; k < nb - 1; ++k)
        {
            for (int c = 0; c < 2; ++c) lpBufs[k].copyFrom (c, 0, work, c, 0, N);
            juce::dsp::AudioBlock<float> blk (lpBufs[k].getArrayOfWritePointers(), 2, (size_t) N);
            lpConv[k]->process (juce::dsp::ProcessContextReplacing<float> (blk));
        }
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < N; ++i)
            {
                lpDelay.pushSample (c, work.getSample (c, i));
                const float xd = lpDelay.popSample (c);
                float prev = 0;
                for (int k = 0; k < nb - 1; ++k) { const float v = lpBufs[k].getSample (c, i); bands[k].buf.setSample (c, i, v - prev); prev = v; }
                bands[nb - 1].buf.setSample (c, i, xd - prev);
            }
    }
    else
    {
        if (lp == 1) for (int c = 0; c < 2; ++c) for (int i = 0; i < N; ++i) { lpDelay.pushSample (c, work.getSample (c, i)); work.setSample (c, i, lpDelay.popSample (c)); }
        double out[kMaxBands];
        for (int s = 0; s < nSub; ++s)
        {
            const int s0 = s * kSub, n = juce::jmin (kSub, N - s0);
            const float* mv = subMods.data() + s * kNumTargets;
            double fr[kMaxBands - 1]; int sl[kMaxBands - 1];
            double prev = 30.0;
            for (int k = 0; k < nb - 1; ++k)
            {
                fr[k] = juce::jlimit (prev * 1.06, 18000.0, (double) mv[kMaxBands * kBandParams + k]); prev = fr[k];
                sl[k] = 6 << (int) param ("slope" + juce::String (k + 1));
            }
            splitter.setup (nb, fr, sl);
            for (int c = 0; c < 2; ++c)
                for (int i = s0; i < s0 + n; ++i)
                {
                    splitter.process (c, work.getSample (c, i), out);
                    for (int b = 0; b < nb; ++b) bands[b].buf.setSample (c, i, (float) out[b]);
                }
        }
    }

    // ---- pass 3: per band processing
    bool anySolo = false;
    for (int b = 0; b < nb; ++b) anySolo |= param (bandID (b, "solo")) > 0.5f;
    work.clear();
    for (int b = 0; b < nb; ++b)
    {
        auto& bd = bands[b];
        const int st = (int) param (bandID (b, "style"));
        if (st != bd.styleIndex)
        {
            bd.styleIndex = st;
            for (int c = 0; c < 2; ++c) bd.style[c].setStyle (kStyles[juce::jlimit (0, kNumStyles - 1, st)]);
        }
        const bool on = param (bandID (b, "on")) > 0.5f;
        const bool audible = ! (param (bandID (b, "mute")) > 0.5f) && (! anySolo || param (bandID (b, "solo")) > 0.5f);
        float pk = 0;
        for (int s = 0; s < nSub; ++s)
        {
            const int s0 = s * kSub, n = juce::jmin (kSub, N - s0);
            const float* mv = subMods.data() + s * kNumTargets + b * kBandParams;
            const double drive = mv[0] * 0.01, dpan = mv[10] * 0.01;
            const double dch[2] = { juce::jlimit (0.0, 1.0, drive * (1.0 - dpan)), juce::jlimit (0.0, 1.0, drive * (1.0 + dpan)) };
            // feedback
            const double fbAmt = mv[2] * 0.01;
            if (! unrestricted && silentTime > 0.05f) bd.fbScale *= std::exp (-(float) n / (float) (0.25 * fs));
            else bd.fbScale = juce::jmin (1.0f, bd.fbScale + n / (float) (0.01 * fs));
            const double fbG = fbAmt * bd.fbScale;
            const double osFs = fs * factor;
            const int D = juce::jlimit (1, (int) bd.fbBuf[0].size() - 2, (int) std::round (osFs / mv[3]));
            const double fbLpC = 1.0 - std::exp (-2 * M_PI * juce::jmin (20000.0, mv[3] * 6.0) / osFs);
            const double fbHpC = 1.0 - std::exp (-2 * M_PI * 30.0 / osFs);

            // band dry copy (aligned with oversampling latency)
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                {
                    bd.dryDelay.pushSample (c, bd.buf.getSample (c, s0 + i));
                    bd.dry.setSample (c, i, bd.dryDelay.popSample (c));
                }

            float* chans[2] = { bd.buf.getWritePointer (0, s0), bd.buf.getWritePointer (1, s0) };
            juce::dsp::AudioBlock<float> blk (chans, 2, (size_t) n);
            juce::dsp::AudioBlock<float> up = hq == 0 ? blk : os[b][lp][hq - 1]->processSamplesUp (blk);
            const int un = (int) up.getNumSamples();
            const int mask = (int) bd.fbBuf[0].size() - 1;
            for (int c = 0; c < 2; ++c)
            {
                float* d = up.getChannelPointer ((size_t) c);
                auto& fbb = bd.fbBuf[c];
                int idx = bd.fbIdx;
                for (int i = 0; i < un; ++i)
                {
                    double u = d[i];
                    if (fbG > 0) u += fbG * std::tanh ((double) fbb[(size_t) ((idx - D) & mask)]);
                    const double y = bd.style[c].process (u, dch[c]);
                    bd.fbLp[c] += fbLpC * (y - bd.fbLp[c]);
                    bd.fbHp[c] += fbHpC * (bd.fbLp[c] - bd.fbHp[c]);
                    fbb[(size_t) (idx & mask)] = (float) (bd.fbLp[c] - bd.fbHp[c]);
                    ++idx;
                    d[i] = (float) y;
                }
                if (c == 1) bd.fbIdx = idx & mask;
            }
            if (hq > 0) os[b][lp][hq - 1]->processSamplesDown (blk);

            // dynamics (right = pumping compression, left = gate / expander), tone, mix, level
            const double dyn = mv[4] * 0.01;
            const double att = std::exp (-1.0 / (0.003 * fs)), rel = std::exp (-1.0 / (0.12 * fs));
            const float tv[4] = { mv[5], mv[6], mv[7], mv[8] };
            for (int k = 0; k < 4; ++k)
                if (tv[k] != bd.lastTone[k])
                {
                    bd.lastTone[k] = tv[k];
                    for (int c = 0; c < 2; ++c)
                    {
                        auto& q = bd.tone[c][k];
                        if (k == 0) q.lowShelf (fs, 100.0, tv[k]);
                        else if (k == 1) q.peak (fs, 1000.0, tv[k], 0.7);
                        else if (k == 2) q.highShelf (fs, 4000.0, tv[k]);
                        else q.peak (fs, 10000.0, tv[k], 1.0);
                    }
                }
            const float lvl = mv[9] <= -47.9f ? 0.0f : juce::Decibels::decibelsToGain (mv[9]);
            const float lpan = mv[11] * 0.01f;
            bd.gainL.setTargetValue (audible ? lvl * juce::jmin (1.0f, 1.0f - lpan) : 0.0f);
            bd.gainR.setTargetValue (audible ? lvl * juce::jmin (1.0f, 1.0f + lpan) : 0.0f);
            bd.mixS.setTargetValue (mv[1] * 0.01f);
            bd.enabledS.setTargetValue (on ? 1.0f : 0.0f);
            for (int i = 0; i < n; ++i)
            {
                double l = chans[0][i], r = chans[1][i];
                const double e = juce::jmax (std::fabs (l), std::fabs (r));
                bd.env = e > bd.env ? att * bd.env + (1 - att) * e : rel * bd.env + (1 - rel) * e;
                if (dyn != 0)
                {
                    const double edb = juce::Decibels::gainToDecibels (bd.env, -120.0);
                    double gdb = 0;
                    if (dyn > 0) gdb = -dyn * 0.92 * juce::jmax (0.0, edb + 30.0) + dyn * 9.0;
                    else         gdb = dyn * 3.0 * juce::jmax (0.0, (-50.0 - dyn * 25.0) - edb);
                    const double g = juce::Decibels::decibelsToGain (juce::jmax (-90.0, gdb));
                    l *= g; r *= g;
                }
                for (int k = 0; k < 4; ++k) if (tv[k] != 0) { l = bd.tone[0][k].process (l); r = bd.tone[1][k].process (r); }
                const float mx = bd.mixS.getNextValue(), en = bd.enabledS.getNextValue();
                const float dl = bd.dry.getSample (0, i), dr = bd.dry.getSample (1, i);
                float wl = (float) l * mx + dl * (1 - mx), wr = (float) r * mx + dr * (1 - mx);
                wl = (wl * bd.gainL.getNextValue()) * en + dl * (1 - en) * (audible ? 1.0f : 0.0f);
                wr = (wr * bd.gainR.getNextValue()) * en + dr * (1 - en) * (audible ? 1.0f : 0.0f);
                work.addSample (0, s0 + i, wl); work.addSample (1, s0 + i, wr);
                pk = juce::jmax (pk, std::fabs (wl), std::fabs (wr));
            }
        }
        bandPeak[b] = juce::jmax (pk, bandPeak[b].load() * 0.85f);
    }

    // ---- output stage
    const float* mvLast = subMods.data() + (nSub - 1) * kNumTargets;
    const int gt = kMaxBands * kBandParams + kMaxBands - 1;
    const float og = juce::Decibels::decibelsToGain (mvLast[gt + 1]);
    const float op = param ("outpan") * 0.01f;
    outG[0].setTargetValue (og * juce::jmin (1.0f, 1.0f - op));
    outG[1].setTargetValue (og * juce::jmin (1.0f, 1.0f + op));
    gMix.setTargetValue (mvLast[gt + 2] * 0.01f);
    bypassS.setTargetValue (param ("bypass") > 0.5f ? 1.0f : 0.0f);
    float oPk = 0;
    for (int i = 0; i < N; ++i)
    {
        float l = work.getSample (0, i), r = work.getSample (1, i);
        if (ms) { const float L = (l + r) * 0.70710678f, R = (l - r) * 0.70710678f; l = L; r = R; }
        dryDelay.pushSample (0, dryBuf.getSample (0, i)); dryDelay.pushSample (1, dryBuf.getSample (1, i));
        const float dl = dryDelay.popSample (0), dr = dryDelay.popSample (1);
        const float mx = gMix.getNextValue();
        l = l * mx + dl * (1 - mx); r = r * mx + dr * (1 - mx);
        l *= outG[0].getNextValue(); r *= outG[1].getNextValue();
        const float bp = bypassS.getNextValue();
        l = l * (1 - bp) + dl * bp; r = r * (1 - bp) + dr * bp;
        if (! std::isfinite (l)) l = 0;
        if (! std::isfinite (r)) r = 0;
        work.setSample (0, i, l); work.setSample (1, i, r);
        oPk = juce::jmax (oPk, std::fabs (l), std::fabs (r));
        const int w = fifoWrite.load (std::memory_order_relaxed);
        fifo[w & (kFifo - 1)] = 0.5f * (l + r);
        fifoWrite.store (w + 1, std::memory_order_relaxed);
    }
    outPeak = juce::jmax (oPk, outPeak.load() * 0.9f);
    if (nch == 1) { buffer.copyFrom (0, 0, work, 0, 0, N); buffer.addFrom (0, 0, work, 1, 0, N); buffer.applyGain (0.5f); }
    else for (int c = 0; c < 2; ++c) buffer.copyFrom (c, 0, work, c, 0, N);
    for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, N);
}

// ============================================================================ bands
void NeptuneProcessor::copyBand (int from, int to)
{
    for (auto* k : { "on", "solo", "mute", "style", "drive", "mix", "fb", "fbfreq", "dyn", "bass", "mid", "treble", "pres", "level", "dpan", "lpan" })
        setParam (bandID (to, k), param (bandID (from, k)));
}

void NeptuneProcessor::splitBandAt (double freq)
{
    const int nb = numBands();
    if (nb >= kMaxBands) return;
    freq = juce::jlimit (40.0, 18000.0, freq);
    int b = 0;
    while (b < nb - 1 && freq > param ("x" + juce::String (b + 1))) ++b;
    undo.beginNewTransaction ("Add band");
    // shift crossovers b.. up by one, insert the new one at b
    for (int k = nb - 1; k > b; --k)
    {
        setParam ("x" + juce::String (k + 1), param ("x" + juce::String (k)));
        setParam ("slope" + juce::String (k + 1), param ("slope" + juce::String (k)));
    }
    setParam ("x" + juce::String (b + 1), (float) freq);
    // shift bands b+1.. up, the new band copies the band that was split
    for (int k = nb; k > b; --k) copyBand (k - 1, k);
    // modulation slots follow their band
    for (int s = 1; s <= kNumSlots; ++s)
    {
        const juce::String id = "m" + juce::String (s) + "_dst";
        const int d = (int) param (id) - 1;
        if (d < 0) continue;
        if (d < kMaxBands * kBandParams && d / kBandParams > b) setParam (id, (float) (d + kBandParams + 1));
        else if (d >= kMaxBands * kBandParams && d < kMaxBands * kBandParams + kMaxBands - 1 && d - kMaxBands * kBandParams >= b) setParam (id, (float) (d + 2));
    }
    setParam ("bands", (float) (nb + 1));
}

void NeptuneProcessor::removeBand (int b)
{
    const int nb = numBands();
    if (nb <= 1 || b < 0 || b >= nb) return;
    undo.beginNewTransaction ("Remove band");
    const int xk = b == nb - 1 ? b - 1 : b;     // crossover that disappears
    for (int k = xk; k < nb - 2; ++k)
    {
        setParam ("x" + juce::String (k + 1), param ("x" + juce::String (k + 2)));
        setParam ("slope" + juce::String (k + 1), param ("slope" + juce::String (k + 2)));
    }
    for (int k = b; k < nb - 1; ++k) copyBand (k + 1, k);
    for (int s = 1; s <= kNumSlots; ++s)
    {
        const juce::String id = "m" + juce::String (s) + "_dst";
        const int d = (int) param (id) - 1;
        if (d < 0) continue;
        if (d < kMaxBands * kBandParams)
        {
            const int db = d / kBandParams;
            if (db == b) setParam (id, 0);
            else if (db > b) setParam (id, (float) (d - kBandParams + 1));
        }
    }
    setParam ("bands", (float) (nb - 1));
}

int NeptuneProcessor::addModulation (int source, int target, float amount)
{
    for (int s = 1; s <= kNumSlots; ++s)
    {
        const juce::String pre = "m" + juce::String (s);
        if ((int) param (pre + "_src") == source && (int) param (pre + "_dst") == target + 1) return s;
    }
    for (int s = 1; s <= kNumSlots; ++s)
    {
        const juce::String pre = "m" + juce::String (s);
        if ((int) param (pre + "_src") == 0 || (int) param (pre + "_dst") == 0)
        {
            undo.beginNewTransaction ("Add modulation");
            setParam (pre + "_src", (float) source); setParam (pre + "_dst", (float) (target + 1)); setParam (pre + "_amt", amount);
            return s;
        }
    }
    return -1;
}

// ============================================================================ presets
juce::StringArray NeptuneProcessor::factoryPresetNames()
{
    return { "Default", "Mastering Glow", "Warm Bus Tape", "Tube Vocal Presence", "3-Band Drum Crunch",
             "Bass Growl", "Lo-Fi Wobble", "Feedback Ringer", "Breakdown Drop", "Wide Side Sparkle" };
}

void NeptuneProcessor::loadFactoryPreset (int index)
{
    undo.beginNewTransaction ("Preset");
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        { rp->beginChangeGesture(); rp->setValueNotifyingHost (rp->getDefaultValue()); rp->endChangeGesture(); }
    auto S = [this] (const juce::String& id, float v) { setParam (id, v); };
    auto B = [this] (int b, const char* k, float v) { setParam (bandID (b, k), v); };
    auto style = [&] (int b, const juce::String& name) { B (b, "style", (float) styleNames().indexOf (name)); };
    switch (index)
    {
        case 1: S ("bands", 2); S ("x1", 120); style (0, "Subtle Tape"); B (0, "drive", 35); style (1, "Subtle Tube"); B (1, "drive", 45);
                S ("hq", 1); S ("linear", 1); break;
        case 2: style (0, "Warm Tape"); B (0, "drive", 30); B (0, "treble", -1.5f); B (0, "bass", 1.0f); S ("hq", 1); break;
        case 3: S ("bands", 2); S ("x1", 2500); style (0, "Clean Tube"); B (0, "drive", 20); style (1, "Warm Tube"); B (1, "drive", 45);
                B (1, "pres", 2.5f); B (1, "mix", 70); break;
        case 4: S ("bands", 3); S ("x1", 160); S ("x2", 3500); style (0, "Warm Transformer"); B (0, "drive", 40); B (0, "dyn", 30);
                style (1, "Gentle Saturation"); B (1, "drive", 55); B (1, "dyn", 45); style (2, "Clean Tape"); B (2, "drive", 30); B (2, "level", 1.5f); break;
        case 5: S ("bands", 2); S ("x1", 180); style (0, "Subtle Transformer"); B (0, "drive", 30); style (1, "British Rock Amp"); B (1, "drive", 30);
                B (1, "mix", 35); B (1, "level", -3); break;
        case 6: style (0, "Destroy"); B (0, "drive", 25); B (0, "mix", 60); S ("m1_src", 1); S ("m1_dst", 1); S ("m1_amt", 20);
                S ("lfo1_sync", 6); S ("lfo1_shape", 1); break;
        case 7: style (0, "Warm Tube"); B (0, "drive", 30); B (0, "fb", 55); B (0, "fbfreq", 660); S ("m1_src", 2); S ("m1_dst", 4); S ("m1_amt", 30);
                S ("lfo2_rate", 0.15f); break;
        case 8: style (0, "Breakdown"); B (0, "drive", 0); S ("m1_src", 4); S ("m1_dst", 1); S ("m1_amt", 60); S ("eg_trig", 1); break;
        case 9: S ("chmode", 1); S ("bands", 2); S ("x1", 5000); style (0, "Subtle Saturation"); style (1, "Gentle Saturation"); B (1, "drive", 30);
                B (1, "lpan", 40); break;
        default: break;
    }
}

// ============================================================================ A/B + state
void NeptuneProcessor::selectSlot (int s)
{
    if (s == abSlot) return;
    abSlots[abSlot] = apvts.copyState();
    abSlot = s;
    apvts.replaceState (abSlots[abSlot].createCopy());
}
void NeptuneProcessor::copyToOtherSlot() { abSlots[1 - abSlot] = apvts.copyState(); }

void NeptuneProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}
void NeptuneProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* NeptuneProcessor::createEditor() { return new NeptuneEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NeptuneProcessor(); }
