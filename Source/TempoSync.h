#pragma once
#include <juce_core/juce_core.h>

// Host-tempo note divisions shared by the LFO sync and the delay sync.
// Lengths are in beats (quarter notes); sorted slow -> fast.
namespace TempoSync
{
    struct Division { const char* label; double beats; };

    static constexpr Division kDivisions[] = {
        { "4/1", 16.0 }, { "2/1", 8.0 }, { "1/1", 4.0 },
        { "1/2D", 3.0 }, { "1/2", 2.0 }, { "1/2T", 4.0 / 3.0 },
        { "1/4D", 1.5 }, { "1/4", 1.0 }, { "1/4T", 2.0 / 3.0 },
        { "1/8D", 0.75 }, { "1/8", 0.5 }, { "1/8T", 1.0 / 3.0 },
        { "1/16D", 0.375 }, { "1/16", 0.25 }, { "1/16T", 1.0 / 6.0 },
        { "1/32", 0.125 },
    };
    static constexpr int kNumDivisions = (int)(sizeof(kDivisions) / sizeof(kDivisions[0]));

    inline double seconds(int div, double bpm)
    {
        return kDivisions[juce::jlimit(0, kNumDivisions - 1, div)].beats * 60.0 / juce::jmax(20.0, bpm);
    }
    inline double hertz(int div, double bpm) { return 1.0 / seconds(div, bpm); }

    // LFO sync choice: "Off" then every division
    inline juce::StringArray lfoChoices()
    {
        juce::StringArray s { "Off" };
        for (auto& d : kDivisions) s.add(d.label);
        return s;
    }

    // Delay sync choice. Indices 0..4 are the original set (stored in old
    // presets/projects, so they keep their place); the rest of the divisions
    // follow. The editor lists them sorted by length.
    static constexpr Division kDelayChoices[] = {
        { "Free", 0.0 }, { "1/8", 0.5 }, { "1/8D", 0.75 }, { "1/4", 1.0 }, { "1/2", 2.0 },
        { "1/1", 4.0 }, { "1/2D", 3.0 }, { "1/2T", 4.0 / 3.0 }, { "1/4D", 1.5 }, { "1/4T", 2.0 / 3.0 },
        { "1/8T", 1.0 / 3.0 }, { "1/16D", 0.375 }, { "1/16", 0.25 }, { "1/16T", 1.0 / 6.0 }, { "1/32", 0.125 },
    };
    static constexpr int kNumDelayChoices = (int)(sizeof(kDelayChoices) / sizeof(kDelayChoices[0]));
    inline double delayBeats(int choice) // 0 = free (returns 0)
    {
        return (choice <= 0 || choice >= kNumDelayChoices) ? 0.0 : kDelayChoices[choice].beats;
    }
}
