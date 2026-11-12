#pragma once
#include "arch.h"
#include <optional>
#include <cstdint>
#include <vector>
#include <windef.h>

#include "../../Common/config/dynamictint.hpp"
#include "../../Common/util/color.hpp"

// Helpers for the dynamic taskbar tint: they work purely on screen pixels and
// on plain RGB/HSV values, so they are independent from the taskbar plumbing.
namespace DynamicTint {
	// Read-only count for diagnostics and capture-resource regression tests.
	std::uint64_t CaptureAllocationCount() noexcept;
	// Picks the strip of screen pixels that best represents the content "behind"
	// the taskbar: just inside the taskbar, towards the middle of the monitor.
	std::optional<RECT> ComputeSampleBand(const RECT &taskbar, const RECT &monitor, int depth) noexcept;

	// Averages the band of the screen into a single colour. Returns nothing if
	// the screen could not be captured (protected content, secure desktop...).
	std::optional<Util::Color> SampleBand(const RECT &band, int width, int height, float nearBias) noexcept;

	// Same capture, but split horizontally into `zones` independent colours.
	// Returns an empty vector when the screen could not be captured.
	std::vector<Util::Color> SampleBandZones(const RECT &band, int zones, int height, float nearBias) noexcept;

	// Boosts saturation and clamps brightness so the tint stays readable.
	Util::Color Adjust(const Util::Color &raw, const DynamicTintConfig &config) noexcept;

	// Moves from one colour towards another by the given factor, interpolating
	// hue along the shortest arc so colour changes never go through grey.
	Util::Color Blend(const Util::Color &from, const Util::Color &to, float factor) noexcept;

	// Sum of the absolute per-channel differences, used to decide whether
	// another appearance update is worth sending.
	int ColorDelta(const Util::Color &a, const Util::Color &b) noexcept;
}