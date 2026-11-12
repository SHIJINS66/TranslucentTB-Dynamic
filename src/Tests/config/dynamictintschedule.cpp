#include <gtest/gtest.h>
#include "util/dynamictintschedule.hpp"
#include "config/dynamictintpanel.hpp"

TEST(DynamicTintSchedule, FirstCaptureIsImmediate)
{
    DynamicTintSchedule s;
    EXPECT_TRUE(s.Due(1000, 25, true));
}
TEST(DynamicTintSchedule, StaticSceneHasHalfSecondFallback)
{
    DynamicTintSchedule s;
    s.Sampled(1000, false);
    EXPECT_EQ(s.Delay(1000, 25, true), 500u);
    EXPECT_FALSE(s.Due(1499, 25, true));
    EXPECT_TRUE(s.Due(1500, 25, true));
}
TEST(DynamicTintSchedule, RepeatedInputCannotPostponePendingCapture)
{
    DynamicTintSchedule s;
    s.Sampled(1000, false);
    s.Activity(1010);
    s.Activity(1060);
    s.Activity(1100);
    EXPECT_EQ(s.Delay(1100, 25, true), 10u);
    EXPECT_TRUE(s.Due(1110, 25, true));
}
TEST(DynamicTintSchedule, MotionIsLimitedToTwentyHzAndStopsAfterStableCaptures)
{
    DynamicTintSchedule s;
    s.Sampled(1000, true);
    EXPECT_EQ(s.Delay(1000, 8, true), 50u);
    s.Sampled(1050, false);
    s.Sampled(1100, false);
    s.Sampled(1150, false);
    EXPECT_EQ(s.Delay(1150, 8, true), 500u);
}
TEST(DynamicTintSchedule, ActivityRespectsLowConfiguredRate)
{
    DynamicTintSchedule s;
    s.Sampled(1000, true);
    s.Activity(1010);
    EXPECT_FALSE(s.Due(1500, 1000, true));
    EXPECT_TRUE(s.Due(2000, 1000, true));
}
TEST(DynamicTintSchedule, ContinuousModePreservesSamplingInterval)
{
    DynamicTintSchedule s;
    s.Sampled(1000, false);
    EXPECT_EQ(s.Delay(1000, 25, false), 25u);
    EXPECT_FALSE(s.Due(1024, 25, false));
    EXPECT_TRUE(s.Due(1025, 25, false));
}
TEST(DynamicTintSchedule, SlowStartupRetainsLateRetries)
{
    DynamicTintSchedule s;
    s.Sampled(1000, false);
    s.Startup(1000);
    s.Sampled(1100, false);
    s.Sampled(1200, false);
    s.Sampled(1400, false);
    s.Sampled(1800, false);
    s.Sampled(2600, false);
    s.Sampled(4100, false);
    EXPECT_EQ(s.Delay(4100, 25, true), 100u); // 3200 ms retry
    s.Sampled(4200, false);
    s.Sampled(6900, false);
    EXPECT_EQ(s.Delay(6900, 25, true), 100u); // 6000 ms retry
    s.Sampled(7000, false);
    EXPECT_EQ(s.Delay(7000, 25, true), 500u);
}
TEST(DynamicTintSchedule, ResetRequestsFreshCapture)
{
    DynamicTintSchedule s;
    s.Sampled(1000, false);
    s.Reset();
    EXPECT_TRUE(s.Due(1001, 25, true));
}
TEST(DynamicTintPanel, ModeSelectionDoesNotEnableFeature)
{
    DynamicTintConfig tint;
    ApplyDynamicTintPanelSettings(tint, 16, 120, 40, 0, false);
    EXPECT_FALSE(tint.AdaptiveSampling);
    EXPECT_FALSE(tint.Enabled);
    ApplyDynamicTintPanelSettings(tint, 16, 120, 40, 0, true);
    EXPECT_TRUE(tint.AdaptiveSampling);
    EXPECT_FALSE(tint.Enabled);
}
TEST(DynamicTintConfig, OldConfigDefaultsToAdaptiveAndExplicitContinuousLoads)
{
    DynamicTintConfig tint;
    EXPECT_TRUE(tint.AdaptiveSampling);
    rj::GenericDocument<rj::UTF16LE<>> document;
    document.Parse(L"{\"enabled\":true,\"adaptive_sampling\":false}");
    ASSERT_FALSE(document.HasParseError());
    tint.Deserialize(document, nullptr);
    EXPECT_TRUE(tint.Enabled);
    EXPECT_FALSE(tint.AdaptiveSampling);
}
