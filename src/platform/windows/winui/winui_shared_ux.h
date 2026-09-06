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
        Microsoft::UI::Xaml::Controls::StackPanel root{nullptr};
        Microsoft::UI::Xaml::Controls::TextBox input{nullptr};
        Microsoft::UI::Xaml::Controls::TextBlock validation{nullptr};
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
        Microsoft::UI::Xaml::Controls::Border root{nullptr};
        Microsoft::UI::Xaml::Controls::TextBlock title{nullptr};
        Microsoft::UI::Xaml::Controls::TextBlock description{nullptr};
        Microsoft::UI::Xaml::Controls::Button action{nullptr};
    };

    struct CardOptions
    {
        winrt::hstring title;
        winrt::hstring description;
        winrt::hstring automationName;
    };

    struct Card
    {
        Microsoft::UI::Xaml::Controls::Border root{nullptr};
        Microsoft::UI::Xaml::Controls::StackPanel content{nullptr};
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
        Microsoft::UI::Xaml::Controls::StackPanel root{nullptr};
        Microsoft::UI::Xaml::Controls::TextBox query{nullptr};
        Microsoft::UI::Xaml::Controls::ComboBox filter{nullptr};
        Microsoft::UI::Xaml::Controls::Button clear{nullptr};
    };

    struct StatusSurface
    {
        Microsoft::UI::Xaml::Controls::Border root{nullptr};
        Microsoft::UI::Xaml::Controls::ProgressRing progress{nullptr};
        Microsoft::UI::Xaml::Controls::TextBlock message{nullptr};
    };

    struct AutosaveSurface
    {
        Microsoft::UI::Xaml::Controls::Border root{nullptr};
        Microsoft::UI::Xaml::Controls::TextBlock state{nullptr};
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
