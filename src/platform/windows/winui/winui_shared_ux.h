#pragma once

#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <vector>

namespace ClassMngrWinUISharedUX
{
    enum class ValidationTone
    {
        None,
        Error,
        Warning,
        Success
    };

    enum class StatusTone
    {
        Neutral,
        Success,
        Warning,
        Error,
        Busy
    };

    enum class AutosaveState
    {
        Saved,
        Saving,
        Unsaved,
        Failed
    };

    struct FormFieldOptions
    {
        winrt::hstring label;
        winrt::hstring description;
        winrt::hstring placeholder;
        winrt::hstring automationName;
        winrt::hstring helpText;
        bool required{false};
    };

    struct FormField
    {
        winrt::Microsoft::UI::Xaml::Controls::StackPanel root{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::TextBox input{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::TextBlock validation{nullptr};
    };

    struct StateSurfaceOptions
    {
        winrt::hstring title;
        winrt::hstring description;
        winrt::hstring automationName;
        winrt::hstring actionLabel;
    };

    struct StateSurface
    {
        winrt::Microsoft::UI::Xaml::Controls::Border root{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::TextBlock title{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::TextBlock description{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::Button action{nullptr};
    };

    struct CardOptions
    {
        winrt::hstring title;
        winrt::hstring description;
        winrt::hstring automationName;
    };

    struct Card
    {
        winrt::Microsoft::UI::Xaml::Controls::Border root{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::StackPanel content{nullptr};
    };

    struct FilterBarOptions
    {
        winrt::hstring queryLabel;
        winrt::hstring queryPlaceholder;
        winrt::hstring filterLabel;
        winrt::hstring automationName;
        std::vector<winrt::hstring> filterValues;
    };

    struct FilterBar
    {
        winrt::Microsoft::UI::Xaml::Controls::StackPanel root{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::TextBox query{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::ComboBox filter{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::Button clear{nullptr};
    };

    struct StatusSurface
    {
        winrt::Microsoft::UI::Xaml::Controls::Border root{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::ProgressRing progress{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::TextBlock message{nullptr};
    };

    struct AutosaveSurface
    {
        winrt::Microsoft::UI::Xaml::Controls::Border root{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::TextBlock state{nullptr};
    };

    FormField buildFormField(FormFieldOptions const& options);
    void setFormFieldValidation(
        FormField const& field,
        ValidationTone tone,
        winrt::hstring const& message
        );

    StateSurface buildEmptyState(StateSurfaceOptions const& options);
    StateSurface buildErrorState(StateSurfaceOptions const& options);

    Card buildCard(CardOptions const& options);
    FilterBar buildFilterBar(FilterBarOptions const& options);

    StatusSurface buildStatusSurface(winrt::hstring const& automationName = {});
    void setStatusSurface(
        StatusSurface const& surface,
        StatusTone tone,
        winrt::hstring const& message
        );

    AutosaveSurface buildAutosaveSurface(winrt::hstring const& automationName = {});
    void setAutosaveState(
        AutosaveSurface const& surface,
        AutosaveState state,
        winrt::hstring const& detail = {}
        );
} // namespace ClassMngrWinUISharedUX
