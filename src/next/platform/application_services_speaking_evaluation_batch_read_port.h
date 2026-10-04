#pragma once

#include "next/application/speaking_evaluation_batch_read_query.h"

class ApplicationServices;

namespace ClassMngr::Next::Platform
{

class ApplicationServicesSpeakingEvaluationBatchReadPort final
    : public Application::SpeakingEvaluationReadBatchPort
{
public:
    explicit ApplicationServicesSpeakingEvaluationBatchReadPort(
        ApplicationServices& services
        ) noexcept;

    explicit ApplicationServicesSpeakingEvaluationBatchReadPort(
        ApplicationServices* services
        ) noexcept;

    ApplicationServicesSpeakingEvaluationBatchReadPort(
        const ApplicationServicesSpeakingEvaluationBatchReadPort&
        ) = delete;
    ApplicationServicesSpeakingEvaluationBatchReadPort& operator=(
        const ApplicationServicesSpeakingEvaluationBatchReadPort&
        ) = delete;
    ApplicationServicesSpeakingEvaluationBatchReadPort(
        ApplicationServicesSpeakingEvaluationBatchReadPort&&
        ) = delete;
    ApplicationServicesSpeakingEvaluationBatchReadPort& operator=(
        ApplicationServicesSpeakingEvaluationBatchReadPort&&
        ) = delete;

    [[nodiscard]] Application::SpeakingEvaluationReadBatchResult
    readEvaluations(
        const Application::SpeakingEvaluationReadBatchQuery& query
        ) const override;

private:
    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
