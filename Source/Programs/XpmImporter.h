#pragma once
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <array>

namespace mpc
{
struct XpmPad
{
    bool  hasSample = false;
    juce::String sampleFile;      // as written in the XPM (resolved in the XPM's folder)
    float level = 1.0f, pan = 0.0f, tuneSemis = 0.0f;
    bool  oneShot = true;
    int   midiNote = -1;          // from PadNoteMap, -1 = unspecified
};

struct XpmProgram
{
    juce::String name;
    std::array<XpmPad, 64> pads;
    int layersIgnored = 0;        // extra velocity layers not imported (first layer only)
};

// Imports MPC-V/MPC Software "Drum" programs. Returns false and fills `error` on failure.
bool parseXpm (const juce::File& file, XpmProgram& out, juce::String& error);
}
