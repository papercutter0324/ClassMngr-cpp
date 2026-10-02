#include "speaking_eval_page_p.h"

#include "features/speaking_eval/ui/speaking_eval_page_validation_adapter.h"
#include "next/application/speaking_evaluation_save_use_case.h"
#include "next/platform/application_services_speaking_evaluation_save_port.h"
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
        || m_classroom.id <= 0
        || m_evaluationName.trimmed().isEmpty()
        )
    {
        return false;
    }

    updateEvaluationValidation();

    const auto request = SpeakingEvalPageValidationAdapter::makeSaveRequest(
        m_classroom.id,
        m_evaluationName,
        m_model->rows(),
        m_model->changedCells(),
        confirmQuestionableLengths
        );
    const auto applicationValidation =
        ClassMngr::Next::Application::validateAndNormalizeSpeakingEvaluation(
            request
            );
    const ValidationResult validation =
        SpeakingEvalPageValidationAdapter::toFormValidation(
            applicationValidation
            );
    if (validation.hasErrors())
    {
        updateActions();
        focusFirstEvaluationError();
        return false;
    }

    if (confirmQuestionableLengths
        && !confirmQuestionableKoreanNameLengths(applicationValidation))
    {
        focusFirstEvaluationError();
        return false;
    }

    const ClassMngr::Next::Platform::
        ApplicationServicesSpeakingEvaluationSavePort port(m_services);
    const auto saved =
        ClassMngr::Next::Application::SpeakingEvaluationSaveUseCase::execute(
            request,
            port
            );

    if (!saved)
    {
        if (showValidationMessages)
        {
            DialogServices::showWarning(
                this,
                tr("Save Failed"),
                QString::fromUtf8(
                    saved.error().message.data(),
                    static_cast<qsizetype>(saved.error().message.size())
                    )
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

QStringList SpeakingEvalPage::questionableKoreanNameRows(
    const ClassMngr::Next::Application::SpeakingEvaluationValidationResult& validation
    ) const
{
    if (!m_model)
    {
        return {};
    }

    QStringList names;
    const int koreanColumn = SpeakingEval::toInt(SpeakingEvalColumn::KoreanName);
    const SpeakingEvalRows rows = m_model->rows();
    QList<int> questionableRows;
    for (const auto& issue : validation.issues)
    {
        if ((issue.code != "student_name.korean.too_short"
             && issue.code != "student_name.korean.too_long")
            || issue.row < 0
            || issue.row >= rows.size()
            || questionableRows.contains(issue.row))
        {
            continue;
        }

        questionableRows.append(issue.row);
        const QString koreanName = rows[issue.row].value(koreanColumn);
        names.append(
            tr("Row %1: %2")
                .arg(issue.row + 1)
                .arg(koreanName)
            );
    }

    return names;
}

bool SpeakingEvalPage::confirmQuestionableKoreanNameLengths(
    const ClassMngr::Next::Application::SpeakingEvaluationValidationResult& validation
    )
{
    const QStringList names = questionableKoreanNameRows(validation);
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
