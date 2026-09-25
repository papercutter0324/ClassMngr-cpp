#include "roster_editor_widget.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "features/roster/ui/roster_model.h"
#include "features/roster/ui/roster_print_dialog.h"
#include "features/roster/services/roster_template_print_service.h"
#include "next/domain/student_name_pair.h"

#include <QDialog>

#include <map>

void RosterEditorWidget::importScores()
{
    if (!m_services || !m_services->speakingEvaluationService() || m_classroom.id <= 0)
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

    int changeCount = 0;
    for (const QString& evaluationName : evaluationColumns)
    {
        const int scoreColumn = findModelColumn(evaluationName);
        if (scoreColumn < 0)
        {
            continue;
        }

        const QList<SpeakingEvalScore> scores =
            m_services->speakingEvaluationService()->rosterScoreImport(
                m_classroom.id,
                evaluationName
                ).value_or(QList<SpeakingEvalScore>{});
        if (scores.isEmpty())
        {
            continue;
        }

        std::map<ClassMngr::Next::Domain::StudentNamePair, QString> lookup;
        for (const SpeakingEvalScore& score : scores)
        {
            const auto namePair =
                ClassMngr::Next::Domain::StudentNamePair::fromNames(
                    score.englishName.trimmed().toStdU16String(),
                    score.koreanName.trimmed().toStdU16String()
                    );
            if (!namePair)
            {
                continue;
            }

            // Preserve QHash::insert's last-write-wins behavior for duplicate
            // imported student pairs.
            lookup.insert_or_assign(
                *namePair,
                score.finalGrade
                );
        }

        for (int row = 0; row < m_model->rowCount(); ++row)
        {
            const QString englishName = m_model->index(row, englishColumn)
                                            .data(Qt::EditRole)
                                            .toString()
                                            .trimmed();
            const QString koreanName = m_model->index(row, koreanColumn)
                                           .data(Qt::EditRole)
                                           .toString()
                                           .trimmed();
            const auto namePair =
                ClassMngr::Next::Domain::StudentNamePair::fromNames(
                    englishName.toStdU16String(),
                    koreanName.toStdU16String()
                    );
            if (!namePair)
            {
                continue;
            }

            const auto score = lookup.find(*namePair);
            if (score == lookup.end())
            {
                continue;
            }

            const QString& finalGrade = score->second;

            const QModelIndex index = m_model->index(row, scoreColumn);
            if (!index.isValid() || index.data(Qt::EditRole).toString() == finalGrade)
            {
                continue;
            }

            if (m_model->setData(index, finalGrade, Qt::EditRole))
            {
                ++changeCount;
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
    request.currentClassId = m_classroom.id;
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
