#include "roster_editor_widget.h"

#include "core/application_services.h"
#include "data/data_service.h"
#include "features/roster/ui/roster_column_layout_controller.h"
#include "features/roster/ui/roster_constants.h"
#include "features/roster/ui/roster_header_view.h"
#include "features/roster/ui/roster_model.h"
#include "features/roster/ui/roster_table_view.h"
#include "ui/shared/styles/roles.h"

#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>

namespace
{

inline constexpr int AutosaveDelayMs = 750;

} // namespace

RosterEditorWidget::RosterEditorWidget(
    ApplicationServices* services,
    bool embedded,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
    , m_embedded(embedded)
{
    setProperty("role", UiRoles::RosterEditorWidget);

    if (m_embedded)
    {
        setPageLayoutMargins({});
    }

    buildUi();

    m_autosaveTimer = new QTimer(this);
    m_autosaveTimer->setSingleShot(true);
    m_autosaveTimer->setInterval(AutosaveDelayMs);
    connect(m_autosaveTimer, &QTimer::timeout, this, &RosterEditorWidget::autosave);
}

void RosterEditorWidget::loadClass(
    const Classroom& classroom
    )
{
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    m_classroom = classroom;
    updateHeaderText();
    m_loadingRoster = true;

    Roster roster;
    if (m_services && m_services->dataService() && m_classroom.id > 0)
    {
        roster = m_services->dataService()->loadRoster(m_classroom.id);
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
    m_loadingRoster = false;
    updateActions();
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
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    return !hasUnsavedChanges() || saveRosterInternal(true);
}

bool RosterEditorWidget::hasUnsavedChanges() const
{
    return m_widthsDirty || (m_model && m_model->isDirty());
}

void RosterEditorWidget::discardChanges()
{
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

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

void RosterEditorWidget::setTestingClassMode(
    bool testingClassMode
    )
{
    m_testingClassMode = testingClassMode;
    if (m_importButton)
    {
        m_importButton->setVisible(!m_testingClassMode);
    }
    if (m_koreanKeyboardButton)
    {
        m_koreanKeyboardButton->setVisible(!m_testingClassMode);
    }

    applyTestingClassColumnVisibility();
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
    bool showValidationMessages
    )
{
    if (!m_services || !m_services->dataService() || m_classroom.id <= 0)
    {
        return false;
    }
    if (!validateRosterBeforeSave(showValidationMessages))
    {
        return false;
    }

    m_services->dataService()->saveRoster(m_classroom.id, currentRosterForSave());
    m_model->clearDirty();
    m_widthsDirty = false;
    updateActions();
    return true;
}

bool RosterEditorWidget::validateRosterBeforeSave(
    bool showValidationMessages
    )
{
    if (!m_model || !m_model->hasDuplicateNameErrors())
    {
        return true;
    }

    if (showValidationMessages)
    {
        QMessageBox message(this);
        message.setIcon(QMessageBox::Warning);
        message.setWindowTitle(tr("Duplicate Student Names"));
        message.setText(
            tr("Resolve duplicate English/Korean student name pairs before saving.")
            );
        message.setDetailedText(
            m_model->duplicateNameErrorList().join(QLatin1Char('\n'))
            );
        message.exec();
    }
    return false;
}

void RosterEditorWidget::autosave()
{
    if (hasUnsavedChanges())
    {
        saveRosterInternal(false);
    }
}

void RosterEditorWidget::scheduleAutosave()
{
    if (
        m_loadingRoster
        || m_saveMode != SaveMode::Automatic
        || !m_autosaveTimer
        || m_classroom.id <= 0
        )
    {
        return;
    }

    m_autosaveTimer->start();
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
