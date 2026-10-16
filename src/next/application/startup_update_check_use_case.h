#pragma once

#include "next/application/automatic_update_startup_eligibility.h"
#include "next/application/startup_update_check.h"

#include <memory>
#include <optional>
#include <utility>

namespace ClassMngr::Next::Application
{

enum class StartupUpdateCheckDisposition
{
    Started,
    Busy,
    Failed,
    NotEligible,
    AlreadyAttempted
};

struct StartupUpdateCheckInput final
{
    bool startupComplete = false;
    bool checkOnStartup = false;
    AutomaticUpdatePreferences preferences;
    bool hasReleasesApiUrl = false;
};

// Owns startup eligibility, the one-shot attempt, and terminal delivery.
// Ineligible attempts may be retried; an eligible Busy/Failed attempt consumes
// the one-shot just as Started does. Destruction suppresses pending delivery,
// without cancelling the port's work. The port must outlive this use case.
class StartupUpdateCheckUseCase final
{
public:
    explicit StartupUpdateCheckUseCase(StartupUpdateCheckPort& port)
        : m_port(port)
    {
    }

    StartupUpdateCheckUseCase(const StartupUpdateCheckUseCase&) = delete;
    StartupUpdateCheckUseCase& operator=(const StartupUpdateCheckUseCase&) = delete;

    [[nodiscard]] StartupUpdateCheckDisposition start(
        const StartupUpdateCheckInput& input,
        StartupUpdateCheckCompletion completion
        )
    {
        if (m_attempted)
        {
            return StartupUpdateCheckDisposition::AlreadyAttempted;
        }
        if (!input.startupComplete || !automaticUpdateStartupCheckIsEligible(
                input.checkOnStartup, input.preferences, input.hasReleasesApiUrl
                ))
        {
            return StartupUpdateCheckDisposition::NotEligible;
        }

        m_attempted = true;
        const auto pending = std::make_shared<Pending>();
        pending->lifetime = m_lifetime;
        pending->completion = std::move(completion);
        const auto dispatch = m_port.check(
            [pending](StartupUpdateCheckOutcome outcome)
            {
                if (pending->received || pending->lifetime.expired())
                {
                    return;
                }
                pending->received = true;
                pending->outcome = std::move(outcome);
                pending->deliver();
            });
        pending->acceptsCompletion = dispatch == StartupUpdateCheckDispatch::Started
            || dispatch == StartupUpdateCheckDispatch::Failed;
        pending->dispatchReturned = true;
        pending->deliver();
        switch (dispatch)
        {
        case StartupUpdateCheckDispatch::Started:
            return StartupUpdateCheckDisposition::Started;
        case StartupUpdateCheckDispatch::Busy:
            return StartupUpdateCheckDisposition::Busy;
        case StartupUpdateCheckDispatch::Failed:
            return StartupUpdateCheckDisposition::Failed;
        }
        return StartupUpdateCheckDisposition::Failed;
    }

private:
    struct Pending final
    {
        std::weak_ptr<int> lifetime;
        StartupUpdateCheckCompletion completion;
        std::optional<StartupUpdateCheckOutcome> outcome;
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

    StartupUpdateCheckPort& m_port;
    std::shared_ptr<int> m_lifetime = std::make_shared<int>(0);
    bool m_attempted = false;
};

} // namespace ClassMngr::Next::Application
