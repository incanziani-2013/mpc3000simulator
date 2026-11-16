#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Programs/XpmImporter.h"

MPCProcessor::MPCProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)) {}

MPCProcessor::~MPCProcessor()
{
    alive->store (false);
    pool.removeAllJobs (true, 5000);
}

void MPCProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer& m)
{
    juce::ScopedNoDenormals nd;
    engine.process (b, m);
}

juce::AudioProcessorEditor* MPCProcessor::createEditor() { return new MPCEditor (*this); }

bool MPCProcessor::assignFileToPad (int pad, const juce::File& f)
{
    if (pad < 0 || pad >= mpc::numPads) return false;
    auto& p = engine.params[(size_t) pad];
    p.samplePath = f.getFullPathName();
    p.sampleName = f.getFileNameWithoutExtension();
    auto s = engine.readSample (f);
    engine.assignSample (pad, s);
    p.missing = (s == nullptr);
    status = p.missing ? (f.existsAsFile() ? "UNREADABLE FILE" : "FILE NOT FOUND") : juce::String();
    return s != nullptr;
}

void MPCProcessor::clearPad (int pad)
{
    if (pad < 0 || pad >= mpc::numPads) return;
    engine.assignSample (pad, nullptr);
    auto& p = engine.params[(size_t) pad];
    p.samplePath.clear(); p.sampleName.clear(); p.missing = false;
}

void MPCProcessor::previewFile (const juce::File& f)
{
    if (auto s = engine.readSample (f)) { engine.setPreview (s); engine.postPreview(); }
}

int MPCProcessor::countMissing() const
{
    int n = 0;
    for (auto& p : engine.params) n += p.missing ? 1 : 0;
    return n;
}

void MPCProcessor::loadXpmAsync (const juce::File& xpm)
{
    auto flag = alive;
    status = "LOADING...";
    pool.addJob ([this, xpm, flag]
    {
        mpc::XpmProgram prog;
        juce::String err;
        const bool ok = mpc::parseXpm (xpm, prog, err);
        std::vector<mpc::Sample::Ptr> smp (mpc::numPads);
        std::vector<juce::File> files (mpc::numPads);
        if (ok)
            for (int i = 0; i < mpc::numPads; ++i)
            {
                auto& xp = prog.pads[(size_t) i];
                if (! xp.hasSample) continue;
                files[(size_t) i] = xpm.getParentDirectory().getChildFile (xp.sampleFile);
                smp[(size_t) i] = engine.readSample (files[(size_t) i]);
            }

        juce::MessageManager::callAsync ([this, flag, ok, err, prog, smp, files, xpm]
        {
            if (! flag->load()) return;
            if (! ok) { status = err; return; }
            programName = prog.name.toUpperCase().substring (0, 16);
            int missing = 0;
            for (int i = 0; i < mpc::numPads; ++i)
            {
                auto& p = engine.params[(size_t) i];
                auto& xp = prog.pads[(size_t) i];
                p = {};
                p.midiNote = xp.midiNote >= 0 ? xp.midiNote : juce::jlimit (0, 127, 36 + i);
                engine.assignSample (i, smp[(size_t) i]);
                if (! xp.hasSample) continue;
                p.samplePath = files[(size_t) i].getFullPathName();
                p.sampleName = files[(size_t) i].getFileNameWithoutExtension();
                p.level = xp.level; p.pan = xp.pan; p.tuneSemis = xp.tuneSemis; p.oneShot = xp.oneShot;
                p.missing = (smp[(size_t) i] == nullptr);
                missing += p.missing ? 1 : 0;
            }
            selectedPad = 0; bank = 0;
            status = missing > 0 ? juce::String (missing) + " SAMPLES MISSING" : "PROGRAM LOADED";
        });
    });
}

void MPCProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree s ("MPC3000");
    s.setProperty ("master", engine.masterVolume, nullptr).setProperty ("program", programName, nullptr)
     .setProperty ("bank", bank.load(), nullptr).setProperty ("sel", selectedPad, nullptr);
    for (int i = 0; i < mpc::numPads; ++i)
    {
        auto& p = engine.params[(size_t) i];
        const auto rel = library.makeRelative (juce::File (p.samplePath));
        juce::ValueTree v ("PAD");
        v.setProperty ("i", i, nullptr).setProperty ("abs", p.samplePath, nullptr).setProperty ("rel", rel, nullptr)
         .setProperty ("note", p.midiNote, nullptr).setProperty ("level", p.level, nullptr)
         .setProperty ("pan", p.pan, nullptr).setProperty ("tune", p.tuneSemis, nullptr)
         .setProperty ("start", p.start, nullptr).setProperty ("end", p.end, nullptr)
         .setProperty ("rev", p.reverse, nullptr).setProperty ("one", p.oneShot, nullptr)
         .setProperty ("mute", p.mute, nullptr).setProperty ("solo", p.solo, nullptr);
        s.addChild (v, -1, nullptr);
    }
    if (auto xml = s.createXml()) copyXmlToBinary (*xml, dest);
}

void MPCProcessor::setStateInformation (const void* d, int n)
{
    auto xml = getXmlFromBinary (d, n);
    if (xml == nullptr) return;
    auto s = juce::ValueTree::fromXml (*xml);
    engine.masterVolume = s.getProperty ("master", 0.8f);
    programName = s.getProperty ("program", "MPC3000").toString();
    bank = juce::jlimit (0, mpc::numBanks - 1, (int) s.getProperty ("bank", 0));
    selectedPad = juce::jlimit (0, mpc::numPads - 1, (int) s.getProperty ("sel", 0));
    for (auto v : s)
    {
        const int i = v.getProperty ("i", -1);
        if (i < 0 || i >= mpc::numPads) continue;
        auto& p = engine.params[(size_t) i];
        p = {};
        p.midiNote = v.getProperty ("note", 36 + i);
        p.level = v.getProperty ("level", 1.0f);  p.pan = v.getProperty ("pan", 0.0f);
        p.tuneSemis = v.getProperty ("tune", 0.0f);
        p.start = v.getProperty ("start", 0.0f);  p.end = v.getProperty ("end", 1.0f);
        p.reverse = v.getProperty ("rev", false); p.oneShot = v.getProperty ("one", true);
        p.mute = v.getProperty ("mute", false);   p.solo = v.getProperty ("solo", false);

        // Prefer the library-relative path (survives moving the library), fall back to absolute.
        const juce::String rel = v.getProperty ("rel", "").toString(), abs = v.getProperty ("abs", "").toString();
        if (abs.isEmpty() && rel.isEmpty()) { engine.assignSample (i, nullptr); continue; }
        juce::File f = (rel.isNotEmpty() && library.isAvailable()) ? library.resolve (rel) : juce::File (abs);
        if (! f.existsAsFile() && abs.isNotEmpty()) f = juce::File (abs);
        p.samplePath = f.existsAsFile() ? f.getFullPathName() : (abs.isNotEmpty() ? abs : f.getFullPathName());
        p.sampleName = f.getFileNameWithoutExtension();
        auto smp = engine.readSample (f);
        engine.assignSample (i, smp);
        p.missing = (smp == nullptr);             // never replaced with another sound
    }
    const int m = countMissing();
    if (m > 0) status = juce::String (m) + " SAMPLES MISSING";
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MPCProcessor(); }
