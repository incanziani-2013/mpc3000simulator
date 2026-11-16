#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Sampler/SamplerEngine.h"
#include "Library/LibraryManager.h"

class MPCProcessor : public juce::AudioProcessor
{
public:
    MPCProcessor();
    ~MPCProcessor() override;
    void prepareToPlay (double sr, int) override { engine.prepare (sr); }
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override
    { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "MPC3000 SIMULATOR"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return programName; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // --- sample / program management (message thread) ---
    bool assignFileToPad (int pad, const juce::File& f);
    void clearPad (int pad);
    void locateMissing (int pad, const juce::File& f) { assignFileToPad (pad, f); }
    void previewFile (const juce::File& f);
    void loadXpmAsync (const juce::File& xpm);
    int  countMissing() const;
    int  absPad (int padInBank) const { return bank.load() * mpc::padsPerBank + padInBank; }

    mpc::SamplerEngine engine;
    LibraryManager library;
    std::atomic<int> bank { 0 };
    int selectedPad = 0;                 // absolute 0..63
    juce::String programName { "MPC3000" };
    juce::String status;                 // short message shown on the LCD
    bool fullLevel = true;

private:
    void applySamplePath (int pad, const juce::String& path);
    std::shared_ptr<std::atomic<bool>> alive { std::make_shared<std::atomic<bool>> (true) };
    juce::ThreadPool pool { 1 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MPCProcessor)
};
