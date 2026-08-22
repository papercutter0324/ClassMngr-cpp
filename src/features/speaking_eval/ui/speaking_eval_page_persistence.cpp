#include "speaking_eval_page_p.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/validation/form_validation_binder.h"

void SpeakingEvalPage::saveData()
{
    saveEvaluationInternal(true, true);
}

bool SpeakingEvalPage::saveChanges()
{
    m_autosave->cancelPendingSave();

    if (!hasUnsavedChanges())
    {
        return true;
    }

    return saveEvaluationInternal(true, false);
}

bool SpeakingEvalPage::hasUnsavedChanges() const
{
    return m_autosave->isDirty();
}

void SpeakingEvalPage::discardChanges()
{
    m_autosave->cancelPendingSave();

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
    m_autosave->setSaveMode(mode);
}

bool SpeakingEvalPage::saveEvaluationInternal(
    bool showValidationMessages,
    bool showSuccessMessage
    )
{
    if (
        !m_services
        || !m_services->speakingEvaluationService()
        || m_classroom.id <= 0
        || m_evaluationName.trimmed().isEmpty()
        )
    {
        return false;
    }

    Q_UNUSED(showValidationMessages);
    updateEvaluationValidation();

    if (m_validationBinder && m_validationBinder->hasErrors())
    {
        updateActions();
        focusFirstEvaluationError();
        return false;
    }

    const Status saved =
        m_services
            ->speakingEvaluationService()
            ->saveEvaluation(
                m_classroom.id,
                m_evaluationName,
                m_model->rows(),
                m_model->changedCells()
                );

    if (!saved)
    {
        if (showValidationMessages)
        {
            DialogServices::showWarning(
                this,
                tr("Save Failed"),
                saved.error()
                );
        }

        return false;
    }

    m_model->markSaved();
    m_autosave->markClean();
    updateActions();

    if (showSuccessMessage)
    {
        DialogServices::showInformation(
            this,
            tr("Saved"),
            tr("Speaking evaluation saved.")
            );
    }

    return true;
}
