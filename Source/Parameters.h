#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace et
{
//==============================================================================
// Parameter IDs. Never change these after release (host automation / projects use them).
namespace pid
{
    // delay
    constexpr const char* sync      = "sync";
    constexpr const char* link      = "link";
    constexpr const char* timeL     = "timeL";
    constexpr const char* timeR     = "timeR";
    constexpr const char* divL      = "divL";
    constexpr const char* divR      = "divR";
    constexpr const char* feedback  = "feedback";
    constexpr const char* pingPong  = "pingpong";
    constexpr const char* width     = "width";
    constexpr const char* lowCut    = "lowCut";
    constexpr const char* highCut   = "highCut";
    constexpr const char* drive     = "drive";
    constexpr const char* mod       = "mod";
    constexpr const char* modRate   = "modRate";
    constexpr const char* diffusion = "diffusion";
    constexpr const char* wetLevel  = "wetLevel";
    constexpr const char* tail      = "tail";

    // preview / export / transport behaviour
    constexpr const char* hearDry   = "hearDry";
    constexpr const char* exportDry = "exportDry";
    constexpr const char* muteLive  = "muteLive";
    constexpr const char* loop      = "loop";

    // EQ (applied to the echoes only)
    constexpr const char* eqOn      = "eqOn";
    constexpr const char* eqHp      = "eqHp";
    constexpr const char* eqLp      = "eqLp";
    constexpr const char* eqLsF     = "eqLsF";
    constexpr const char* eqLsG     = "eqLsG";
    constexpr const char* eq1F      = "eq1F";
    constexpr const char* eq1G      = "eq1G";
    constexpr const char* eq1Q      = "eq1Q";
    constexpr const char* eq2F      = "eq2F";
    constexpr const char* eq2G      = "eq2G";
    constexpr const char* eq2Q      = "eq2Q";
    constexpr const char* eqHsF     = "eqHsF";
    constexpr const char* eqHsG     = "eqHsG";
}

//==============================================================================
struct SyncDivision
{
    const char* name;
    double beats;      // length in quarter notes
};

constexpr int kNumDivisions = 14;
const std::array<SyncDivision, kNumDivisions>& syncDivisions();
constexpr int kDefaultDivision = 5; // 1/8

/** Delay time in seconds for a division at a tempo. */
double divisionSeconds (int index, double bpm);

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// text helpers shared with the UI
juce::String formatHz (float hz);
juce::String formatMs (float ms);
juce::String formatDb (float db);
juce::String formatPercent (float normalised);

} // namespace et
