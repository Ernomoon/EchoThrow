// Renders the editor to PNG files without opening a window. Run: UiSnapshot <output-dir>
#include "TestSupport.h"
#include "Presets.h"
#include <cstdio>

using namespace et;
using namespace testsupport;

static void snap (juce::AudioProcessorEditor& ed, const juce::File& file)
{
    pump (250);
    auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, 2.0f);
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat png;
    png.writeImageToStream (img, out);
    std::printf ("wrote %s (%dx%d)\n", file.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
}

static void setParam (EchoThrowProcessor& p, const char* id, float real)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (real));
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::File outDir = argc > 1 ? juce::File (argv[1]) : juce::File::getCurrentWorkingDirectory();
    outDir.createDirectory();
    const double SR = 48000.0;

    EchoThrowProcessor proc;
    proc.setPlayConfigDetails (2, 2, SR, 512);
    proc.prepareToPlay (SR, 512);

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setVisible (true);
    pump (100);

    snap (*ed, outDir.getChildFile ("ui_1_empty.png"));

    proc.toggleRecord();
    run (proc, silence (SR, 0.1));
    snap (*ed, outDir.getChildFile ("ui_2_armed.png"));
    proc.toggleRecord();
    run (proc, silence (SR, 0.05));

    recordTake (proc, SR, 0.9);
    setParam (proc, pid::tail, 6.0f);
    snap (*ed, outDir.getChildFile ("ui_3_clip.png"));

    proc.startListening();
    run (proc, silence (SR, 0.9));
    snap (*ed, outDir.getChildFile ("ui_4_playing.png"));
    proc.stopListening();
    run (proc, silence (SR, 0.1));

    applyFactoryPreset (proc.apvts, 6);    // Dream Wash
    setParam (proc, pid::eq1G, 4.5f);  setParam (proc, pid::eq1F, 620.0f);
    setParam (proc, pid::eq2G, -5.0f); setParam (proc, pid::eq2F, 3200.0f); setParam (proc, pid::eq2Q, 2.2f);
    setParam (proc, pid::eqLsG, -3.0f);
    setParam (proc, pid::link, 0.0f);
    setParam (proc, pid::divR, 7.0f);
    proc.presetName = "Dream Wash";
    pump (400);
    snap (*ed, outDir.getChildFile ("ui_5_preset_eq.png"));

    std::printf ("done\n");
    return 0;
}
