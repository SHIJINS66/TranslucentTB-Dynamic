#pragma once
#include "../arch.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <string_view>

#if __has_include(<rapidjson/rapidjson.h>)
#define HAS_RAPIDJSON
#include <rapidjson/document.h>
#include "rapidjsonhelper.hpp"
#endif

// Settings for the dynamic taskbar tint: instead of a fixed acrylic colour, the
// tint follows the dominant colour of the content behind the taskbar.
struct DynamicTintConfig {
	bool Enabled = false;
	bool AdaptiveSampling = true;
	bool OnlyFullscreen = false;

	// Sampling rate, clamped to [1, 60].
	int SamplesPerSecond = 30;

	// Animation rate of the bar itself. Rendering faster than sampling makes
	// short fades look smooth without sampling the screen more often.
	int Fps = 60;

	// How much of the way to the target colour we move each frame, 0..1.
	// Lower is smoother but laggier; 0.18 at 30 fps settles in about 300 ms.
	float Smoothing = 0.10f;

	// Minimum milliseconds between screen samples. 0 = sample on every tick
	// (keeps the tint perfectly in sync; filtering handles flicker instead).
	int SampleInterval = 0;

	// Median window over the last N samples (1 = off, odd numbers preferred).
	// A median ignores single-frame flashes and scene cuts far better than a
	// plain moving average, which is what makes video content flicker.
	int MedianWindow = 5;

	// Ignore colour changes smaller than this (sum of RGB deltas) to stop the
	// bar from chasing sensor noise.
	int Deadband = 3;

	// A change larger than this is treated as a scene cut: the temporal filter
	// is bypassed and the bar transitions with cut_smoothing instead, so cuts
	// feel immediate while in-scene flicker stays filtered.
	int CutThreshold = 50;

	// Scene-sized changes must hold this long before being accepted (window
	// animations change every frame and never stabilise). Gradual changes below
	// cut_threshold bypass this and are followed in real time.
	int StabilityMs = 120;

	// A scene-cut candidate that persists this long is accepted even if it
	// never quite stabilised.
	int FailsafeMs = 600;


	// Duration of the eased transition used for scene cuts. A fixed-duration
	// smoothstep is both fast and smooth: it advances evenly and lands exactly
	// on the target, unlike an exponential approach that drags a long tail.
	int CutDurationMs = 220;

	// Tint brightness is clamped into this range so the bar never becomes a
	// glaring pastel or an unreadable black slab.
	float MinValue = 0.12f;
	float MaxValue = 0.50f;

	// Slight saturation boost so washed-out content still produces a tint.
	float SaturationBoost = 1.20f;

	// Alpha (0-255) of the acrylic tint.
	uint8_t Alpha = 168;

	// How deep (in pixels) the sampled area reaches into the screen, measured
	// from the taskbar edge. 0 means "auto": the nearest 1/pi of the visible
	// area, which keeps the colour tied to what is next to the bar without
	// ignoring the rest of the frame.
	int SampleDepth = 0;

	// Vertical weighting: rows next to the taskbar count more than rows further
	// away. 0 = flat (whole area equally), 1 = only the first row matters.
	float NearBias = 0.95f;
	int SampleWidth = 96;

	// Number of independent colour zones across the bar. 1 = single acrylic
	// colour, 2+ = horizontal gradient built from that many samples.
	int Zones = 20;

	static constexpr std::wstring_view ADAPTIVE_KEY = L"adaptive_sampling";
	static constexpr std::wstring_view ENABLED_KEY = L"enabled";
	static constexpr std::wstring_view ONLY_FULLSCREEN_KEY = L"only_fullscreen";
	static constexpr std::wstring_view SAMPLES_PER_SECOND_KEY = L"samples_per_second";
	static constexpr std::wstring_view FPS_KEY = L"fps";
	static constexpr std::wstring_view SMOOTHING_KEY = L"smoothing";
	static constexpr std::wstring_view MIN_VALUE_KEY = L"min_value";
	static constexpr std::wstring_view MAX_VALUE_KEY = L"max_value";
	static constexpr std::wstring_view SATURATION_BOOST_KEY = L"saturation_boost";
	static constexpr std::wstring_view ALPHA_KEY = L"alpha";
	static constexpr std::wstring_view SAMPLE_DEPTH_KEY = L"sample_depth";
	static constexpr std::wstring_view SAMPLE_WIDTH_KEY = L"sample_width";
	static constexpr std::wstring_view ZONES_KEY = L"zones";
	static constexpr std::wstring_view NEAR_BIAS_KEY = L"near_bias";
	static constexpr std::wstring_view SAMPLE_INTERVAL_KEY = L"sample_interval";
	static constexpr std::wstring_view MEDIAN_WINDOW_KEY = L"median_window";
	static constexpr std::wstring_view DEADBAND_KEY = L"deadband";
	static constexpr std::wstring_view CUT_THRESHOLD_KEY = L"cut_threshold";
	static constexpr std::wstring_view CUT_DURATION_KEY = L"cut_duration_ms";
	static constexpr std::wstring_view STABILITY_KEY = L"stability_ms";
	static constexpr std::wstring_view FAILSAFE_KEY = L"failsafe_ms";

	inline void Clamp() noexcept
	{
		SamplesPerSecond = std::clamp(SamplesPerSecond, 1, 60);
		Fps = std::clamp(Fps, 20, 120);
		Smoothing = std::clamp(Smoothing, 0.01f, 1.0f);
		MinValue = std::clamp(MinValue, 0.0f, 1.0f);
		MaxValue = std::clamp(MaxValue, MinValue, 1.0f);
		SaturationBoost = std::clamp(SaturationBoost, 0.0f, 4.0f);
		SampleDepth = std::clamp(SampleDepth, 0, 4096);
		NearBias = std::clamp(NearBias, 0.0f, 1.0f);
		SampleInterval = std::clamp(SampleInterval, 0, 1000);
		MedianWindow = std::clamp(MedianWindow, 1, 9);
		Deadband = std::clamp(Deadband, 0, 60);
		CutThreshold = std::clamp(CutThreshold, 0, 765);
		CutDurationMs = std::clamp(CutDurationMs, 40, 2000);
		StabilityMs = std::clamp(StabilityMs, 0, 5000);
		FailsafeMs = std::clamp(FailsafeMs, 0, 10000);
		SampleWidth = std::clamp(SampleWidth, 4, 512);
		Zones = std::clamp(Zones, 1, 32);
	}

#ifdef HAS_RAPIDJSON
	template<class Writer>
	inline void Serialize(Writer &writer) const
	{
		rjh::Serialize(writer, Enabled, ENABLED_KEY);
		rjh::Serialize(writer, AdaptiveSampling, ADAPTIVE_KEY);
		rjh::Serialize(writer, OnlyFullscreen, ONLY_FULLSCREEN_KEY);
		rjh::Serialize(writer, static_cast<float>(SamplesPerSecond), SAMPLES_PER_SECOND_KEY);
		rjh::Serialize(writer, static_cast<float>(Fps), FPS_KEY);
		rjh::Serialize(writer, Smoothing, SMOOTHING_KEY);
		rjh::Serialize(writer, MinValue, MIN_VALUE_KEY);
		rjh::Serialize(writer, MaxValue, MAX_VALUE_KEY);
		rjh::Serialize(writer, SaturationBoost, SATURATION_BOOST_KEY);
		rjh::Serialize(writer, static_cast<float>(Alpha), ALPHA_KEY);
		rjh::Serialize(writer, static_cast<float>(SampleDepth), SAMPLE_DEPTH_KEY);
		rjh::Serialize(writer, static_cast<float>(SampleWidth), SAMPLE_WIDTH_KEY);
		rjh::Serialize(writer, static_cast<float>(Zones), ZONES_KEY);
		rjh::Serialize(writer, NearBias, NEAR_BIAS_KEY);
		rjh::Serialize(writer, static_cast<float>(SampleInterval), SAMPLE_INTERVAL_KEY);
		rjh::Serialize(writer, static_cast<float>(MedianWindow), MEDIAN_WINDOW_KEY);
		rjh::Serialize(writer, static_cast<float>(Deadband), DEADBAND_KEY);
		rjh::Serialize(writer, static_cast<float>(CutThreshold), CUT_THRESHOLD_KEY);
		rjh::Serialize(writer, static_cast<float>(CutDurationMs), CUT_DURATION_KEY);
		rjh::Serialize(writer, static_cast<float>(StabilityMs), STABILITY_KEY);
		rjh::Serialize(writer, static_cast<float>(FailsafeMs), FAILSAFE_KEY);
	}

	void Deserialize(const rjh::value_t &obj, void (*unknownKeyCallback)(std::wstring_view))
	{
		rjh::EnsureType(rj::Type::kObjectType, obj.GetType(), L"root node");

		for (auto it = obj.MemberBegin(); it != obj.MemberEnd(); ++it)
		{
			rjh::EnsureType(rj::Type::kStringType, it->name.GetType(), L"member name");

			const auto key = rjh::ValueToStringView(it->name);
			if (key == ENABLED_KEY)
			{
				rjh::Deserialize(it->value, Enabled, key);
			}
			else if (key == ADAPTIVE_KEY)
			{
				rjh::Deserialize(it->value, AdaptiveSampling, key);
			}
			else if (key == ONLY_FULLSCREEN_KEY)
			{
				rjh::Deserialize(it->value, OnlyFullscreen, key);
			}
			else if (key == SAMPLES_PER_SECOND_KEY)
			{
				SamplesPerSecond = ReadInt(it->value, key);
			}
			else if (key == FPS_KEY)
			{
				Fps = ReadInt(it->value, key);
			}
			else if (key == SMOOTHING_KEY)
			{
				rjh::Deserialize(it->value, Smoothing, key);
			}
			else if (key == MIN_VALUE_KEY)
			{
				rjh::Deserialize(it->value, MinValue, key);
			}
			else if (key == MAX_VALUE_KEY)
			{
				rjh::Deserialize(it->value, MaxValue, key);
			}
			else if (key == SATURATION_BOOST_KEY)
			{
				rjh::Deserialize(it->value, SaturationBoost, key);
			}
			else if (key == ALPHA_KEY)
			{
				Alpha = static_cast<uint8_t>(std::clamp(ReadInt(it->value, key), 0, 255));
			}
			else if (key == SAMPLE_DEPTH_KEY)
			{
				SampleDepth = ReadInt(it->value, key);
			}
			else if (key == SAMPLE_WIDTH_KEY)
			{
				SampleWidth = ReadInt(it->value, key);
			}
			else if (key == ZONES_KEY)
			{
				Zones = ReadInt(it->value, key);
			}
			else if (key == NEAR_BIAS_KEY)
			{
				rjh::Deserialize(it->value, NearBias, key);
			}
			else if (key == SAMPLE_INTERVAL_KEY)
			{
				SampleInterval = ReadInt(it->value, key);
			}
			else if (key == MEDIAN_WINDOW_KEY)
			{
				MedianWindow = ReadInt(it->value, key);
			}
			else if (key == DEADBAND_KEY)
			{
				Deadband = ReadInt(it->value, key);
			}
			else if (key == CUT_THRESHOLD_KEY)
			{
				CutThreshold = ReadInt(it->value, key);
			}
			else if (key == CUT_DURATION_KEY)
			{
				CutDurationMs = ReadInt(it->value, key);
			}
			else if (key == STABILITY_KEY)
			{
				StabilityMs = ReadInt(it->value, key);
			}
			else if (key == FAILSAFE_KEY)
			{
				FailsafeMs = ReadInt(it->value, key);
			}
			else if (unknownKeyCallback)
			{
				unknownKeyCallback(key);
			}
		}

		Clamp();
	}

private:
	static inline int ReadInt(const rjh::value_t &value, std::wstring_view key)
	{
		rjh::EnsureType(rj::Type::kNumberType, value.GetType(), key);

		// The app itself writes these fields back as doubles ("fps": 120.0), so
		// require an integer only loosely: accept any JSON number and round.
		if (value.IsInt())
		{
			return value.GetInt();
		}
		if (value.IsUint())
		{
			return static_cast<int>(value.GetUint());
		}
		return static_cast<int>(std::lround(value.GetDouble()));
	}
#endif
};