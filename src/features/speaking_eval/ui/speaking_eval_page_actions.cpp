#include "speaking_eval_page_p.h"

void SpeakingEvalPage::refresh()
{
    BasePage::refresh();

    if (m_table)
    {
        m_table->viewport()->update();
    }

    if (m_table && m_table->horizontalHeader())
    {
        m_table->horizontalHeader()->viewport()->update();
    }
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
        tr("Create Reports"),
        tr("Export / Print Reports")
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

    if (m_services && m_services->dataService())
    {
        classInfo =
            m_services
                ->dataService()
                ->loadClassInfo(m_classroom.id);
    }

    SpeakingEvalReportDialog dialog(
        m_model->rows(),
        classInfo,
        this
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

void SpeakingEvalPage::exportReports()
{
    if (!m_model || m_classroom.id <= 0)
    {
        return;
    }

    ClassInfo classInfo;
    if (m_services && m_services->dataService())
    {
        classInfo = m_services->dataService()->loadClassInfo(m_classroom.id);
    }

    const QList<SpeakingEvalBatchReportService::StudentReport> reports =
        buildSpeakingEvalStudentReports(m_model->rows(), classInfo);

    int currentReportIndex = 0;
    const int selectedRow = m_table ? m_table->currentIndex().row() : -1;
    if (selectedRow >= 0)
    {
        int reportIndex = 0;
        for (int row = 0; row < m_model->rows().size(); ++row)
        {
            const QString englishName = m_model->rows().at(row).value(
                SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)
                ).trimmed();
            const QString koreanName = m_model->rows().at(row).value(
                SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)
                ).trimmed();
            if (englishName.isEmpty() && koreanName.isEmpty())
            {
                continue;
            }
            if (row == selectedRow)
            {
                currentReportIndex = reportIndex;
                break;
            }
            ++reportIndex;
        }
    }

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

