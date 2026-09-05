#pragma once

#include "classmngr/engine/platform_services.h"
#include "classmngr/engine/result.h"
#include "classmngr/engine/validation_result.h"

#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Windows.Foundation.h>

#include <functional>
#include <memory>
#include <string_view>

namespace winrt::ClassMngrWinUI::implementation
{

// A small binding-facing base for WinUI pages.  Derived view models keep their
// engine calls platform-neutral and use these presentation helpers at the UI
// boundary.
struct ObservableViewModel : winrt::implements<
    ObservableViewModel,
    Microsoft::UI::Xaml::Data::INotifyPropertyChanged>
{
    [[nodiscard]] bool IsBusy() const noexcept;
    [[nodiscard]] winrt::hstring StatusText() const;
    [[nodiscard]] winrt::hstring ErrorMessage() const;
    [[nodiscard]] bool HasError() const noexcept;
    [[nodiscard]] winrt::hstring ValidationSummary() const;
    [[nodiscard]] bool HasValidationErrors() const noexcept;

    winrt::event_token PropertyChanged(
        Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler
        );
    void PropertyChanged(winrt::event_token const& token) noexcept;

    void PresentError(classmngr::engine::Error const& error);
    void ClearError();
    void PresentValidation(classmngr::engine::ValidationResult const& result);
    void ClearValidation();

protected:
    void setIsBusy(bool value);
    void setStatusText(winrt::hstring const& value);
    void setErrorMessage(winrt::hstring const& value);
    void setValidationSummary(winrt::hstring const& value);
    void setHasValidationErrors(bool value);
    void raisePropertyChanged(std::wstring_view propertyName);

private:
    winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler>
        m_propertyChanged;
    bool m_isBusy{};
    winrt::hstring m_statusText;
    winrt::hstring m_errorMessage;
    winrt::hstring m_validationSummary;
    bool m_hasValidationErrors{};
};

// ICommand adapter for platform-neutral asynchronous engine work.  Command
// state changes are always completed through the supplied DispatcherQueue.
struct AsyncCommand : winrt::implements<
    AsyncCommand,
    Microsoft::UI::Xaml::Input::ICommand>
{
    using AsyncWork = std::function<winrt::Windows::Foundation::IAsyncAction(
        classmngr::engine::CancellationToken const&)>;

    AsyncCommand(
        Microsoft::UI::Dispatching::DispatcherQueue const& dispatcherQueue,
        AsyncWork work
        );

    [[nodiscard]] bool CanExecute(
        winrt::Windows::Foundation::IInspectable const& parameter
        ) const;
    void Execute(winrt::Windows::Foundation::IInspectable const& parameter);
    winrt::event_token CanExecuteChanged(
        winrt::Windows::Foundation::EventHandler<
            winrt::Windows::Foundation::IInspectable> const& handler
        );
    void CanExecuteChanged(winrt::event_token const& token) noexcept;

    [[nodiscard]] bool IsRunning() const noexcept;
    [[nodiscard]] bool IsCancellationRequested() const noexcept;
    void Cancel();

private:
    static winrt::fire_and_forget observeCompletion(
        winrt::weak_ref<AsyncCommand> weakCommand,
        Microsoft::UI::Dispatching::DispatcherQueue dispatcherQueue,
        std::shared_ptr<classmngr::engine::CancellationSource> cancellation,
        winrt::Windows::Foundation::IAsyncAction operation
        );
    void complete(
        std::shared_ptr<classmngr::engine::CancellationSource> const& cancellation
        );
    void raiseCanExecuteChanged();

    Microsoft::UI::Dispatching::DispatcherQueue m_dispatcherQueue{nullptr};
    AsyncWork m_work;
    winrt::event<winrt::Windows::Foundation::EventHandler<
        winrt::Windows::Foundation::IInspectable>> m_canExecuteChanged;
    std::shared_ptr<classmngr::engine::CancellationSource> m_cancellation;
    winrt::Windows::Foundation::IAsyncAction m_operation{nullptr};
    bool m_isRunning{};
};

} // namespace winrt::ClassMngrWinUI::implementation
