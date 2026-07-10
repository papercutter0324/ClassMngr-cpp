#include "roster_page.h"

#include "core/application_services.h"
#include "data/data_service.h"
#include "features/roster/ui/roster_column_layout_controller.h"
#include "features/roster/ui/roster_constants.h"
#include "features/roster/ui/roster_model.h"
#include "features/roster/ui/roster_table_view.h"
#include "ui/shared/styles/roles.h"

#include <QMessageBox>
#include <QTimer>

namespace
{

inline constexpr int AutosaveDelayMs = 750;

} // namespace

RosterPage::RosterPage(
    ApplicationServices* services,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
{
    setProperty("role", UiRoles::RosterPage);
    buildUi();

    m_autosaveTimer = new QTimer(this);
    m_autosaveTimer->setSingleShot(true);
    m_autosaveTimer->setInterval(AutosaveDelayMs);
    connect(m_autosaveTimer, &QTimer::timeout, this, &RosterPage::autosave);
}

void RosterPage::loadClass(
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

    for (int row = 0; row < m_model->rowCount(); ++row)
    {
        m_table->setRowHeight(row, RosterUi::RowHeight);
    }

    m_model->clearDirty();
    m_widthsDirty = false;
    m_loadingRoster = false;
    updateActions();
}

void RosterPage::saveData()
{
    saveRosterInternal(true);
}

bool RosterPage::saveChanges()
{
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    return !hasUnsavedChanges() || saveRosterInternal(true);
}

bool RosterPage::hasUnsavedChanges() const
{
    return m_widthsDirty || (m_model && m_model->isDirty());
}

void RosterPage::discardChanges()
{
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    loadClass(m_classroom);
}

QString RosterPage::unsavedChangesTitle() const
{
    return tr("Unsaved Roster Changes");
}

QString RosterPage::unsavedChangesMessage() const
{
    return tr("This roster has unsaved changes.");
}

void RosterPage::setSaveMode(
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

bool RosterPage::saveRosterInternal(
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

bool RosterPage::validateRosterBeforeSave(
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

void RosterPage::autosave()
{
    if (hasUnsavedChanges())
    {
        saveRosterInternal(false);
    }
}

void RosterPage::scheduleAutosave()
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

Roster RosterPage::currentRosterForSave() const
{
    Roster roster = m_model ? m_model->toRoster() : Roster();
    roster.columnWidths = m_layoutController
        ? m_layoutController->currentWidths()
        : QVector<int>();
    return roster;
}

QVector<int> RosterPage::normalizedColumnWidths(
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
