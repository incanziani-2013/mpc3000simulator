#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

// LCD-style library browser: search, folder navigation (category prefix), audition,
// double-click to load into the selected pad / import an XPM program, drag onto pads.
class BrowserPanel : public juce::Component, private juce::ListBoxModel
{
public:
    explicit BrowserPanel (MPCProcessor& p) : proc (p)
    {
        search.setTextToShowWhenEmpty ("SEARCH...", juce::Colour (0xff3b4a2a));
        search.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff86ba3c));
        search.setColour (juce::TextEditor::textColourId, juce::Colour (0xff1c2a10));
        search.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff1c2a10));
        search.setFont (juce::Font (juce::FontOptions ("Courier New", 12.0f, juce::Font::bold)));
        search.onTextChange = [this] { applyFilter(); };
        addAndMakeVisible (search);

        for (auto* b : { &modeBtn, &folderBtn, &closeBtn })
        {
            b->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff86ba3c));
            b->setColour (juce::TextButton::textColourOffId, juce::Colour (0xff1c2a10));
            addAndMakeVisible (*b);
        }
        modeBtn.onClick = [this] { toggleMode(); };
        folderBtn.onClick = [this] { chooseFolder(); };
        closeBtn.onClick = [this] { setVisible (false); if (onClose) onClose(); };

        list.setModel (this);
        list.setRowHeight (15);
        list.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff86ba3c));
        list.setColour (juce::ListBox::outlineColourId, juce::Colour (0xff1c2a10));
        addAndMakeVisible (list);
        refresh();
    }

    std::function<void()> onClose;
    void toggleMode() { programs = ! programs; modeBtn.setButtonText (programs ? "PROGRAMS" : "SAMPLES"); applyFilter(); }
    void chooseFolder()
    {
        chooser = std::make_unique<juce::FileChooser> ("Select the MPC3000 library folder", proc.library.getRoot());
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [this] (const juce::FileChooser& fc)
                              { if (fc.getResult().isDirectory()) { proc.library.setRoot (fc.getResult()); refresh(); } });
    }
    void rescan() { proc.library.rescan(); refresh(); }

    void refresh()
    {
        applyFilter();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff9bd24a));
        g.setColour (juce::Colour (0xff1c2a10));
        g.setFont (juce::Font (juce::FontOptions ("Courier New", 12.0f, juce::Font::bold)));
        auto& lib = proc.library;
        juce::String info = lib.isAvailable()
            ? juce::String (shown.size()) + " ITEMS  (" + juce::String (lib.numSamples) + " SMP / " + juce::String (lib.numPrograms) + " PGM)"
            : "NO LIBRARY FOUND - PRESS FOLDER";
        g.drawText (info, 4, getHeight() - 16, getWidth() - 8, 14, juce::Justification::left);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (3);
        auto top = b.removeFromTop (20);
        closeBtn.setBounds (top.removeFromRight (26)); top.removeFromRight (2);
        folderBtn.setBounds (top.removeFromRight (62)); top.removeFromRight (2);
        modeBtn.setBounds (top.removeFromRight (84)); top.removeFromRight (2);
        search.setBounds (top);
        b.removeFromTop (3);
        b.removeFromBottom (15);
        list.setBounds (b);
    }

    // For drag & drop onto pads.
    static juce::String dragPrefix() { return "mpclib:"; }

private:
    void applyFilter()
    {
        shown.clear();
        const auto q = search.getText().trim();
        const auto& all = proc.library.getEntries();
        for (int i = 0; i < (int) all.size(); ++i)
        {
            if (all[(size_t) i].isProgram != programs) continue;
            if (q.isNotEmpty() && ! (all[(size_t) i].name + " " + all[(size_t) i].folder).containsIgnoreCase (q)) continue;
            shown.push_back (i);
        }
        list.updateContent(); list.repaint(); repaint();
    }
    const LibraryEntry* entryAt (int row) const
    {
        if (row < 0 || row >= (int) shown.size()) return nullptr;
        return &proc.library.getEntries()[(size_t) shown[(size_t) row]];
    }

    int getNumRows() override { return (int) shown.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool sel) override
    {
        auto* e = entryAt (row);
        if (e == nullptr) return;
        if (sel) g.fillAll (juce::Colour (0xff1c2a10));
        g.setColour (sel ? juce::Colour (0xff9bd24a) : juce::Colour (0xff1c2a10));
        g.setFont (juce::Font (juce::FontOptions ("Courier New", 12.0f, juce::Font::bold)));
        g.drawText (e->folder.toUpperCase().substring (0, 4) + " " + e->name, 3, 0, w - 6, h, juce::Justification::centredLeft, true);
    }
    void selectedRowsChanged (int row) override
    {
        if (auto* e = entryAt (row); e != nullptr && ! e->isProgram)
            proc.previewFile (proc.library.fileFor (*e));       // audition before loading
    }
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
    {
        auto* e = entryAt (row);
        if (e == nullptr) return;
        auto f = proc.library.fileFor (*e);
        if (e->isProgram) proc.loadXpmAsync (f);
        else proc.assignFileToPad (proc.selectedPad, f);
    }
    juce::var getDragSourceDescription (const juce::SparseSet<int>& rows) override
    {
        if (rows.size() == 0) return {};
        auto* e = entryAt (rows[0]);
        return (e != nullptr && ! e->isProgram) ? juce::var (dragPrefix() + e->relPath) : juce::var();
    }

    MPCProcessor& proc;
    juce::TextEditor search;
    juce::TextButton modeBtn { "SAMPLES" }, folderBtn { "FOLDER" }, closeBtn { "X" };
    juce::ListBox list;
    std::vector<int> shown;
    bool programs = false;
    std::unique_ptr<juce::FileChooser> chooser;
};
