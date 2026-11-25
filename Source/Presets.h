#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace et
{
struct PresetValue { const char* id; float value; };

struct FactoryPreset
{
    const char* name;
    std::vector<PresetValue> values;
};

const std::vector<FactoryPreset>& factoryPresets();

/** Resets every sound parameter to its default, then applies the listed values.
    Behaviour switches (loop, hear dry, export dry, mute live, max tail) are left alone. */
void applyFactoryPreset (juce::AudioProcessorValueTreeState& apvts, int index);

// ---- user presets: small XML files in <Documents>/EchoThrow/Presets ----
juce::File getUserPresetFolder();
juce::StringArray listUserPresets();
bool saveUserPreset (juce::AudioProcessorValueTreeState& apvts, const juce::String& name);
bool loadUserPreset (juce::AudioProcessorValueTreeState& apvts, const juce::String& name);

/** Resets all sound parameters to their defaults. */
void resetSoundParameters (juce::AudioProcessorValueTreeState& apvts);

} // namespace et
