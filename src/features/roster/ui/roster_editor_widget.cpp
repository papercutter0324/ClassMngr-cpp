#include "roster_editor_widget.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "core/utils/student_name_utils.h"
#include "features/roster/ui/roster_column_layout_controller.h"
#include "features/roster/ui/roster_constants.h"
#include "features/roster/ui/roster_header_view.h"
#include "features/roster/ui/roster_model.h"
#include "features/roster/ui/roster_table_view.h"
#include "domain/validation/roster_validator.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/pages/autosave_coordinator.h"
#include "ui/shared/validation/form_validation_binder.h"

#include <QPushButton>
#include <QSignalBlocker>

RosterEditorWidget::RosterEditorWidget(
    ApplicationServices* services,
    bool embedded,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
    , m_embedded(embedded)
    , m_autosave(new AutosaveCoordinator(this))
{
    setProperty("role", UiRoles::RosterEditorWidget);

    if (m_embedded)
    {
        setPageLayoutMargins({});
    }

    buildUi();
    m_autosave->bindSaveButton(m_saveButton);
    connect(
        m_autosave,
        &AutosaveCoordinator::saveRequested,
        this,
        [this](bool interactive) { saveRosterInternal(interactive); }
        );
    connect(
        m_autosave,
        &AutosaveCoordinator::dirtyChanged,
        this,
        &RosterEditorWidget::unsavedChangesChanged
        );
    updateActions();
}

void RosterEditorWidget::loadClass(
    const Classroom& classroom
    )
{
    m_autosave->setLoading(true);
    m_classroom = classroom;
    updateHeaderText();
    m_loadingRoster = true;

    Roster roster;
    if (m_services && m_services->rosterService() && m_classroom.id > 0)
    {
        roster = m_services->rosterService()
            ->roster(m_classroom.id)
            .value_or(Roster{});
    }

    m_model->setRoster(roster);
    m_layoutController->attach(m_table, m_model);
    m_layoutController->applyWidths(
        normalizedColumnWidths(roster, m_model->columnNames())
        );
    applyTestingClassColumnVisibility();

    for (int row = 0; row < m_model->rowCount(); ++row)
    {
        m_table->setRowHeight(row, RosterUi::RowHeight);
    }

    m_model->clearDirty();
    m_widthsDirty = false;
    updateRosterValidation();
    m_loadingRoster = false;
    m_autosave->setLoading(false);
    m_autosave->markClean();
    updateActions();
    emit outputCapabilitiesChanged();
}

PageOutputCapabilities RosterEditorWidget::outputCapabilities() const
{
    const bool enabled = m_classroom.id > 0;
    return {enabled, enabled};
}

void RosterEditorWidget::printCurrentPage()
{
    outputRosters(true);
}

void RosterEditorWidget::saveCurrentPageAs()
{
    outputRosters(false);
}

void RosterEditorWidget::clearDatabaseState()
{
    m_resolvingDuplicateName = false;
    m_removingRosterRow = false;
    m_movingRosterRow = false;
    loadClass({});
}

void RosterEditorWidget::saveData()
{
    saveRosterInternal(true);
}

bool RosterEditorWidget::saveChanges()
{
    m_autosave->cancelPendingSave();

    return !hasUnsavedChanges() || saveRosterInternal(true, true);
}

bool RosterEditorWidget::hasUnsavedChanges() const
{
    return m_autosave->isDirty();
}

void RosterEditorWidget::discardChanges()
{
    m_autosave->cancelPendingSave();

    loadClass(m_classroom);
}

QString RosterEditorWidget::unsavedChangesTitle() const
{
    return tr("Unsaved Roster Changes");
}

QString RosterEditorWidget::unsavedChangesMessage() const
{
    return tr("This roster has unsaved changes.");
}

void RosterEditorWidget::setSaveMode(
    SaveMode mode
    )
{
    m_autosave->setSaveMode(mode);
}

void RosterEditorWidget::setTestingClassMode(
    bool testingClassMode
    )
{
    m_testingClassMode = testingClassMode;
    if (m_importButton)
    {
        m_importButton->setVisible(!m_testingClassMode);
    }
    updateKeyboardButtonVisibility();

    applyTestingClassColumnVisibility();
}

void RosterEditorWidget::setBottomKeyboardButtonVisible(
    bool visible
    )
{
    m_bottomKeyboardButtonVisible = visible;
    updateKeyboardButtonVisibility();
}

void RosterEditorWidget::updateKeyboardButtonVisibility()
{
    if (m_koreanKeyboardButton)
    {
        m_koreanKeyboardButton->setVisible(
            m_bottomKeyboardButtonVisible
            && !m_testingClassMode
            );
    }
}

void RosterEditorWidget::applyTestingClassColumnVisibility()
{
    if (!m_table || !m_model)
    {
        return;
    }

    const QSignalBlocker blocker(
        m_table->horizontalHeader()
        );

    for (int column = 0; column < m_model->columnCount(); ++column)
    {
        m_table->setColumnHidden(
            column,
            m_testingClassMode
            && RosterUi::isEvaluationColumn(
                m_model->columnName(column)
                )
            );
    }

    if (m_header && m_header->viewport())
    {
        m_header->viewport()->update();
    }
}

bool RosterEditorWidget::saveRosterInternal(
    bool showValidationMessages,
    bool confirmQuestionableLengths
    )
{
    if (!m_services || !m_services->rosterService() || m_classroom.id <= 0)
    {
        return false;
    }
    if (!validateRosterBeforeSave(
            showValidationMessages,
            confirmQuestionableLengths
            ))
    {
        return false;
    }

    const Status saved = m_services->rosterService()->saveRoster(
        m_classroom.id,
        currentRosterForSave(),
        confirmQuestionableLengths
        );
    if (!saved)
    {
        if (showValidationMessages)
        {
            DialogServices::showWarning(
                this,
                tr("Save Roster"),
                saved.error()
                );
        }
        return false;
    }

    m_model->clearDirty();
    m_widthsDirty = false;
    m_autosave->markClean();
    updateActions();
    return true;
}

bool RosterEditorWidget::validateRosterBeforeSave(
    bool showValidationMessages,
    bool confirmQuestionableLengths
    )
{
    Q_UNUSED(showValidationMessages);

    updateRosterValidation();
    const ValidationResult validation = RosterValidator::validate(
        RosterValidator::normalized(currentRosterForSave()),
        confirmQuestionableLengths
        );
    if (validation.hasErrors())
    {
        focusFirstRosterError();
        return false;
    }

    if (confirmQuestionableLengths
        && !confirmQuestionableKoreanNameLengths())
    {
        focusFirstRosterError();
        return false;
    }

    return true;
}

QStringList RosterEditorWidget::questionableKoreanNameRows() const
{
    if (!m_model)
    {
        return {};
    }

    const int koreanColumn = m_model->koreanNameColumn();
    if (koreanColumn < 0)
    {
        return {};
    }

    QStringList names;
    const Roster roster = m_model->toRoster();
    for (int row = 0; row < roster.rows.size(); ++row)
    {
        const QString koreanName = roster.rows[row].value(koreanColumn);
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

bool RosterEditorWidget::confirmQuestionableKoreanNameLengths()
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

void RosterEditorWidget::scheduleAutosave()
{
    updateRosterValidation();
    m_autosave->setSaveAvailable(m_classroom.id > 0);
    m_autosave->setDirty(
        m_widthsDirty || (m_model && m_model->isDirty())
        );
}

void RosterEditorWidget::updateRosterValidation()
{
    if (!m_validationBinder || !m_model || m_updatingValidation)
    {
        return;
    }

    const Roster roster = RosterValidator::normalized(currentRosterForSave());
    const ValidationResult validation = RosterValidator::validate(roster);

    m_updatingValidation = true;
    m_validationBinder->setValidation(
        validation,
        [](const ValidationIssue& issue)
        {
            return issue.field.startsWith(QStringLiteral("rows["))
                ? QObject::tr("Correct the highlighted roster cells.")
                : QString();
        }
        );
    m_model->setDomainValidation(validation);
    m_updatingValidation = false;
}

void RosterEditorWidget::focusFirstRosterError()
{
    if (!m_validationBinder)
    {
        return;
    }

    for (const ValidationIssue& issue : m_validationBinder->validation().errors())
    {
        if (issue.row >= 0 && issue.column >= 0)
        {
            selectRosterCell(issue.row, issue.column);
            return;
        }
    }

    m_validationBinder->focusFirstError();
}

Roster RosterEditorWidget::currentRosterForSave() const
{
    Roster roster = m_model ? m_model->toRoster() : Roster();
    roster.columnWidths = m_layoutController
        ? m_layoutController->currentWidths()
        : QVector<int>();
    return roster;
}

QVector<int> RosterEditorWidget::normalizedColumnWidths(
    const Roster& roster,
    const QStringList& columns
    ) const
{
    QVector<int> widths;
    widths.reserve(columns.size());

    for (const QString& columnName : columns)
    {
        int sourceColumn = -1;
        for (int index = 0; index < roster.columns.size(); ++index)
        {
            const bool exactMatch =
                roster.columns[index].compare(columnName, Qt::CaseInsensitive) == 0;
            const bool legacyFallMatch =
                columnName.compare(QStringLiteral("Fall"), Qt::CaseInsensitive) == 0
                && roster.columns[index].compare(
                    QStringLiteral("Autumn"),
                    Qt::CaseInsensitive
                    ) == 0;
            if (exactMatch || legacyFallMatch)
            {
                sourceColumn = index;
                break;
            }
        }

        widths.append(
            sourceColumn >= 0 && sourceColumn < roster.columnWidths.size()
                ? roster.columnWidths[sourceColumn]
                : 0
            );
    }

    return widths;
}
