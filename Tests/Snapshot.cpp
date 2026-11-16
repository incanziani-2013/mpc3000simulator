// Renders the editor headlessly to a PNG (run under xvfb) to compare against the reference image.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

int main (int argc, char** argv)
{
    if (argc < 4) return 2;
    juce::ScopedJuceInitialiser_GUI init;
    MPCProcessor proc;
    proc.library.setRoot (juce::File (argv[1]));
    proc.loadXpmAsync (juce::File (argv[2]));
    auto* mm = juce::MessageManager::getInstance();
    for (int i = 0; i < 40; ++i) mm->runDispatchLoopUntil (100);
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (896, 1200);
    for (int i = 0; i < 5; ++i) mm->runDispatchLoopUntil (100);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File out (argv[3]);
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    return 0;
}
