#include <gtest/gtest.h>
#include "config/dynamictintpanel.hpp"

TEST(DynamicTintPanel, FrequencyControlReplacesLegacyInterval)
{
    DynamicTintConfig tint;
    tint.SampleInterval = 500;
    ApplyDynamicTintPanelSettings(tint, 12, 75, 45, 160);
    EXPECT_EQ(tint.SampleInterval, 0);
    EXPECT_EQ(tint.Zones, 12);
    EXPECT_EQ(tint.Fps, 75);
    EXPECT_EQ(tint.SamplesPerSecond, 45);
    EXPECT_EQ(tint.SampleDepth, 160);
}

TEST(DynamicTintPanel, EditingParametersPreservesEnableStateAndAppearanceOptions)
{
    for (bool enabled : {false, true})
    {
        DynamicTintConfig tint;
        tint.Enabled = enabled;
        tint.OnlyFullscreen = true;
        tint.Alpha = 211;
        tint.NearBias = 0.8f;
        tint.CutDurationMs = 300;
        ApplyDynamicTintPanelSettings(tint, 1, 20, 1, 0);
        EXPECT_EQ(tint.Enabled, enabled);
        EXPECT_TRUE(tint.OnlyFullscreen);
        EXPECT_EQ(tint.Alpha, 211);
        EXPECT_FLOAT_EQ(tint.NearBias, 0.8f);
        EXPECT_EQ(tint.CutDurationMs, 300);
        EXPECT_EQ(tint.SampleDepth, 0); // Auto remains a valid choice.
    }
}

TEST(DynamicTintPanel, NonUiCallersStillReceiveBoundedParameters)
{
    DynamicTintConfig tint;
    ApplyDynamicTintPanelSettings(tint, -1, -1, -1, -1);
    EXPECT_EQ(tint.Zones, 1);
    EXPECT_EQ(tint.Fps, 20);
    EXPECT_EQ(tint.SamplesPerSecond, 1);
    EXPECT_EQ(tint.SampleDepth, 0);
    ApplyDynamicTintPanelSettings(tint, 10000, 10000, 10000, 10000);
    EXPECT_EQ(tint.Zones, 32);
    EXPECT_EQ(tint.Fps, 120);
    EXPECT_EQ(tint.SamplesPerSecond, 60);
    EXPECT_EQ(tint.SampleDepth, 4096);
}
