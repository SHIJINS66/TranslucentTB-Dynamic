#include <gtest/gtest.h>
#include <Windows.h>
#include "../../TranslucentTB/taskbar/dynamictint.hpp"

TEST(DynamicTintCapture, ReusesBitmapAcrossRepeatedCaptures)
{
    const RECT band {0, 0, 512, 16};
    if (DynamicTint::SampleBandZones(band, 16, 12, 0.95f).empty()) GTEST_SKIP() << "Interactive desktop capture unavailable";
    const auto allocations = DynamicTint::CaptureAllocationCount();
    const auto resources = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    for (int i = 0; i < 100; ++i)
        ASSERT_EQ(DynamicTint::SampleBandZones(band, 16, 12, 0.95f).size(), 16u);
    EXPECT_EQ(DynamicTint::CaptureAllocationCount(), allocations);
    EXPECT_LE(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS), resources + 1);
}
TEST(DynamicTintCapture, ResizingAndSingleZonePathsDoNotLeakGdiObjects)
{
    const RECT band {0, 0, 512, 16};
    if (DynamicTint::SampleBandZones(band, 16, 12, 0.95f).empty()) GTEST_SKIP() << "Interactive desktop capture unavailable";
    const auto resources = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    for (int i = 0; i < 50; ++i)
    {
        ASSERT_EQ(DynamicTint::SampleBandZones(band, 8, 12, 0.95f).size(), 8u);
        ASSERT_TRUE(DynamicTint::SampleBand(band, 96, 12, 0.95f).has_value());
        ASSERT_EQ(DynamicTint::SampleBandZones(band, 20, 12, 0.95f).size(), 20u);
    }
    EXPECT_LE(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS), resources + 1);
}
