#pragma once

#include "next/application/speaking_evaluation_query.h"

class ApplicationServices;

namespace ClassMngr::Next::Platform
{

class ApplicationServicesSpeakingEvaluationReadPort final
    : public Application::SpeakingEvaluationReadPort
{
public:
    explicit ApplicationServicesSpeakingEvaluationReadPort(
        ApplicationServices& services
        ) noexcept;

    explicit ApplicationServicesSpeakingEvaluationReadPort(
        ApplicationServices* services
        ) noexcept;

    ApplicationServicesSpeakingEvaluationReadPort(
        const ApplicationServicesSpeakingEvaluationReadPort&
        ) = delete;
    ApplicationServicesSpeakingEvaluationReadPort& operator=(
        const ApplicationServicesSpeakingEvaluationReadPort&
        ) = delete;
    ApplicationServicesSpeakingEvaluationReadPort(
        ApplicationServicesSpeakingEvaluationReadPort&&
        ) = delete;
    ApplicationServicesSpeakingEvaluationReadPort& operator=(
        ApplicationServicesSpeakingEvaluationReadPort&&
        ) = delete;

    [[nodiscard]] Application::SpeakingEvaluationReadResult readEvaluation(
        const Application::SpeakingEvaluationReadQuery& query
        ) const override;

private:
    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
