#pragma once
#include "next/application/speaking_evaluation_report_context_read_port.h"
#include <QString>
namespace ClassMngr::Next::Platform
{
class WorkspaceStoreSpeakingEvaluationReportPort final
    : public Application::SpeakingEvaluationReportContextReadPort
{
public:
    explicit WorkspaceStoreSpeakingEvaluationReportPort(QString path);
    [[nodiscard]] Application::SpeakingEvaluationReportContextReadResult readReportContext(
        const Domain::ClassId& classId) const override;
private:
    QString m_path;
};
}
