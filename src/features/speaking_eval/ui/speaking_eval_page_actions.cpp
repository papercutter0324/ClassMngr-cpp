#include "speaking_eval_page_p.h"

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
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

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

    if (m_importNamesButton)
    {
        m_importNamesButton->setText(
            tr("Import Names")
            );
    }

    const QList<QString> reportLabels{
        tr("New Report"),
        tr("Print Reports"),
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
        m_koreanKeyboardButton->setText(
            tr("Korean Keyboard")
            );
        m_koreanKeyboardButton->setToolTip(
            tr("Open Korean typing website")
            );
    }

    if (m_saveButton)
    {
        m_saveButton->setText(
            tr("Save Changes")
            );
    }

    updateActions();
}

void SpeakingEvalPage::importNames()
{
    if (
        !m_services
        || !m_services->dataService()
        || m_classroom.id <= 0
        )
    {
        return;
    }

    const Roster roster =
        m_services
            ->dataService()
            ->loadRoster(
                m_classroom.id
                );

    if (roster.rows.isEmpty())
    {
        QMessageBox::warning(
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
        QMessageBox::warning(
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
        QMessageBox::information(
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

    QMessageBox::information(
        this,
        tr("Import Names"),
        tr("Roster names imported successfully.")
        );
}

void SpeakingEvalPage::autosave()
{
    if (!hasUnsavedChanges())
    {
        return;
    }

    saveEvaluationInternal(false, false);
}

void SpeakingEvalPage::openKoreanKeyboard()
{
    QDesktopServices::openUrl(
        QUrl(QStringLiteral("https://www.branah.com/korean"))
        );
}

void SpeakingEvalPage::updateActions()
{
    if (!m_saveButton || !m_model)
    {
        return;
    }

    const bool showSaveButton =
        m_saveMode != SaveMode::Automatic;

    const bool hasActiveClass =
        m_classroom.id > 0;

    m_saveButton->setVisible(
        showSaveButton
        );

    m_saveButton->setEnabled(
        showSaveButton
        && hasActiveClass
        && !m_model->changedCells().isEmpty()
        );

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

    bool hasStudents = false;

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
            hasStudents = true;
            break;
        }
    }

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
}

void SpeakingEvalPage::showReports()
{
    if (!m_model || m_classroom.id <= 0)
    {
        return;
    }

    ClassInfo classInfo;
    QByteArray signatureImage;

    if (m_services && m_services->dataService())
    {
        DataService* dataService =
            m_services->dataService();
        classInfo =
            dataService->loadClassInfo(m_classroom.id);
        signatureImage =
            PersonalDetailsRepository(dataService)
                .load()
                .signatureImage;
    }

    const QList<SpeakingEvalBatchReportService::StudentReport> reports =
        buildSpeakingEvalStudentReports(
            m_model->rows(),
            classInfo,
            signatureImage
            );
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
    if (m_services && m_services->dataService())
    {
        classInfo =
            m_services
                ->dataService()
                ->loadClassInfo(
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

void SpeakingEvalPage::exportReports()
{
    if (!m_model || m_classroom.id <= 0)
    {
        return;
    }

    ClassInfo classInfo;
    QByteArray signatureImage;
    if (m_services && m_services->dataService())
    {
        DataService* dataService =
            m_services->dataService();
        classInfo =
            dataService->loadClassInfo(m_classroom.id);
        signatureImage =
            PersonalDetailsRepository(dataService)
                .load()
                .signatureImage;
    }

    const QList<SpeakingEvalBatchReportService::StudentReport> reports =
        buildSpeakingEvalStudentReports(
            m_model->rows(),
            classInfo,
            signatureImage
            );

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
        this
        );
    dialog.exec();
}
