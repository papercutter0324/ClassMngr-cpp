#pragma once

#include "next/application/manual_update_check.h"

#include <memory>
#include <optional>
#include <utility>
#include <variant>

namespace ClassMngr::Next::Application
{

// Repeatable manual checks share one result stream. Opening displays the
// cached result, requests IfStale when needed, and observes checks started by
// other application flows. Retry always requests Force.
class ManualUpdateCheckUseCase final
{
public:
    explicit ManualUpdateCheckUseCase(ManualUpdateCheckPort& port)
        : m_port(port)
    {
        const std::weak_ptr<int> lifetime = m_lifetime;
        m_port.setObserver([this, lifetime](ManualUpdateCheckEvent event)
        {
            if (lifetime.expired())
            {
                return;
            }
            const bool terminal = !std::holds_alternative<ManualUpdateCheckStarted>(event);
            publish(std::move(event), terminal);
        });
    }

    ~ManualUpdateCheckUseCase()
    {
        m_lifetime.reset();
        m_port.setObserver({});
    }

    ManualUpdateCheckUseCase(const ManualUpdateCheckUseCase&) = delete;
    ManualUpdateCheckUseCase& operator=(const ManualUpdateCheckUseCase&) = delete;

    void setObserver(ManualUpdateCheckObserver observer)
    {
        m_observer = std::move(observer);
    }

    [[nodiscard]] ManualUpdateCheckSnapshot snapshot() const
    {
        return m_port.snapshot();
    }

    [[nodiscard]] ManualUpdateCheckDispatch open()
    {
        const std::weak_ptr<int> lifetime = m_lifetime;
        const ManualUpdateCheckSnapshot current = m_port.snapshot();
        if (current.cachedResult)
        {
            publish(*current.cachedResult);
            if (lifetime.expired())
            {
                return ManualUpdateCheckDispatch::Busy;
            }
        }

        if (current.requestInProgress)
        {
            publish(ManualUpdateCheckStarted{});
            return ManualUpdateCheckDispatch::Busy;
        }

        if (current.resultFresh)
        {
            return ManualUpdateCheckDispatch::FreshCache;
        }

        return dispatch(ManualUpdateCheckPolicy::IfStale);
    }

    [[nodiscard]] ManualUpdateCheckDispatch retry()
    {
        return dispatch(ManualUpdateCheckPolicy::Force);
    }

private:
    struct Pending final
    {
        std::weak_ptr<int> lifetime;
        ManualUpdateCheckCompletion completion;
        std::optional<ManualUpdateCheckOutcome> outcome;
        bool received = false;
        bool dispatchReturned = false;
        bool acceptsCompletion = false;

        void deliver()
        {
            if (!dispatchReturned || !acceptsCompletion || !outcome
                || lifetime.expired())
            {
                return;
            }
            auto callback = std::move(completion);
            auto result = std::move(*outcome);
            outcome.reset();
            if (callback)
            {
                callback(std::move(result));
            }
        }
    };

    [[nodiscard]] ManualUpdateCheckDispatch dispatch(
        ManualUpdateCheckPolicy policy
        )
    {
        const auto pending = std::make_shared<Pending>();
        pending->lifetime = m_lifetime;
        const std::weak_ptr<int> lifetime = m_lifetime;
        pending->completion = [this, lifetime](ManualUpdateCheckOutcome outcome)
        {
            if (lifetime.expired())
            {
                return;
            }
            std::visit([this](auto value)
            {
                publish(std::move(value), true);
            }, std::move(outcome));
        };

        const ManualUpdateCheckDispatch result = m_port.check(
            policy,
            [pending](ManualUpdateCheckOutcome outcome)
            {
                if (pending->received || pending->lifetime.expired())
                {
                    return;
                }
                pending->received = true;
                pending->outcome = std::move(outcome);
                pending->deliver();
            });

        if (lifetime.expired())
        {
            return result;
        }

        pending->acceptsCompletion =
            result == ManualUpdateCheckDispatch::Started
            || result == ManualUpdateCheckDispatch::Failed;
        pending->dispatchReturned = true;

        if (result == ManualUpdateCheckDispatch::Failed && !pending->received)
        {
            pending->received = true;
            pending->outcome = ManualUpdateCheckError{
                .code = ManualUpdateCheckErrorCode::Dispatch,
                .message = "The update check could not be dispatched."
            };
        }

        if (result == ManualUpdateCheckDispatch::Started
            || result == ManualUpdateCheckDispatch::Busy)
        {
            publish(ManualUpdateCheckStarted{});
        }
        pending->deliver();
        return result;
    }

    void publish(ManualUpdateCheckEvent event, bool terminal = false)
    {
        if (std::holds_alternative<ManualUpdateCheckStarted>(event))
        {
            if (m_requestActive)
            {
                return;
            }
            m_requestActive = true;
        }
        else if (terminal)
        {
            m_requestActive = false;
        }

        auto observer = m_observer;
        if (observer)
        {
            observer(std::move(event));
        }
    }

    ManualUpdateCheckPort& m_port;
    ManualUpdateCheckObserver m_observer;
    std::shared_ptr<int> m_lifetime = std::make_shared<int>(0);
    bool m_requestActive = false;
};

} // namespace ClassMngr::Next::Application
