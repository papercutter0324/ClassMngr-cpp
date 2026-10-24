#pragma once

#include "next/application/speaking_evaluation_query.h"

#include <atomic>
#include <cstddef>

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

    // Cumulative rows from fully successful reads. Failed or partial reads do
    // not contribute to this diagnostic.
    [[nodiscard]] std::size_t successfulCompletedRows() const noexcept;

private:
    ApplicationServices* m_services = nullptr;
    mutable std::atomic<std::size_t> m_successfulCompletedRows = 0;
};

} // namespace ClassMngr::Next::Platform
