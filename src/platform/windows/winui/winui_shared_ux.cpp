#include "pch.h"
#include "winui_shared_ux.h"

#include <string>

namespace
{
    using namespace winrt::Microsoft::UI::Xaml;
    using namespace winrt::Microsoft::UI::Xaml::Controls;
    using winrt::box_value;
    using winrt::hstring;

    void setAutomation(
        DependencyObject const& element,
        hstring const& name,
        hstring const& helpText = {}
        )
    {
        if (!name.empty())
        {
            winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(
                element,
                name
                );
        }
        if (!helpText.empty())
        {
            winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetHelpText(
                element,
                helpText
                );
        }
    }

    void applyStyle(FrameworkElement const& element, wchar_t const* key)
    {
        auto application = Application::Current();
        if (!application)
        {
            return;
        }

        auto resource = application.Resources().Lookup(box_value(hstring(key)));
        if (resource)
        {
            element.Style(resource.as<Style>());
        }
    }

    hstring withRequiredMarker(hstring const& label, bool required)
    {
        if (!required || label.empty())
        {
            return label;
        }
        return label + L" *";
    }

    hstring tonePrefix(ClassMngrWinUISharedUX::ValidationTone tone)
    {
        using ClassMngrWinUISharedUX::ValidationTone;
        switch (tone)
        {
        case ValidationTone::Error:
            return L"Error: ";
        case ValidationTone::Warning:
            return L"Warning: ";
        case ValidationTone::Success:
            return L"OK: ";
        case ValidationTone::None:
        default:
            return {};
        }
    }
}

namespace ClassMngrWinUISharedUX
{
    FormField buildFormField(FormFieldOptions const& options)
    {
        FormField result;
        result.root = StackPanel();
        result.root.Spacing(4.0);
        applyStyle(result.root, L"Phase4FormFieldStackPanelStyle");
        setAutomation(result.root, options.automationName);

        result.input = TextBox();
        result.input.Header(box_value(withRequiredMarker(options.label, options.required)));
        result.input.PlaceholderText(options.placeholder);
        if (!options.description.empty())
        {
            result.input.Description(box_value(options.description));
        }
        result.input.IsTabStop(true);
        result.input.TabIndex(0);
        applyStyle(result.input, L"Phase4FormFieldTextBoxStyle");
        setAutomation(
            result.input,
            options.automationName.empty() ? options.label : options.automationName,
            options.helpText
            );

        result.validation = TextBlock();
        result.validation.Visibility(Visibility::Collapsed);
        result.validation.TextWrapping(TextWrapping::Wrap);
        applyStyle(result.validation, L"Phase4ValidationTextBlockStyle");
        setAutomation(result.validation, options.label + L" validation");

        result.root.Children().Append(result.input);
        result.root.Children().Append(result.validation);
        return result;
    }

    void setFormFieldValidation(
        FormField const& field,
        ValidationTone tone,
        hstring const& message
        )
    {
        if (!field.validation)
        {
            return;
        }

        if (tone == ValidationTone::None || message.empty())
        {
            field.validation.Text({});
            field.validation.Visibility(Visibility::Collapsed);
            winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetHelpText(
                field.input,
                {}
                );
            return;
        }

        field.validation.Text(tonePrefix(tone) + message);
        field.validation.Visibility(Visibility::Visible);
        winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetHelpText(
            field.input,
            message
            );
    }

    StateSurface buildStateSurface(
        StateSurfaceOptions const& options,
        wchar_t const* styleKey,
        wchar_t const* titleStyleKey
        )
    {
        StateSurface result;
        result.root = Border();
        result.root.Padding(Thickness{16.0, 16.0, 16.0, 16.0});
        applyStyle(result.root, styleKey);
        setAutomation(result.root, options.automationName);

        auto content = StackPanel();
        content.Spacing(8.0);
        result.title = TextBlock();
        result.title.Text(options.title);
        result.title.TextWrapping(TextWrapping::Wrap);
        applyStyle(result.title, titleStyleKey);
        setAutomation(result.title, options.title);

        result.description = TextBlock();
        result.description.Text(options.description);
        result.description.TextWrapping(TextWrapping::Wrap);
        applyStyle(result.description, L"Phase4StateDescriptionTextBlockStyle");
        setAutomation(result.description, options.title + L" details");
        content.Children().Append(result.title);
        content.Children().Append(result.description);

        if (!options.actionLabel.empty())
        {
            result.action = Button();
            result.action.Content(box_value(options.actionLabel));
            result.action.IsTabStop(true);
            applyStyle(result.action, L"Phase4StateActionButtonStyle");
            setAutomation(result.action, options.actionLabel);
            content.Children().Append(result.action);
        }

        result.root.Child(content);
        return result;
    }

    StateSurface buildEmptyState(StateSurfaceOptions const& options)
    {
        return buildStateSurface(
            options,
            L"Phase4EmptyStateBorderStyle",
            L"Phase4StateTitleTextBlockStyle"
            );
    }

    StateSurface buildErrorState(StateSurfaceOptions const& options)
    {
        return buildStateSurface(
            options,
            L"Phase4ErrorStateBorderStyle",
            L"Phase4ErrorStateTitleTextBlockStyle"
            );
    }

    Card buildCard(CardOptions const& options)
    {
        Card result;
        result.root = Border();
        result.root.Padding(Thickness{16.0, 16.0, 16.0, 16.0});
        applyStyle(result.root, L"Phase4CardBorderStyle");
        setAutomation(result.root, options.automationName.empty() ? options.title : options.automationName);

        result.content = StackPanel();
        result.content.Spacing(8.0);
        if (!options.title.empty())
        {
            auto title = TextBlock();
            title.Text(options.title);
            title.TextWrapping(TextWrapping::Wrap);
            applyStyle(title, L"Phase4CardTitleTextBlockStyle");
            setAutomation(title, options.title);
            result.content.Children().Append(title);
        }
        if (!options.description.empty())
        {
            auto description = TextBlock();
            description.Text(options.description);
            description.TextWrapping(TextWrapping::Wrap);
            applyStyle(description, L"Phase4CardDescriptionTextBlockStyle");
            setAutomation(description, options.title + L" details");
            result.content.Children().Append(description);
        }

        result.root.Child(result.content);
        return result;
    }

    FilterBar buildFilterBar(FilterBarOptions const& options)
    {
        FilterBar result;
        result.root = StackPanel();
        result.root.Orientation(Orientation::Horizontal);
        result.root.Spacing(8.0);
        result.root.VerticalAlignment(VerticalAlignment::Center);
        applyStyle(result.root, L"Phase4FilterBarStackPanelStyle");
        setAutomation(result.root, options.automationName.empty() ? L"Filter bar" : options.automationName);

        result.query = TextBox();
        result.query.Header(box_value(options.queryLabel));
        result.query.PlaceholderText(options.queryPlaceholder);
        result.query.MinWidth(220.0);
        result.query.IsTabStop(true);
        applyStyle(result.query, L"Phase4FilterTextBoxStyle");
        setAutomation(result.query, options.queryLabel.empty() ? L"Filter query" : options.queryLabel);
        result.root.Children().Append(result.query);

        result.filter = ComboBox();
        result.filter.Header(box_value(options.filterLabel));
        result.filter.IsTabStop(true);
        result.filter.MinWidth(160.0);
        applyStyle(result.filter, L"Phase4FilterComboBoxStyle");
        setAutomation(result.filter, options.filterLabel.empty() ? L"Filter selection" : options.filterLabel);
        for (auto const& value : options.filterValues)
        {
            auto item = ComboBoxItem();
            item.Content(box_value(value));
            setAutomation(item, value);
            result.filter.Items().Append(item);
        }
        result.root.Children().Append(result.filter);

        result.clear = Button();
        result.clear.Content(box_value(hstring(L"Clear")));
        result.clear.IsTabStop(true);
        applyStyle(result.clear, L"Phase4FilterClearButtonStyle");
        setAutomation(result.clear, L"Clear filters");
        result.root.Children().Append(result.clear);
        return result;
    }

    StatusSurface buildStatusSurface(hstring const& automationName)
    {
        StatusSurface result;
        result.root = Border();
        result.root.Padding(Thickness{12.0, 12.0, 12.0, 12.0});
        applyStyle(result.root, L"Phase4StatusSurfaceBorderStyle");
        setAutomation(result.root, automationName.empty() ? L"Status" : automationName);

        auto content = StackPanel();
        content.Orientation(Orientation::Horizontal);
        content.Spacing(8.0);
        result.progress = ProgressRing();
        result.progress.Width(20.0);
        result.progress.Height(20.0);
        result.progress.IsActive(false);
        result.progress.Visibility(Visibility::Collapsed);
        setAutomation(result.progress, L"Status progress");
        content.Children().Append(result.progress);

        result.message = TextBlock();
        result.message.TextWrapping(TextWrapping::Wrap);
        applyStyle(result.message, L"Phase4StatusTextBlockStyle");
        setAutomation(result.message, L"Status message");
        content.Children().Append(result.message);
        result.root.Child(content);
        return result;
    }

    void setStatusSurface(
        StatusSurface const& surface,
        StatusTone tone,
        hstring const& message
        )
    {
        if (!surface.message)
        {
            return;
        }
        surface.message.Text(message);
        const bool busy = tone == StatusTone::Busy;
        surface.progress.IsActive(busy);
        surface.progress.Visibility(busy ? Visibility::Visible : Visibility::Collapsed);
        winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetHelpText(
            surface.root,
            message
            );
    }

    AutosaveSurface buildAutosaveSurface(hstring const& automationName)
    {
        AutosaveSurface result;
        result.root = Border();
        result.root.Padding(Thickness{8.0, 8.0, 8.0, 8.0});
        applyStyle(result.root, L"Phase4AutosaveStateBorderStyle");
        setAutomation(result.root, automationName.empty() ? L"Autosave state" : automationName);

        result.state = TextBlock();
        result.state.TextWrapping(TextWrapping::Wrap);
        applyStyle(result.state, L"Phase4AutosaveStateTextBlockStyle");
        setAutomation(result.state, L"Autosave state");
        result.root.Child(result.state);
        return result;
    }

    void setAutosaveState(
        AutosaveSurface const& surface,
        AutosaveState state,
        hstring const& detail
        )
    {
        if (!surface.state)
        {
            return;
        }

        hstring text;
        switch (state)
        {
        case AutosaveState::Saved:
            text = L"Saved";
            break;
        case AutosaveState::Saving:
            text = L"Saving...";
            break;
        case AutosaveState::Unsaved:
            text = L"Unsaved changes";
            break;
        case AutosaveState::Failed:
            text = L"Autosave failed";
            break;
        }
        if (!detail.empty())
        {
            text = hstring(
                std::wstring(text.c_str(), text.size()) + L" "
                + std::wstring(detail.c_str(), detail.size())
                );
        }
        surface.state.Text(text);
        winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetHelpText(
            surface.root,
            text
            );
    }
} // namespace ClassMngrWinUISharedUX
