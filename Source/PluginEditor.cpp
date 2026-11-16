#include "PluginEditor.h"
#include "BinaryData.h"

using juce::Colour;
using juce::Rectangle;

static const char* bankLetters = "ABCD";
static const Colour kInk (0xff2b2a27);

static juce::Font lcdFont (float h = 21.0f)
{
    static juce::Typeface::Ptr tf = juce::Typeface::createSystemTypefaceFor (BinaryData::VT323Regular_ttf, (size_t) BinaryData::VT323Regular_ttfSize);
    return juce::Font (juce::FontOptions (tf).withHeight (h));
}

static void section (juce::Graphics& g, const juce::String& t, float cx, float y, float x1, float x2)
{
    juce::Font f (juce::FontOptions (13.5f, juce::Font::bold));
    g.setFont (f);
    const float w = f.getStringWidthFloat (t);
    g.setColour (kInk);
    g.drawText (t, juce::Rectangle<float> (cx - w * 0.5f - 3, y - 8, w + 6, 16).toNearestInt(), juce::Justification::centred);
    g.setColour (kInk.withAlpha (0.55f));
    g.drawLine (x1, y, cx - w * 0.5f - 9, y, 1.0f);
    g.drawLine (cx + w * 0.5f + 9, y, x2, y, 1.0f);
}

// ------------------------------------------------------------------ construction
HwButton* MPCSurface::mk (const juce::String& label, const juce::String& text, HwButton::Style s, Rectangle<int> btn,
                          int off, bool led, std::function<void()> fn)
{
    auto b = std::make_unique<HwButton> ();
    b->label = label; b->text = text; b->style = s; b->off = off; b->hasLed = led; b->onClick = std::move (fn);
    const int tp = (int) b->topPad();
    b->setBounds (btn.getX() - 14, btn.getY() - tp, btn.getWidth() + 28, btn.getHeight() + tp);
    addAndMakeVisible (*b);
    auto* raw = b.get();
    buttons.push_back (std::move (b));
    return raw;
}

MPCSurface::MPCSurface (MPCProcessor& p) : proc (p), browser (p)
{
    using S = HwButton::Style;
    setSize (W, H);

    masterKnob.value = proc.engine.masterVolume;
    masterKnob.onChange = [this] (float v) { proc.engine.masterVolume = v; };
    masterKnob.setBounds (41, 146, 64, 64);
    recKnob.cap = Colour (0xffc7382e);
    recKnob.value = proc.engine.previewLevel;                         // audition level
    recKnob.onChange = [this] (float v) { proc.engine.previewLevel = v; };
    recKnob.setBounds (41, 253, 64, 64);
    contrastKnob.cap = Colour (0xff3a3a38);
    contrastKnob.value = 1.0f;
    contrastKnob.onChange = [this] (float v) { lcdBright = 0.45f + 0.55f * v; };
    contrastKnob.setBounds (55, 363, 48, 48);
    for (auto* k : { &masterKnob, &recKnob, &contrastKnob }) addAndMakeVisible (*k);

    wheel.setBounds (693, 578, 96, 96);
    wheel.onTurn = [this] (int s) { adjust (s); };
    addAndMakeVisible (wheel);

    fader.setBounds (62, 905, 38, 165);
    fader.onChange = [this] (float v)
    {
        auto& pp = P();
        if (noteVarBtn->led) pp.tuneSemis = juce::jlimit (-12.0f, 12.0f, (v * 24.0f - 12.0f));
        else if (afterBtn->led) pp.pan = juce::jlimit (-1.0f, 1.0f, v * 2.0f - 1.0f);
        else pp.level = v * 1.5f;
    };
    addAndMakeVisible (fader);

    browser.setBounds (148, 148, 448, 227);
    browser.onClose = [this] { setPage (Page::Main); };
    addChildComponent (browser);

    // soft keys
    for (int i = 0; i < 4; ++i)
        mk ("SOFT KEY " + juce::String (i + 1), {}, S::Cream, { 211 + 93 * i, 464, 42, 22 }, 12, false, [this, i] { softKey (i); });

    // data entry keypad
    const char* keys[] = { "7", "8", "9", "4", "5", "6", "1", "2", "3", "0", ".", "ENTER" };
    const int kx[] = { 653, 720, 787 }, ky[] = { 165, 218, 270, 323 };
    for (int i = 0; i < 12; ++i)
    {
        const juce::String k = keys[i];
        mk (k, {}, S::Key, { kx[i % 3], ky[i / 3], 45, 24 }, 9, false, [this, k] { keypad (k); });
    }

    // commands
    const int cx1[] = { 663, 717, 771, 824 };
    mk ("DISK",     {}, S::Cream, { cx1[0] - 21, 465, 42, 23 }, 15, false, [this] { setPage (page == Page::Load ? Page::Main : Page::Load); });
    mk ("MIDI",     {}, S::Cream, { cx1[1] - 21, 465, 42, 23 }, 15, false, [this] { setPage (Page::Midi); });
    mk ("SEQ EDIT", {}, S::Cream, { cx1[2] - 21, 465, 42, 23 }, 15, false, [this] { setPage (Page::Sample); });
    mk ("OTHER",    {}, S::Cream, { cx1[3] - 21, 465, 42, 23 }, 15, false, [this] { setPage (Page::Program); });
    const int cx2[] = { 663, 717, 770, 824 };
    fStart = mk ("TEMPO",    {}, S::Cream, { cx2[0] - 21, 526, 42, 23 }, 18, true, [this] { field = Field::Start; refreshLeds(); });
    fEnd   = mk ("/SYNC",    {}, S::Cream, { cx2[1] - 21, 526, 42, 23 }, 18, true, [this] { field = Field::End;   refreshLeds(); });
    fTune  = mk ("COUNT",    {}, S::Cream, { cx2[2] - 21, 526, 42, 23 }, 18, true, [this] { field = Field::Tune;  refreshLeds(); });
    fLevel = mk ("WAIT SEQ", {}, S::Cream, { cx2[3] - 21, 526, 42, 23 }, 18, true, [this] { field = Field::Level; refreshLeds(); });

    // cursor: up/down = previous/next field, left/right = previous/next pad
    mk ({}, {}, S::Dark, { 723, 687, 42, 22 }, 0, false, [this] { field = (Field) (((int) field + 4) % 5); refreshLeds(); });
    mk ({}, {}, S::Dark, { 665, 710, 42, 22 }, 0, false, [this] { selectPad ((proc.selectedPad + 63) % 64); });
    mk ({}, {}, S::Dark, { 780, 710, 42, 22 }, 0, false, [this] { selectPad ((proc.selectedPad + 1) % 64); });
    mk ({}, {}, S::Dark, { 723, 735, 42, 22 }, 0, false, [this] { field = (Field) (((int) field + 1) % 5); refreshLeds(); });

    // real time
    const int rx[] = { 657, 712, 768, 822 };
    mk ({}, "MAIN\nSCREEN",   S::Inside, { rx[0] - 22, 801, 44, 37 }, 0, false, [this] { setPage (Page::Main); });
    mk ({}, "TIMING\nCORRECT", S::Inside, { rx[1] - 22, 801, 44, 37 }, 0, false, [this]
        { auto& q = P(); q.level = 1; q.pan = 0; q.tuneSemis = 0; q.start = 0; q.end = 1; q.reverse = false; q.oneShot = true; q.mute = q.solo = false; toast ("PAD RESET"); });
    mk ({}, "TAP\nTEMPO",     S::Inside, { rx[2] - 22, 801, 44, 37 }, 0, false, [this] { P().solo = ! P().solo; toast (P().solo ? "SOLO ON" : "SOLO OFF"); });
    mk ({}, "HELP",           S::Inside, { rx[3] - 22, 801, 44, 37 }, 0, false, [this] { setPage (page == Page::Help ? Page::Main : Page::Help); });
    mk ({}, "EDIT\nLOOP",     S::Inside, { rx[0] - 22, 853, 44, 37 }, 0, false, [this] { P().reverse = ! P().reverse; toast (P().reverse ? "REVERSE ON" : "REVERSE OFF"); });
    mk ({}, "EDIT\nPUNCH",    S::Inside, { rx[1] - 22, 853, 44, 37 }, 0, false, [this] { P().oneShot = ! P().oneShot; toast (P().oneShot ? "ONE SHOT" : "GATE"); });
    mk ({}, "EDIT\nSTOP",     S::Inside, { rx[2] - 22, 853, 44, 37 }, 0, false, [this] { P().mute = ! P().mute; toast (P().mute ? "MUTE ON" : "MUTE OFF"); });
    mk ({}, "PLAY\nSTART",    S::Inside, { rx[3] - 22, 853, 44, 37 }, 0, false, [this] { playSelected (1.0f); });

    // play / rec
    mk ({}, "<<", S::Inside, { rx[0] - 23, 940, 46, 36 }, 0, false, [this] { selectPad (((proc.bank.load() + 3) % 4) * 16 + proc.selectedPad % 16); });
    mk ({}, "<",  S::Inside, { rx[1] - 23, 940, 46, 36 }, 0, false, [this] { selectPad ((proc.selectedPad + 63) % 64); });
    mk ({}, ">",  S::Inside, { rx[2] - 23, 940, 46, 36 }, 0, false, [this] { selectPad ((proc.selectedPad + 1) % 64); });
    mk ({}, ">>", S::Inside, { rx[3] - 23, 940, 46, 36 }, 0, false, [this] { selectPad (((proc.bank.load() + 1) % 4) * 16 + proc.selectedPad % 16); });
    mk ({}, "REC",  S::Red, { rx[0] - 23, 1013, 46, 36 }, 0, false, [this]            // MIDI learn arm
        { if (proc.engine.learnPad.load() >= 0) { proc.engine.learnPad = -1; toast ("LEARN OFF"); }
          else { proc.engine.learnPad = proc.selectedPad; toast ("PLAY A MIDI NOTE..."); } });
    mk ({}, "STOP", S::Red, { rx[1] - 23, 1013, 46, 36 }, 0, false, [this] { proc.engine.postStopAll(); });
    mk ({}, "PLAY", S::Green, { rx[2] - 23, 1013, 46, 36 }, 0, false, [this] { playSelected (1.0f); });
    mk ({}, "PLAY\nSTART", S::Inside, { rx[3] - 23, 1013, 46, 36 }, 0, false, [this]    // raw sample audition
        { if (auto s = proc.engine.getSample (proc.selectedPad)) { proc.engine.setPreview (s); proc.engine.postPreview(); } });

    // left column
    mk ("PAD BANK", {}, S::Cream, { 61, 580, 42, 23 }, 10, false, [this]
        { selectPad (((proc.bank.load() + 1) % 4) * 16 + proc.selectedPad % 16); });
    fullBtn = mk ("FULL LEVEL", {}, S::Cream, { 61, 648, 42, 23 }, 21, true, [this]
        { proc.fullLevel = ! proc.fullLevel; if (proc.fullLevel) levelsBtn->led = false; refreshLeds(); });
    levelsBtn = mk ("16 LEVELS", {}, S::Cream, { 61, 717, 42, 23 }, 21, true, [this]
        { levelsBtn->led = ! levelsBtn->led; if (levelsBtn->led) proc.fullLevel = false; refreshLeds(); });
    noteVarBtn = mk (" ", {}, S::Cream, { 61, 800, 42, 23 }, 14, true, [this]
        { noteVarBtn->led = ! noteVarBtn->led; if (noteVarBtn->led) afterBtn->led = false; refreshLeds(); syncFader(); });
    afterBtn = mk ("AFTER", {}, S::Cream, { 61, 863, 42, 23 }, 20, true, [this]
        { afterBtn->led = ! afterBtn->led; if (afterBtn->led) noteVarBtn->led = false; refreshLeds(); syncFader(); });

    refreshLeds();
    setPage (Page::Main);
    startTimerHz (25);
}

// ------------------------------------------------------------------ behaviour
void MPCSurface::refreshLeds()
{
    fullBtn->led = proc.fullLevel;
    fStart->led = field == Field::Start; fEnd->led = field == Field::End;
    fTune->led = field == Field::Tune;   fLevel->led = field == Field::Level;
    for (auto& b : buttons) b->repaint();
    repaint();
}

void MPCSurface::setPage (Page p)
{
    page = p;
    browser.setVisible (p == Page::Load);
    if (p == Page::Load) browser.refresh();
    repaint();
}

std::array<juce::String, 4> MPCSurface::softLabels() const
{
    switch (page)
    {
        case Page::Main:    return { "SAMPLE", "PROGRAM", "LOAD", "SAVE" };
        case Page::Sample:  return { "REVERSE", "1SHOT/GATE", "MUTE", "MAIN" };
        case Page::Program: return { "PREV BANK", "NEXT BANK", "CLEAR PAD", "MAIN" };
        case Page::Load:    return { "SMP/PGM", "FOLDER", "RESCAN", "MAIN" };
        case Page::Save:    return { "SAVE PGM", "LOAD PGM", "", "MAIN" };
        case Page::Midi:    return { "LEARN", "NOTE -", "NOTE +", "MAIN" };
        case Page::Help:    return { "", "", "", "MAIN" };
    }
    return {};
}

void MPCSurface::softKey (int i)
{
    auto& p = P();
    switch (page)
    {
        case Page::Main:    setPage (i == 0 ? Page::Sample : i == 1 ? Page::Program : i == 2 ? Page::Load : Page::Save); break;
        case Page::Sample:
            if (i == 0) p.reverse = ! p.reverse; else if (i == 1) p.oneShot = ! p.oneShot;
            else if (i == 2) p.mute = ! p.mute; else setPage (Page::Main);
            break;
        case Page::Program:
            if (i == 0) selectPad (((proc.bank.load() + 3) % 4) * 16 + proc.selectedPad % 16);
            else if (i == 1) selectPad (((proc.bank.load() + 1) % 4) * 16 + proc.selectedPad % 16);
            else if (i == 2) proc.clearPad (proc.selectedPad); else setPage (Page::Main);
            break;
        case Page::Load:
            if (i == 0) browser.toggleMode(); else if (i == 1) browser.chooseFolder();
            else if (i == 2) browser.rescan(); else setPage (Page::Main);
            break;
        case Page::Save:
            if (i == 0)
            {
                chooser = std::make_unique<juce::FileChooser> ("Save program", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                                   .getChildFile (proc.programName + ".m3kprog"), "*.m3kprog");
                chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                    [this] (const juce::FileChooser& fc)
                    {
                        if (fc.getResult() == juce::File()) return;
                        juce::MemoryBlock mb; proc.getStateInformation (mb);
                        toast (fc.getResult().withFileExtension ("m3kprog").replaceWithData (mb.getData(), mb.getSize()) ? "PROGRAM SAVED" : "SAVE FAILED");
                    });
            }
            else if (i == 1)
            {
                chooser = std::make_unique<juce::FileChooser> ("Load program", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.m3kprog");
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [this] (const juce::FileChooser& fc)
                    {
                        juce::MemoryBlock mb;
                        if (fc.getResult().existsAsFile() && fc.getResult().loadFileAsData (mb))
                        { proc.setStateInformation (mb.getData(), (int) mb.getSize()); toast ("PROGRAM LOADED"); }
                    });
            }
            else if (i == 3) setPage (Page::Main);
            break;
        case Page::Midi:
            if (i == 0) { proc.engine.learnPad = proc.selectedPad; toast ("PLAY A MIDI NOTE..."); }
            else if (i == 1) p.midiNote = juce::jmax (0, p.midiNote - 1);
            else if (i == 2) p.midiNote = juce::jmin (127, p.midiNote + 1);
            else setPage (Page::Main);
            break;
        case Page::Help: if (i == 3) setPage (Page::Main); break;
    }
    repaint();
}

void MPCSurface::selectPad (int a)
{
    proc.selectedPad = juce::jlimit (0, mpc::numPads - 1, a);
    proc.bank = proc.selectedPad / mpc::padsPerBank;
    syncFader(); repaint();
}

void MPCSurface::adjust (int s)
{
    auto& p = P();
    switch (field)
    {
        case Field::Start: p.start = juce::jlimit (0.0f, p.end - 0.002f, p.start + 0.005f * (float) s); break;
        case Field::End:   p.end   = juce::jlimit (p.start + 0.002f, 1.0f, p.end + 0.005f * (float) s); break;
        case Field::Tune:  p.tuneSemis = juce::jlimit (-24.0f, 24.0f, std::round ((p.tuneSemis + 0.1f * (float) s) * 100.0f) / 100.0f); break;
        case Field::Level: p.level = juce::jlimit (0.0f, 1.5f, p.level + 0.01f * (float) s); break;
        case Field::Pan:   p.pan   = juce::jlimit (-1.0f, 1.0f, p.pan + 0.02f * (float) s); break;
    }
    repaint();
}

void MPCSurface::keypad (const juce::String& k)
{
    if (k == ".") { entry.clear(); return; }
    if (k == "ENTER")
    {
        const int n = entry.getIntValue();
        if (entry.isNotEmpty() && n >= 1 && n <= 64) { selectPad (n - 1); toast ("PAD " + juce::String (n)); }
        else toast ("ENTER PAD 1-64");
        entry.clear(); return;
    }
    if (entry.length() < 2) entry += k;
    repaint();
}

void MPCSurface::playSelected (float vel) { proc.engine.postNoteOn (proc.selectedPad, vel); }

void MPCSurface::syncFader()
{
    auto& p = P();
    fader.value = noteVarBtn->led ? (p.tuneSemis + 12.0f) / 24.0f : afterBtn->led ? (p.pan + 1.0f) * 0.5f : p.level / 1.5f;
    fader.value = juce::jlimit (0.0f, 1.0f, fader.value);
    fader.repaint();
}

void MPCSurface::timerCallback()
{
    const auto hits = proc.engine.hitMask.exchange (0);
    for (int i = 0; i < mpc::numPads; ++i)
        if (hits & (1ull << i)) litFrames[i] = 5;
    for (auto& f : litFrames) if (f > 0) --f;
    if (! fader.isMouseButtonDown()) syncFader();

    if (proc.status != lastStatus) { lastStatus = proc.status; statusAge = 0; }
    else if (proc.status.isNotEmpty() && ++statusAge > 75 && proc.engine.learnPad.load() < 0) { proc.status.clear(); lastStatus.clear(); }
    repaint();
}

// ------------------------------------------------------------------ pads
Rectangle<float> MPCSurface::padRect (int pad) const
{
    static const float ys[] = { 577, 698, 820, 941 }, xs[] = { 157, 268, 379, 490 };
    return { xs[pad % 4], ys[3 - pad / 4], 94.f, 96.f };
}

int MPCSurface::padAt (juce::Point<int> pt) const
{
    for (int i = 0; i < mpc::padsPerBank; ++i)
        if (padRect (i).contains (pt.toFloat())) return i;
    return -1;
}

void MPCSurface::mouseDown (const juce::MouseEvent& e)
{
    const int pb = padAt (e.getPosition());
    if (pb < 0) return;
    const int abs = proc.absPad (pb);
    if (e.mods.isPopupMenu()) { selectPad (abs); showPadMenu (pb); return; }
    heldPad = pb;
    const auto r = padRect (pb);
    if (levelsBtn->led)                      // 16 LEVELS: pad number = velocity level of the selected pad's sample
    {
        proc.engine.postNoteOn (proc.selectedPad, (float) (pb + 1) / 16.0f);
        heldPad = -1;
        return;
    }
    selectPad (abs);
    proc.status.clear();
    const float vel = proc.fullLevel ? 1.0f : 0.35f + 0.65f * ((float) e.position.y - r.getY()) / r.getHeight();
    proc.engine.postNoteOn (abs, juce::jlimit (0.1f, 1.0f, vel));
}

void MPCSurface::mouseUp (const juce::MouseEvent&)
{
    if (heldPad >= 0) proc.engine.postNoteOff (proc.absPad (heldPad));
    heldPad = -1;
}

void MPCSurface::showPadMenu (int pb)
{
    const int abs = proc.absPad (pb);
    auto& p = proc.engine.params[(size_t) abs];
    juce::PopupMenu m;
    m.addItem (1, "Load sample file...");
    m.addItem (2, "Locate missing file...", p.missing || p.samplePath.isNotEmpty());
    m.addItem (3, "Clear pad", p.samplePath.isNotEmpty());
    m.addSeparator();
    m.addItem (4, "Reverse", true, p.reverse);
    m.addItem (5, "One-shot (off = gate)", true, p.oneShot);
    m.addItem (6, "Mute", true, p.mute);
    m.addItem (7, "Solo", true, p.solo);
    m.showMenuAsync (juce::PopupMenu::Options(), [this, abs] (int r)
    {
        auto& pr = proc.engine.params[(size_t) abs];
        switch (r)
        {
            case 1: case 2:
                chooser = std::make_unique<juce::FileChooser> (r == 1 ? "Load sample" : "Locate " + pr.sampleName, proc.library.getRoot(),
                                                               "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                      [this, abs] (const juce::FileChooser& fc)
                                      { if (fc.getResult().existsAsFile()) proc.assignFileToPad (abs, fc.getResult()); });
                break;
            case 3: proc.clearPad (abs); break;
            case 4: pr.reverse = ! pr.reverse; break;
            case 5: pr.oneShot = ! pr.oneShot; break;
            case 6: pr.mute = ! pr.mute; break;
            case 7: pr.solo = ! pr.solo; break;
            default: break;
        }
    });
}

void MPCSurface::filesDropped (const juce::StringArray& files, int x, int y)
{
    const int p = padAt ({ x, y });
    if (files.isEmpty()) return;
    juce::File f (files[0]);
    if (f.hasFileExtension ("xpm")) { proc.loadXpmAsync (f); return; }
    if (p < 0) return;
    proc.assignFileToPad (proc.absPad (p), f);
    selectPad (proc.absPad (p));
}

void MPCSurface::itemDropped (const SourceDetails& d)
{
    dragPad = -1;
    const auto s = d.description.toString();
    const int p = padAt (d.localPosition);
    if (p < 0 || ! s.startsWith (BrowserPanel::dragPrefix())) return;
    proc.assignFileToPad (proc.absPad (p), proc.library.resolve (s.fromFirstOccurrenceOf (BrowserPanel::dragPrefix(), false, false)));
    selectPad (proc.absPad (p));
}

// ------------------------------------------------------------------ painting
void MPCSurface::drawChassis (juce::Graphics& g)
{
    g.fillAll (Colour (0xffa8a392));
    Rectangle<float> body (26, 0, 844, 1190);
    g.setGradientFill (juce::ColourGradient (Colour (0xffd6d2c2), 0, 0, Colour (0xffc6c1ae), 0, 1190, false));
    g.fillRoundedRectangle (body, 10);
    g.setColour (Colour (0xff8f8a78)); g.drawRoundedRectangle (body, 10, 2);
    g.setColour (Colour (0xffe6e2d4)); g.drawRoundedRectangle (body.reduced (3), 8, 1);

    Rectangle<float> flap (48, 1094, 800, 96);
    g.setGradientFill (juce::ColourGradient (Colour (0xff8a8472), 0, flap.getY(), Colour (0xff6f6958), 0, flap.getBottom(), false));
    g.fillRect (flap);
    g.setColour (Colour (0xff4f4a3d));
    g.drawVerticalLine (48, 1094, 1190); g.drawVerticalLine (847, 1094, 1190); g.drawHorizontalLine (1094, 48, 847);

    // title
    {
        juce::Font f (juce::FontOptions (54.0f, juce::Font::bold));
        const juce::String t = "MPC3000 SIMULATOR";
        f.setHorizontalScale (537.0f / f.getStringWidthFloat (t));
        g.setFont (f); g.setColour (Colour (0xff35342f));
        g.drawText (t, 68, 34, 540, 48, juce::Justification::centredLeft, false);
        juce::Font s (juce::FontOptions (21.0f, juce::Font::bold));
        const juce::String sub = "VIRTUAL HARDWARE SAMPLER";
        s.setHorizontalScale (335.0f / s.getStringWidthFloat (sub));
        g.setFont (s); g.drawText (sub, 170, 76, 340, 24, juce::Justification::centredLeft, false);
    }
    // speaker slits
    g.setColour (Colour (0xff2c2b28));
    for (int gx : { 650, 720, 788 })
        for (int i = 0; i < 8; ++i) g.fillRoundedRectangle ((float) gx, 40.0f + (float) i * 8.6f, 53.0f, 3.4f, 1.5f);

    // knob captions
    g.setColour (kInk);
    const juce::Font cap (juce::FontOptions (13.5f, juce::Font::bold));
    mpcui::fitText (g, "STEREO VOLUME", { 28, 125, 102, 16 }, juce::Justification::centred, cap);
    mpcui::fitText (g, "RECORD LEVEL", { 28, 232, 102, 16 }, juce::Justification::centred, cap);
    mpcui::fitText (g, "DISPLAY", { 28, 333, 102, 16 }, juce::Justification::centred, cap);
    mpcui::fitText (g, "CONTRAST", { 28, 347, 102, 16 }, juce::Justification::centred, cap);
    g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::plain)));
    g.drawText ("0", 52, 204, 10, 10, juce::Justification::centred); g.drawText ("0", 84, 204, 10, 10, juce::Justification::centred);

    // LCD bezel
    g.setGradientFill (juce::ColourGradient (Colour (0xffc2c0b2), 0, 124, Colour (0xff85836f), 0, 423, false));
    g.fillRoundedRectangle (126, 124, 488, 299, 9);
    g.setColour (Colour (0xff3d3c36)); g.drawRoundedRectangle (126.5f, 124.5f, 487, 298, 9, 1.5f);
    g.setColour (Colour (0xff4b4a42)); g.fillRoundedRectangle (138, 138, 464, 272, 6);

    // section titles
    section (g, "DATA ENTRY", 744, 131, 649, 838);
    section (g, "COMMANDS", 744, 421, 649, 838);
    section (g, "REAL TIME", 741, 783, 636, 844);
    section (g, "DRUMS", 372, 521, 131, 614);
    section (g, "DATA WHEEL", 741, 568, 700, 782);
    g.setColour (kInk); g.setFont (juce::Font (juce::FontOptions (13.5f, juce::Font::bold)));
    g.drawText ("CURSOR", 650, 688, 80, 16, juce::Justification::centred);
    g.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
    g.drawText ("<", 646, 712, 14, 18, juce::Justification::centred); g.drawText (">", 826, 712, 14, 18, juce::Justification::centred);
    g.drawText ("^", 735, 672, 14, 14, juce::Justification::centred); g.drawText ("v", 735, 758, 14, 14, juce::Justification::centred);

    // pad panel
    g.setColour (Colour (0xff6d6d61)); g.fillRoundedRectangle (129, 537, 485, 537, 3);
    g.setColour (Colour (0xff3d3d36)); g.drawRoundedRectangle (129.5f, 537.5f, 484, 536, 3, 1);

    // play/rec panel
    g.setColour (Colour (0xff5b5c50)); g.fillRoundedRectangle (624, 903, 233, 172, 4);
    g.setColour (Colour (0xff3d3d36)); g.drawRoundedRectangle (624.5f, 903.5f, 232, 171, 4, 1);
    g.setColour (Colour (0xffd9d6c8)); g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText ("PLAY/REC", 690, 909, 100, 16, juce::Justification::centred);
    g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::plain)));
    const char* abc[] = { "A", "B", "C", "D" };
    for (int i = 0; i < 4; ++i) g.drawText (abc[i], 636 + i * 55, 929, 12, 11, juce::Justification::centred);
    g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    g.drawText ("spice", 740, 996, 56, 14, juce::Justification::centred); g.drawText ("A/a", 795, 996, 56, 14, juce::Justification::centred);
    const bool learning = proc.engine.learnPad.load() >= 0;
    g.setColour (learning ? Colour (0xffff5a4a) : Colour (0xff1f1f1c)); g.fillEllipse (653, 998, 8, 8);
    g.setColour (Colour (0xff1f1f1c)); g.fillEllipse (726, 998, 8, 8);

    // note-variation box
    g.setColour (kInk.withAlpha (0.7f)); g.drawRect (43, 775, 75, 298, 1);
    g.setColour (Colour (0xffcfcab9)); g.fillRect (52, 764, 58, 36);
    g.setColour (kInk); g.setFont (juce::Font (juce::FontOptions (13.5f, juce::Font::bold)));
    g.drawText ("NOTE", 43, 763, 75, 16, juce::Justification::centred);
    g.drawText ("VARIATION", 43, 778, 75, 16, juce::Justification::centred);
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::plain)));
    g.drawText ("10", 46, 916, 22, 14, juce::Justification::centred); g.drawText ("10", 94, 916, 22, 14, juce::Justification::centred);
    g.drawText ("0", 46, 1034, 22, 14, juce::Justification::centred);  g.drawText ("0", 94, 1034, 22, 14, juce::Justification::centred);

    // bank indicators
    g.setFont (juce::Font (juce::FontOptions (13.5f, juce::Font::bold)));
    for (int i = 0; i < 4; ++i)
    {
        const int lx = (i % 2 == 0) ? 45 : 106, dx = (i % 2 == 0) ? 61 : 94, y = 524 + (i / 2) * 18;
        g.setColour (kInk); g.drawText (juce::String::charToString (bankLetters[i]), lx, y, 14, 14, juce::Justification::centred);
        g.setColour (i == proc.bank.load() ? Colour (0xffeee45a) : Colour (0xff3b3b38));
        g.fillEllipse ((float) dx, (float) y + 4, 7, 7);
    }
}

void MPCSurface::drawWave (juce::Graphics& g, const mpc::Sample* s, const mpc::PadParams& p, Rectangle<int> box)
{
    const Colour fg (0xff1c2a10);
    g.setColour (fg); g.drawRect (box, 1);
    if (s != nullptr)
    {
        for (int x = 1; x < box.getWidth() - 1; ++x)
        {
            const float pk = s->peaks[(size_t) juce::jlimit (0, 255, x * 256 / box.getWidth())];
            const float h = juce::jmax (0.5f, pk * (float) box.getHeight() * 0.48f);
            g.drawVerticalLine (box.getX() + x, (float) box.getCentreY() - h, (float) box.getCentreY() + h);
        }
    }
    else { lcdText (g, p.missing ? "!! FILE MISSING !!" : "-- EMPTY --", box.getX(), box.getCentreY(), box.getWidth(), juce::Justification::centred); }
    for (float t : { p.start, p.end })
    {
        const float x = (float) box.getX() + t * (float) (box.getWidth() - 1);
        juce::Path tri; tri.addTriangle (x, (float) box.getBottom() + 3, x - 4.5f, (float) box.getBottom() + 11, x + 4.5f, (float) box.getBottom() + 11);
        g.fillPath (tri);
    }
}

void MPCSurface::lcdText (juce::Graphics& g, const juce::String& t, int x, int cy, int w, juce::Justification j, bool invert)
{
    const Colour fg (0xff1c2a10);
    const auto f = lcdFont();
    g.setFont (f);
    Rectangle<int> r (x, cy - 10, w, 20);
    if (invert)
    {
        const int tw = (int) f.getStringWidthFloat (t) + 6;
        Rectangle<int> bx = j == juce::Justification::right ? Rectangle<int> (x + w - tw + 3, cy - 10, tw, 20)
                          : j == juce::Justification::centred ? Rectangle<int> (x + (w - tw) / 2, cy - 10, tw, 20)
                          : Rectangle<int> (x - 3, cy - 10, tw, 20);
        g.setColour (fg); g.fillRect (bx);
        g.setColour (Colour (0xff9bd24a).withMultipliedBrightness (lcdBright));
    }
    else g.setColour (fg);
    g.drawText (t, r, j, false);
}

void MPCSurface::drawLcd (juce::Graphics& g)
{
    const auto bg = Colour (0xff9bd24a).withMultipliedBrightness (lcdBright);
    g.setGradientFill (juce::ColourGradient (bg.brighter (0.08f), 372, 148, bg.darker (0.12f), 372, 395, false));
    g.fillRoundedRectangle (146, 148, 450, 247, 5);
    if (page == Page::Load) { auto l = softLabels(); (void) l; }

    const int sel = proc.selectedPad;
    auto& p = proc.engine.params[(size_t) sel];
    auto s = proc.engine.getSample (sel);
    const juce::String id = juce::String::charToString (bankLetters[sel / 16]) + juce::String (sel % 16 + 1);
    const juce::String nm = p.sampleName.isEmpty() ? "--" : p.sampleName.toUpperCase().substring (0, 16);
    const juce::String stat = proc.status.isNotEmpty() ? proc.status.toUpperCase()
                            : entry.isNotEmpty() ? "PAD: " + entry + "_"
                            : p.mute ? "MUTED" : p.reverse ? "REVERSE" : p.oneShot ? "PLAYBACK" : "GATE";
    const juce::String len = s != nullptr ? juce::String (s->lengthSeconds(), 2) + "s" : juce::String ("--");

    // soft key captions
    auto sl = softLabels();
    for (int i = 0; i < 4; ++i) lcdText (g, sl[(size_t) i], 211 + 93 * i - 30, 384, 102, juce::Justification::centred);

    if (page == Page::Load) return;                                   // browser draws the rest

    if (page == Page::Main || page == Page::Sample)
    {
        const bool smp = (page == Page::Sample);
        lcdText (g, "START " + juce::String (p.start, 2), 188, 189, 130, juce::Justification::left, field == Field::Start);
        lcdText (g, "SAMPLE " + id, 272, 189, 200, juce::Justification::centred);
        lcdText (g, "END " + juce::String (p.end, 2), 440, 189, 120, juce::Justification::right, field == Field::End);
        drawWave (g, s.get(), p, { 190, 200, 361, 97 });
        if (! smp)
        {
            lcdText (g, "PROGRAM: " + proc.programName, 180, 324, 300, juce::Justification::left);
            lcdText (g, "SAMPLE NAME: " + nm, 180, 341, 300, juce::Justification::left);
            lcdText (g, "MIDI NOTE:" + juce::String (p.midiNote), 180, 357, 140, juce::Justification::left);
            lcdText (g, "LENGTH " + len, 300, 357, 140, juce::Justification::left);
            lcdText (g, "TUNE " + juce::String (p.tuneSemis, 2), 420, 324, 142, juce::Justification::right, field == Field::Tune);
            lcdText (g, "LEVEL " + juce::String (juce::roundToInt (p.level * 100)), 420, 341, 142, juce::Justification::right, field == Field::Level);
            lcdText (g, stat, 380, 357, 182, juce::Justification::right);
        }
        else
        {
            lcdText (g, "SAMPLE NAME: " + nm + "  " + len, 180, 324, 380, juce::Justification::left);
            lcdText (g, "TUNE " + juce::String (p.tuneSemis, 2), 180, 341, 110, juce::Justification::left, field == Field::Tune);
            lcdText (g, "LEVEL " + juce::String (juce::roundToInt (p.level * 100)), 300, 341, 110, juce::Justification::left, field == Field::Level);
            lcdText (g, "PAN " + juce::String (juce::roundToInt (p.pan * 50.0f)), 430, 341, 130, juce::Justification::left, field == Field::Pan);
            lcdText (g, juce::String (p.reverse ? "REV " : "FWD ") + (p.oneShot ? "1SHOT " : "GATE ") + (p.mute ? "MUTE " : "") + (p.solo ? "SOLO" : ""), 180, 357, 250, juce::Justification::left);
            lcdText (g, stat, 380, 357, 182, juce::Justification::right);
        }
        return;
    }

    if (page == Page::Program)
    {
        lcdText (g, "PROGRAM: " + proc.programName, 170, 170, 270, juce::Justification::left);
        lcdText (g, "BANK " + juce::String::charToString (bankLetters[proc.bank.load()]) + "  MISSING:" + juce::String (proc.countMissing()), 380, 170, 190, juce::Justification::right);
        for (int i = 0; i < 16; ++i)
        {
            const int abs = proc.bank.load() * 16 + i;
            auto& q = proc.engine.params[(size_t) abs];
            const int col = i / 8, row = i % 8;
            const juce::String t = juce::String::charToString (bankLetters[proc.bank.load()]) + juce::String (i + 1).paddedLeft ('0', 2) + " "
                                 + (q.sampleName.isEmpty() ? "--" : q.sampleName.toUpperCase().substring (0, 11)) + (q.missing ? "!" : "");
            lcdText (g, t, 170 + col * 215, 196 + row * 21, 210, juce::Justification::left, abs == sel);
        }
        return;
    }

    std::vector<juce::String> lines;
    if (page == Page::Save)
        lines = { "SAVE / LOAD PROGRAM", "", "PROGRAM: " + proc.programName, "FILE TYPE: .M3KPROG", "STORES PADS, SAMPLE PATHS,", "MIDI NOTES AND PARAMETERS", "", stat };
    else if (page == Page::Midi)
        lines = { "MIDI SETUP", "", "PAD " + id + "   NOTE " + juce::String (p.midiNote), "LEARN: " + juce::String (proc.engine.learnPad.load() >= 0 ? "WAITING FOR NOTE..." : "OFF"),
                  "CHANNEL: OMNI", "", "PRESS REC OR LEARN, THEN HIT", "A PAD ON YOUR MPC / KEYBOARD" };
    else
        lines = { "HELP", "", "DISK = LIBRARY    MIDI = NOTES", "SEQ EDIT = SAMPLE  OTHER = PROGRAM", "WHEEL EDITS THE LIT FIELD:", "TEMPO START  /SYNC END", "COUNT TUNE  WAIT SEQ LEVEL", "RIGHT-CLICK A PAD FOR MORE" };
    for (size_t i = 0; i < lines.size(); ++i)
        lcdText (g, lines[i], 175, 172 + (int) i * 24, 400, juce::Justification::left);
}

void MPCSurface::paint (juce::Graphics& g)
{
    drawChassis (g);
    drawLcd (g);

    const int bk = proc.bank.load();
    for (int i = 0; i < mpc::padsPerBank; ++i)
    {
        const int abs = bk * mpc::padsPerBank + i;
        auto& pp = proc.engine.params[(size_t) abs];
        auto r = padRect (i);
        const bool lit = (i == heldPad) || litFrames[abs] > 0;
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (r.translated (0, 2), 4.f);
        g.setGradientFill (juce::ColourGradient (lit ? Colour (0xff7d8070) : Colour (0xff62615d), 0, r.getY(),
                                                 lit ? Colour (0xff62664f) : Colour (0xff4b4a46), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 4.f);
        g.setColour (Colour (0xff22221f)); g.drawRoundedRectangle (r, 4.f, 1.5f);
        g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (r.reduced (2), 3.f, 1.f);
        if (i == dragPad) { g.setColour (Colour (0xff9bd24a)); g.drawRoundedRectangle (r, 4.f, 3.f); }
        if (abs == proc.selectedPad) { g.setColour (Colour (0xffeee45a)); g.drawRoundedRectangle (r.expanded (2), 5.f, 2.f); }
        if (pp.missing) { g.setColour (Colour (0xffd9392b)); g.fillEllipse (r.getRight() - 16, r.getBottom() - 16, 10, 10); }

        g.setColour (pp.missing ? Colour (0xffa51f14) : kInk);
        mpcui::fitText (g, pp.sampleName.toUpperCase(), { (int) r.getX() + 2, (int) r.getY() - 20, 70, 16 },
                        juce::Justification::centredLeft, juce::Font (juce::FontOptions (13.0f, juce::Font::bold)), 0.68f);
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        g.drawText (juce::String (i + 1), (int) r.getRight() - 24, (int) r.getY() - 20, 24, 16, juce::Justification::centredRight);
    }
}
