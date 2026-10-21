#pragma once

#include "data/database/workspace_store_fwd.h"

#include "core/application_services.h"
#include "next/application/speaking_evaluation_save_use_case.h"


namespace ClassMngr::Next::Platform
{

class ApplicationServicesSpeakingEvaluationSavePort final
    : public Application::SpeakingEvaluationSavePort
{
public:
    explicit ApplicationServicesSpeakingEvaluationSavePort(
        ApplicationServices& services
        ) noexcept
        : m_store(services.workspaceStore())
    {
    }

    explicit ApplicationServicesSpeakingEvaluationSavePort(
        ApplicationServices* services
        ) noexcept
        : m_store(services ? services->workspaceStore() : nullptr)
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
    WorkspaceStore* m_store = nullptr;
};

} // namespace ClassMngr::Next::Platform
