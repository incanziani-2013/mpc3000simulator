#pragma once
#include "PluginProcessor.h"
#include "UI/Widgets.h"
#include "UI/BrowserPanel.h"

// The whole front panel, laid out in the reference image's pixel space (896 x 1200).
class MPCSurface : public juce::Component,
                   public juce::DragAndDropContainer,
                   public juce::DragAndDropTarget,
                   public juce::FileDragAndDropTarget,
                   private juce::Timer
{
public:
    static constexpr int W = 896, H = 1200;
    explicit MPCSurface (MPCProcessor&);
    ~MPCSurface() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray&, int, int) override;
    bool isInterestedInDragSource (const SourceDetails&) override { return true; }
    void itemDragMove (const SourceDetails& d) override { dragPad = padAt (d.localPosition); repaint(); }
    void itemDragExit (const SourceDetails&) override { dragPad = -1; repaint(); }
    void itemDropped (const SourceDetails&) override;

private:
    enum class Page { Main, Sample, Program, Load, Save, Midi, Help };
    enum class Field { Start, End, Tune, Level, Pan };

    void timerCallback() override;
    juce::Rectangle<float> padRect (int padInBank) const;
    int padAt (juce::Point<int>) const;
    void showPadMenu (int padInBank);

    HwButton* mk (const juce::String& label, const juce::String& text, HwButton::Style s, juce::Rectangle<int> btn,
                  int off, bool led, std::function<void()> fn);
    void setPage (Page);
    void softKey (int i);
    std::array<juce::String, 4> softLabels() const;
    void selectPad (int absPad);
    void adjust (int steps);
    void toast (const juce::String& s) { proc.status = s; }
    mpc::PadParams& P() { return proc.engine.params[(size_t) proc.selectedPad]; }
    void playSelected (float vel);
    void keypad (const juce::String& k);
    void syncFader();
    void refreshLeds();

    void drawChassis (juce::Graphics&);
    void drawLcd (juce::Graphics&);
    void drawWave (juce::Graphics&, const mpc::Sample*, const mpc::PadParams&, juce::Rectangle<int> box);
    void lcdText (juce::Graphics&, const juce::String&, int x, int cy, int w, juce::Justification, bool invert = false);

    MPCProcessor& proc;
    Knob masterKnob, recKnob, contrastKnob;
    DataWheel wheel;
    Fader fader;
    BrowserPanel browser;
    std::vector<std::unique_ptr<HwButton>> buttons;
    HwButton *fullBtn = nullptr, *levelsBtn = nullptr, *noteVarBtn = nullptr, *afterBtn = nullptr,
             *fStart = nullptr, *fEnd = nullptr, *fTune = nullptr, *fLevel = nullptr;
    std::unique_ptr<juce::FileChooser> chooser;

    Page page = Page::Main;
    Field field = Field::Start;
    juce::String entry;
    int heldPad = -1, dragPad = -1, statusAge = 0;
    juce::String lastStatus;
    int litFrames[mpc::numPads] {};
    float lcdBright = 1.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MPCSurface)
};

class MPCEditor : public juce::AudioProcessorEditor
{
public:
    explicit MPCEditor (MPCProcessor& p) : AudioProcessorEditor (p), surface (p)
    {
        addAndMakeVisible (surface);
        setResizable (true, true);
        setResizeLimits (448, 600, 1344, 1800);
        getConstrainer()->setFixedAspectRatio ((double) MPCSurface::W / MPCSurface::H);
        setSize (672, 900);
    }
    void resized() override
    {
        surface.setBounds (0, 0, MPCSurface::W, MPCSurface::H);
        surface.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) MPCSurface::W));
    }
private:
    MPCSurface surface;
};
