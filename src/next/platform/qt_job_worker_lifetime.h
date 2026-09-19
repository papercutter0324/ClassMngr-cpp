#pragma once

#include "next/domain/operation_result.h"

#include <QThread>

#include <atomic>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Read-only cooperative-cancellation view passed to task code. Only the
// owning QtJobWorkerLifetime can record a cancellation request.
class QtJobCancellationToken final
{
public:
    QtJobCancellationToken() = default;

    QtJobCancellationToken(const QtJobCancellationToken&) = delete;
    QtJobCancellationToken& operator=(const QtJobCancellationToken&) = delete;
    QtJobCancellationToken(QtJobCancellationToken&&) = delete;
    QtJobCancellationToken& operator=(QtJobCancellationToken&&) = delete;

    [[nodiscard]] bool isCancellationRequested() const noexcept
    {
        return m_cancellationRequested.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool cancellationRequested() const noexcept
    {
        return isCancellationRequested();
    }

private:
    friend class QtJobWorkerLifetime;

    void requestCancellation() noexcept
    {
        m_cancellationRequested.store(true, std::memory_order_release);
    }

    std::atomic_bool m_cancellationRequested = false;
};

// Owns exactly one joinable QThread at a time. A finished thread is joined
// before its thread object, task, and cancellation token are released. The
// destructor cooperatively requests cancellation and joins; it never detaches.
// This primitive owns no application state and publishes no application events.
class QtJobWorkerLifetime final
{
public:
    using Task = std::function<Domain::Result<void>(
        const QtJobCancellationToken&
        )>;

    QtJobWorkerLifetime() = default;

    ~QtJobWorkerLifetime() noexcept
    {
        std::lock_guard joinLock(m_joinMutex);

        QThread* thread = nullptr;
        {
            std::lock_guard stateLock(m_stateMutex);
            if (m_thread && m_thread->isRunning())
            {
                m_token->requestCancellation();
            }
            thread = m_thread.get();
        }

        if (thread != nullptr)
        {
            (void)thread->wait();
        }

        std::lock_guard stateLock(m_stateMutex);
        m_thread.reset();
        m_task = {};
        m_token.reset();
    }

    QtJobWorkerLifetime(const QtJobWorkerLifetime&) = delete;
    QtJobWorkerLifetime& operator=(const QtJobWorkerLifetime&) = delete;
    QtJobWorkerLifetime(QtJobWorkerLifetime&&) = delete;
    QtJobWorkerLifetime& operator=(QtJobWorkerLifetime&&) = delete;

    [[nodiscard]] Domain::Result<void> start(
        Task task
        )
    {
        if (!task)
        {
            return Domain::Result<void>::failure(emptyTaskError());
        }

        std::lock_guard joinLock(m_joinMutex);

        QThread* finishedThread = nullptr;
        {
            std::lock_guard stateLock(m_stateMutex);
            if (m_thread && m_thread->isRunning())
            {
                return Domain::Result<void>::failure(activeTaskError());
            }
            finishedThread = m_thread.get();
        }

        // QThread destruction is legal only after the prior worker has been
        // joined. Keep its task and token alive through that join as well.
        if (finishedThread != nullptr)
        {
            (void)finishedThread->wait();
        }

        std::lock_guard stateLock(m_stateMutex);
        m_thread.reset();
        m_task = {};
        m_token.reset();
        m_lastResult.reset();

        try
        {
            m_task = std::move(task);
            m_token = std::make_unique<QtJobCancellationToken>();
            m_thread.reset(QThread::create([this]()
            {
                Domain::Result<void> result = invokeTask();
                std::lock_guard resultLock(m_stateMutex);
                m_lastResult = std::move(result);
            }));
            m_thread->start();
        }
        catch (const std::exception&)
        {
            m_thread.reset();
            m_task = {};
            m_token.reset();
            return Domain::Result<void>::failure(threadCreationError());
        }
        catch (...)
        {
            m_thread.reset();
            m_task = {};
            m_token.reset();
            return Domain::Result<void>::failure(
                unknownThreadCreationError()
                );
        }

        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> requestCancellation()
    {
        std::lock_guard stateLock(m_stateMutex);
        if (!m_thread || !m_thread->isRunning())
        {
            return Domain::Result<void>::failure(noActiveTaskError());
        }

        m_token->requestCancellation();
        return Domain::Result<void>::success();
    }

    [[nodiscard]] bool isRunning() const
    {
        std::lock_guard stateLock(m_stateMutex);
        return m_thread && m_thread->isRunning();
    }

    void waitForFinished()
    {
        std::lock_guard joinLock(m_joinMutex);

        QThread* thread = nullptr;
        {
            std::lock_guard stateLock(m_stateMutex);
            thread = m_thread.get();
        }

        if (thread != nullptr)
        {
            (void)thread->wait();
        }
    }

    [[nodiscard]] std::optional<Domain::Result<void>> lastResult() const
    {
        std::lock_guard stateLock(m_stateMutex);
        return m_lastResult;
    }

private:
    [[nodiscard]] Domain::Result<void> invokeTask() noexcept
    {
        try
        {
            return m_task(*m_token);
        }
        catch (const std::exception&)
        {
            return Domain::Result<void>::failure(taskExceptionError());
        }
        catch (...)
        {
            return Domain::Result<void>::failure(unknownTaskExceptionError());
        }
    }

    [[nodiscard]] static Domain::OperationError emptyTaskError()
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::InvalidInput,
            .message = "Qt job worker task must not be empty.",
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::OperationError activeTaskError()
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Conflict,
            .message = "Cannot start a Qt job worker while a task is active.",
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::OperationError noActiveTaskError()
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Conflict,
            .message = "Qt job worker cancellation requires an active task.",
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::OperationError taskExceptionError()
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Technical,
            .message = "Qt job worker task raised an exception.",
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::OperationError unknownTaskExceptionError()
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Technical,
            .message = "Qt job worker task raised an unknown exception.",
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::OperationError threadCreationError()
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Technical,
            .message = "Qt job worker thread creation raised an exception.",
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::OperationError unknownThreadCreationError()
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Technical,
            .message = "Qt job worker thread creation raised an unknown exception.",
            .recoverable = false
        };
    }

    mutable std::mutex m_joinMutex;
    mutable std::mutex m_stateMutex;
    Task m_task;
    std::unique_ptr<QtJobCancellationToken> m_token;
    std::unique_ptr<QThread> m_thread;
    std::optional<Domain::Result<void>> m_lastResult;
};

} // namespace ClassMngr::Next::Platform
