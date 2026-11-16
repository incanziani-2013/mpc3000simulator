#include "XpmImporter.h"

namespace mpc
{
bool parseXpm (const juce::File& file, XpmProgram& out, juce::String& error)
{
    auto doc = juce::XmlDocument::parse (file);
    if (doc == nullptr) { error = "XPM: invalid XML"; return false; }
    auto* prog = doc->getChildByName ("Program");
    if (prog == nullptr) { error = "XPM: no <Program>"; return false; }

    out = {};
    out.name = prog->getChildElementAllSubText ("Name", file.getFileNameWithoutExtension());

    if (auto* ins = prog->getChildByName ("Instruments"))
    {
        for (auto* in : ins->getChildWithTagNameIterator ("Instrument"))
        {
            const int idx = in->getIntAttribute ("number") - 1;
            if (idx < 0 || idx >= 64) continue;
            auto& pad = out.pads[(size_t) idx];

            auto* layers = in->getChildByName ("Layers");
            auto* parent = layers != nullptr ? layers : in;
            int found = 0;
            for (auto* l : parent->getChildWithTagNameIterator ("Layer"))
            {
                const auto sf = l->getChildElementAllSubText ("SampleFile", {}).trim();
                if (sf.isEmpty()) continue;
                if (++found > 1) { ++out.layersIgnored; continue; }
                pad.hasSample = true; pad.sampleFile = sf;
            }
            if (! pad.hasSample) continue;

            const float vol = (float) in->getChildElementAllSubText ("Volume", "0.5").getDoubleValue();
            pad.level = juce::jlimit (0.0f, 1.5f, vol * 2.0f);              // 0.5 = unity in MPC programs
            pad.pan = juce::jlimit (-1.0f, 1.0f, ((float) in->getChildElementAllSubText ("Pan", "0.5").getDoubleValue() - 0.5f) * 2.0f);
            pad.tuneSemis = (float) in->getChildElementAllSubText ("TuneCoarse", "0").getIntValue()
                          + (float) in->getChildElementAllSubText ("TuneFine", "0").getIntValue() / 100.0f;
            pad.oneShot = in->getChildElementAllSubText ("OneShot", "True").equalsIgnoreCase ("true");
        }
    }

    if (auto* map = prog->getChildByName ("PadNoteMap"))
        for (auto* pn : map->getChildWithTagNameIterator ("PadNote"))
        {
            const int idx = pn->getIntAttribute ("number") - 1;
            if (idx >= 0 && idx < 64)
                out.pads[(size_t) idx].midiNote = pn->getChildElementAllSubText ("Note", "-1").getIntValue();
        }
    return true;
}
}
