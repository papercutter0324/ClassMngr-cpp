#include "pch.h"

#include "winui_view_model.h"

#include <string>
#include <string_view>
#include <utility>

namespace
{

std::string formatError(classmngr::engine::Error const& error)
{
    std::string text = "error=";
    text += classmngr::engine::errorCodeName(error.code);
    if (error.nativeCode.has_value())
    {
        text += " native-code=" + std::to_string(*error.nativeCode);
    }
    if (!error.message.empty())
    {
        text += " message=" + error.message;
    }
    return text;
}

std::string formatValidationIssue(
    classmngr::engine::ValidationIssue const& issue
    )
{
    std::string text = issue.isError() ? "error" : "warning";
    text += " code=" + issue.code;
    if (!issue.field.empty())
    {
        text += " field=" + issue.field;
    }
    if (issue.row >= 0)
    {
        text += " row=" + std::to_string(issue.row);
    }
    if (issue.column >= 0)
    {
        text += " column=" + std::to_string(issue.column);
    }
    return text;
}

} // namespace

namespace winrt::ClassMngrWinUI::implementation
{

bool ObservableViewModel::IsBusy() const noexcept
{
    return m_isBusy;
}

winrt::hstring ObservableViewModel::StatusText() const
{
    return m_statusText;
}

winrt::hstring ObservableViewModel::ErrorMessage() const
{
    return m_errorMessage;
}

bool ObservableViewModel::HasError() const noexcept
{
    return !m_errorMessage.empty();
}

winrt::hstring ObservableViewModel::ValidationSummary() const
{
    return m_validationSummary;
}

bool ObservableViewModel::HasValidationErrors() const noexcept
{
    return m_hasValidationErrors;
}

winrt::event_token ObservableViewModel::PropertyChanged(
    Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler
    )
{
    return m_propertyChanged.add(handler);
}

void ObservableViewModel::PropertyChanged(winrt::event_token const& token) noexcept
{
    m_propertyChanged.remove(token);
}

void ObservableViewModel::PresentError(classmngr::engine::Error const& error)
{
    setErrorMessage(winrt::to_hstring(formatError(error)));
}

void ObservableViewModel::ClearError()
{
    setErrorMessage({});
}

void ObservableViewModel::PresentValidation(
    classmngr::engine::ValidationResult const& result
    )
{
    std::string summary;
    for (auto const& issue : result.issues())
    {
        if (!summary.empty())
        {
            summary += '\n';
        }
        summary += formatValidationIssue(issue);
    }
    setValidationSummary(winrt::to_hstring(summary));
    setHasValidationErrors(result.hasErrors());
}

void ObservableViewModel::ClearValidation()
{
    setValidationSummary({});
    setHasValidationErrors(false);
}

void ObservableViewModel::setIsBusy(bool value)
{
    if (m_isBusy != value)
    {
        m_isBusy = value;
        raisePropertyChanged(L"IsBusy");
    }
}

void ObservableViewModel::setStatusText(winrt::hstring const& value)
{
    if (m_statusText != value)
    {
        m_statusText = value;
        raisePropertyChanged(L"StatusText");
    }
}

void ObservableViewModel::setErrorMessage(winrt::hstring const& value)
{
    const bool hadError = HasError();
    if (m_errorMessage != value)
    {
        m_errorMessage = value;
        raisePropertyChanged(L"ErrorMessage");
    }
    if (hadError != HasError())
    {
        raisePropertyChanged(L"HasError");
    }
}

void ObservableViewModel::setValidationSummary(winrt::hstring const& value)
{
    if (m_validationSummary != value)
    {
        m_validationSummary = value;
        raisePropertyChanged(L"ValidationSummary");
    }
}

void ObservableViewModel::setHasValidationErrors(bool value)
{
    if (m_hasValidationErrors != value)
    {
        m_hasValidationErrors = value;
        raisePropertyChanged(L"HasValidationErrors");
    }
}

void ObservableViewModel::raisePropertyChanged(std::wstring_view propertyName)
{
    m_propertyChanged(
        *this,
        Microsoft::UI::Xaml::Data::PropertyChangedEventArgs(
            winrt::hstring(propertyName)
            )
        );
}

AsyncCommand::AsyncCommand(
    Microsoft::UI::Dispatching::DispatcherQueue const& dispatcherQueue,
    AsyncWork work
    )
    : m_dispatcherQueue(dispatcherQueue)
    , m_work(std::move(work))
{
    if (!m_dispatcherQueue || !m_work)
    {
        throw winrt::hresult_invalid_argument();
    }
}

bool AsyncCommand::CanExecute(
    winrt::Windows::Foundation::IInspectable const& parameter
    ) const
{
    static_cast<void>(parameter);
    return !m_isRunning;
}

void AsyncCommand::Execute(
    winrt::Windows::Foundation::IInspectable const& parameter
    )
{
    if (!CanExecute(parameter))
    {
        return;
    }

    const auto cancellation = std::make_shared<
        classmngr::engine::CancellationSource>();
    m_cancellation = cancellation;
    m_isRunning = true;
    raiseCanExecuteChanged();

    try
    {
        m_operation = m_work(cancellation->token());
        if (!m_operation)
        {
            throw winrt::hresult_invalid_argument();
        }
        observeCompletion(
            get_weak(),
            m_dispatcherQueue,
            cancellation,
            m_operation
            );
    }
    catch (...)
    {
        complete(cancellation);
    }
}

winrt::event_token AsyncCommand::CanExecuteChanged(
    winrt::Windows::Foundation::EventHandler<
        winrt::Windows::Foundation::IInspectable> const& handler
    )
{
    return m_canExecuteChanged.add(handler);
}

void AsyncCommand::CanExecuteChanged(winrt::event_token const& token) noexcept
{
    m_canExecuteChanged.remove(token);
}

bool AsyncCommand::IsRunning() const noexcept
{
    return m_isRunning;
}

bool AsyncCommand::IsCancellationRequested() const noexcept
{
    return m_cancellation && m_cancellation->isCancellationRequested();
}

void AsyncCommand::Cancel()
{
    if (!m_isRunning || !m_cancellation)
    {
        return;
    }

    m_cancellation->requestCancellation();
    if (m_operation)
    {
        m_operation.Cancel();
    }
}

winrt::fire_and_forget AsyncCommand::observeCompletion(
    winrt::weak_ref<AsyncCommand> weakCommand,
    Microsoft::UI::Dispatching::DispatcherQueue dispatcherQueue,
    std::shared_ptr<classmngr::engine::CancellationSource> cancellation,
    winrt::Windows::Foundation::IAsyncAction operation
    )
{
    try
    {
        co_await operation;
    }
    catch (...)
    {
        // ICommand has no error channel. Engine callers present explicit Error
        // state through ObservableViewModel at their UI boundary.
    }

    static_cast<void>(dispatcherQueue.TryEnqueue(
        [weakCommand, cancellation]() {
            if (auto command = weakCommand.get())
            {
                command->complete(cancellation);
            }
        }
        ));
}

void AsyncCommand::complete(
    std::shared_ptr<classmngr::engine::CancellationSource> const& cancellation
    )
{
    if (!m_isRunning || cancellation != m_cancellation)
    {
        return;
    }

    m_operation = nullptr;
    m_cancellation.reset();
    m_isRunning = false;
    raiseCanExecuteChanged();
}

void AsyncCommand::raiseCanExecuteChanged()
{
    m_canExecuteChanged(*this, nullptr);
}

} // namespace winrt::ClassMngrWinUI::implementation
