#include "PluginEditor.h"
#include "BinaryData.h"

using namespace nep;

// ============================================================================ palette / fonts
juce::Colour ui::bandColour (int b)
{
    static const juce::uint32 c[6] = { 0xff3aa0ff, 0xff2fd1c4, 0xff8a8cff, 0xff4fc3ff, 0xff5be3a4, 0xffb58cff };
    return juce::Colour (c[juce::jlimit (0, 5, b)]);
}
juce::Colour ui::sourceColour (int s)
{
    static const juce::uint32 c[8] = { 0xff7189ab, 0xff4fb6ff, 0xff36d6e7, 0xffffc24a, 0xff9d7bff, 0xff5be3a4, 0xff5be3a4, 0xffff7fb0 };
    return juce::Colour (c[juce::jlimit (0, 7, s)]);
}
juce::Font ui::font (float h, int w)
{
    static auto light = juce::Typeface::createSystemTypefaceFor (BinaryData::InterLight_ttf, BinaryData::InterLight_ttfSize);
    static auto semi  = juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize);
    static auto bold  = juce::Typeface::createSystemTypefaceFor (BinaryData::InterBold_ttf, BinaryData::InterBold_ttfSize);
    return juce::Font (juce::FontOptions (w == 0 ? light : w == 1 ? semi : bold).withHeight (h));
}

namespace
{
struct NepLnf : juce::LookAndFeel_V4
{
    NepLnf()
    {
        setColourScheme ({ ui::panel, ui::bg, ui::panel2, ui::edge, ui::text, ui::panel2, ui::text, ui::accent, ui::text });
        setColour (juce::ComboBox::backgroundColourId, ui::panel2);
        setColour (juce::ComboBox::outlineColourId, ui::edge);
        setColour (juce::ComboBox::arrowColourId, ui::accent2);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff0b1626));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::accent.withAlpha (0.35f));
        setColour (juce::PopupMenu::headerTextColourId, ui::accent2);
        setColour (juce::Slider::trackColourId, ui::accent);
        setColour (juce::Slider::backgroundColourId, ui::panel2);
        setColour (juce::Slider::textBoxTextColourId, ui::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf00b1626));
        setColour (juce::TooltipWindow::textColourId, ui::text);
        setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0b1626));
        setColour (juce::TextEditor::highlightColourId, ui::accent.withAlpha (0.4f));
        setColour (juce::TextEditor::outlineColourId, ui::accent);
        setColour (juce::TextEditor::focusedOutlineColourId, ui::accent2);
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return ui::font (12.5f); }
    juce::Font getPopupMenuFont() override { return ui::font (13.5f); }
    juce::Font getSliderPopupFont (juce::Slider&) override { return ui::font (12.0f); }
    juce::Font getLabelFont (juce::Label&) override { return ui::font (12.0f); }
    void drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name) override
    {
        g.setFont (ui::font (11.5f, 2)); g.setColour (ui::accent2);
        g.drawFittedText (name.toUpperCase(), area.reduced (12, 0), juce::Justification::bottomLeft, 1);
    }
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (ui::panel2); g.fillRoundedRectangle (r, 4);
        g.setColour (ui::edge); g.drawRoundedRectangle (r, 4, 1);
        juce::Path p; const float cx = w - 11.0f, cy = h * 0.5f;
        p.addTriangle (cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3);
        g.setColour (ui::accent2); g.fillPath (p);
    }
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s) override
    {
        auto r = juce::Rectangle<float> ((float) x, y + h * 0.5f - 3, (float) w, 6);
        g.setColour (ui::panel2); g.fillRoundedRectangle (r, 3);
        const float mid = x + w * 0.5f;
        g.setColour (ui::accent);
        g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (mid, pos), r.getY(), std::abs (pos - mid), 6), 3);
        g.setColour (ui::text); g.fillEllipse (pos - 5, y + h * 0.5f - 5, 10, 10);
        juce::ignoreUnused (s);
    }
};
NepLnf& lnf() { static NepLnf l; return l; }

void drawIcon (juce::Graphics& g, const juce::String& icon, juce::Rectangle<float> r, juce::Colour c)
{
    g.setColour (c);
    const auto cx = r.getCentreX(), cy = r.getCentreY(); const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
    juce::Path p;
    if (icon == "power")
    {
        p.addCentredArc (cx, cy, s * 0.7f, s * 0.7f, 0, juce::degreesToRadians (35.0f), juce::degreesToRadians (325.0f), true);
        g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.drawLine (cx, cy - s * 0.85f, cx, cy - s * 0.15f, 1.6f);
    }
    else if (icon == "x") { g.drawLine (cx - s * .45f, cy - s * .45f, cx + s * .45f, cy + s * .45f, 1.6f); g.drawLine (cx + s * .45f, cy - s * .45f, cx - s * .45f, cy + s * .45f, 1.6f); }
    else if (icon == "+") { g.drawLine (cx - s * .5f, cy, cx + s * .5f, cy, 1.8f); g.drawLine (cx, cy - s * .5f, cx, cy + s * .5f, 1.8f); }
    else if (icon == "<" || icon == ">")
    {
        const float d = icon == "<" ? 1.f : -1.f;
        p.startNewSubPath (cx + d * s * 0.25f, cy - s * 0.45f); p.lineTo (cx - d * s * 0.25f, cy); p.lineTo (cx + d * s * 0.25f, cy + s * 0.45f);
        g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
    }
    else if (icon == "undo" || icon == "redo")
    {
        const float d = icon == "undo" ? 1.f : -1.f;
        p.addCentredArc (cx, cy + s * 0.1f, s * 0.55f, s * 0.5f, 0, d * juce::degreesToRadians (-80.0f), d * juce::degreesToRadians (100.0f), true);
        g.strokePath (p, juce::PathStrokeType (1.5f));
        juce::Path a; const float ax = cx - d * s * 0.55f, ay = cy + s * 0.1f;
        a.addTriangle (ax - s * 0.3f, ay - s * 0.15f, ax + s * 0.3f, ay - s * 0.15f, ax, ay + s * 0.3f);
        g.fillPath (a);
    }
    else if (icon == "menu") { for (int i = -1; i <= 1; ++i) g.drawLine (cx - s * .5f, cy + i * s * .35f, cx + s * .5f, cy + i * s * .35f, 1.4f); }
}
}

// ============================================================================ FlatButton
void FlatButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();
    g.setColour (down ? ui::panel2.brighter (0.15f) : over ? ui::panel2.brighter (0.07f) : ui::panel2);
    g.fillRoundedRectangle (r, 4);
    g.setColour (on ? onColour.withAlpha (0.9f) : ui::edge);
    g.drawRoundedRectangle (r, 4, 1.0f);
    juce::Colour tc = on ? onColour.brighter (0.3f) : (over ? ui::text : ui::text.withAlpha (0.82f));
    if (! isEnabled()) tc = ui::dim.withAlpha (0.5f);
    if (icon.isNotEmpty() && getButtonText().isEmpty())
    {
        drawIcon (g, icon, r.reduced (r.getHeight() * 0.22f), tc);
        return;
    }
    auto tr = r.reduced (6, 0);
    if (drawAsLed)
    {
        auto led = tr.removeFromLeft (8).withSizeKeepingCentre (7, 7);
        g.setColour (on ? onColour : ui::edge.brighter (0.2f)); g.fillEllipse (led);
        if (on) { g.setColour (onColour.withAlpha (0.35f)); g.fillEllipse (led.expanded (3)); }
        tr.removeFromLeft (5);
    }
    g.setColour (tc); g.setFont (ui::font (12.5f));
    g.drawFittedText (getButtonText(), tr.toNearestInt(), drawAsLed ? juce::Justification::centredLeft : juce::Justification::centred, 1);
}

// ============================================================================ Knob
Knob::Knob (NeptuneProcessor& p, juce::String l, bool b) : label (std::move (l)), proc (p), big (b) {}
Knob::~Knob() = default;

void Knob::attach (const juce::String& id, const juce::String& ringID, int target)
{
    att.reset(); ringAtt.reset();
    param = proc.apvts.getParameter (id);
    ringParam = ringID.isNotEmpty() ? proc.apvts.getParameter (ringID) : nullptr;
    modTarget = target;
    if (param) att = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); }, &proc.undo);
    if (ringParam) ringAtt = std::make_unique<juce::ParameterAttachment> (*ringParam, [this] (float) { repaint(); }, &proc.undo);
    if (att) att->sendInitialUpdate();
    repaint();
}

juce::Rectangle<float> Knob::knobArea() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromBottom (big ? 18.0f : 15.0f);
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    return r.withSizeKeepingCentre (d, d);
}
bool Knob::onRing (juce::Point<float> p) const
{
    if (! ringParam) return false;
    auto k = knobArea();
    return p.getDistanceFrom (k.getCentre()) > k.getWidth() * 0.5f * 0.80f;
}

void Knob::paint (juce::Graphics& g)
{
    if (! param) return;
    auto k = knobArea();
    const float R = k.getWidth() * 0.5f;
    const auto c = k.getCentre();
    const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
    const float v = param->getValue();
    const float bodyR = ringParam ? R * 0.74f : R * 0.86f;
    const float trackR = ringParam ? R * 0.80f : R * 0.97f;

    // outer track + value arc
    juce::Path track; track.addCentredArc (c.x, c.y, trackR, trackR, 0, a0, a1, true);
    g.setColour (ui::bg.brighter (0.06f)); g.strokePath (track, juce::PathStrokeType (big ? 4.0f : 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const bool bip = param->getNormalisableRange().start < 0 && param->getNormalisableRange().end > 0;
    const float from = bip ? (a0 + a1) * 0.5f : a0;
    const float to = a0 + (a1 - a0) * v;
    juce::Path val; val.addCentredArc (c.x, c.y, trackR, trackR, 0, juce::jmin (from, to), juce::jmax (from, to), true);
    g.setColour (colour.withAlpha (hover ? 1.0f : 0.92f));
    g.strokePath (val, juce::PathStrokeType (big ? 4.0f : 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // modulation arc (what the modulation does to the value right now)
    if (modTarget >= 0)
    {
        const float mv = proc.modNorm[modTarget].load();
        if (std::abs (mv - v) > 0.002f)
        {
            juce::Path m; const float mr = trackR + (big ? 5.0f : 4.0f);
            m.addCentredArc (c.x, c.y, mr, mr, 0, a0 + (a1 - a0) * juce::jmin (v, mv), a0 + (a1 - a0) * juce::jmax (v, mv), true);
            g.setColour (ui::sourceColour (1).withAlpha (0.95f));
            g.strokePath (m, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // pan ring
    if (ringParam)
    {
        const float rr = R * 0.95f, pv = ringParam->getValue();
        juce::Path ring; ring.addCentredArc (c.x, c.y, rr, rr, 0, a0, a1, true);
        g.setColour (ui::bg.brighter (0.04f)); g.strokePath (ring, juce::PathStrokeType (2.5f));
        juce::Path pr; const float mid = (a0 + a1) * 0.5f, pa = a0 + (a1 - a0) * pv;
        pr.addCentredArc (c.x, c.y, rr, rr, 0, juce::jmin (mid, pa), juce::jmax (mid, pa) + 0.001f, true);
        g.setColour (ui::accent2.withAlpha (0.8f)); g.strokePath (pr, juce::PathStrokeType (2.5f));
        const juce::Point<float> dot (c.x + rr * std::sin (pa), c.y - rr * std::cos (pa));
        g.setColour (ui::accent2); g.fillEllipse (juce::Rectangle<float> (5, 5).withCentre (dot));
    }

    // body: flat dark disc with soft top light
    juce::ColourGradient body (ui::panel2.brighter (0.22f), c.x, c.y - bodyR, ui::panel.darker (0.35f), c.x, c.y + bodyR, false);
    g.setGradientFill (body); g.fillEllipse (juce::Rectangle<float> (bodyR * 2, bodyR * 2).withCentre (c));
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawEllipse (juce::Rectangle<float> (bodyR * 2, bodyR * 2).withCentre (c), 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.06f)); g.drawEllipse (juce::Rectangle<float> (bodyR * 2 - 2, bodyR * 2 - 2).withCentre (c), 1.0f);
    if (dropHover) { g.setColour (ui::sourceColour (1).withAlpha (0.35f)); g.fillEllipse (juce::Rectangle<float> (bodyR * 2, bodyR * 2).withCentre (c)); }

    // pointer
    const float pa = a0 + (a1 - a0) * v;
    const juce::Point<float> p0 (c.x + bodyR * 0.35f * std::sin (pa), c.y - bodyR * 0.35f * std::cos (pa));
    const juce::Point<float> p1 (c.x + bodyR * 0.86f * std::sin (pa), c.y - bodyR * 0.86f * std::cos (pa));
    g.setColour (ui::text); g.drawLine ({ p0, p1 }, big ? 2.6f : 2.0f);

    // label / value
    auto lr = getLocalBounds().toFloat().removeFromBottom (big ? 18.0f : 15.0f);
    const bool showVal = hover || showValueAlways || draggingRing || isMouseButtonDown();
    juce::String t = label;
    if (showVal) t = (draggingRing && ringParam) ? ringParam->getCurrentValueAsText() : param->getCurrentValueAsText();
    g.setColour (showVal ? ui::accent2 : ui::dim);
    g.setFont (ui::font (big ? 12.5f : 11.0f, showVal ? 1 : 1));
    g.drawFittedText (t, lr.toNearestInt(), juce::Justification::centred, 1);
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    if (! param) return;
    draggingRing = onRing (e.position);
    auto* p = getP (draggingRing);
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture();
        return;
    }
    proc.undo.beginNewTransaction();
    p->beginChangeGesture();
    dragStartNorm = p->getValue(); dragStart = e.position;
}
void Knob::mouseDrag (const juce::MouseEvent& e)
{
    if (! param || e.mods.isCommandDown() || e.mods.isCtrlDown()) return;
    auto* p = getP (draggingRing);
    const float sens = e.mods.isShiftDown() ? 0.0008f : 0.005f;
    const float d = (dragStart.y - e.position.y) + (e.position.x - dragStart.x) * 0.3f;
    p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, dragStartNorm + d * sens));
    repaint();
}
void Knob::mouseUp (const juce::MouseEvent&)
{
    if (! param) return;
    getP (draggingRing)->endChangeGesture();
    draggingRing = false; repaint();
}
void Knob::mouseDoubleClick (const juce::MouseEvent&)
{
    if (! param) return;
    entry = std::make_unique<juce::TextEditor>();
    addAndMakeVisible (*entry);
    entry->setFont (ui::font (12.0f)); entry->setJustification (juce::Justification::centred);
    entry->setBounds (getLocalBounds().removeFromBottom (18).withSizeKeepingCentre (juce::jmin (getWidth(), 70), 18));
    entry->setText (param->getCurrentValueAsText(), false); entry->selectAll(); entry->grabKeyboardFocus();
    juce::Component::SafePointer<Knob> safe (this);
    auto done = [this, safe]
    {
        auto txt = entry->getText().trim().toLowerCase();
        float v = txt.getFloatValue();
        if (txt.endsWith ("k") || txt.contains ("khz")) v *= 1000.0f;
        if (txt.endsWith ("%") && param->getNormalisableRange().end > 101) v = param->convertFrom0to1 (v * 0.01f);
        if (txt.isNotEmpty()) { param->beginChangeGesture(); param->setValueNotifyingHost (param->convertTo0to1 (v)); param->endChangeGesture(); }
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->entry.reset(); });
    };
    entry->onReturnKey = done; entry->onFocusLost = done;
    entry->onEscapeKey = [safe] { juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->entry.reset(); }); };
}
void Knob::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (! param) return;
    auto* p = getP (onRing (e.position));
    const float step = (e.mods.isShiftDown() ? 0.002f : 0.02f) * (w.deltaY > 0 ? 1.0f : -1.0f);
    p->beginChangeGesture(); p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, p->getValue() + step)); p->endChangeGesture();
}
void Knob::itemDropped (const SourceDetails& d)
{
    dropHover = false;
    const int src = d.description.toString().fromFirstOccurrenceOf ("src:", false, false).getIntValue();
    if (modTarget >= 0 && src > 0) proc.addModulation (src, modTarget, 30.0f);
    repaint();
}

// ============================================================================ BandDisplay
BandDisplay::BandDisplay (NeptuneProcessor& p, NeptuneEditor& e) : proc (p), ed (e)
{
    fftBuf.assign (2 * 4096, 0.0f); spec.assign (2048, -120.0f); specSmooth.assign (2048, -120.0f);
}
float BandDisplay::xForFreq (double f) const { return (float) (std::log (f / 20.0) / std::log (20000.0 / 20.0)) * getWidth(); }
double BandDisplay::freqForX (float x) const { return 20.0 * std::pow (1000.0, x / juce::jmax (1, getWidth())); }
float BandDisplay::yForLevel (float db) const
{
    const float top = 34, bot = getHeight() - 26.0f;
    return juce::jmap (juce::jlimit (-36.0f, 36.0f, db), 36.0f, -36.0f, top, bot);
}
int BandDisplay::bandAt (float x) const
{
    const int nb = proc.numBands();
    int b = 0; while (b < nb - 1 && x > xForFreq (proc.crossover (b))) ++b;
    return b;
}
static float bandCentreX (const BandDisplay& d, const NeptuneProcessor& p, int b)
{
    const int nb = p.numBands();
    const float l = b == 0 ? 0.0f : d.xForFreq (p.crossover (b - 1));
    const float r = b == nb - 1 ? (float) d.getWidth() : d.xForFreq (p.crossover (b));
    return (l + r) * 0.5f;
}
juce::Rectangle<float> BandDisplay::levelButton (int b) const
{
    const float lvl = proc.param (bandID (b, "level"));
    return juce::Rectangle<float> (26, 26).withCentre ({ bandCentreX (*this, proc, b), yForLevel (lvl <= -47.9f ? -36.0f : lvl) });
}
juce::Rectangle<float> BandDisplay::plusButton() const { return juce::Rectangle<float> (20, 20).withCentre ({ hoverX, 13.0f }); }
juce::Rectangle<float> BandDisplay::soloRect (int b) const
{
    const float l = b == 0 ? 0.0f : xForFreq (proc.crossover (b - 1));
    return { l + 6, 30, 17, 15 };
}
juce::Rectangle<float> BandDisplay::muteRect (int b) const { return soloRect (b).translated (20, 0); }
juce::Rectangle<float> BandDisplay::slopeRect (int k) const
{
    return juce::Rectangle<float> (34, 16).withCentre ({ xForFreq (proc.crossover (k)), getHeight() * 0.5f });
}

void BandDisplay::updateSpectrum()
{
    const int N = 4096;
    const int w = proc.fifoWrite.load();
    for (int i = 0; i < N; ++i)
    {
        const float win = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * i / (N - 1));
        fftBuf[(size_t) i] = proc.fifo[(w - N + i) & (NeptuneProcessor::kFifo - 1)] * win;
    }
    std::fill (fftBuf.begin() + N, fftBuf.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftBuf.data(), true);
    for (int i = 0; i < 2048; ++i)
    {
        const float db = juce::Decibels::gainToDecibels (fftBuf[(size_t) i] / (N * 0.25f), -140.0f);
        specSmooth[(size_t) i] = db > specSmooth[(size_t) i] ? db : specSmooth[(size_t) i] * 0.86f + db * 0.14f;
    }
}

void BandDisplay::paint (juce::Graphics& g)
{
    const float W = (float) getWidth(), H = (float) getHeight();
    juce::ColourGradient bgG (juce::Colour (0xff0b1830), 0, 0, juce::Colour (0xff050a14), 0, H, false);
    g.setGradientFill (bgG); g.fillRect (getLocalBounds());

    // grid
    g.setFont (ui::font (10.5f));
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const float x = xForFreq (f);
        g.setColour (ui::grid); g.drawVerticalLine ((int) x, 26, H - 18);
        g.setColour (ui::dim.withAlpha (0.8f));
        g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), (int) x - 20, (int) H - 17, 40, 14, juce::Justification::centred);
    }
    for (int db = -24; db <= 24; db += 12)
    {
        const float y = yForLevel ((float) db);
        g.setColour (db == 0 ? ui::grid.brighter (0.25f) : ui::grid); g.drawHorizontalLine ((int) y, 0, W);
        g.setColour (ui::dim.withAlpha (0.7f));
        g.drawText ((db > 0 ? "+" : "") + juce::String (db), (int) W - 34, (int) y - 8, 30, 14, juce::Justification::centredRight);
    }

    // analyzer (output)
    {
        const double fs = proc.getFs();
        juce::Path p; bool started = false;
        for (int px = 0; px < (int) W; px += 2)
        {
            const double f = freqForX ((float) px);
            const float bin = (float) (f / fs * 4096.0);
            float v;
            if (bin < 12.0f)
            {
                const int b0 = juce::jlimit (1, 2046, (int) bin); const float fr = bin - (float) b0;
                const float a = specSmooth[(size_t) juce::jmax (1, b0 - 1)], b = specSmooth[(size_t) b0], c = specSmooth[(size_t) b0 + 1], d = specSmooth[(size_t) juce::jmin (2047, b0 + 2)];
                v = b + 0.5f * fr * (c - a + fr * (2.0f * a - 5.0f * b + 4.0f * c - d + fr * (3.0f * (b - c) + d - a)));   // Catmull-Rom
            }
            else
            {
                const int lo = juce::jlimit (1, 2046, (int) (bin * 0.97f)), hi = juce::jlimit (lo + 1, 2047, (int) (bin * 1.03f) + 1);
                float pw = 0; for (int k = lo; k <= hi; ++k) pw += std::pow (10.0f, specSmooth[(size_t) k] * 0.1f);
                v = 10.0f * std::log10 (pw / (float) (hi - lo + 1) + 1e-20f);
            }
            v += 4.5f * (float) std::log2 (juce::jmax (1.0, f / 1000.0));
            const float y = juce::jmap (juce::jlimit (-96.0f, 0.0f, v), 0.0f, -96.0f, 34.0f, H - 18);
            if (! started) { p.startNewSubPath ((float) px, H - 18); started = true; }
            p.lineTo ((float) px, y);
        }
        p.lineTo (W, H - 18); p.closeSubPath();
        g.setGradientFill (juce::ColourGradient (ui::accent.withAlpha (0.28f), 0, H * 0.3f, ui::accent.withAlpha (0.02f), 0, H, false));
        g.fillPath (p);
        g.setColour (ui::accent2.withAlpha (0.45f)); g.strokePath (p, juce::PathStrokeType (1.0f));
    }

    // bands
    const int nb = proc.numBands();
    const int sel = ed.selectedBand();
    bool anySolo = false;
    for (int b = 0; b < nb; ++b) anySolo |= proc.param (bandID (b, "solo")) > 0.5f;
    for (int b = 0; b < nb; ++b)
    {
        const float l = b == 0 ? 0.0f : xForFreq (proc.crossover (b - 1));
        const float r = b == nb - 1 ? W : xForFreq (proc.crossover (b));
        const auto col = ui::bandColour (b);
        const bool on = proc.param (bandID (b, "on")) > 0.5f;
        const bool audible = proc.param (bandID (b, "mute")) < 0.5f && (! anySolo || proc.param (bandID (b, "solo")) > 0.5f);
        g.setGradientFill (juce::ColourGradient (col.withAlpha (b == sel ? 0.20f : 0.07f), 0, 26, col.withAlpha (0.0f), 0, H, false));
        if (audible) g.fillRect (juce::Rectangle<float> (l, 26, r - l, H - 44));
        if (b == sel) { g.setColour (col.withAlpha (0.9f)); g.fillRect (juce::Rectangle<float> (l, 26, r - l, 2)); }

        // band name: style + drive
        const int st = (int) proc.param (bandID (b, "style"));
        g.setColour (on ? (b == sel ? ui::text : ui::dim) : ui::dim.withAlpha (0.5f));
        g.setFont (ui::font (11.5f, 1));
        auto lab = juce::Rectangle<float> (l + 4, H - 44, r - l - 8, 16).toNearestInt();
        g.drawFittedText (styleNames()[st] + "  " + juce::String (juce::roundToInt (proc.param (bandID (b, "drive")))) + "%", lab, juce::Justification::centred, 1);

        // level / drive button
        auto lb = levelButton (b);
        const float drive = proc.param (bandID (b, "drive")) * 0.01f;
        const float pk = juce::jlimit (0.0f, 1.0f, proc.bandPeak[b].load() * 1.2f);
        g.setColour (col.withAlpha (0.18f + 0.3f * pk)); g.fillEllipse (lb.expanded (5 + 6 * pk));
        juce::Path arc; arc.addCentredArc (lb.getCentreX(), lb.getCentreY(), 15.5f, 15.5f, 0, -2.4f, -2.4f + 4.8f * drive, true);
        g.setColour (col.brighter (0.3f)); g.strokePath (arc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (b == sel ? col : ui::panel2); g.fillEllipse (lb);
        g.setColour (b == sel ? col.brighter (0.6f) : col); g.drawEllipse (lb, 1.5f);
        g.setColour (b == sel ? ui::bg : ui::text); g.setFont (ui::font (10.0f, 2));
        g.drawText (juce::String (b + 1), lb.toNearestInt(), juce::Justification::centred);
        if (! on) { g.setColour (ui::warn); g.drawLine (lb.getX() + 4, lb.getBottom() - 4, lb.getRight() - 4, lb.getY() + 4, 1.5f); }

        // solo / mute
        if (nb > 1 && (hoverBand == b || proc.param (bandID (b, "solo")) > 0.5f || proc.param (bandID (b, "mute")) > 0.5f))
        {
            for (int m = 0; m < 2; ++m)
            {
                auto rr = m == 0 ? soloRect (b) : muteRect (b);
                const bool act = proc.param (bandID (b, m == 0 ? "solo" : "mute")) > 0.5f;
                g.setColour (act ? (m == 0 ? juce::Colour (0xffffc24a) : ui::warn) : ui::panel2.withAlpha (0.9f)); g.fillRoundedRectangle (rr, 3);
                g.setColour (act ? ui::bg : ui::text); g.setFont (ui::font (10.0f, 2));
                g.drawText (m == 0 ? "S" : "M", rr.toNearestInt(), juce::Justification::centred);
            }
        }
    }
    // crossovers
    for (int k = 0; k < nb - 1; ++k)
    {
        const float x = xForFreq (proc.crossover (k));
        g.setColour (ui::accent2.withAlpha (drag == crossoverDrag && dragIndex == k ? 1.0f : 0.55f));
        g.drawLine (x, 26, x, H - 18, 1.4f);
        auto sr = slopeRect (k);
        g.setColour (ui::panel); g.fillRoundedRectangle (sr, 4);
        g.setColour (ui::accent2.withAlpha (0.7f)); g.drawRoundedRectangle (sr, 4, 1);
        g.setColour (ui::text); g.setFont (ui::font (10.0f, 1));
        g.drawText (juce::String (6 << (int) proc.param ("slope" + juce::String (k + 1))), sr.toNearestInt(), juce::Justification::centred);
        g.setColour (ui::dim); g.setFont (ui::font (10.0f));
        const double f = proc.crossover (k);
        g.drawText (f >= 1000 ? juce::String (f / 1000.0, 2) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz", (int) x - 40, (int) sr.getBottom() + 3, 80, 12, juce::Justification::centred);
    }
    // '+' button
    if (hoverX >= 0 && nb < kMaxBands)
    {
        auto pr = plusButton();
        g.setColour (ui::accent.withAlpha (0.9f)); g.fillEllipse (pr);
        drawIcon (g, "+", pr.reduced (4), ui::bg);
        g.setColour (ui::accent.withAlpha (0.25f)); g.drawVerticalLine ((int) hoverX, pr.getBottom(), H - 18);
    }
    g.setColour (ui::edge); g.drawRect (getLocalBounds(), 1);
}

void BandDisplay::mouseMove (const juce::MouseEvent& e)
{
    hoverX = e.position.y < 40 ? e.position.x : -1;
    hoverBand = bandAt (e.position.x);
    repaint();
}
void BandDisplay::mouseDown (const juce::MouseEvent& e)
{
    const int nb = proc.numBands();
    proc.undo.beginNewTransaction();
    drag = none;
    if (hoverX >= 0 && nb < kMaxBands && plusButton().expanded (3).contains (e.position))
    {
        proc.splitBandAt (freqForX (e.position.x));
        ed.selectBand (juce::jmin (proc.numBands() - 1, bandAt (e.position.x) ));
        return;
    }
    for (int k = 0; k < nb - 1; ++k)
        if (slopeRect (k).contains (e.position))
        {
            const juce::String id = "slope" + juce::String (k + 1);
            proc.setParam (id, (float) (((int) proc.param (id) + 1) % 4));
            return;
        }
    for (int b = 0; b < nb && nb > 1; ++b)
    {
        if (soloRect (b).contains (e.position)) { const auto id = bandID (b, "solo"); proc.setParam (id, proc.param (id) > 0.5f ? 0.0f : 1.0f); return; }
        if (muteRect (b).contains (e.position)) { const auto id = bandID (b, "mute"); proc.setParam (id, proc.param (id) > 0.5f ? 0.0f : 1.0f); return; }
    }
    for (int b = 0; b < nb; ++b)
        if (levelButton (b).expanded (4).contains (e.position))
        {
            ed.selectBand (b);
            if (e.mods.isCommandDown() || e.mods.isCtrlDown()) { proc.setParam (bandID (b, "level"), 0); return; }
            drag = levelDrag; dragIndex = b; dragStart = e.position;
            dragStartLevel = proc.param (bandID (b, "level")); dragStartDrive = proc.param (bandID (b, "drive"));
            dragStartLo = b > 0 ? proc.param ("x" + juce::String (b)) : 0;
            dragStartHi = b < nb - 1 ? proc.param ("x" + juce::String (b + 1)) : 0;
            return;
        }
    for (int k = 0; k < nb - 1; ++k)
        if (std::abs (e.position.x - xForFreq (proc.crossover (k))) < 6)
        { drag = crossoverDrag; dragIndex = k; return; }
    ed.selectBand (bandAt (e.position.x));
}
void BandDisplay::mouseDrag (const juce::MouseEvent& e)
{
    const int nb = proc.numBands();
    if (drag == crossoverDrag)
    {
        const double lo = dragIndex > 0 ? proc.crossover (dragIndex - 1) * 1.07 : 40.0;
        const double hi = dragIndex < nb - 2 ? proc.crossover (dragIndex + 1) / 1.07 : 18000.0;
        proc.setParam ("x" + juce::String (dragIndex + 1), (float) juce::jlimit (lo, hi, freqForX (e.position.x)));
    }
    else if (drag == levelDrag)
    {
        const int b = dragIndex;
        const float dy = dragStart.y - e.position.y, fine = e.mods.isShiftDown() ? 0.15f : 1.0f;
        if (e.mods.isAltDown()) proc.setParam (bandID (b, "drive"), juce::jlimit (0.0f, 100.0f, dragStartDrive + dy * 0.5f * fine));
        else
        {
            const float range = yForLevel (-36) - yForLevel (36);
            proc.setParam (bandID (b, "level"), juce::jlimit (-48.0f, 36.0f, dragStartLevel + dy / range * 72.0f * fine));
        }
        // horizontal: move both edges of the band
        const float dx = e.position.x - dragStart.x;
        if (std::abs (dx) > 4 && nb > 1)
        {
            const double ratio = std::pow (1000.0, dx / getWidth());
            if (b > 0) proc.setParam ("x" + juce::String (b), (float) juce::jlimit (40.0, 18000.0, dragStartLo * ratio));
            if (b < nb - 1) proc.setParam ("x" + juce::String (b + 1), (float) juce::jlimit (40.0, 18000.0, dragStartHi * ratio));
        }
    }
    repaint();
}
void BandDisplay::mouseUp (const juce::MouseEvent&) { drag = none; repaint(); }
void BandDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    for (int b = 0; b < proc.numBands(); ++b)
        if (levelButton (b).contains (e.position)) { proc.setParam (bandID (b, "level"), 0); return; }
}
void BandDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int b = bandAt (e.position.x);
    const float step = (w.deltaY > 0 ? 1.0f : -1.0f) * (e.mods.isShiftDown() ? 0.1f : 1.0f);
    if (e.mods.isAltDown()) proc.setParam (bandID (b, "drive"), juce::jlimit (0.0f, 100.0f, proc.param (bandID (b, "drive")) + step * 2));
    else proc.setParam (bandID (b, "level"), juce::jlimit (-48.0f, 36.0f, proc.param (bandID (b, "level")) + step * 0.5f));
}

// ============================================================================ BandPanel
BandPanel::BandPanel (NeptuneProcessor& p, NeptuneEditor& e)
    : mix (p, "MIX"), fb (p, "FEEDBACK"), fbFreq (p, "FREQ"), dyn (p, "DYNAMICS"), drive (p, "DRIVE", true),
      bass (p, "BASS"), mid (p, "MID"), treble (p, "TREBLE"), pres (p, "PRESENCE"), level (p, "LEVEL", true), proc (p), ed (e)
{
    for (auto* k : { &mix, &fb, &fbFreq, &dyn, &drive, &bass, &mid, &treble, &pres, &level }) addAndMakeVisible (*k);
    enable.icon = "power"; enable.setButtonText ({}); enable.setTooltip ("Enable / bypass the selected band");
    remove.icon = "x"; remove.setButtonText ({}); remove.setTooltip ("Remove the selected band");
    presetBtn.icon = "menu"; presetBtn.setButtonText ({}); presetBtn.setTooltip ("Band presets");
    for (auto* b : { &enable, &remove, &styleBtn, &presetBtn }) addAndMakeVisible (*b);
    styleBtn.onClick = [this] { showStyleMenu(); };
    presetBtn.onClick = [this] { showBandPresetMenu(); };
    remove.onClick = [this] { proc.removeBand (band); ed.selectBand (juce::jmin (band, proc.numBands() - 1)); };
    setBand (0);
}
void BandPanel::setBand (int b)
{
    band = b;
    const int t = b * kBandParams;
    mix.attach (bandID (b, "mix"), {}, t + 1);
    fb.attach (bandID (b, "fb"), {}, t + 2);
    fbFreq.attach (bandID (b, "fbfreq"), {}, t + 3);
    dyn.attach (bandID (b, "dyn"), {}, t + 4);
    bass.attach (bandID (b, "bass"), {}, t + 5);
    mid.attach (bandID (b, "mid"), {}, t + 6);
    treble.attach (bandID (b, "treble"), {}, t + 7);
    pres.attach (bandID (b, "pres"), {}, t + 8);
    drive.attach (bandID (b, "drive"), bandID (b, "dpan"), t + 0);
    level.attach (bandID (b, "level"), bandID (b, "lpan"), t + 9);
    const auto col = ui::bandColour (b);
    for (auto* k : { &mix, &fb, &fbFreq, &dyn, &drive, &bass, &mid, &treble, &pres, &level }) k->colour = col;
    enableAtt = std::make_unique<juce::ButtonParameterAttachment> (*proc.apvts.getParameter (bandID (b, "on")), enable, &proc.undo);
    enable.onColour = col;
    repaint();
}
void BandPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    auto left = r.removeFromLeft (30);
    enable.setBounds (left.removeFromTop (28)); left.removeFromTop (6); presetBtn.setBounds (left.removeFromTop (28));
    remove.setBounds (r.removeFromRight (28).removeFromTop (28));
    r.removeFromLeft (6);
    auto knob = [&] (Knob& k, int w) { k.setBounds (r.removeFromLeft (w)); };
    auto small = r.getHeight() - 30;
    juce::ignoreUnused (small);
    auto col1 = r.removeFromLeft (64); mix.setBounds (col1.withSizeKeepingCentre (64, 70).translated (0, 8));
    auto col2 = r.removeFromLeft (118);
    fb.setBounds (col2.removeFromLeft (64).withSizeKeepingCentre (64, 70).translated (0, 8));
    fbFreq.setBounds (col2.withSizeKeepingCentre (52, 56).translated (0, 15));
    auto col3 = r.removeFromLeft (70); dyn.setBounds (col3.withSizeKeepingCentre (66, 70).translated (0, 8));
    r.removeFromLeft (8);
    auto st = r.removeFromLeft (150); styleBtn.setBounds (st.withSizeKeepingCentre (146, 44));
    r.removeFromLeft (6);
    knob (drive, 112);
    r.removeFromLeft (8);
    auto tone = r.removeFromLeft (132);
    auto t1 = tone.removeFromTop (tone.getHeight() / 2), t2 = tone;
    bass.setBounds (t1.removeFromLeft (66)); mid.setBounds (t1);
    treble.setBounds (t2.removeFromLeft (66)); pres.setBounds (t2);
    r.removeFromLeft (8);
    knob (level, 96);
}
void BandPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1);
    const auto col = ui::bandColour (band);
    juce::ColourGradient bgG (ui::panel2, 0, 0, ui::panel, 0, r.getBottom(), false);
    g.setGradientFill (bgG); g.fillRoundedRectangle (r, 8);
    g.setColour (col.withAlpha (0.65f)); g.drawRoundedRectangle (r, 8, 1.2f);
    g.setColour (col); g.fillRoundedRectangle (r.withHeight (3).reduced (14, 0), 1.5f);
    // style button content
    auto sb = styleBtn.getBounds().toFloat();
    const int st = (int) proc.param (bandID (band, "style"));
    g.setColour (ui::dim); g.setFont (ui::font (10.0f, 2));
    g.drawText ("STYLE  -  " + styleGroups()[st].toUpperCase(), sb.withY (sb.getY() - 16).withHeight (14).toNearestInt(), juce::Justification::centred);
    g.setColour (ui::dim); g.setFont (ui::font (10.0f, 2));
    auto tone = juce::Rectangle<int> (bass.getX(), juce::jmax (8, bass.getY() - 12), pres.getRight() - bass.getX(), 12);
    juce::ignoreUnused (tone);
    g.drawText ("BAND " + juce::String (band + 1), juce::Rectangle<int> (8, getHeight() - 18, 60, 14), juce::Justification::centredLeft);
}

void BandPanel::showStyleMenu()
{
    juce::PopupMenu m;
    const int cur = (int) proc.param (bandID (band, "style"));
    juce::String last;
    for (int i = 0; i < kNumStyles; ++i)
    {
        if (styleGroups()[i] != last)
        {
            last = styleGroups()[i];
            if (last == "Saturation" || last == "FX") m.addColumnBreak();
            m.addSectionHeader (last == "FX" ? "FX" : last);
        }
        m.addItem (i + 1, styleNames()[i], true, i == cur);
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (styleBtn).withMinimumNumColumns (3),
        [this] (int r) { if (r > 0) { proc.undo.beginNewTransaction(); proc.setParam (bandID (band, "style"), (float) (r - 1)); repaint(); } });
}
void BandPanel::showBandPresetMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Band " + juce::String (band + 1));
    m.addItem (1, "Reset band to default");
    if (proc.numBands() > 1)
    {
        juce::PopupMenu cp;
        for (int b = 0; b < proc.numBands(); ++b) if (b != band) cp.addItem (100 + b, "Band " + juce::String (b + 1));
        m.addSubMenu ("Copy settings to", cp);
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetBtn), [this] (int r)
    {
        proc.undo.beginNewTransaction();
        if (r == 1)
            for (auto* k : { "on", "solo", "mute", "style", "drive", "mix", "fb", "fbfreq", "dyn", "bass", "mid", "treble", "pres", "level", "dpan", "lpan" })
                if (auto* p = proc.apvts.getParameter (bandID (band, k))) { p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); }
        if (r >= 100) proc.copyBand (band, r - 100);
    });
}

// ============================================================================ Modulation
ModSource::ModSource (NeptuneProcessor& p, NeptuneEditor& e, int s) : src (s), proc (p), ed (e) { history.assign (64, 0.0f); }
void ModSource::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2);
    const auto col = ui::sourceColour (src);
    g.setColour (ui::panel2); g.fillRoundedRectangle (r, 6);
    g.setColour (ed.proc.param ("bypass") > 0.5f ? ui::edge : col.withAlpha (0.55f)); g.drawRoundedRectangle (r, 6, 1);
    // in-use dot in the drag handle
    bool used = false;
    for (int s = 1; s <= kNumSlots; ++s)
        used |= (int) proc.param ("m" + juce::String (s) + "_src") == src && (int) proc.param ("m" + juce::String (s) + "_dst") > 0;
    auto h = handle();
    g.setColour (col); g.drawEllipse (h, 1.5f);
    if (used) g.fillEllipse (h.reduced (3.5f));
    // mini graph
    auto gr = r.reduced (8, 0).withTrimmedTop (20).withTrimmedBottom (18);
    g.setColour (ui::bg.withAlpha (0.6f)); g.fillRoundedRectangle (gr, 3);
    juce::Path p;
    const bool bip = src == 1 || src == 2;
    for (int i = 0; i < (int) history.size(); ++i)
    {
        const float v = bip ? history[(size_t) i] * 0.5f + 0.5f : history[(size_t) i];
        const float x = gr.getX() + gr.getWidth() * i / (history.size() - 1.0f), y = gr.getBottom() - 3 - v * (gr.getHeight() - 6);
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (col); g.strokePath (p, juce::PathStrokeType (1.5f));
    g.setColour (ui::text.withAlpha (0.85f)); g.setFont (ui::font (11.0f, 1));
    g.drawText (sourceNames()[src], r.removeFromBottom (17).toNearestInt(), juce::Justification::centred);
}
void ModSource::mouseDown (const juce::MouseEvent& e) { dragging = handle().expanded (6).contains (e.position); }
void ModSource::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging && e.getDistanceFromDragStart() > 4)
    {
        juce::Image img (juce::Image::ARGB, 18, 18, true);
        { juce::Graphics g (img); g.setColour (ui::sourceColour (src)); g.fillEllipse (1, 1, 16, 16); }
        ed.startDragging ("src:" + juce::String (src), this, juce::ScaledImage (img), true);
        dragging = false;
    }
}
void ModSource::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mouseWasDraggedSinceMouseDown()) ed.openSource (src);
}

ModStrip::ModStrip (NeptuneProcessor& p, NeptuneEditor& e) : proc (p), ed (e)
{
    for (int s = 1; s < kNumSources; ++s) { auto* m = sources.add (new ModSource (p, e, s)); addAndMakeVisible (m); }
    slotsBtn.setButtonText ("Mod Slots"); addAndMakeVisible (slotsBtn);
    slotsBtn.onClick = [this] { ed.openSlots(); };
    slotsBtn.setTooltip ("Show every modulation connection and its amount");
}
void ModStrip::paint (juce::Graphics& g)
{
    g.setColour (ui::panel); g.fillRect (getLocalBounds());
    g.setColour (ui::edge); g.drawHorizontalLine (0, 0, (float) getWidth());
    g.setColour (ui::dim); g.setFont (ui::font (10.5f, 2));
    g.drawText ("MODULATION", 14, 10, 120, 14, juce::Justification::centredLeft);
    g.setFont (ui::font (10.5f));
    g.drawFittedText ("Drag a source's dot onto any knob", juce::Rectangle<int> (14, 26, 118, 30), juce::Justification::topLeft, 2);
}
void ModStrip::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    auto left = r.removeFromLeft (128);
    slotsBtn.setBounds (left.removeFromBottom (26).withWidth (110).translated (4, 0));
    const int w = (r.getWidth() - 6 * 6) / 7;
    for (auto* s : sources) { s->setBounds (r.removeFromLeft (w)); r.removeFromLeft (6); }
}
void ModStrip::tick()
{
    for (auto* s : sources)
    {
        s->history.erase (s->history.begin()); s->history.push_back (proc.srcValue[s->src].load());
        s->repaint();
    }
}

// ============================================================================ PopPanel
namespace
{
class XYPad : public juce::Component
{
public:
    XYPad (NeptuneProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1);
        g.setColour (ui::bg); g.fillRoundedRectangle (r, 4);
        g.setColour (ui::grid); for (int i = 1; i < 4; ++i) { g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * i / 4), r.getX(), r.getRight()); g.drawVerticalLine ((int) (r.getX() + r.getWidth() * i / 4), r.getY(), r.getBottom()); }
        g.setColour (ui::edge); g.drawRoundedRectangle (r, 4, 1);
        const float x = r.getX() + r.getWidth() * proc.param ("xy_x") * 0.01f, y = r.getBottom() - r.getHeight() * proc.param ("xy_y") * 0.01f;
        g.setColour (ui::sourceColour (5).withAlpha (0.3f)); g.fillEllipse (x - 11, y - 11, 22, 22);
        g.setColour (ui::sourceColour (5)); g.fillEllipse (x - 6, y - 6, 12, 12);
    }
    void mouseDown (const juce::MouseEvent& e) override { proc.undo.beginNewTransaction(); mouseDrag (e); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto r = getLocalBounds().toFloat().reduced (1);
        proc.setParam ("xy_x", juce::jlimit (0.0f, 100.0f, (e.position.x - r.getX()) / r.getWidth() * 100.0f));
        proc.setParam ("xy_y", juce::jlimit (0.0f, 100.0f, (r.getBottom() - e.position.y) / r.getHeight() * 100.0f));
        repaint();
    }
private:
    NeptuneProcessor& proc;
};
class AmountSlider : public juce::Slider
{
public:
    AmountSlider() : juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight) { setTextBoxStyle (juce::Slider::TextBoxRight, false, 46, 18); }
};
}

PopPanel::PopPanel (NeptuneProcessor& p, NeptuneEditor& e) : proc (p), ed (e)
{
    close.icon = "x"; close.setButtonText ({}); addAndMakeVisible (close);
    close.onClick = [this] { clear(); setVisible (false); mode = -1; };
}
void PopPanel::clear() { sliderAtts.clear(); batts.clear(); catts.clear(); knobs.clear(); extras.clear(); }
void PopPanel::addChoice (const juce::String& id, juce::Rectangle<int> r)
{
    auto* cb = new juce::ComboBox(); extras.add (cb); addAndMakeVisible (cb);
    cb->addItemList (proc.apvts.getParameter (id)->getAllValueStrings(), 1);
    cb->setBounds (r);
    catts.add (new juce::ComboBoxParameterAttachment (*proc.apvts.getParameter (id), *cb, &proc.undo));
}
void PopPanel::addToggle (const juce::String& id, const juce::String& label, juce::Rectangle<int> r)
{
    auto* b = new FlatButton (label, true); b->drawAsLed = true; extras.add (b); addAndMakeVisible (b); b->setBounds (r);
    batts.add (new juce::ButtonParameterAttachment (*proc.apvts.getParameter (id), *b, &proc.undo));
}
void PopPanel::showSource (int s)
{
    clear(); mode = s; title = sourceNames()[s];
    auto K = [&] (const juce::String& id, const juce::String& lab, int x)
    {
        auto* k = knobs.add (new Knob (proc, lab)); k->attach (id); k->colour = ui::sourceColour (s);
        addAndMakeVisible (k); k->setBounds (x, 40, 66, 74); return k;
    };
    auto L = [&] (const juce::String& t, juce::Rectangle<int> r)
    {
        auto* l = new juce::Label ({}, t); l->setFont (ui::font (11.0f, 1)); l->setColour (juce::Label::textColourId, ui::dim);
        extras.add (l); addAndMakeVisible (l); l->setBounds (r);
    };
    if (s == 1 || s == 2)
    {
        const juce::String pre = "lfo" + juce::String (s);
        K (pre + "_rate", "RATE", 20);
        L ("Shape", { 104, 40, 120, 16 }); addChoice (pre + "_shape", { 104, 58, 150, 24 });
        L ("Tempo sync", { 270, 40, 120, 16 }); addChoice (pre + "_sync", { 270, 58, 120, 24 });
        L ("Sync follows the Studio Pro tempo and song position when the transport runs.", { 104, 92, 420, 18 });
    }
    else if (s == 3)
    {
        K ("ef_att", "ATTACK", 20); K ("ef_rel", "RELEASE", 96); K ("ef_gain", "GAIN", 172);
        L ("Follows the plug-in input level (-60 dB to 0 dB maps to 0..100%).", { 260, 60, 320, 18 });
    }
    else if (s == 4)
    {
        L ("Trigger", { 20, 40, 80, 16 }); addChoice ("eg_trig", { 20, 58, 90, 24 });
        K ("eg_thr", "THRESHOLD", 120); K ("eg_a", "ATTACK", 196); K ("eg_d", "DECAY", 272); K ("eg_s", "SUSTAIN", 348); K ("eg_r", "RELEASE", 424);
    }
    else if (s == 5 || s == 6)
    {
        auto* pad = new XYPad (proc); extras.add (pad); addAndMakeVisible (pad); pad->setBounds (20, 36, 150, 112);
        K ("xy_x", "X", 196); K ("xy_y", "Y", 272);
        L ("Drag the dot. Automate X / Y from Studio Pro for hands-on control.", { 352, 60, 260, 34 });
    }
    else
        L ("Moves with MIDI CC 1 (mod wheel). In Studio Pro, route a MIDI track to this plug-in to use it.", { 20, 50, 520, 36 });
    setVisible (true); resized(); repaint();
}
void PopPanel::showSlots()
{
    clear(); mode = 100; title = "Modulation slots";
    for (int s = 0; s < kNumSlots; ++s)
    {
        const int col = s / 6, row = s % 6;
        const int x = 16 + col * 330, y = 32 + row * 22;
        const juce::String pre = "m" + juce::String (s + 1);
        addChoice (pre + "_src", { x, y, 104, 20 });
        addChoice (pre + "_dst", { x + 108, y, 130, 20 });
        auto* sl = new AmountSlider(); extras.add (sl); addAndMakeVisible (sl); sl->setBounds (x + 242, y, 80, 20);
        sl->setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        sl->setTooltip ("Amount");
        auto* a = new juce::SliderParameterAttachment (*proc.apvts.getParameter (pre + "_amt"), *sl, &proc.undo);
        sliderAtts.add (a);
    }
    setVisible (true); resized(); repaint();
}
void PopPanel::showOutput()
{
    clear(); mode = 200; title = "Output options";
    auto K = [&] (const juce::String& id, const juce::String& ring, const juce::String& lab, int x, int t)
    {
        auto* k = knobs.add (new Knob (proc, lab)); k->attach (id, ring, t); addAndMakeVisible (k); k->setBounds (x, 36, 76, 84);
    };
    const int base = kMaxBands * kBandParams + kMaxBands - 1;
    K ("input", "inpan", "INPUT", 20, base); K ("output", "outpan", "OUTPUT", 106, base + 1); K ("gmix", {}, "MIX", 192, base + 2);
    auto* l = new juce::Label ({}, "Channel mode"); l->setFont (ui::font (11.0f, 1)); l->setColour (juce::Label::textColourId, ui::dim);
    extras.add (l); addAndMakeVisible (l); l->setBounds (300, 38, 120, 16);
    addChoice ("chmode", { 300, 56, 130, 24 });
    addToggle ("unrestricted", "Unrestricted Feedback", { 300, 92, 190, 26 });
    setVisible (true); resized(); repaint();
}
void PopPanel::resized() { close.setBounds (getWidth() - 30, 6, 22, 22); }
void PopPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1);
    g.setColour (juce::Colour (0xf20b1626)); g.fillRoundedRectangle (r, 8);
    const auto col = mode >= 0 && mode < 8 ? ui::sourceColour (mode) : ui::accent;
    g.setColour (col.withAlpha (0.7f)); g.drawRoundedRectangle (r, 8, 1.2f);
    g.setColour (ui::text); g.setFont (ui::font (13.0f, 2));
    g.drawText (title.toUpperCase(), 16, 8, 300, 18, juce::Justification::centredLeft);
}
void PopPanel::tick() { for (auto* k : knobs) k->repaint(); }

// ============================================================================ Editor
NeptuneEditor::NeptuneEditor (NeptuneProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p, *this), bandPanel (p, *this), modStrip (p, *this), pop (p, *this)
{
    setLookAndFeel (&lnf());
    addAndMakeVisible (display); addAndMakeVisible (bandPanel); addAndMakeVisible (modStrip);
    addChildComponent (pop);
    undoBtn.icon = "undo"; redoBtn.icon = "redo"; prevBtn.icon = "<"; nextBtn.icon = ">";
    for (auto* b : { &undoBtn, &redoBtn, &prevBtn, &nextBtn }) b->setButtonText ({});
    for (auto* b : { &undoBtn, &redoBtn, &aBtn, &bBtn, &copyBtn, &helpBtn, &prevBtn, &nextBtn, &presetBtn, &linBtn, &hqBtn, &bypassBtn, &outBtn })
        addAndMakeVisible (*b);
    aBtn.setClickingTogglesState (false); bBtn.setClickingTogglesState (false);
    undoBtn.onClick = [this] { proc.undo.undo(); };
    redoBtn.onClick = [this] { proc.undo.redo(); };
    aBtn.onClick = [this] { proc.selectSlot (0); };
    bBtn.onClick = [this] { proc.selectSlot (1); };
    copyBtn.setButtonText ("Copy A>B"); copyBtn.onClick = [this] { proc.copyToOtherSlot(); };
    copyBtn.setTooltip ("Copy the current settings to the other A/B slot");
    helpBtn.onClick = []
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Neptune MB",
            "Multiband saturation in the spirit of FabFilter Saturn 2.\n\n"
            "Display: click + at the top to split a band, drag a split to move it, click the slope badge to change it, "
            "drag a band's number button up/down for level (Alt = drive), sideways to move the band.\n"
            "Knobs: drag, Shift = fine, Ctrl-click = reset, double-click = type a value. Outer ring = pan.\n"
            "Modulation: drag a source's dot onto any knob, then edit amounts in Mod Slots.");
    };
    auto showPresets = [this]
    {
        juce::PopupMenu m; m.addSectionHeader ("Factory presets");
        auto names = NeptuneProcessor::factoryPresetNames();
        for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, i == presetIndex);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetBtn), [this] (int r)
        { if (r > 0) { presetIndex = r - 1; proc.loadFactoryPreset (presetIndex); selectBand (0); } });
    };
    presetBtn.onClick = showPresets;
    prevBtn.onClick = [this] { const int n = NeptuneProcessor::factoryPresetNames().size(); presetIndex = (presetIndex + n - 1) % n; proc.loadFactoryPreset (presetIndex); selectBand (0); };
    nextBtn.onClick = [this] { const int n = NeptuneProcessor::factoryPresetNames().size(); presetIndex = (presetIndex + 1) % n; proc.loadFactoryPreset (presetIndex); selectBand (0); };
    linAtt = std::make_unique<juce::ButtonParameterAttachment> (*proc.apvts.getParameter ("linear"), linBtn, &proc.undo);
    bypassAtt = std::make_unique<juce::ButtonParameterAttachment> (*proc.apvts.getParameter ("bypass"), bypassBtn, &proc.undo);
    linBtn.drawAsLed = true; bypassBtn.drawAsLed = true; bypassBtn.onColour = ui::warn;
    hqBtn.onClick = [this]
    {
        juce::PopupMenu m; m.addSectionHeader ("High Quality (oversampling)");
        const int cur = (int) proc.param ("hq");
        m.addItem (1, "Off", true, cur == 0); m.addItem (2, "Good  (8x)", true, cur == 1); m.addItem (3, "Superb  (32x)", true, cur == 2);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (hqBtn), [this] (int r) { if (r > 0) proc.setParam ("hq", (float) (r - 1)); });
    };
    outBtn.onClick = [this] { openOutput(); };
    linBtn.setTooltip ("Linear-phase crossovers and oversampling (adds latency, best for mastering)");
    setResizable (true, true);
    setResizeLimits (900, 590, 1600, 1046);
    getConstrainer()->setFixedAspectRatio (1040.0 / 680.0);
    setSize (1040, 680);
    addMouseListener (this, true);
    startTimerHz (30);
}
NeptuneEditor::~NeptuneEditor() { setLookAndFeel (nullptr); }

void NeptuneEditor::mouseDown (const juce::MouseEvent&) { proc.undo.beginNewTransaction(); }

void NeptuneEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);
    // top bar
    auto top = getLocalBounds().removeFromTop (42).toFloat();
    g.setGradientFill (juce::ColourGradient (ui::panel2, 0, 0, ui::panel, 0, 42, false)); g.fillRect (top);
    g.setColour (ui::edge); g.drawHorizontalLine (41, 0, (float) getWidth());
    // logo: ringed planet
    const juce::Point<float> c (26, 21);
    g.setGradientFill (juce::ColourGradient (ui::accent2, c.x - 6, c.y - 6, juce::Colour (0xff1550c8), c.x + 7, c.y + 7, true));
    g.fillEllipse (c.x - 8, c.y - 8, 16, 16);
    juce::Path ring; ring.addEllipse (c.x - 15, c.y - 4.5f, 30, 9);
    ring.applyTransform (juce::AffineTransform::rotation (-0.35f, c.x, c.y));
    g.setColour (ui::accent2.withAlpha (0.9f)); g.strokePath (ring, juce::PathStrokeType (1.4f));
    g.setColour (ui::text); g.setFont (ui::font (17.0f, 2)); g.drawText ("NEPTUNE", 48, 11, 100, 20, juce::Justification::centredLeft);
    g.setColour (ui::accent); g.setFont (ui::font (17.0f, 0)); g.drawText ("MB", 135, 11, 40, 20, juce::Justification::centredLeft);
    g.setColour (ui::dim); g.setFont (ui::font (10.0f, 1)); g.drawText ("MULTIBAND SATURATION", 172, 15, 150, 14, juce::Justification::centredLeft);

    // band panel tray + bottom bar
    auto tray = juce::Rectangle<int> (0, displayArea.getBottom(), getWidth(), bandPanel.getHeight() + 12);
    g.setColour (ui::bg.brighter (0.02f)); g.fillRect (tray);
    auto bottom = getLocalBounds().removeFromBottom (36);
    g.setColour (ui::panel); g.fillRect (bottom);
    g.setColour (ui::edge); g.drawHorizontalLine (bottom.getY(), 0, (float) getWidth());
    g.setColour (ui::dim); g.setFont (ui::font (10.5f));
    const int lat = proc.getLatencySamples();
    g.drawText ("Latency " + juce::String (lat) + " smp (" + juce::String (lat * 1000.0 / proc.getFs(), 1) + " ms)", bottom.reduced (12, 0).withTrimmedLeft (330), juce::Justification::centredLeft);
}
void NeptuneEditor::paintOverChildren (juce::Graphics& g)
{
    // little pointer that connects the band panel to its band
    const float bx = display.getX() + (display.levelButton (selBand).getCentreX());
    const float y = (float) bandPanel.getY();
    juce::Path tri; tri.addTriangle (bx - 8, y + 1, bx + 8, y + 1, bx, y - 7);
    g.setColour (ui::bandColour (selBand).withAlpha (0.85f)); g.fillPath (tri);
}

void NeptuneEditor::resized()
{
    const float s = getWidth() / 1040.0f;
    // Everything is laid out at 1040 x 680 and scaled
    for (auto* c : getChildren()) c->setTransform (juce::AffineTransform::scale (s));
    const int W = 1040, H = 680;
    displayArea = { 10, 50, W - 20, 330 };
    display.setBounds (displayArea);
    bandPanel.setSize (900, 142);
    layoutBandPanel();
    modStrip.setBounds (0, H - 36 - 104, W, 104);
    pop.setBounds (150, H - 36 - 104 - 176, 690, 174);
    // top bar
    prevBtn.setBounds (380, 9, 26, 24); presetBtn.setBounds (410, 9, 220, 24); nextBtn.setBounds (634, 9, 26, 24);
    undoBtn.setBounds (W - 300, 9, 30, 24); redoBtn.setBounds (W - 266, 9, 30, 24);
    aBtn.setBounds (W - 226, 9, 28, 24); bBtn.setBounds (W - 194, 9, 28, 24); copyBtn.setBounds (W - 162, 9, 82, 24);
    helpBtn.setBounds (W - 72, 9, 60, 24);
    // bottom bar
    linBtn.setBounds (12, H - 30, 118, 24); hqBtn.setBounds (138, H - 30, 170, 24);
    bypassBtn.setBounds (W - 300, H - 30, 86, 24); outBtn.setBounds (W - 206, H - 30, 194, 24);
    // the editor itself is scaled through the children transforms
    juce::ignoreUnused (s);
}

void NeptuneEditor::layoutBandPanel()
{
    const float target = juce::jlimit (10.0f, 1040.0f - 10.0f - bandPanel.getWidth(),
                                       displayArea.getX() + display.levelButton (selBand).getCentreX() - bandPanel.getWidth() * 0.5f);
    panelX = panelX < 0 ? target : panelX + (target - panelX) * 0.35f;
    bandPanel.setTopLeftPosition ((int) panelX, displayArea.getBottom() + 10);
}

void NeptuneEditor::selectBand (int b)
{
    selBand = juce::jlimit (0, proc.numBands() - 1, b);
    bandPanel.setBand (selBand);
    repaint();
}
void NeptuneEditor::openSource (int s) { if (pop.isVisible() && pop.mode == s) { pop.setVisible (false); pop.mode = -1; return; } pop.showSource (s); pop.toFront (false); }
void NeptuneEditor::openSlots()        { if (pop.isVisible() && pop.mode == 100) { pop.setVisible (false); pop.mode = -1; return; } pop.showSlots(); pop.toFront (false); }
void NeptuneEditor::openOutput()       { if (pop.isVisible() && pop.mode == 200) { pop.setVisible (false); pop.mode = -1; return; } pop.showOutput(); pop.toFront (false); }

void NeptuneEditor::timerCallback()
{
    if (selBand >= proc.numBands()) selectBand (proc.numBands() - 1);
    display.updateSpectrum(); display.repaint();
    layoutBandPanel();
    for (auto* k : { &bandPanel.mix, &bandPanel.fb, &bandPanel.fbFreq, &bandPanel.dyn, &bandPanel.drive, &bandPanel.bass,
                     &bandPanel.mid, &bandPanel.treble, &bandPanel.pres, &bandPanel.level }) k->refreshMod();
    bandPanel.styleBtn.setButtonText (styleNames()[juce::jlimit (0, kNumStyles - 1, (int) proc.param (bandID (bandPanel.band, "style")))]);
    bandPanel.repaint();
    modStrip.tick();
    if (pop.isVisible()) pop.tick();
    const int hq = (int) proc.param ("hq");
    hqBtn.setButtonText (juce::String ("High Quality: ") + (hq == 0 ? "Off" : hq == 1 ? "Good 8x" : "Superb 32x"));
    hqBtn.setToggleState (hq > 0, juce::dontSendNotification);
    aBtn.setToggleState (proc.getSlot() == 0, juce::dontSendNotification);
    bBtn.setToggleState (proc.getSlot() == 1, juce::dontSendNotification);
    undoBtn.setEnabled (proc.undo.canUndo()); redoBtn.setEnabled (proc.undo.canRedo());
    presetBtn.setButtonText (NeptuneProcessor::factoryPresetNames()[presetIndex]);
    auto fmt = [] (float v) { return (v > 0 ? "+" : "") + juce::String (v, 1); };
    outBtn.setButtonText ("In " + fmt (proc.param ("input")) + " dB   Out " + fmt (proc.param ("output")) + " dB");
    outBtn.setToggleState (pop.isVisible() && pop.mode == 200, juce::dontSendNotification);
}
