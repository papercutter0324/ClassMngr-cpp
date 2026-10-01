#pragma once

#include "core/application_services.h"
#include "next/application/speaking_evaluation_save_use_case.h"

class DatabaseSession;

namespace ClassMngr::Next::Platform
{

class ApplicationServicesSpeakingEvaluationSavePort final
    : public Application::SpeakingEvaluationSavePort
{
public:
    explicit ApplicationServicesSpeakingEvaluationSavePort(
        ApplicationServices& services
        ) noexcept
        : m_session(services.databaseSession())
    {
    }

    explicit ApplicationServicesSpeakingEvaluationSavePort(
        ApplicationServices* services
        ) noexcept
        : m_session(services ? services->databaseSession() : nullptr)
    {
    }

    ApplicationServicesSpeakingEvaluationSavePort(
        const ApplicationServicesSpeakingEvaluationSavePort&
        ) = delete;
    ApplicationServicesSpeakingEvaluationSavePort& operator=(
        const ApplicationServicesSpeakingEvaluationSavePort&
        ) = delete;
    ApplicationServicesSpeakingEvaluationSavePort(
        ApplicationServicesSpeakingEvaluationSavePort&&
        ) = delete;
    ApplicationServicesSpeakingEvaluationSavePort& operator=(
        ApplicationServicesSpeakingEvaluationSavePort&&
        ) = delete;

    [[nodiscard]] Domain::Result<void> saveEvaluation(
        const Application::SpeakingEvaluationSaveRequest& request
        ) const override;

private:
    DatabaseSession* m_session = nullptr;
};

} // namespace ClassMngr::Next::Platform
