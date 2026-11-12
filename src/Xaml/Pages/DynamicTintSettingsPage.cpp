#include "pch.h"
#include "DynamicTintSettingsPage.h"
#if __has_include("Pages/DynamicTintSettingsPage.g.cpp")
#include "Pages/DynamicTintSettingsPage.g.cpp"
#endif
#include <cmath>

namespace winrt::TranslucentTB::Xaml::Pages::implementation
{
    DynamicTintSettingsPage::DynamicTintSettingsPage(int32_t zones, int32_t fps, int32_t samplesPerSecond, int32_t sampleDepth, bool adaptiveSampling) :
        m_AdaptiveSampling(adaptiveSampling), m_Zones(zones), m_Fps(fps), m_SamplesPerSecond(samplesPerSecond), m_SampleDepth(sampleDepth)
    { }

    void DynamicTintSettingsPage::InitializeComponent()
    {
        ComponentConnectorT::InitializeComponent();
        SamplingModeBox().SelectedIndex(m_AdaptiveSampling ? 0 : 1);
        ZonesBox().Value(m_Zones);
        FpsBox().Value(m_Fps);
        SamplingBox().Value(m_SamplesPerSecond);
        DepthBox().Value(m_SampleDepth);
    }

    void DynamicTintSettingsPage::ApplyClicked(const IInspectable &, const wux::RoutedEventArgs &)
    {
        const auto valid = [](double value, int minimum, int maximum) noexcept
        {
            return std::isfinite(value) && value == std::floor(value) && value >= minimum && value <= maximum;
        };
        const double zones = ZonesBox().Value(), fps = FpsBox().Value();
        const double sampling = SamplingBox().Value(), depth = DepthBox().Value();
        if (!valid(zones, 1, 32) || !valid(fps, 20, 120) || !valid(sampling, 1, 60) || !valid(depth, 0, 4096))
        {
            ValidationMessage().Visibility(wux::Visibility::Visible);
            return;
        }
        m_ParametersCommitted(static_cast<int32_t>(zones), static_cast<int32_t>(fps),
            static_cast<int32_t>(sampling), static_cast<int32_t>(depth), SamplingModeBox().SelectedIndex() == 0);
        base_type::Close();
    }

    void DynamicTintSettingsPage::CancelClicked(const IInspectable &, const wux::RoutedEventArgs &)
    {
        base_type::Close();
    }

    void DynamicTintSettingsPage::DefaultsClicked(const IInspectable &, const wux::RoutedEventArgs &)
    {
        SamplingModeBox().SelectedIndex(0);
        ZonesBox().Value(20);
        FpsBox().Value(60);
        SamplingBox().Value(30);
        DepthBox().Value(0);
        ValidationMessage().Visibility(wux::Visibility::Collapsed);
    }
}
