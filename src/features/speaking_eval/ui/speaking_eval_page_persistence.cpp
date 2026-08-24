#include "speaking_eval_page_p.h"

#include "domain/validation/speaking_eval_validator.h"
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

    return saveEvaluationInternal(true, false, true);
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
    bool showSuccessMessage,
    bool confirmQuestionableLengths
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

    const ValidationResult validation = SpeakingEvalValidator::validate(
        m_classroom.id,
        m_evaluationName,
        SpeakingEvalValidator::normalized(m_model->rows()),
        confirmQuestionableLengths
        );
    if (validation.hasErrors())
    {
        updateActions();
        focusFirstEvaluationError();
        return false;
    }

    if (confirmQuestionableLengths
        && !confirmQuestionableKoreanNameLengths())
    {
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
                m_model->changedCells(),
                confirmQuestionableLengths
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

QStringList SpeakingEvalPage::questionableKoreanNameRows() const
{
    if (!m_model)
    {
        return {};
    }

    QStringList names;
    const int koreanColumn = SpeakingEval::toInt(SpeakingEvalColumn::KoreanName);
    const SpeakingEvalRows rows = m_model->rows();
    for (int row = 0; row < rows.size(); ++row)
    {
        const QString koreanName = rows[row].value(koreanColumn);
        const auto issues = StudentNameUtils::validateKoreanName(koreanName);
        if (!issues.contains(StudentNameUtils::ValidationIssue::KoreanTooShort)
            && !issues.contains(StudentNameUtils::ValidationIssue::KoreanTooLong))
        {
            continue;
        }

        names.append(
            tr("Row %1: %2")
                .arg(row + 1)
                .arg(koreanName)
            );
    }

    return names;
}

bool SpeakingEvalPage::confirmQuestionableKoreanNameLengths()
{
    const QStringList names = questionableKoreanNameRows();
    if (names.isEmpty())
    {
        return true;
    }

    return DialogServices::confirm(
        this,
        tr("Verify Korean Name Lengths"),
        tr("These Korean names have 1 or 5+ syllables and may be incorrect:\n%1\n\nSave them anyway?")
            .arg(names.join(QLatin1Char('\n'))),
        tr("Save Anyway"),
        tr("Go Back")
        ) == PromptChoice::Accepted;
}
