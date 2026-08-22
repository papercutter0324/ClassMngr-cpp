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
#include "features/classes/class_navigation_preferences.h"
#include "features/schedule/schedule_display_mode_preferences.h"
#include "features/speaking_eval/ui/speaking_eval_delegate.h"
#include "features/speaking_eval/ui/speaking_eval_batch_export_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_model.h"
#include "features/speaking_eval/ui/speaking_eval_report_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_header_view.h"
#include "features/classes/models/class_tab_navigation_model.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"
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

#include <utility>

namespace
{

constexpr int DayFilterSpacer = 16;

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
    ClassService* classService,
    TeacherService* teacherService,
    int classId
    )
{
    if (!classService || !teacherService || classId <= 0)
    {
        return {};
    }

    const ClassInfo classInfo =
        classService->classInfo(
            classId
            ).value_or(ClassInfo{});

    Teacher teacher;

    if (classInfo.teacherId > 0)
    {
        teacher = teacherService->teacher(classInfo.teacherId)
            .value_or(Teacher{});
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
