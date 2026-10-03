#include "speaking_eval_page.h"

#include "ui/shared/widgets/text_fit_push_button.h"
#include "ui/shared/widgets/on_screen_keyboard.h"
#include "ui/shared/pages/autosave_coordinator.h"
#include "ui/shared/pages/page_header.h"

#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "core/fontmanager.h"
#include "core/utils/sidebar_node_naming.h"
#include "core/utils/student_name_utils.h"
#include "domain/models/class_info.h"
#include "domain/models/roster.h"
#include "domain/models/speaking_evaluation.h"
#include "domain/models/teacher.h"
#include "data/data_service.h"
#include "features/classes/evaluation_default_selection.h"
#include "features/speaking_eval/ui/speaking_eval_delegate.h"
#include "features/speaking_eval/ui/speaking_eval_batch_export_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_model.h"
#include "features/speaking_eval/ui/speaking_eval_report_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_header_view.h"
#include "features/classes/models/class_tab_navigation_model.h"
#include "next/application/roster_read_query.h"
#include "next/application/selected_class_subtitle_read_query.h"
#include "next/platform/application_services_roster_read_port.h"
#include "next/platform/application_services_selected_class_subtitle_read_port.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/qt_text_adapter.h"
#include "ui/shared/widgets/navigation_pill_button.h"
#include "ui/shared/widgets/navigation_pill_style.h"
#include "ui/shared/widgets/navigation_tab_widget.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QDialog>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QSet>
#include <QSizePolicy>
#include <QTableView>
#include <QTimer>
#include <QUndoStack>
#include <QVBoxLayout>

#include <string>
#include <utility>
#include <vector>

namespace
{

constexpr int DayFilterSpacer = 16;

Roster readRosterForSpeakingEvaluationPage(
    ApplicationServices* services,
    int classId
    )
{
    if (!services || classId <= 0)
    {
        return {};
    }

    const auto typedClassId =
        ClassMngr::Next::Domain::ClassId::fromString(
            std::to_string(classId)
            );
    if (!typedClassId)
    {
        return {};
    }

    const ClassMngr::Next::Application::RosterReadQuery query{
        .classId = *typedClassId
    };
    const ClassMngr::Next::Platform::
        ApplicationServicesRosterReadPort port(services);
    const ClassMngr::Next::Application::RosterReadResult loaded =
        ClassMngr::Next::Application::RosterReadUseCase::execute(
            query,
            port
            );
    if (!loaded)
    {
        return {};
    }

    const ClassMngr::Next::Application::RosterSnapshot& snapshot =
        loaded.value();
    Roster roster;
    for (const std::u16string& column : snapshot.columns)
    {
        roster.columns.append(
            Ui::QtTextAdapter::fromUtf16String(column)
            );
    }

    for (const int width : snapshot.columnWidths)
    {
        roster.columnWidths.append(width);
    }

    for (const std::vector<std::u16string>& snapshotRow : snapshot.rows)
    {
        QStringList row;
        for (const std::u16string& cell : snapshotRow)
        {
            row.append(
                Ui::QtTextAdapter::fromUtf16String(cell)
                );
        }
        roster.rows.append(std::move(row));
    }

    return roster;
}

struct DayFilterButtonDefinition
{
    QString key;
    QString objectName;
};

const QList<DayFilterButtonDefinition>& dayFilterButtonDefinitions()
{
    static const QList<DayFilterButtonDefinition> definitions{
        {QStringLiteral("Monday"), QStringLiteral("speakingEvalMondayFilterButton")},
        {QStringLiteral("Tuesday"), QStringLiteral("speakingEvalTuesdayFilterButton")},
        {QStringLiteral("Wednesday"), QStringLiteral("speakingEvalWednesdayFilterButton")},
        {QStringLiteral("Thursday"), QStringLiteral("speakingEvalThursdayFilterButton")},
        {QStringLiteral("Friday"), QStringLiteral("speakingEvalFridayFilterButton")},
        {QStringLiteral("Wkend"), QStringLiteral("speakingEvalWeekendFilterButton")}
    };

    return definitions;
}

ClassTabNavigation::ScheduleSource scheduleSourceForMode(
    ScheduleDisplayMode mode
    )
{
    return mode == ScheduleDisplayMode::Intensive
        ? ClassTabNavigation::ScheduleSource::Intensive
        : ClassTabNavigation::ScheduleSource::Regular;
}

const QStringList& evaluationNames()
{
    static const QStringList names{
        QStringLiteral("Winter"),
        QStringLiteral("Speech Contest"),
        QStringLiteral("Summer"),
        QStringLiteral("Fall")
    };

    return names;
}

QString evaluationLabel(
    const QString& evaluationName
    )
{
    if (evaluationName == QStringLiteral("Winter"))
    {
        return QObject::tr("Winter");
    }

    if (evaluationName == QStringLiteral("Speech Contest"))
    {
        return QObject::tr("Speech Contest");
    }

    if (evaluationName == QStringLiteral("Summer"))
    {
        return QObject::tr("Summer");
    }

    if (evaluationName == QStringLiteral("Fall"))
    {
        return QObject::tr("Fall");
    }

    return evaluationName;
}

QString normalizedEvaluationName(
    const QString& evaluationName
    )
{
    return evaluationNames().contains(evaluationName)
        ? evaluationName
        : evaluationNames().constFirst();
}

QString sidebarClassDisplayName(
    ApplicationServices* services,
    int classId
    )
{
    if (!services || classId <= 0)
    {
        return {};
    }

    const auto selectedClassId =
        ClassMngr::Next::Domain::ClassId::fromString(
            std::to_string(classId)
            );
    if (!selectedClassId)
    {
        return {};
    }

    ClassMngr::Next::Platform::
        ApplicationServicesSelectedClassSubtitleReadPort readPort(services);
    const ClassMngr::Next::Application::SelectedClassSubtitleReadQuery query(
        readPort
        );
    const auto loadedSubtitle = query.execute(*selectedClassId);

    ClassInfo classInfo;
    Teacher teacher;
    if (loadedSubtitle)
    {
        const auto& subtitle = loadedSubtitle.value();
        if (subtitle.classFields)
        {
            const auto& fields = subtitle.classFields.value();
            classInfo.classGrade = QString::fromStdU16String(fields.classGrade);
            classInfo.classLevel = QString::fromStdU16String(fields.classLevel);
            classInfo.classTimes.reserve(
                static_cast<qsizetype>(fields.regularSchedule.size())
                );
            for (const auto& row : fields.regularSchedule)
            {
                ClassTime time;
                time.day = QString::fromStdU16String(row.day);
                time.startTime = QString::fromStdU16String(row.startTime);
                time.endTime.clear();
                classInfo.classTimes.append(std::move(time));
            }
        }

        if (
            subtitle.assignedTeacher
            && subtitle.assignedTeacher.value().has_value()
            )
        {
            const auto& fields = subtitle.assignedTeacher.value().value();
            teacher.teacherKr = QString::fromStdU16String(fields.teacherKr);
            teacher.teacherEn = QString::fromStdU16String(fields.teacherEn);
            teacher.preferredRomanization = QString::fromStdU16String(
                fields.preferredRomanization
                );
            teacher.preferredName = QString::fromStdU16String(
                fields.preferredName
                );
        }
    }

    return SidebarNodeNaming::formatClassDisplayName(
        classInfo,
        teacher
        );
}

int findColumn(
    const QStringList& columns,
    const QString& name
    )
{
    for (int column = 0; column < columns.size(); ++column)
    {
        if (columns[column].compare(name, Qt::CaseInsensitive) == 0)
        {
            return column;
        }
    }

    return -1;
}

void clearLayout(
    QLayout* layout
    )
{
    if (!layout)
    {
        return;
    }

    while (layout->count() > 0)
    {
        QLayoutItem* item =
            layout->takeAt(0);

        if (QWidget* widget = item->widget())
        {
            widget->deleteLater();
        }

        delete item;
    }
}


} // namespace
