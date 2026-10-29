#pragma once

#include "next/application/speaking_evaluation_query.h"
#include "next/application/speaking_evaluation_roster_names_read_port.h"
#include "next/application/speaking_evaluation_save_use_case.h"
#include "next/application/selected_class_grade_read_port.h"

#include <QString>

namespace ClassMngr::Next::Platform
{

// Path-scoped direct WorkspaceStore adapters for the Next evaluation page.
// Each call opens only the active profile path and releases its store before
// returning; no Legacy ApplicationServices object is involved.
class WorkspaceStoreSpeakingEvaluationReadPort final
    : public Application::SpeakingEvaluationReadPort
{
public:
    explicit WorkspaceStoreSpeakingEvaluationReadPort(QString databasePath);

    [[nodiscard]] Application::SpeakingEvaluationReadResult readEvaluation(
        const Application::SpeakingEvaluationReadQuery& query
        ) const override;

private:
    QString m_databasePath;
};

class WorkspaceStoreSpeakingEvaluationSavePort final
    : public Application::SpeakingEvaluationSavePort
{
public:
    explicit WorkspaceStoreSpeakingEvaluationSavePort(QString databasePath);

    [[nodiscard]] Domain::Result<void> saveEvaluation(
        const Application::SpeakingEvaluationSaveRequest& request
        ) const override;

private:
    QString m_databasePath;
};

class WorkspaceStoreSpeakingEvaluationRosterNamesReadPort final
    : public Application::SpeakingEvaluationRosterNamesReadPort
{
public:
    explicit WorkspaceStoreSpeakingEvaluationRosterNamesReadPort(
        QString databasePath
        );

    [[nodiscard]] Application::SpeakingEvaluationRosterNamesReadResult
    readSpeakingEvaluationRosterNames(
        const Application::SpeakingEvaluationRosterNamesReadRequest& request
        ) const override;

private:
    QString m_databasePath;
};

class WorkspaceStoreSpeakingEvaluationClassGradeReadPort final
    : public Application::SelectedClassGradeReadPort
{
public:
    explicit WorkspaceStoreSpeakingEvaluationClassGradeReadPort(
        QString databasePath
        );

    [[nodiscard]] Application::SelectedClassGradeReadResult
    readSelectedClassGrade(
        const Domain::ClassId& classId
        ) const override;

private:
    QString m_databasePath;
};

} // namespace ClassMngr::Next::Platform
