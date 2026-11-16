// Headless test: imports a real XPM, loads its samples, triggers pads by MIDI, checks audio.
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/Sampler/SamplerEngine.h"
#include "../Source/Programs/XpmImporter.h"
#include "../Source/Library/LibraryManager.h"
#include <iostream>

int main (int argc, char** argv)
{
    if (argc < 2) { std::cout << "usage: EngineTest <file.xpm>\n"; return 2; }
    juce::File xpm (argv[1]);
    mpc::XpmProgram prog; juce::String err;
    if (! mpc::parseXpm (xpm, prog, err)) { std::cout << "FAIL parse: " << err << "\n"; return 1; }

    mpc::SamplerEngine eng; eng.prepare (48000.0);
    int loaded = 0, missing = 0, withSample = 0;
    for (int i = 0; i < 64; ++i)
    {
        auto& xp = prog.pads[(size_t) i];
        if (! xp.hasSample) continue;
        ++withSample;
        auto s = eng.readSample (xpm.getParentDirectory().getChildFile (xp.sampleFile));
        if (s == nullptr) { ++missing; continue; }
        eng.assignSample (i, s); ++loaded;
        auto& p = eng.params[(size_t) i];
        p.midiNote = xp.midiNote >= 0 ? xp.midiNote : 36 + i; p.level = xp.level; p.tuneSemis = xp.tuneSemis; p.pan = xp.pan;
    }
    std::cout << "program " << prog.name << ": pads with sample " << withSample << ", loaded " << loaded << ", missing " << missing << "\n";

    int silent = 0, played = 0;
    for (int i = 0; i < 64; ++i)
    {
        if (eng.getSample (i) == nullptr) continue;
        juce::AudioBuffer<float> buf (2, 4800); juce::MidiBuffer mb;
        mb.addEvent (juce::MidiMessage::noteOn (1, eng.params[(size_t) i].midiNote, (juce::uint8) 100), 0);
        eng.process (buf, mb);
        ++played;
        if (buf.getMagnitude (0, 4800) <= 1.0e-5f) ++silent;
    }
    // reverse + gate + all-notes-off must not crash
    eng.params[0].reverse = true;
    juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m;
    m.addEvent (juce::MidiMessage::allNotesOff (1), 10);
    eng.process (b, m);
    std::cout << "triggered " << played << " pads, silent: " << silent << "\n";
    return (loaded > 0 && silent == 0 && missing == 0) ? 0 : 1;
}
