#pragma once

#include "classmngr/engine/platform_services.h"

#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Windows.Foundation.h>

#include <functional>

namespace ClassMngrWinUIThreading
{

// These are presentation-boundary rules, rather than a general UI abstraction:
// engine work receives only portable values, and UI updates are posted to the
// owning DispatcherQueue.  Background work must not capture XAML objects or
// update controls from its callback.
struct ThreadingPolicy final
{
    static constexpr bool backgroundWorkExcludesXamlObjects = true;
    static constexpr bool uiMutationsRequireDispatcherQueue = true;
    static constexpr bool backgroundWorkRequiresCancellationToken = true;
};

using UiUpdate = std::function<void()>;
using BackgroundWork = std::function<winrt::Windows::Foundation::IAsyncAction(
    classmngr::engine::CancellationToken const&)>;

// The only helper-owned path for invoking a UI update is the callback supplied
// to DispatcherQueue::TryEnqueue.  A missing or shutting-down queue rejects
// the update without invoking it.
class UiThreadDispatcher final
{
public:
    explicit UiThreadDispatcher(
        winrt::Microsoft::UI::Dispatching::DispatcherQueue const& dispatcherQueue
        ) noexcept;

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] bool hasThreadAccess() const noexcept;
    [[nodiscard]] bool enqueue(UiUpdate update) const;

private:
    winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcherQueue{nullptr};
};

// Use this boundary when an engine operation must start away from the XAML
// thread.  The operation receives the explicit engine cancellation token and
// must communicate results through value types; it must not capture XAML or
// controls.  The token source must outlive the returned async operation.
[[nodiscard]] winrt::Windows::Foundation::IAsyncAction runBackgroundWork(
    BackgroundWork work,
    classmngr::engine::CancellationToken const& cancellation
    );

// Deterministic policy/type contract check; it does not require a live window
// or DispatcherQueue.  Application smoke tests may call this directly.
[[nodiscard]] bool runThreadingContractChecks() noexcept;

} // namespace ClassMngrWinUIThreading
