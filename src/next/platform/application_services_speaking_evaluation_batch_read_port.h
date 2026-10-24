#pragma once

#include "next/application/speaking_evaluation_batch_read_query.h"

#include <atomic>
#include <cstddef>

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

    // Cumulative rows from fully successful reads. Failed or partial reads do
    // not contribute to this diagnostic.
    [[nodiscard]] std::size_t successfulCompletedRows() const noexcept;

private:
    ApplicationServices* m_services = nullptr;
    mutable std::atomic<std::size_t> m_successfulCompletedRows = 0;
};

} // namespace ClassMngr::Next::Platform
