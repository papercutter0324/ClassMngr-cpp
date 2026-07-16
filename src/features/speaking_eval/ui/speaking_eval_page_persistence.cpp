#include "speaking_eval_page_p.h"

void SpeakingEvalPage::saveData()
{
    saveEvaluationInternal(true, true);
}

bool SpeakingEvalPage::saveChanges()
{
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    if (!hasUnsavedChanges())
    {
        return true;
    }

    return saveEvaluationInternal(true, false);
}

bool SpeakingEvalPage::hasUnsavedChanges() const
{
    return m_model && m_model->isDirty();
}

void SpeakingEvalPage::discardChanges()
{
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    loadEvaluation(
        m_classroom,
        m_evaluationName
        );
}

QString SpeakingEvalPage::unsavedChangesTitle() const
{
    return tr("Unsaved Speaking Evaluation Changes");
}

QString SpeakingEvalPage::unsavedChangesMessage() const
{
    return tr("This speaking evaluation has unsaved changes.");
}

void SpeakingEvalPage::setSaveMode(
    SaveMode mode
    )
{
    if (m_saveMode == mode)
    {
        return;
    }

    m_saveMode = mode;

    updateActions();

    if (!m_autosaveTimer)
    {
        return;
    }

    if (m_saveMode == SaveMode::Automatic && hasUnsavedChanges())
    {
        m_autosaveTimer->start();
    }
    else
    {
        m_autosaveTimer->stop();
    }
}

bool SpeakingEvalPage::saveEvaluationInternal(
    bool showValidationMessages,
    bool showSuccessMessage
    )
{
    if (
        !m_services
        || !m_services->dataService()
        || m_classroom.id <= 0
        || m_evaluationName.trimmed().isEmpty()
        )
    {
        return false;
    }

    m_model->revalidateAll();
    m_table->viewport()->update();

    if (m_model->hasErrors())
    {
        if (showValidationMessages)
        {
            QMessageBox message(this);
            message.setIcon(QMessageBox::Warning);
            message.setWindowTitle(
                tr("Validation Errors")
                );
            message.setText(
                tr("Fix validation errors before saving.")
                );
            message.setDetailedText(
                m_model->errorList().join(QLatin1Char('\n'))
                );
            message.exec();
        }

        return false;
    }

    const bool saved =
        m_services
            ->dataService()
            ->saveSpeakingEval(
                m_classroom.id,
                m_evaluationName,
                m_model->rows(),
                m_model->changedCells()
                );

    if (!saved)
    {
        if (showValidationMessages)
        {
            QMessageBox::warning(
                this,
                tr("Save Failed"),
                tr("The speaking evaluation could not be saved.")
                );
        }

        return false;
    }

    m_model->markSaved();
    updateActions();

    if (showSuccessMessage)
    {
        QMessageBox::information(
            this,
            tr("Saved"),
            tr("Speaking evaluation saved.")
            );
    }

    return true;
}

