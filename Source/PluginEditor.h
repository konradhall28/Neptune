#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

namespace ui
{
// Blue Studio-style palette
const juce::Colour bg       { 0xff070d18 };
const juce::Colour panel    { 0xff0d1829 };
const juce::Colour panel2   { 0xff12223a };
const juce::Colour edge     { 0xff1d3150 };
const juce::Colour grid     { 0xff15253c };
const juce::Colour text     { 0xffd2e0f4 };
const juce::Colour dim      { 0xff7189ab };
const juce::Colour accent   { 0xff3aa0ff };
const juce::Colour accent2  { 0xff79d6ff };
const juce::Colour warn     { 0xffff5a6e };
juce::Colour bandColour (int b);
juce::Colour sourceColour (int s);
juce::Font font (float h, int weight = 1);    // 0 light, 1 semibold, 2 bold
}

class NeptuneEditor;

// ---------------------------------------------------------------- knob
class Knob : public juce::Component, public juce::DragAndDropTarget, public juce::SettableTooltipClient
{
public:
    Knob (NeptuneProcessor& p, juce::String label, bool big = false);
    ~Knob() override;
    void attach (const juce::String& paramID, const juce::String& ringID = {}, int modTarget = -1);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }
    bool isInterestedInDragSource (const SourceDetails& d) override { return modTarget >= 0 && d.description.toString().startsWith ("src:"); }
    void itemDragEnter (const SourceDetails&) override { dropHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override  { dropHover = false; repaint(); }
    void itemDropped (const SourceDetails& d) override;
    void refreshMod() { repaint(); }
    juce::Colour colour = ui::accent;
    bool showValueAlways = false;
    juce::String label;
private:
    juce::RangedAudioParameter* getP (bool ring) const { return ring ? ringParam : param; }
    juce::Rectangle<float> knobArea() const;
    bool onRing (juce::Point<float>) const;
    NeptuneProcessor& proc;
    bool big;
    juce::RangedAudioParameter* param = nullptr;
    juce::RangedAudioParameter* ringParam = nullptr;
    std::unique_ptr<juce::ParameterAttachment> att, ringAtt;
    int modTarget = -1;
    bool hover = false, dropHover = false, draggingRing = false;
    float dragStartNorm = 0; juce::Point<float> dragStart;
    std::unique_ptr<juce::TextEditor> entry;
};

// ---------------------------------------------------------------- small toggle / text button
class FlatButton : public juce::Button
{
public:
    FlatButton (const juce::String& name, bool toggle = false) : juce::Button (name) { setClickingTogglesState (toggle); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
    juce::Colour onColour = ui::accent;
    bool drawAsLed = false;
    juce::String icon;     // "power", "x", "+", "undo", "redo", "<", ">", "menu"
};

// ---------------------------------------------------------------- interactive display
class BandDisplay : public juce::Component
{
public:
    BandDisplay (NeptuneProcessor& p, NeptuneEditor& e);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverX = -1; hoverBand = -1; repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void updateSpectrum();
    float xForFreq (double f) const;
    double freqForX (float x) const;
    float yForLevel (float db) const;
    int bandAt (float x) const;
    juce::Rectangle<float> levelButton (int b) const;
    juce::Rectangle<float> plusButton() const;
    juce::Rectangle<float> soloRect (int b) const;
    juce::Rectangle<float> muteRect (int b) const;
    juce::Rectangle<float> slopeRect (int k) const;
private:
    NeptuneProcessor& proc; NeptuneEditor& ed;
    float hoverX = -1; int hoverBand = -1;
    enum Drag { none, crossoverDrag, levelDrag, bandDrag } drag = none;
    int dragIndex = -1; float dragStartLevel = 0, dragStartDrive = 0; juce::Point<float> dragStart;
    double dragStartLo = 0, dragStartHi = 0;
    std::vector<float> spec, specSmooth;
    juce::dsp::FFT fft { 12 };
    std::vector<float> fftBuf;
};

// ---------------------------------------------------------------- band controls panel
class BandPanel : public juce::Component
{
public:
    BandPanel (NeptuneProcessor& p, NeptuneEditor& e);
    void setBand (int b);
    void paint (juce::Graphics&) override;
    void resized() override;
    int band = 0;
    Knob mix, fb, fbFreq, dyn, drive, bass, mid, treble, pres, level;
    FlatButton enable { "Enable", true }, remove { "Remove" }, styleBtn { "Style" }, presetBtn { "Presets" };
private:
    NeptuneProcessor& proc; NeptuneEditor& ed;
    std::unique_ptr<juce::ButtonParameterAttachment> enableAtt;
    void showStyleMenu();
    void showBandPresetMenu();
};

// ---------------------------------------------------------------- modulation strip
class ModSource : public juce::Component
{
public:
    ModSource (NeptuneProcessor& p, NeptuneEditor& e, int sourceIndex);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    juce::Rectangle<float> handle() const { return { getWidth() * 0.5f - 7, 3, 14, 14 }; }
    int src;
private:
    NeptuneProcessor& proc; NeptuneEditor& ed;
    bool dragging = false;
    std::vector<float> history;
    friend class ModStrip;
};

class ModStrip : public juce::Component
{
public:
    ModStrip (NeptuneProcessor& p, NeptuneEditor& e);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
    juce::OwnedArray<ModSource> sources;
    FlatButton slotsBtn { "Slots" };
private:
    NeptuneProcessor& proc; NeptuneEditor& ed;
};

// expanded editor for one source / the slot list / the output options
class PopPanel : public juce::Component
{
public:
    PopPanel (NeptuneProcessor& p, NeptuneEditor& e);
    void showSource (int src);
    void showSlots();
    void showOutput();
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
    int mode = -1;   // 0..7 source, 100 slots, 200 output
    juce::String title;
private:
    NeptuneProcessor& proc; NeptuneEditor& ed;
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<juce::Component> extras;
    juce::OwnedArray<juce::ButtonParameterAttachment> batts;
    juce::OwnedArray<juce::ComboBoxParameterAttachment> catts;
    juce::OwnedArray<juce::SliderParameterAttachment> sliderAtts;
    FlatButton close { "Close" };
    void clear();
    void addChoice (const juce::String& paramID, juce::Rectangle<int> r);
    void addToggle (const juce::String& paramID, const juce::String& label, juce::Rectangle<int> r);
};

// ---------------------------------------------------------------- editor
class NeptuneEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit NeptuneEditor (NeptuneProcessor&);
    ~NeptuneEditor() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void selectBand (int b);
    int  selectedBand() const { return selBand; }
    void syncNow() { timerCallback(); }
    void openSource (int s);
    void openSlots();
    void openOutput();
    juce::Rectangle<int> displayArea;
    NeptuneProcessor& proc;
    juce::TooltipWindow tips { this, 600 };
    int presetIndex = 0;
private:
    void timerCallback() override;
    void layoutBandPanel();
    int selBand = 0;
    BandDisplay display;
    BandPanel bandPanel;
    ModStrip modStrip;
    PopPanel pop;
    FlatButton undoBtn { "Undo" }, redoBtn { "Redo" }, aBtn { "A" }, bBtn { "B" }, copyBtn { "Copy" }, helpBtn { "Help" },
               prevBtn { "Prev" }, nextBtn { "Next" }, presetBtn { "Preset" },
               linBtn { "Linear Phase", true }, hqBtn { "High Quality" }, bypassBtn { "Bypass", true }, outBtn { "Output" };
    std::unique_ptr<juce::ButtonParameterAttachment> linAtt, bypassAtt;
    float panelX = -1;
};
