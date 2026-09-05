#include "pch.h"

#include "winui_threading.h"

#include <atomic>
#include <type_traits>
#include <utility>

namespace
{

static_assert(ClassMngrWinUIThreading::ThreadingPolicy::
                  backgroundWorkExcludesXamlObjects);
static_assert(ClassMngrWinUIThreading::ThreadingPolicy::
                  uiMutationsRequireDispatcherQueue);
static_assert(ClassMngrWinUIThreading::ThreadingPolicy::
                  backgroundWorkRequiresCancellationToken);
static_assert(std::is_invocable_r_v<
                  winrt::Windows::Foundation::IAsyncAction,
                  ClassMngrWinUIThreading::BackgroundWork const&,
                  classmngr::engine::CancellationToken const&>);

} // namespace

namespace ClassMngrWinUIThreading
{

UiThreadDispatcher::UiThreadDispatcher(
    winrt::Microsoft::UI::Dispatching::DispatcherQueue const& dispatcherQueue
    ) noexcept
    : m_dispatcherQueue(dispatcherQueue)
{
}

bool UiThreadDispatcher::isValid() const noexcept
{
    return static_cast<bool>(m_dispatcherQueue);
}

bool UiThreadDispatcher::hasThreadAccess() const noexcept
{
    return m_dispatcherQueue && m_dispatcherQueue.HasThreadAccess();
}

bool UiThreadDispatcher::enqueue(UiUpdate update) const
{
    if (!m_dispatcherQueue || !update)
    {
        return false;
    }

    // Do not invoke update before TryEnqueue accepts it.  This is the sole
    // helper-owned callback path allowed to perform UI mutations.
    return m_dispatcherQueue.TryEnqueue(
        [update = std::move(update)]() mutable {
            update();
        }
        );
}

winrt::Windows::Foundation::IAsyncAction runBackgroundWork(
    BackgroundWork work,
    classmngr::engine::CancellationToken const& cancellation
    )
{
    if (!work || cancellation.isCancellationRequested())
    {
        co_return;
    }

    co_await winrt::resume_background();
    if (cancellation.isCancellationRequested())
    {
        co_return;
    }

    winrt::Windows::Foundation::IAsyncAction operation = work(cancellation);
    if (!operation)
    {
        throw winrt::hresult_invalid_argument();
    }
    co_await operation;
}

bool runThreadingContractChecks() noexcept
{
    try
    {
        std::atomic_bool updateInvoked{};
        const UiThreadDispatcher missingDispatcher(nullptr);
        const bool rejectedWithoutInvocation = !missingDispatcher.enqueue(
            [&updateInvoked]() noexcept {
                updateInvoked.store(true, std::memory_order_relaxed);
            }
            );

        return ThreadingPolicy::backgroundWorkExcludesXamlObjects
            && ThreadingPolicy::uiMutationsRequireDispatcherQueue
            && ThreadingPolicy::backgroundWorkRequiresCancellationToken
            && !missingDispatcher.isValid()
            && !missingDispatcher.hasThreadAccess()
            && rejectedWithoutInvocation
            && !updateInvoked.load(std::memory_order_relaxed);
    }
    catch (...)
    {
        return false;
    }
}

} // namespace ClassMngrWinUIThreading
