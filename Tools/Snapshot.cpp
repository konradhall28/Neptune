// Renders the editor to PNGs and runs a DSP self-test against the measured reference data.
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/StyleTable.h"
#include <iostream>
#include <chrono>

using P = NeptuneProcessor;
static const double kFs = 48000.0;
static const int kBs = 512;

static void prep (P& p) { p.setPlayConfigDetails (2, 2, kFs, kBs); p.prepareToPlay (kFs, kBs); }
static void set (P& p, const juce::String& id, float v) { p.setParam (id, v); }
static void setB (P& p, int b, const char* k, float v) { p.setParam (nep::bandID (b, k), v); }
static void waitMs (int ms) { juce::Thread::sleep (ms); }

// music-like test signal: chords + drums
static void fillMusic (juce::AudioBuffer<float>& b, int offset)
{
    static juce::Random r (3);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = (offset + i) / kFs, tb = std::fmod (t, 0.5);
        double v = 0.5 * std::exp (-tb * 14) * std::sin (2 * juce::MathConstants<double>::pi * (50 + 80 * std::exp (-tb * 30)) * tb);
        for (double f : { 110.0, 220.0, 277.2, 329.6, 659.3, 1318.5 }) v += 0.06 * std::sin (2 * juce::MathConstants<double>::pi * f * t);
        v += 0.04 * (r.nextFloat() * 2 - 1) * std::exp (-std::fmod (t, 0.25) * 30);
        for (int c = 0; c < b.getNumChannels(); ++c) b.setSample (c, i, (float) (v * (c ? 0.9 : 1.0)));
    }
}
static void runMusic (P& p, int blocks)
{
    juce::AudioBuffer<float> buf (2, kBs); juce::MidiBuffer m;
    for (int k = 0; k < blocks; ++k) { fillMusic (buf, k * kBs); p.processBlock (buf, m); }
}

// pushes a 1 kHz sine (-6 dBFS) through and returns harmonic levels
struct Harm { double fund, thd, h[8]; };
static Harm sineTest (P& p, double freq = 1000.0, double levelDb = -6.0)
{
    const int settle = 48000, len = 48000;
    const double a = juce::Decibels::decibelsToGain (levelDb);
    std::vector<double> out; out.reserve ((size_t) len);
    juce::AudioBuffer<float> buf (2, kBs); juce::MidiBuffer m;
    for (int n0 = 0; n0 < settle + len; n0 += kBs)
    {
        for (int i = 0; i < kBs; ++i)
        {
            const float v = (float) (a * std::sin (2 * juce::MathConstants<double>::pi * freq * (n0 + i) / kFs));
            buf.setSample (0, i, v); buf.setSample (1, i, v);
        }
        p.processBlock (buf, m);
        for (int i = 0; i < kBs; ++i) if (n0 + i >= settle && (int) out.size() < len) out.push_back (buf.getSample (0, i));
    }
    auto amp = [&] (double f)
    {
        double re = 0, im = 0;
        for (int n = 0; n < len; ++n)
        {
            const double w = 0.5 - 0.5 * std::cos (2 * juce::MathConstants<double>::pi * n / (len - 1));
            re += out[(size_t) n] * w * std::cos (2 * juce::MathConstants<double>::pi * f * n / kFs);
            im -= out[(size_t) n] * w * std::sin (2 * juce::MathConstants<double>::pi * f * n / kFs);
        }
        return 4.0 * std::sqrt (re * re + im * im) / len;   // Hann gain 0.5
    };
    Harm h {}; const double f1 = amp (freq); double s = 0;
    h.fund = 20 * std::log10 (std::max (f1, 1e-12)) - levelDb;
    for (int k = 2; k <= 7; ++k)
    {
        const double ak = (k * freq < kFs * 0.5) ? amp (k * freq) : 0.0;
        h.h[k] = 20 * std::log10 (std::max (ak, 1e-12) / std::max (f1, 1e-12)); s += ak * ak;
    }
    for (int k = 8; k * freq < kFs * 0.5 && k < 20; ++k) { const double ak = amp (k * freq); s += ak * ak; }
    h.thd = 100.0 * std::sqrt (s) / std::max (f1, 1e-12);
    return h;
}

class SnapshotApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "UISnapshot"; }
    const juce::String getApplicationVersion() override { return "1.0"; }

    void initialise (const juce::String& args) override
    {
        const auto pathArg = args.upToFirstOccurrenceOf (" --", false, false).trim();
        auto outDir = juce::File (pathArg.isEmpty() ? juce::File::getCurrentWorkingDirectory().getFullPathName() : pathArg);
        outDir.createDirectory();
        const bool skipTest = args.contains ("--ui-only");

        auto shoot = [&] (const juce::String& name, std::function<void (P&)> pre, std::function<void (P&, NeptuneEditor&)> stage = nullptr)
        {
            auto procH = std::make_unique<P>(); P& proc = *procH; prep (proc);
            if (pre) pre (proc);
            std::unique_ptr<NeptuneEditor> ed (dynamic_cast<NeptuneEditor*> (proc.createEditor()));
            for (int i = 0; i < 40; ++i) { runMusic (proc, 4); ed->syncNow(); }
            if (stage) stage (proc, *ed);
            for (int i = 0; i < 10; ++i) { runMusic (proc, 2); ed->syncNow(); }
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
            auto f = outDir.getChildFile (name + ".png"); f.deleteFile();
            juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        };

        shoot ("01_single_band", [] (P& p) { setB (p, 0, "style", 6); setB (p, 0, "drive", 35); });
        shoot ("02_three_bands", [] (P& p)
        {
            set (p, "bands", 3); set (p, "x1", 180); set (p, "x2", 3200);
            setB (p, 0, "style", 20); setB (p, 0, "drive", 30);
            setB (p, 1, "style", 10); setB (p, 1, "drive", 45); setB (p, 1, "fb", 25); setB (p, 1, "dyn", 30); setB (p, 1, "mid", 3); setB (p, 1, "level", -2);
            setB (p, 2, "style", 4); setB (p, 2, "drive", 20);
            p.addModulation (1, 1 * nep::kBandParams + 0, 30);
        }, [] (P&, NeptuneEditor& e) { e.selectBand (1); });
        shoot ("03_modulation_slots", [] (P& p)
        {
            set (p, "bands", 2); set (p, "x1", 900); setB (p, 0, "style", 1); setB (p, 1, "style", 24);
            p.addModulation (1, 0, 40); p.addModulation (3, nep::kBandParams + 0, -25); p.addModulation (5, nep::kBandParams * nep::kMaxBands, 50);
        }, [] (P&, NeptuneEditor& e) { e.openSlots(); });
        shoot ("04_lfo_editor", [] (P& p) { p.addModulation (1, 0, 40); }, [] (P&, NeptuneEditor& e) { e.openSource (1); });
        shoot ("05_output_options", [] (P& p) { set (p, "hq", 1); set (p, "linear", 1); }, [] (P&, NeptuneEditor& e) { e.openOutput(); });

        if (skipTest) { quit(); return; }

        // ---------------- self-test ----------------
        std::cout << "Style match vs. measured reference (1 kHz, -6 dBFS, HQ Good):" << std::endl;
        std::vector<double> all;
        for (int s = 0; s < nep::kNumStyles; ++s)
        {
            std::vector<double> errs; juce::String line;
            for (int d = 0; d < 3; ++d)
            {
                auto pH = std::make_unique<P>(); P& p = *pH; set (p, "hq", 1); setB (p, 0, "style", (float) s); setB (p, 0, "drive", d * 50.0f); prep (p);
                const auto h = sineTest (p); const auto& t = kStyleTargets[s][d];
                const int hk[5] = { 2, 3, 4, 5, 7 };
                for (int k = 0; k < 5; ++k)
                    if (t.h[k] > -60) errs.push_back (std::abs (h.h[hk[k]] - t.h[k]));
                if (t.fund > -100) errs.push_back (std::abs (h.fund - t.fund));
                line << "  d" << d * 50 << " fund " << juce::String (h.fund, 1) << "/" << juce::String (t.fund, 1)
                     << " THD " << juce::String (h.thd, 1) << "/" << juce::String (t.thd, 1);
            }
            std::sort (errs.begin(), errs.end());
            const double med = errs.empty() ? 0 : errs[errs.size() / 2];
            all.push_back (med);
            std::cout << "  " << juce::String (kStyleNames[s]).paddedRight (' ', 20) << " median err " << juce::String (med, 1) << " dB |" << line << std::endl;
        }
        std::sort (all.begin(), all.end());
        std::cout << "  overall median " << juce::String (all[all.size() / 2], 2) << " dB" << std::endl;

        // null test: all bands off -> crossover sum must be transparent
        for (int lin = 0; lin < 2; ++lin)
        {
            auto pH = std::make_unique<P>(); P& p = *pH; set (p, "bands", 4); set (p, "linear", (float) lin);
            for (int b = 0; b < 4; ++b) setB (p, b, "on", 0);
            prep (p); if (lin) { p.applyLinearPhaseIRs(); waitMs (250); }
            double maxDev = 0;
            for (double f : { 60.0, 150.0, 700.0, 2500.0, 6000.0, 12000.0 })
                maxDev = std::max (maxDev, std::abs (sineTest (p, f, -12).fund));
            std::cout << "  4 bands bypassed, " << (lin ? "linear phase" : "minimum phase") << ": max level deviation " << juce::String (maxDev, 3) << " dB" << std::endl;
        }
        for (int lin = 0; lin < 2; ++lin)
            for (int hq = 0; hq < 3; ++hq)
            {
                auto pH = std::make_unique<P>(); P& p = *pH; set (p, "hq", (float) hq); set (p, "linear", (float) lin); prep (p); runMusic (p, 4);
                std::cout << "  latency HQ " << hq << (lin ? " linear" : " normal") << ": " << p.getLatencySamples() << " samples" << std::endl;
            }
        {
            auto pH = std::make_unique<P>(); P& p = *pH; set (p, "unrestricted", 1); setB (p, 0, "fb", 100); setB (p, 0, "drive", 100); setB (p, 0, "style", 13); prep (p);
            juce::AudioBuffer<float> buf (2, kBs); juce::MidiBuffer m; float peak = 0; bool bad = false;
            for (int k = 0; k < 400; ++k) { fillMusic (buf, k * kBs); p.processBlock (buf, m); for (int i = 0; i < kBs; ++i) { auto y = buf.getSample (0, i); bad |= ! std::isfinite (y); peak = std::max (peak, std::abs (y)); } }
            std::cout << "  max feedback stability: peak " << juce::String (juce::Decibels::gainToDecibels (peak), 1) << " dBFS" << (bad ? " NON-FINITE" : " ok") << std::endl;
        }
        for (int hq = 0; hq < 3; ++hq)
        {
            auto pH = std::make_unique<P>(); P& p = *pH; set (p, "bands", 6); set (p, "hq", (float) hq);
            for (int b = 0; b < 6; ++b) { setB (p, b, "style", (float) (b * 4)); setB (p, b, "drive", 40); }
            prep (p); runMusic (p, 20);
            auto t0 = std::chrono::steady_clock::now(); runMusic (p, 200);
            const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::cout << "  CPU 6 bands HQ " << hq << ": " << juce::String (100.0 * sec / (200.0 * kBs / kFs), 1) << " % of one core" << std::endl;
        }
        quit();
    }
    void shutdown() override {}
};

START_JUCE_APPLICATION (SnapshotApp)
