#include "sub_prep_page.h"

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/fontmanager.h"
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
#include "ui/shared/widgets/on_screen_keyboard.h"

#include <algorithm>
#include <string>
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

QString campusMetadataText(
    const std::string& value
    )
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

QString campusDisplayName(
    const ClassMngr::Next::Application::SubPrepCampusMetadata& campus
    )
{
    return campusMetadataText(campus.displayName);
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
