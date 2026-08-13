#include "sub_prep_page.h"

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/resource_paths.h"
#include "features/campus/data/campus_json_repository.h"
#include "features/classes/models/class_tab_navigation_model.h"
#include "features/sub_prep/ui/sub_prep_class_information_model.h"
#include "features/sub_prep/ui/sub_prep_print_dialog.h"
#include "features/sub_prep/services/sub_prep_package_service.h"
#include "features/sub_prep/services/sub_prep_print_service.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/utils/widget_sizing.h"
#include "ui/shared/widgets/sectioncards/class_info_section_card.h"
#include "features/schedule/ui/schedule_widget.h"
#include "ui/shared/widgets/text_fit_push_button.h"
#include "ui/shared/widgets/navigation_tab_widget.h"

#include <algorithm>
#include <utility>

#include <QEvent>
#include <QDate>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTextEdit>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>
#include <QtAssert>

namespace
{
constexpr int AutosaveDelayMs = 750;
constexpr int OfficeNumberFieldWidth = 115;
constexpr int CompactFieldWidth = 170;
constexpr int TextEditVerticalPadding = 24;
constexpr int ClassNotesLines = 4;
constexpr int TeacherNotesLines = 4;

const QString NotAvailableText =
    QStringLiteral("N/A");

namespace SettingsKeys
{
const QString MyInfoCampus =
    QStringLiteral("myInfo/campus");
const QString MyInfoZoomLoginId =
    QStringLiteral("myInfo/zoomLoginId");
const QString MyInfoZoomPassword =
    QStringLiteral("myInfo/zoomPassword");
const QString MyInfoZoomNotAvailable =
    QStringLiteral("myInfo/zoomNotAvailable");
const QString LegacyZoomLoginId =
    QStringLiteral("subPrep/personalZoomEmail");
const QString LegacyZoomPassword =
    QStringLiteral("subPrep/personalZoomPassword");
const QString LegacyZoomNotAvailable =
    QStringLiteral("subPrep/personalZoomNotAvailable");
const QString ClassMaterials =
    QStringLiteral("subPrep/classMaterials");
const QString BookReportGrading =
    QStringLiteral("subPrep/bookReportGrading");
const QString BookReportSpecialInstructions =
    QStringLiteral("subPrep/bookReportSpecialInstructions");
const QString SubNotes =
    QStringLiteral("subPrep/subComments");
}

SettingsService* openSettingsService(
    ApplicationServices* services
    )
{
    auto* service =
        services
            ? services->settingsService()
            : nullptr;

    return service && service->isAvailable()
        ? service
        : nullptr;
}

ClassService* openClassService(
    ApplicationServices* services
    )
{
    auto* service =
        services
            ? services->classService()
            : nullptr;

    return service && service->isAvailable()
        ? service
        : nullptr;
}

TeacherService* openTeacherService(
    ApplicationServices* services
    )
{
    auto* service =
        services
            ? services->teacherService()
            : nullptr;

    return service && service->isAvailable()
        ? service
        : nullptr;
}

RosterService* openRosterService(
    ApplicationServices* services
    )
{
    auto* service =
        services
            ? services->rosterService()
            : nullptr;

    return service && service->isAvailable()
        ? service
        : nullptr;
}

CalendarService* openCalendarService(
    ApplicationServices* services
    )
{
    auto* service =
        services
            ? services->calendarService()
            : nullptr;

    return service && service->isAvailable()
        ? service
        : nullptr;
}

CampusJsonRepository campusRepository()
{
    return CampusJsonRepository(
        ResourcePaths::Campuses::directory()
        );
}

QString campusDisplayName(
    const CampusInfo& campus
    )
{
    return campus.campusName.trimmed().isEmpty()
        ? campus.id.trimmed()
        : campus.campusName.trimmed();
}

QString valueOrNa(
    const QString& value
    )
{
    const QString trimmed =
        value.trimmed();

    return trimmed.isEmpty()
        ? NotAvailableText
        : trimmed;
}

QVariant loadSettingWithLegacyFallback(
    SettingsService* settingsService,
    const QString& primaryKey,
    const QString& legacyKey,
    const QVariant& defaultValue
    )
{
    if (!settingsService)
    {
        return defaultValue;
    }

    QVariant value =
        settingsService->load(
            primaryKey,
            QVariant()
            );

    if (value.isValid())
    {
        return value;
    }

    value =
        settingsService->load(
            legacyKey,
            QVariant()
            );

    if (!value.isValid())
    {
        return defaultValue;
    }

    settingsService->save(
        primaryKey,
        value
        );

    return value;
}

int textEditHeightForLines(
    const QTextEdit* edit,
    int lines
    )
{
    if (!edit)
    {
        return 0;
    }

    return edit->fontMetrics().lineSpacing() * lines
        + TextEditVerticalPadding;
}

void clearLayout(
    QLayout* layout
    )
{
    if (!layout)
    {
        return;
    }

    while (QLayoutItem* item = layout->takeAt(0))
    {
        if (auto* childLayout = item->layout())
        {
            clearLayout(childLayout);
            delete childLayout;
        }

        if (auto* widget = item->widget())
        {
            widget->deleteLater();
        }

        delete item;
    }
}

QLabel* createInlineValue(
    const QString& label,
    const QString& value,
    QWidget* parent
    )
{
    auto* field =
        new QLabel(
            QStringLiteral("%1: %2")
                .arg(
                    label,
                    valueOrNa(value)
                    ),
            parent
            );

    field->setTextInteractionFlags(
        Qt::TextSelectableByMouse
        | Qt::TextSelectableByKeyboard
        );
    field->setWordWrap(true);
    field->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );

    return field;
}

ScheduleViewModel scheduleForDays(
    const ScheduleViewModel& source,
    const QStringList& selectedDays
    )
{
    ScheduleViewModel result = source;
    result.days.clear();
    result.rows.clear();

    for (const QString& day : source.days)
    {
        if (selectedDays.contains(day))
        {
            result.days.append(day);
        }
    }

    if (result.days.isEmpty())
    {
        return result;
    }

    for (const ScheduleRowView& sourceRow : source.rows)
    {
        ScheduleRowView row = sourceRow;
        row.cells.clear();

        for (const ScheduleCellView& cell : sourceRow.cells)
        {
            if (result.days.contains(cell.day))
            {
                row.cells.append(cell);
            }
        }

        result.rows.append(row);
    }

    return result;
}

QSet<int> visibleClassIds(
    const ScheduleViewModel& schedule
    )
{
    QSet<int> classIds;

    for (const ScheduleRowView& row : schedule.rows)
    {
        for (const ScheduleCellView& cell : row.cells)
        {
            for (const ScheduleEntry& entry : cell.entries)
            {
                if (entry.classId > 0)
                {
                    classIds.insert(entry.classId);
                }
            }
        }
    }

    return classIds;
}
}
