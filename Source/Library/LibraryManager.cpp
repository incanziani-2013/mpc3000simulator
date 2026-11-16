#include "LibraryManager.h"

juce::File LibraryManager::settingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("MPC3000 SIMULATOR").getChildFile ("library_path.txt");
}

LibraryManager::LibraryManager()
{
    juce::Array<juce::File> candidates;
    const auto s = settingsFile();
    if (s.existsAsFile()) candidates.add (juce::File (s.loadFileAsString().trim()));
    candidates.add (juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)
                        .getChildFile ("MPC3000 SIMULATOR").getChildFile ("Library"));
    candidates.add (juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                        .getParentDirectory().getChildFile ("Library"));
    for (auto& c : candidates)
        if (c.isDirectory()) { root = c; break; }
    rescan();
}

bool LibraryManager::setRoot (const juce::File& f)
{
    if (! f.isDirectory()) return false;
    root = f;
    auto s = settingsFile();
    s.getParentDirectory().createDirectory();
    s.replaceWithText (f.getFullPathName());
    rescan();
    return true;
}

void LibraryManager::rescan()
{
    entries.clear(); numSamples = numPrograms = 0;
    if (! root.isDirectory()) return;
    for (const auto& f : root.findChildFiles (juce::File::findFiles, true, "*"))
    {
        const auto ext = f.getFileExtension().toLowerCase();
        const bool prog = (ext == ".xpm");
        const bool audio = ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac" || ext == ".ogg" || ext == ".mp3";
        if (! prog && ! audio) continue;
        LibraryEntry e;
        e.name = f.getFileNameWithoutExtension();
        e.relPath = f.getRelativePathFrom (root).replaceCharacter ('\\', '/');
        e.folder = e.relPath.upToFirstOccurrenceOf ("/", false, false);
        if (! e.relPath.contains ("/")) e.folder = "";
        e.isProgram = prog;
        (prog ? numPrograms : numSamples)++;
        entries.push_back (std::move (e));
    }
    std::sort (entries.begin(), entries.end(), [] (const LibraryEntry& a, const LibraryEntry& b)
               { return a.relPath.compareIgnoreCase (b.relPath) < 0; });
}

juce::String LibraryManager::makeRelative (const juce::File& f) const
{
    if (! root.isDirectory() || ! f.isAChildOf (root)) return {};
    return f.getRelativePathFrom (root).replaceCharacter ('\\', '/');
}
