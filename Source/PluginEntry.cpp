#include "PluginProcessor.h"

// This is the one function the plugin wrapper looks for.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new et::EchoThrowProcessor();
}
