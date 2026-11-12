#pragma once
#include "../event.h"
#include "../factory.h"
#include "winrt.hpp"
#include "FramelessPage.h"
#include "Pages/DynamicTintSettingsPage.g.h"

namespace winrt::TranslucentTB::Xaml::Pages::implementation
{
    struct DynamicTintSettingsPage : wux::Markup::ComponentConnectorT<DynamicTintSettingsPageT<DynamicTintSettingsPage>>
    {
        DynamicTintSettingsPage(int32_t zones, int32_t fps, int32_t samplesPerSecond, int32_t sampleDepth, bool adaptiveSampling);
        void InitializeComponent();

        DECL_EVENT(DynamicTintParametersCommittedDelegate, ParametersCommitted, m_ParametersCommitted);

        void ApplyClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);
        void CancelClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);
        void DefaultsClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);

    private:
        bool m_AdaptiveSampling;
        int32_t m_Zones, m_Fps, m_SamplesPerSecond, m_SampleDepth;
    };
}

FACTORY(winrt::TranslucentTB::Xaml::Pages, DynamicTintSettingsPage);
