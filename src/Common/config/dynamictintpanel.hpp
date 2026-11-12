#pragma once
#include "dynamictint.hpp"

// Applies only the controls exposed by the optional parameter panel. Enabling
// the feature and all original TranslucentTB appearance rules are preserved.
inline void ApplyDynamicTintPanelSettings(DynamicTintConfig &tint, int zones, int fps, int samplesPerSecond, int sampleDepth, bool adaptiveSampling = true) noexcept
{
    tint.AdaptiveSampling = adaptiveSampling;
    tint.Zones = zones;
    tint.Fps = fps;
    tint.SamplesPerSecond = samplesPerSecond;
    tint.SampleDepth = sampleDepth;
    // The frequency control takes precedence over the legacy interval option.
    tint.SampleInterval = 0;
    tint.Clamp();
}
