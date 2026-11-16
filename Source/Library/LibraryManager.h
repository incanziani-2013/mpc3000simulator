#pragma once
#include <juce_core/juce_core.h>
#include <vector>

// Locates the factory library (never a temp path), indexes it, resolves library-relative paths.
struct LibraryEntry
{
    juce::String name;      // file name without extension
    juce::String folder;    // category = top-level folder under the root (Natural, Percussion, ...)
    juce::String relPath;   // relative to root, '/' separators
    bool isProgram = false; // .xpm
};

class LibraryManager
{
public:
    LibraryManager();
    bool setRoot (const juce::File& newRoot);        // saves to settings, rescans
    juce::File getRoot() const { return root; }
    bool isAvailable() const { return root.isDirectory(); }
    void rescan();
    juce::File fileFor (const LibraryEntry& e) const { return root.getChildFile (e.relPath); }
    juce::File resolve (const juce::String& relPath) const { return root.getChildFile (relPath); }
    juce::String makeRelative (const juce::File& f) const;   // empty if f is not inside root
    const std::vector<LibraryEntry>& getEntries() const { return entries; }
    int numSamples = 0, numPrograms = 0;

private:
    static juce::File settingsFile();
    juce::File root;
    std::vector<LibraryEntry> entries;
};
