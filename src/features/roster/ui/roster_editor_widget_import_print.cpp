#include "roster_editor_widget.h"

#include "core/application_services.h"
#include "core/utils/student_name_utils.h"
#include "data/data_service.h"
#include "features/roster/ui/roster_model.h"
#include "features/roster/ui/roster_print_dialog.h"
#include "features/roster/services/roster_template_print_service.h"

#include <QDialog>
#include <QHash>
#include <QMessageBox>

void RosterEditorWidget::importScores()
{
    if (!m_services || !m_services->dataService() || m_classroom.id <= 0)
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
        QMessageBox::warning(
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
            m_services->dataService()->buildRosterScoreImport(
                m_classroom.id,
                evaluationName
                );
        if (scores.isEmpty())
        {
            continue;
        }

        QHash<QString, QString> lookup;
        for (const SpeakingEvalScore& score : scores)
        {
            lookup.insert(
                StudentNameUtils::namePairKey(
                    score.englishName,
                    score.koreanName
                    ),
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
            const QString namePairKey =
                StudentNameUtils::namePairKey(
                    englishName,
                    koreanName
                    );

            if (namePairKey.isEmpty())
            {
                continue;
            }

            if (!lookup.contains(namePairKey))
            {
                continue;
            }

            const QString finalGrade = lookup.value(namePairKey);

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
        QMessageBox::information(
            this,
            tr("Import Scores"),
            tr("Scores are already up to date.")
            );
        return;
    }

    updateActions();
    scheduleAutosave();
    QMessageBox::information(
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
        QMessageBox::warning(this, tr("Print Rosters"), result.message);
    }
}
