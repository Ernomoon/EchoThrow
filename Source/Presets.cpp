#include "Presets.h"
#include "Parameters.h"

namespace et
{
namespace
{
    bool isBehaviourParam (const juce::String& id)
    {
        return id == pid::loop || id == pid::hearDry || id == pid::exportDry || id == pid::muteLive || id == pid::tail;
    }

    void setReal (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float realValue)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (realValue));
            p->endChangeGesture();
        }
    }

    float defaultReal (juce::RangedAudioParameter& p)
    {
        return p.convertFrom0to1 (p.getDefaultValue());
    }
}

const std::vector<FactoryPreset>& factoryPresets()
{
    using namespace pid;
    static const std::vector<FactoryPreset> presets = {
        { "Default", {} },

        { "Slapback", {
            { sync, 0 }, { timeL, 105 }, { link, 1 }, { feedback, 0.12f }, { lowCut, 320 }, { highCut, 4200 },
            { drive, 0.25f }, { mod, 0.10f }, { eqHp, 140 } } },

        { "1/8 Dotted Throw", {
            { divL, 7 }, { feedback, 0.38f }, { lowCut, 280 }, { highCut, 5000 }, { drive, 0.15f }, { mod, 0.20f },
            { eqHp, 120 }, { eq2G, -2.5f }, { eq2F, 3800 } } },

        { "1/4 Dark Throw", {
            { divL, 8 }, { feedback, 0.45f }, { lowCut, 350 }, { highCut, 2800 }, { drive, 0.20f }, { mod, 0.25f },
            { eqHsG, -3.0f }, { eqHsF, 5000 } } },

        { "Ping-Pong Wide", {
            { divL, 5 }, { pingPong, 1 }, { feedback, 0.52f }, { width, 1.5f }, { lowCut, 260 }, { highCut, 5500 },
            { diffusion, 0.15f }, { mod, 0.25f }, { eqHp, 130 } } },

        { "Tape Wobble", {
            { sync, 0 }, { timeL, 380 }, { feedback, 0.55f }, { lowCut, 180 }, { highCut, 3400 }, { drive, 0.50f },
            { mod, 0.60f }, { modRate, 1.5f }, { eqLsG, -2.0f } } },

        { "Dream Wash", {
            { divL, 8 }, { feedback, 0.62f }, { lowCut, 300 }, { highCut, 4800 }, { drive, 0.10f }, { diffusion, 0.75f },
            { mod, 0.40f }, { modRate, 0.35f }, { width, 1.7f }, { eqHp, 200 }, { eqHsG, -2.0f } } },

        { "Long Throw 1/2", {
            { divL, 11 }, { feedback, 0.50f }, { lowCut, 300 }, { highCut, 3600 }, { drive, 0.20f }, { mod, 0.20f },
            { eqHp, 150 } } },

        { "Telephone Echo", {
            { divL, 5 }, { feedback, 0.42f }, { lowCut, 500 }, { highCut, 3200 }, { drive, 0.35f }, { mod, 0.10f },
            { eqHp, 600 }, { eqLp, 3600 }, { eq1G, 3.0f }, { eq1F, 1200 } } },

        { "Haas Spread", {
            { link, 0 }, { divL, 2 }, { divR, 7 }, { feedback, 0.30f }, { lowCut, 250 }, { highCut, 6000 },
            { width, 1.4f }, { mod, 0.18f }, { eqHp, 140 } } },

        { "Clean Digital", {
            { divL, 5 }, { feedback, 0.35f }, { lowCut, 120 }, { highCut, 12000 }, { drive, 0.0f }, { mod, 0.0f },
            { eqHp, 100 } } },
    };
    return presets;
}

void resetSoundParameters (juce::AudioProcessorValueTreeState& apvts)
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! isBehaviourParam (r->paramID))
                setReal (apvts, r->paramID, defaultReal (*r));
}

void applyFactoryPreset (juce::AudioProcessorValueTreeState& apvts, int index)
{
    const auto& all = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) all.size()))
        return;

    resetSoundParameters (apvts);
    for (auto& v : all[(size_t) index].values)
        setReal (apvts, v.id, v.value);
}

//==============================================================================
juce::File getUserPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("EchoThrow").getChildFile ("Presets");
}

juce::StringArray listUserPresets()
{
    juce::StringArray names;
    for (auto& f : getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.xml"))
        names.add (f.getFileNameWithoutExtension());
    names.sort (true);
    return names;
}

bool saveUserPreset (juce::AudioProcessorValueTreeState& apvts, const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty())
        return false;

    juce::XmlElement root ("EchoThrowPreset");
    root.setAttribute ("name", name);
    for (auto* p : apvts.processor.getParameters())
    {
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            if (isBehaviourParam (r->paramID))
                continue;
            auto* e = root.createNewChildElement ("P");
            e->setAttribute ("id", r->paramID);
            e->setAttribute ("v", (double) r->convertFrom0to1 (r->getValue()));
        }
    }

    auto folder = getUserPresetFolder();
    folder.createDirectory();
    return root.writeTo (folder.getChildFile (name + ".xml"));
}

bool loadUserPreset (juce::AudioProcessorValueTreeState& apvts, const juce::String& name)
{
    auto file = getUserPresetFolder().getChildFile (name + ".xml");
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("EchoThrowPreset"))
        return false;

    resetSoundParameters (apvts);
    for (auto* e : xml->getChildWithTagNameIterator ("P"))
    {
        const auto id = e->getStringAttribute ("id");
        if (! isBehaviourParam (id))
            setReal (apvts, id, (float) e->getDoubleAttribute ("v"));
    }
    return true;
}

} // namespace et
