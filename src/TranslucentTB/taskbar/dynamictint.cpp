#include "arch.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <windef.h>
#include <wingdi.h>

#include "../../Common/win32.hpp"
#include "dynamictint.hpp"

namespace {

    class CaptureSurface {
        HDC m_Dc = nullptr;
        HBITMAP m_Bitmap = nullptr;
        HGDIOBJ m_Original = nullptr;
        void *m_Bits = nullptr;
        int m_Width = 0, m_Height = 0;
        std::uint64_t m_Allocations = 0;
    public:
        CaptureSurface() = default;
        CaptureSurface(const CaptureSurface &) = delete;
        CaptureSurface &operator=(const CaptureSurface &) = delete;
        ~CaptureSurface()
        {
            if (m_Dc && m_Original) SelectObject(m_Dc, m_Original);
            if (m_Bitmap) DeleteObject(m_Bitmap);
            if (m_Dc) DeleteDC(m_Dc);
        }
        bool Ensure(HDC screen, int width, int height) noexcept
        {
            if (!m_Dc) m_Dc = CreateCompatibleDC(screen);
            if (!m_Dc) return false;
            if (m_Bitmap && width == m_Width && height == m_Height) return true;
            BITMAPINFO info { };
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = width;
            info.bmiHeader.biHeight = -height;
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            void *bits = nullptr;
            const HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
            if (!bitmap || !bits) { if (bitmap) DeleteObject(bitmap); return false; }
            const HGDIOBJ old = SelectObject(m_Dc, bitmap);
            if (!old || old == HGDI_ERROR) { DeleteObject(bitmap); return false; }
            if (!m_Original) m_Original = old;
            if (m_Bitmap) DeleteObject(m_Bitmap);
            ++m_Allocations;
            m_Bitmap = bitmap; m_Bits = bits; m_Width = width; m_Height = height;
            return true;
        }
        std::uint64_t Allocations() const noexcept { return m_Allocations; }
        HDC Dc() const noexcept { return m_Dc; }
        void *Bits() const noexcept { return m_Bits; }
    };
    // A single bounded buffer per sampling thread, shared by single/zone paths.
    CaptureSurface &CaptureBuffer() { static thread_local CaptureSurface surface; return surface; }

	inline double SrgbToLinear(uint8_t value) noexcept
	{
		const double c = value / 255.0;
		return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
	}

	inline uint8_t LinearToSrgb(double value) noexcept
	{
		value = std::clamp(value, 0.0, 1.0);
		const double c = value <= 0.0031308 ? value * 12.92 : 1.055 * std::pow(value, 1.0 / 2.4) - 0.055;
		return static_cast<uint8_t>(std::lround(c * 255.0));
	}

	inline double WrapHue(double hue) noexcept
	{
		hue = std::fmod(hue, 360.0);
		return hue < 0.0 ? hue + 360.0 : hue;
	}
}

std::uint64_t DynamicTint::CaptureAllocationCount() noexcept { return CaptureBuffer().Allocations(); }

std::optional<RECT> DynamicTint::ComputeSampleBand(const RECT &taskbar, const RECT &monitor, int depth) noexcept
{
	const LONG taskbarWidth = taskbar.right - taskbar.left;
	const LONG taskbarHeight = taskbar.bottom - taskbar.top;
	if (taskbarWidth <= 0 || taskbarHeight <= 0)
	{
		return std::nullopt;
	}

	const LONG gap = 10; // skip the bar's own border/shadow entirely
	RECT band { };

	const bool horizontal = taskbarWidth >= taskbarHeight;

	if (depth <= 0)
	{
		// Automatic depth: the nearest 1/50 of the visible area. A very tight
		// slice under the bar keeps the tint tied to the content it sits on and
		// ignores motion further down the frame.
		const LONG available = horizontal
			? (taskbar.top - monitor.top <= monitor.bottom - taskbar.bottom ? monitor.bottom - taskbar.bottom : taskbar.top - monitor.top)
			: (taskbar.left - monitor.left <= monitor.right - taskbar.right ? monitor.right - taskbar.right : taskbar.left - monitor.left);
		depth = std::max<LONG>(2, static_cast<LONG>(std::lround(available / 50.0)));
	}

	const LONG reach = depth;
	if (horizontal)
	{
		// A horizontal bar either hugs the top or the bottom of the monitor.
		const bool atTop = taskbar.top - monitor.top <= monitor.bottom - taskbar.bottom;
		if (atTop)
		{
			const LONG top = taskbar.bottom + gap;
			const LONG bottom = reach > 0 ? std::min(top + reach, monitor.bottom) : monitor.bottom;
			band = { taskbar.left, top, taskbar.right, bottom };
		}
		else
		{
			const LONG bottom = taskbar.top - gap;
			const LONG top = reach > 0 ? std::max(bottom - reach, monitor.top) : monitor.top;
			band = { taskbar.left, top, taskbar.right, bottom };
		}
	}
	else
	{
		const bool atLeft = taskbar.left - monitor.left <= monitor.right - taskbar.right;
		if (atLeft)
		{
			const LONG left = taskbar.right + gap;
			const LONG right = reach > 0 ? std::min(left + reach, monitor.right) : monitor.right;
			band = { left, taskbar.top, right, taskbar.bottom };
		}
		else
		{
			const LONG right = taskbar.left - gap;
			const LONG left = reach > 0 ? std::max(right - reach, monitor.left) : monitor.left;
			band = { left, taskbar.top, right, taskbar.bottom };
		}
	}

	// Keep the band inside the monitor so we never sample another screen's
	// content or the desktop's outside area.
	band.left = std::max(band.left, monitor.left);
	band.top = std::max(band.top, monitor.top);
	band.right = std::min(band.right, monitor.right);
	band.bottom = std::min(band.bottom, monitor.bottom);

	if (band.right - band.left < 2 || band.bottom - band.top < 2)
	{
		return std::nullopt;
	}

	return band;
}

std::optional<Util::Color> DynamicTint::SampleBand(const RECT &band, int width, int height, float nearBias) noexcept
{
	const LONG sourceWidth = band.right - band.left;
	const LONG sourceHeight = band.bottom - band.top;
	if (sourceWidth < 1 || sourceHeight < 1 || width < 1 || height < 1)
	{
		return std::nullopt;
	}

	HDC screenDc = GetDC(nullptr);
	if (!screenDc)
	{
		return std::nullopt;
	}

	auto &surface = CaptureBuffer();
	if (!surface.Ensure(screenDc, width, height))
	{
		ReleaseDC(nullptr, screenDc);
		return std::nullopt;
	}
	const HDC memDc = surface.Dc();
	void *const bits = surface.Bits();

	// Let GDI do the downsampling for us; the band is tiny so quality does not
	// matter much, but a halftone stretch avoids aliasing artefacts.
	SetStretchBltMode(memDc, COLORONCOLOR);

	std::optional<Util::Color> result;
	if (StretchBlt(memDc, 0, 0, width, height, screenDc, band.left, band.top, sourceWidth, sourceHeight, SRCCOPY))
	{
		GdiFlush();
		double sumR = 0.0, sumG = 0.0, sumB = 0.0;
		const auto *pixels = static_cast<const uint32_t *>(bits);
		const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

		double weightSum = 0.0;
		const double rowSpan = height > 1 ? static_cast<double>(height - 1) : 1.0;
		for (std::size_t i = 0; i < count; ++i)
		{
			// 0xAARRGGBB, GDI writes in BGRA order on little endian.
			const uint32_t pixel = pixels[i];
			const uint8_t r = static_cast<uint8_t>(pixel >> 16);
			const uint8_t g = static_cast<uint8_t>(pixel >> 8);
			const uint8_t b = static_cast<uint8_t>(pixel);

			// Weight saturated pixels much more than grey ones: averaging a
			// colourful scene arithmetically washes it out into a muddy grey,
			// while weighting by saturation keeps the scene's actual colour.
			const uint8_t maximum = std::max({ r, g, b });
			const uint8_t minimum = std::min({ r, g, b });
			const double saturation = maximum == 0 ? 0.0 : static_cast<double>(maximum - minimum) / maximum;

			// Rows closer to the taskbar are weighted more, so the bar stays
			// visually connected to the content it sits on.
			const double row = static_cast<double>((i / static_cast<std::size_t>(width)));
			const double proximity = 1.0 - static_cast<double>(nearBias) * (row / rowSpan);
			const double weight = (0.70 + 0.60 * saturation * saturation) * proximity;

			sumB += SrgbToLinear(b) * weight;
			sumG += SrgbToLinear(g) * weight;
			sumR += SrgbToLinear(r) * weight;
			weightSum += weight;
		}

		if (weightSum > 0.0)
		{
			result = Util::Color {
				LinearToSrgb(sumR / weightSum),
				LinearToSrgb(sumG / weightSum),
				LinearToSrgb(sumB / weightSum),
				255
			};
		}
	}

	ReleaseDC(nullptr, screenDc);

	return result;
}

std::vector<Util::Color> DynamicTint::SampleBandZones(const RECT &band, int zones, int height, float nearBias) noexcept
{
	std::vector<Util::Color> result;

	zones = std::clamp(zones, 2, 32);
	height = std::clamp(height, 2, 32);

	const LONG sourceWidth = band.right - band.left;
	const LONG sourceHeight = band.bottom - band.top;
	if (sourceWidth < zones || sourceHeight < 1)
	{
		return result;
	}

	static constexpr int COLUMNS_PER_ZONE = 8;
	const int width = zones * COLUMNS_PER_ZONE;

	HDC screenDc = GetDC(nullptr);
	if (!screenDc)
	{
		return result;
	}

	auto &surface = CaptureBuffer();
	if (!surface.Ensure(screenDc, width, height))
	{
		ReleaseDC(nullptr, screenDc);
		return result;
	}
	const HDC memDc = surface.Dc();
	void *const bits = surface.Bits();
	SetStretchBltMode(memDc, COLORONCOLOR);

	if (StretchBlt(memDc, 0, 0, width, height, screenDc, band.left, band.top, sourceWidth, sourceHeight, SRCCOPY))
	{
		GdiFlush();
		result.resize(zones);
		const auto *pixels = static_cast<const uint32_t *>(bits);

		// Per zone: first work out the zone's dominant brightness, then drop rows
		// that are much brighter than it. On a top taskbar the band's far edge
		// lands on window chrome (title text, search boxes, app icons). Those
		// pixels are a small part of the band, but they are bright and saturated,
		// and because colours are mixed in linear light a white glyph is ~90x
		// brighter than the dark window behind it, so it used to drag the whole
		// zone towards its own hue - which is what made an otherwise white/black
		// gradient show blue and yellow stops. Dropping those rows keeps the tint
		// on the content the bar actually sits on.
		for (int zone = 0; zone < zones; ++zone)
		{
			double rowLuma[32] = { };
			bool keepRow[32] = { };
			for (int y = 0; y < height; ++y)
			{
				double sum = 0.0;
				for (int x = zone * COLUMNS_PER_ZONE; x < (zone + 1) * COLUMNS_PER_ZONE; ++x)
				{
					const uint32_t pixel = pixels[y * width + x];
					sum += 0.299 * ((pixel >> 16) & 0xFF) + 0.587 * ((pixel >> 8) & 0xFF) + 0.114 * (pixel & 0xFF);
				}
				rowLuma[y] = sum / COLUMNS_PER_ZONE;
			}

			double sortedLuma[32] = { };
			for (int y = 0; y < height; ++y)
			{
				sortedLuma[y] = rowLuma[y];
			}
			std::sort(sortedLuma, sortedLuma + height);
			const double medianLuma = sortedLuma[height / 2];
			const double rejectAbove = std::max(medianLuma * 1.5 + 6.0, 40.0);

			int keepCount = 0;
			for (int y = 0; y < height; ++y)
			{
				keepRow[y] = rowLuma[y] <= rejectAbove;
				keepCount += keepRow[y] ? 1 : 0;
			}
			if (keepCount * 2 < height)
			{
				// Most of the band is bright, so there is nothing UI-like to
				// discard; use every row.
				for (int y = 0; y < height; ++y)
				{
					keepRow[y] = true;
				}
			}

			struct ColumnSample
			{
				double sumR = 0.0, sumG = 0.0, sumB = 0.0, weightSum = 0.0;
			};
			ColumnSample columns[COLUMNS_PER_ZONE] = { };

			for (int x = zone * COLUMNS_PER_ZONE; x < (zone + 1) * COLUMNS_PER_ZONE; ++x)
			{
				auto &column = columns[x - zone * COLUMNS_PER_ZONE];

				for (int y = 0; y < height; ++y)
				{
					if (!keepRow[y])
					{
						continue;
					}

					const uint32_t pixel = pixels[y * width + x];
					const uint8_t r = static_cast<uint8_t>(pixel >> 16);
					const uint8_t g = static_cast<uint8_t>(pixel >> 8);
					const uint8_t b = static_cast<uint8_t>(pixel);

					const uint8_t maximum = std::max({ r, g, b });
					const uint8_t minimum = std::min({ r, g, b });
					const double saturation = maximum == 0 ? 0.0 : static_cast<double>(maximum - minimum) / maximum;
					const double row = static_cast<double>(y);
					const double rowSpan = height > 1 ? static_cast<double>(height - 1) : 1.0;
					const double proximity = 1.0 - static_cast<double>(nearBias) * (row / rowSpan);
					const double weight = (0.70 + 0.60 * saturation * saturation) * proximity;

					column.sumB += SrgbToLinear(b) * weight;
					column.sumG += SrgbToLinear(g) * weight;
					column.sumR += SrgbToLinear(r) * weight;
					column.weightSum += weight;
				}
			}

			// Per-column colour (linear space), ignoring empty columns.
			std::vector<std::pair<double, Util::Color>> columnColors;
			columnColors.reserve(COLUMNS_PER_ZONE);
			for (const auto &column : columns)
			{
				if (column.weightSum > 0.0)
				{
					const Util::Color color {
						LinearToSrgb(column.sumR / column.weightSum),
						LinearToSrgb(column.sumG / column.weightSum),
						LinearToSrgb(column.sumB / column.weightSum),
						255
					};
					columnColors.emplace_back(0.299 * color.R + 0.587 * color.G + 0.114 * color.B, color);
				}
			}

			if (!columnColors.empty())
			{
				// Median column by luminance: robust against a small saturated
				// feature occupying a minority of the zone's width.
				std::sort(columnColors.begin(), columnColors.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
				result[zone] = columnColors[columnColors.size() / 2].second;
			}
		}
	}

	ReleaseDC(nullptr, screenDc);

	return result;
}

Util::Color DynamicTint::Adjust(const Util::Color &raw, const DynamicTintConfig &config) noexcept
{
	Util::HsvColor hsv = raw.ToHSV();

	// Near-black pixels have a tiny max channel, which makes their *relative*
	// saturation huge: (3,8,2) reports S = 0.75 and would be lifted into a vivid
	// green by the brightness floor. Such colours are noise, not hue, so they
	// are treated as neutral and only get a faint tint as they brighten.
	if (hsv.V < 0.08)
	{
		hsv.S = 0.0;
	}
	else if (hsv.V < 0.20)
	{
		hsv.S *= hsv.V / 0.20;
	}

	hsv.S = std::clamp(hsv.S * config.SaturationBoost, 0.0, 1.0);
	hsv.V = std::clamp(hsv.V, static_cast<double>(config.MinValue), static_cast<double>(config.MaxValue));

	return Util::Color::FromHSV(WrapHue(hsv.H), hsv.S, hsv.V);
}

Util::Color DynamicTint::Blend(const Util::Color &from, const Util::Color &to, float factor) noexcept
{
	factor = std::clamp(factor, 0.0f, 1.0f);

	// RGB interpolation only. Hue-arc blending sounds appealing but swings the
	// fade through saturated colours that exist in neither scene (orange to
	// blue via red or green, grey to blue via purple), which reads as the tint
	// "jumping" to random hues. A straight RGB crossfade is what mixing the two
	// scenes' light actually looks like.
	const auto lerp = [factor](uint8_t x, uint8_t y)
	{
		return static_cast<uint8_t>(std::lround(x + (static_cast<double>(y) - x) * factor));
	};
	return { lerp(from.R, to.R), lerp(from.G, to.G), lerp(from.B, to.B), 255 };
}

int DynamicTint::ColorDelta(const Util::Color &a, const Util::Color &b) noexcept
{
	return std::abs(static_cast<int>(a.R) - static_cast<int>(b.R)) +
		std::abs(static_cast<int>(a.G) - static_cast<int>(b.G)) +
		std::abs(static_cast<int>(a.B) - static_cast<int>(b.B));
}