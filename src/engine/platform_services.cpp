#include "classmngr/engine/platform_services.h"

#include <utility>

namespace classmngr::engine
{

AtomicCancellationToken::AtomicCancellationToken()
    : m_state(std::make_shared<std::atomic_bool>(false))
{
}

AtomicCancellationToken::AtomicCancellationToken(
    std::shared_ptr<std::atomic_bool> state
    )
    : m_state(std::move(state))
{
}

bool AtomicCancellationToken::isCancellationRequested() const noexcept
{
    return m_state->load(std::memory_order_acquire);
}

void AtomicCancellationToken::requestCancellation() noexcept
{
    m_state->store(true, std::memory_order_release);
}

void AtomicCancellationToken::reset() noexcept
{
    m_state->store(false, std::memory_order_release);
}

CancellationSource::CancellationSource() = default;

const CancellationToken& CancellationSource::token() const noexcept
{
    return m_token;
}

AtomicCancellationToken& CancellationSource::mutableToken() noexcept
{
    return m_token;
}

bool CancellationSource::isCancellationRequested() const noexcept
{
    return m_token.isCancellationRequested();
}

void CancellationSource::requestCancellation() noexcept
{
    m_token.requestCancellation();
}

void CancellationSource::reset() noexcept
{
    m_token.reset();
}

CallbackCancellationToken::CallbackCancellationToken(
    std::function<bool()> callback
    )
    : m_callback(std::move(callback))
{
}

bool CallbackCancellationToken::isCancellationRequested() const noexcept
{
    if (!m_callback)
    {
        return false;
    }

    try
    {
        return m_callback();
    }
    catch (...)
    {
        // Cancellation checks are noexcept by contract.  A callback that
        // cannot answer safely is treated as a cancellation request so the
        // caller can unwind instead of terminating the process.
        return true;
    }
}

WallClockTimePoint SystemClock::nowUtc() const noexcept
{
    return std::chrono::system_clock::now();
}

MonotonicTimePoint SystemClock::monotonicNow() const noexcept
{
    return std::chrono::steady_clock::now();
}

void NullLogger::log(const LogRecord& record) noexcept
{
    static_cast<void>(record);
}

} // namespace classmngr::engine
