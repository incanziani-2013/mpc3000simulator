#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

namespace mpc
{
constexpr int padsPerBank = 16;
constexpr int numBanks    = 4;                       // A, B, C, D
constexpr int numPads     = padsPerBank * numBanks;  // 64 (matches MPC XPM programs)
constexpr int maxVoices   = 48;

struct Sample : public juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<Sample>;
    juce::AudioBuffer<float> data;
    double sampleRate = 44100.0;
    juce::String path;
    std::array<float, 256> peaks {};                 // for the LCD waveform
    double lengthSeconds() const { return sampleRate > 0 ? data.getNumSamples() / sampleRate : 0.0; }
};

struct PadParams
{
    juce::String samplePath, sampleName;
    bool  missing = false;                           // file referenced but not found / unreadable
    int   midiNote = 36;
    float level = 1.0f, pan = 0.0f, tuneSemis = 0.0f;
    float start = 0.0f, end = 1.0f;                  // normalised 0..1
    bool  reverse = false, oneShot = true, mute = false, solo = false;
};

class SamplerEngine
{
public:
    SamplerEngine();
    void prepare (double hostSampleRate) { hostRate = hostSampleRate; }

    // Disk work: safe on any non-audio thread.
    Sample::Ptr readSample (const juce::File& file) const;

    // Message thread.
    void assignSample (int pad, Sample::Ptr s);
    Sample::Ptr getSample (int pad);
    void setPreview (Sample::Ptr s);

    // Lock-free requests (message thread -> audio thread).
    void postNoteOn (int pad, float velocity) { post ({ 0, pad, velocity }); }
    void postNoteOff (int pad)                { post ({ 1, pad, 0.0f }); }
    void postPreview()                        { post ({ 2, -1, 1.0f }); }
    void postStopAll()                        { post ({ 3, -1, 0.0f }); }

    void process (juce::AudioBuffer<float>&, juce::MidiBuffer&);
    int  padForNote (int note) const;

    std::array<PadParams, numPads> params;           // edit on message thread only
    float masterVolume  = 0.8f;
    float previewLevel  = 0.8f;
    std::atomic<int> learnPad { -1 };               // >=0: next MIDI note-on is assigned to that pad
    std::atomic<uint64_t> hitMask { 0 };             // bit per pad, set on trigger, cleared by UI

private:
    struct Event { int type; int pad; float vel; };
    struct Voice
    {
        Sample::Ptr sample;
        int pad = -1;
        double pos = 0, inc = 1, endPos = 0;
        float gain = 0, panL = 1, panR = 1, env = 1, fade = 0;
        bool active = false, releasing = false, reverse = false, oneShot = true;
        int age = 0;
    };

    void post (Event e);
    void handle (const Event&);
    void noteOn (int pad, float vel);
    void noteOff (int pad);
    void startVoice (Sample::Ptr, int pad, const PadParams&, float gain);
    void renderVoice (Voice&, juce::AudioBuffer<float>&, int start, int n);

    juce::AbstractFifo fifo { 256 };
    std::array<Event, 256> events {};
    std::array<Sample::Ptr, numPads> samples;
    Sample::Ptr previewSample;
    juce::SpinLock lock;
    std::array<Voice, maxVoices> voices;
    double hostRate = 44100.0;
    int ageCounter = 0;
};
}
