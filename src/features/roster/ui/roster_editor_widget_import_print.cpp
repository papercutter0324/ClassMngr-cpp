#include "roster_editor_widget.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "features/roster/ui/roster_model.h"
#include "features/roster/ui/roster_print_dialog.h"
#include "features/roster/services/roster_template_print_service.h"
#include "next/application/speaking_evaluation_roster_score_import_batch_use_case.h"
#include "next/application/speaking_evaluation_roster_score_row_assignments.h"
#include "next/application/classes_list_read_query.h"
#include "next/application/roster_print_class_info_read_query.h"
#include "next/domain/domain_types.h"
#include "next/platform/application_services_classes_list_read_port.h"
#include "next/platform/application_services_roster_print_class_info_read_port.h"
#include "next/platform/application_services_roster_read_port.h"
#include "next/platform/application_services_speaking_evaluation_batch_read_port.h"
#include "next/platform/application_services_speaking_evaluation_read_port.h"

#include <QDialog>

#include <optional>
#include <string>
#include <utility>
#include <vector>

void RosterEditorWidget::importScores()
{
    if (!m_services || m_classroom.id <= 0)
    {
        return;
    }

    const auto classId =
        ClassMngr::Next::Domain::ClassId::fromString(
            std::to_string(m_classroom.id)
            );
    if (!classId)
    {
        return;
    }

    const auto findModelColumn =
        [this](const QString& name)
        {
            for (int column = 0; column < m_model->columnCount(); ++column)
            {
                if (m_model->columnName(column).compare(name, Qt::CaseInsensitive) == 0)
                {
                    return column;
                }
            }

            return -1;
        };

    const int englishColumn = findModelColumn(QStringLiteral("English"));
    const int koreanColumn = findModelColumn(QStringLiteral("Korean"));
    if (englishColumn < 0 || koreanColumn < 0)
    {
        DialogServices::showWarning(
            this,
            tr("Import Scores"),
            tr("Roster must contain 'English' and 'Korean' columns.")
            );
        return;
    }

    const QStringList evaluationColumns{
        QStringLiteral("Winter"),
        QStringLiteral("Speech Contest"),
        QStringLiteral("Summer"),
        QStringLiteral("Fall")
    };

    std::vector<std::u16string> requestedEvaluationNames;
    std::vector<int> scoreColumns;
    requestedEvaluationNames.reserve(
        static_cast<std::size_t>(evaluationColumns.size())
        );
    scoreColumns.reserve(static_cast<std::size_t>(evaluationColumns.size()));
    for (const QString& evaluationName : evaluationColumns)
    {
        const int scoreColumn = findModelColumn(evaluationName);
        if (scoreColumn < 0)
        {
            continue;
        }

        requestedEvaluationNames.push_back(evaluationName.toStdU16String());
        scoreColumns.push_back(scoreColumn);
    }

    int changeCount = 0;
    ClassMngr::Next::Platform::ApplicationServicesSpeakingEvaluationReadPort
        singleEvaluationPort(m_services);
    ClassMngr::Next::Platform::
        ApplicationServicesSpeakingEvaluationBatchReadPort batchEvaluationPort(
            m_services
            );
    const auto importedEvaluations = ClassMngr::Next::Application::
        SpeakingEvaluationRosterScoreImportBatchUseCase::execute(
            {
                .classId = *classId,
                .evaluationNames = requestedEvaluationNames
            },
            batchEvaluationPort,
            singleEvaluationPort
            );
    if (importedEvaluations)
    {
        for (std::size_t evaluationIndex = 0;
             evaluationIndex < requestedEvaluationNames.size();
             ++evaluationIndex)
        {
            const auto& importedScores =
                importedEvaluations.value()[evaluationIndex];
            if (!importedScores || importedScores.value().empty())
            {
                continue;
            }

            const int scoreColumn = scoreColumns[evaluationIndex];

            std::vector<
                ClassMngr::Next::Application::SpeakingEvaluationRosterScoreRow
                > rosterRows;
            const int rosterRowCount = m_model->rowCount();
            rosterRows.reserve(static_cast<std::size_t>(rosterRowCount));
            for (int row = 0; row < rosterRowCount; ++row)
            {
                const QModelIndex scoreIndex = m_model->index(row, scoreColumn);
                std::optional<std::u16string> currentGrade;
                if (scoreIndex.isValid())
                {
                    currentGrade = scoreIndex.data(Qt::EditRole)
                        .toString()
                        .toStdU16String();
                }

                rosterRows.push_back({
                    .names = {
                        m_model->index(row, englishColumn)
                            .data(Qt::EditRole)
                            .toString()
                            .toStdU16String(),
                        m_model->index(row, koreanColumn)
                            .data(Qt::EditRole)
                            .toString()
                            .toStdU16String()
                    },
                    .currentGrade = std::move(currentGrade)
                });
            }

            const auto assignments =
                ClassMngr::Next::Application::
                    speakingEvaluationRosterScoreRowAssignments(
                        rosterRows,
                        importedScores.value()
                        );
            for (const auto& assignment : assignments)
            {
                const int row = static_cast<int>(assignment.rosterRowIndex);
                const QModelIndex index = m_model->index(row, scoreColumn);
                if (!index.isValid())
                {
                    continue;
                }

                const QString finalGrade = QString::fromUtf16(
                    assignment.finalGrade.data(),
                    static_cast<qsizetype>(assignment.finalGrade.size())
                    );
                if (m_model->setData(index, finalGrade, Qt::EditRole))
                {
                    ++changeCount;
                }
            }
        }
    }

    if (changeCount == 0)
    {
        DialogServices::showInformation(
            this,
            tr("Import Scores"),
            tr("Scores are already up to date.")
            );
        return;
    }

    updateActions();
    scheduleAutosave();
    DialogServices::showInformation(
        this,
        tr("Import Scores"),
        tr("Scores imported successfully.")
        );
}

void RosterEditorWidget::outputRosters(
    bool print
    )
{
    if (hasUnsavedChanges() && !saveChanges())
    {
        return;
    }

    const RosterPrintDialog::Action action =
        print
            ? RosterPrintDialog::Action::Print
            : RosterPrintDialog::Action::SaveAs;

    RosterPrintDialog dialog(
        m_services,
        m_classroom.id,
        RosterTemplatePrintService::Scope::CurrentClass,
        action,
        this,
        m_testingClassMode
        );
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    RosterTemplatePrintService::Request request;
    request.parent = this;
    request.services = m_services;
    ClassMngr::Next::Platform::ApplicationServicesClassesListReadPort
        classesReadPort(m_services);
    ClassMngr::Next::Platform::ApplicationServicesRosterReadPort
        rosterReadPort(m_services);
    ClassMngr::Next::Platform::
        ApplicationServicesRosterPrintClassInfoReadPort classInfoReadPort(
            m_services
            );
    const ClassMngr::Next::Application::ClassesListReadQuery classesQuery(
        classesReadPort
        );
    const ClassMngr::Next::Application::RosterPrintClassInfoReadQuery
        classInfoQuery(classInfoReadPort);
    request.classesListReadQuery = &classesQuery;
    request.rosterReadPort = &rosterReadPort;
    request.rosterPrintClassInfoReadQuery = &classInfoQuery;
    request.currentClassId = m_classroom.id;
    request.currentClassName = m_classroom.name;
    request.scope = dialog.selectedScope();
    request.selectedClassIds = dialog.selectedClassIds();
    request.templateId = dialog.selectedTemplateId();
    request.selectedExtraColumns = dialog.selectedExtraColumns();
    request.perClassExtraInfoOrientation = dialog.selectedPerClassExtraInfoOrientation();

    RosterTemplatePrintService::Result result;
    switch (action)
    {
    case RosterPrintDialog::Action::SaveAs:
        result = RosterTemplatePrintService::saveRostersPdf(
            request,
            dialog.selectedSavePath()
            );
        break;

    case RosterPrintDialog::Action::Print:
    default:
        result = RosterTemplatePrintService::printRosters(request);
        break;
    }

    if (result.status == RosterTemplatePrintService::Status::Failed)
    {
        DialogServices::showWarning(this, tr("Print Rosters"), result.message);
    }
}
