#include "speaking_eval_page_p.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "features/my_info/data/personal_details_repository.h"
#include "features/speaking_eval/ui/speaking_eval_ai_batch_dialog.h"

void SpeakingEvalPage::refresh()
{
    BasePage::refresh();

    syncEvaluationTabFont();

    if (m_table)
    {
        m_table->viewport()->update();
    }

    if (m_table && m_table->horizontalHeader())
    {
        m_table->horizontalHeader()->viewport()->update();
    }
}

void SpeakingEvalPage::clearDatabaseState()
{
    m_autosave->cancelPendingSave();

    if (m_undoStack)
    {
        m_undoStack->clear();
    }

    m_importingNames = false;
    m_resolvingDuplicateName = false;
    loadEvaluations();
}

void SpeakingEvalPage::retranslateUi()
{
    updateHeaderText();

    if (m_emptyLabel)
    {
        m_emptyLabel->setText(
            tr("No classes available")
            );
    }

    if (m_evaluationTabs)
    {
        for (int index = 0; index < m_evaluationTabs->count(); ++index)
        {
            QWidget* page =
                m_evaluationTabs->widget(index);

            if (page)
            {
                m_evaluationTabs->setTabText(
                    index,
                    evaluationLabel(
                        page->property("evaluation_name").toString()
                        )
                    );
            }
        }
    }

    if (m_embeddedEvaluationCombo)
    {
        for (int index = 0;
             index < m_embeddedEvaluationCombo->count();
             ++index)
        {
            m_embeddedEvaluationCombo->setItemText(
                index,
                evaluationLabel(
                    m_embeddedEvaluationCombo->itemData(index).toString()
                    )
                );
        }
    }

    if (m_importNamesButton)
    {
        m_importNamesButton->setText(
            tr("Import Names")
            );
    }

    const QList<QString> reportLabels{
        tr("Report Editor"),
        tr("Generate Comments")
    };

    for (
        int index = 0;
        index < m_reportButtons.size()
            && index < reportLabels.size();
        ++index
        )
    {
        m_reportButtons[index]->setText(
            reportLabels[index]
            );
    }

    if (m_koreanKeyboardButton)
    {
        m_koreanKeyboardButton->setToolTip(
            tr("Open Korean / English on-screen keyboard")
            );
        m_koreanKeyboardButton->setAccessibleName(
            tr("Korean Keyboard")
            );
    }

    updateActions();
}

void SpeakingEvalPage::importNames()
{
    if (
        !m_services
        || !m_services->rosterService()
        || m_classroom.id <= 0
        )
    {
        return;
    }

    const Roster roster =
        m_services
            ->rosterService()
            ->roster(
                m_classroom.id
                );

    if (roster.rows.isEmpty())
    {
        DialogServices::showWarning(
            this,
            tr("Import Names"),
            tr("No roster data found.")
            );
        return;
    }

    if (
        findColumn(
            roster.columns,
            QStringLiteral("English")
            ) < 0
        || findColumn(
            roster.columns,
            QStringLiteral("Korean")
            ) < 0
        )
    {
        DialogServices::showWarning(
            this,
            tr("Import Names"),
            tr("Roster must contain 'English' and 'Korean' columns.")
            );
        return;
    }

    const QList<SpeakingEvalCellEdit> changes =
        nameImportChanges(
            roster.columns,
            roster.rows
            );

    if (changes.isEmpty())
    {
        DialogServices::showInformation(
            this,
            tr("Import Names"),
            tr("Names are already up to date.")
            );
        return;
    }

    m_importingNames = true;

    m_table->applyChanges(
        changes,
        tr("Import Names")
        );

    m_importingNames = false;
    m_table->viewport()->update();

    updateActions();

    DialogServices::showInformation(
        this,
        tr("Import Names"),
        tr("Roster names imported successfully.")
        );
}

void SpeakingEvalPage::openKoreanKeyboard()
{
    if (m_onScreenKeyboard)
    {
        m_onScreenKeyboard->showFor(m_table);
    }
}

void SpeakingEvalPage::showKoreanKeyboard()
{
    openKoreanKeyboard();
}

void SpeakingEvalPage::updateActions()
{
    if (!m_saveButton || !m_model)
    {
        return;
    }

    const bool hasActiveClass =
        m_classroom.id > 0;
    m_autosave->setSaveAvailable(
        hasActiveClass
        && !m_evaluationName.trimmed().isEmpty()
        );
    m_autosave->setValid(!m_model->hasErrors());
    m_autosave->setDirty(m_model->isDirty());
    m_autosave->setSaveMode(m_autosave->saveMode());

    if (m_importNamesButton)
    {
        m_importNamesButton->setEnabled(
            hasActiveClass
            );
    }

    if (m_koreanKeyboardButton)
    {
        m_koreanKeyboardButton->setEnabled(
            hasActiveClass
            );
    }

    const bool hasStudents = hasReportStudents();

    for (QPushButton* button : m_reportButtons)
    {
        if (button)
        {
            button->setEnabled(
                hasActiveClass && hasStudents
                );
            button->setToolTip(
                hasActiveClass && hasStudents
                    ? QString()
                    : hasActiveClass
                        ? tr("Import or enter a student name to create reports.")
                        : tr("Select a class to create reports.")
            );
        }
    }

    emit outputCapabilitiesChanged();
}

bool SpeakingEvalPage::hasReportStudents() const
{
    if (!m_model)
    {
        return false;
    }

    for (const QStringList& row : m_model->rows())
    {
        if (
            !row.value(
                SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)
                ).trimmed().isEmpty()
            || !row.value(
                SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)
                ).trimmed().isEmpty()
            )
        {
            return true;
        }
    }

    return false;
}

PageOutputCapabilities SpeakingEvalPage::outputCapabilities() const
{
    const bool enabled =
        isDatabaseOpen()
        && m_classroom.id > 0
        && hasReportStudents();
    return {enabled, enabled};
}

void SpeakingEvalPage::printCurrentPage()
{
    outputReports(true);
}

void SpeakingEvalPage::saveCurrentPageAs()
{
    outputReports(false);
}

void SpeakingEvalPage::showReports()
{
    if (!m_model || m_classroom.id <= 0)
    {
        return;
    }

    ClassInfo classInfo;
    QByteArray signatureImage;

    if (m_services && m_services->classService())
    {
        classInfo =
            m_services->classService()->classInfo(m_classroom.id);
        signatureImage =
            PersonalDetailsRepository(m_services->settingsService())
                .load()
                .signatureImage;
    }

    const QList<SpeakingEvalBatchReportService::StudentReport> reports =
        buildSpeakingEvalStudentReports(
            m_model->rows(),
            classInfo,
            signatureImage
            );

    if (reports.isEmpty())
    {
        return;
    }
    const int selectedRow =
        m_table ? m_table->currentIndex().row() : -1;
    const int currentReportIndex =
        qMax(
            0,
            speakingEvalReportIndexForSourceRow(
                reports,
                selectedRow
                )
            );

    SpeakingEvalReportDialog dialog(
        reports,
        currentReportIndex,
        this,
        true
        );

    connect(
        &dialog,
        &SpeakingEvalReportDialog::reportValueEdited,
        this,
        [this](int row, SpeakingEvalColumn column, const QString& value)
        {
            if (!m_model)
            {
                return;
            }

            const QModelIndex index =
                m_model->index(row, SpeakingEval::toInt(column));
            if (index.isValid())
            {
                m_model->setData(index, value, Qt::EditRole);
            }
        }
        );

    dialog.exec();
}

void SpeakingEvalPage::generateClassAiComments()
{
    if (!m_model || !m_table || m_classroom.id <= 0)
    {
        return;
    }

    ClassInfo classInfo;
    if (m_services && m_services->classService())
    {
        classInfo =
            m_services
                ->classService()
                ->classInfo(
                    m_classroom.id
                    );
    }

    const QList<SpeakingEvalBatchReportService::StudentReport> reports =
        buildSpeakingEvalStudentReports(
            m_model->rows(),
            classInfo
            );
    if (reports.isEmpty())
    {
        return;
    }

    SpeakingEvalAiBatchDialog dialog(
        reports,
        this
        );
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    QList<SpeakingEvalCellEdit> changes;
    for (
        const SpeakingEvalAiBatchAcceptedComment& comment :
        dialog.acceptedComments()
        )
    {
        if (
            comment.sourceRow < 0
            || comment.sourceRow >= m_model->rowCount()
            )
        {
            continue;
        }
        changes.append(
            {
                comment.sourceRow,
                SpeakingEval::toInt(
                    SpeakingEvalColumn::Comments
                    ),
                comment.oldComment,
                comment.newComment
            }
            );
    }

    m_table->applyChanges(
        changes,
        tr("Apply AI Comments")
        );
}

void SpeakingEvalPage::outputReports(
    bool print
    )
{
    if (!m_model || m_classroom.id <= 0)
    {
        return;
    }

    ClassInfo classInfo;
    QByteArray signatureImage;
    if (m_services && m_services->classService())
    {
        classInfo =
            m_services->classService()->classInfo(m_classroom.id);
        signatureImage =
            PersonalDetailsRepository(m_services->settingsService())
                .load()
                .signatureImage;
    }

    const QList<SpeakingEvalBatchReportService::StudentReport> reports =
        buildSpeakingEvalStudentReports(
            m_model->rows(),
            classInfo,
            signatureImage
            );

    if (reports.isEmpty())
    {
        return;
    }

    const int selectedRow = m_table ? m_table->currentIndex().row() : -1;
    const int currentReportIndex =
        qMax(
            0,
            speakingEvalReportIndexForSourceRow(
                reports,
                selectedRow
                )
            );

    SpeakingEvalBatchExportDialog dialog(
        reports,
        currentReportIndex,
        SpeakingEvalBatchReportService::defaultOutputDirectory(
            classInfo,
            m_evaluationName
            ),
        print
            ? SpeakingEvalBatchExportDialog::Mode::Print
            : SpeakingEvalBatchExportDialog::Mode::SaveAs,
        this
        );
    dialog.exec();
}
