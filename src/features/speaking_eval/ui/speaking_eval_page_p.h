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
#include "next/application/evaluation_default_selection.h"
#include "next/application/speaking_evaluation_roster_names_read_query.h"
#include "next/application/selected_class_subtitle_read_query.h"
#include "next/platform/application_services_speaking_evaluation_roster_names_read_port.h"
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

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{

constexpr int DayFilterSpacer = 16;

std::optional<ClassMngr::Next::Application::
    SpeakingEvaluationRosterNamesReadSnapshot>
readRosterNamesForSpeakingEvaluationPage(
    ApplicationServices* services,
    int classId
    )
{
    if (!services || classId <= 0)
    {
        return std::nullopt;
    }

    const auto typedClassId =
        ClassMngr::Next::Domain::ClassId::fromString(
            std::to_string(classId)
            );
    if (!typedClassId)
    {
        return std::nullopt;
    }

    const ClassMngr::Next::Application::
        SpeakingEvaluationRosterNamesReadRequest query{
        .classId = *typedClassId
    };
    const ClassMngr::Next::Platform::
        ApplicationServicesSpeakingEvaluationRosterNamesReadPort port(services);
    ClassMngr::Next::Application::
        SpeakingEvaluationRosterNamesReadResult loaded =
        ClassMngr::Next::Application::
            SpeakingEvaluationRosterNamesReadQuery::execute(
            query,
            port
            );
    if (!loaded)
    {
        return std::nullopt;
    }

    return std::move(loaded.value());
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

ClassInfo readReportClassInfoForSpeakingEvaluationPage(
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
    if (!loadedSubtitle || !loadedSubtitle.value().classFields)
    {
        return {};
    }

    const auto& subtitle = loadedSubtitle.value();
    const auto& fields = subtitle.classFields.value();
    ClassInfo classInfo;
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

    if (subtitle.assignedTeacher)
    {
        const auto& assignedTeacher = subtitle.assignedTeacher.value();
        if (assignedTeacher)
        {
            classInfo.teacherKr = QString::fromStdU16String(
                assignedTeacher->teacherKr
                );
            classInfo.teacherEn = QString::fromStdU16String(
                assignedTeacher->teacherEn
                );
        }
    }

    return classInfo;
}

const QStringList& evaluationNames()
{
    static const QStringList names = []
    {
        QStringList canonicalNames;
        canonicalNames.reserve(static_cast<qsizetype>(
            ClassMngr::Next::Application::kStoredEvaluationNames.size()
            ));
        for (const std::u16string_view storedName :
             ClassMngr::Next::Application::kStoredEvaluationNames)
        {
            canonicalNames.append(
                QString::fromStdU16String(std::u16string(storedName))
                );
        }
        return canonicalNames;
    }();

    return names;
}

QString evaluationLabel(
    const QString& evaluationName
    )
{
    const QStringList& storedNames = evaluationNames();
    if (evaluationName == storedNames.at(0))
    {
        return QObject::tr("Winter");
    }

    if (evaluationName == storedNames.at(1))
    {
        return QObject::tr("Speech Contest");
    }

    if (evaluationName == storedNames.at(2))
    {
        return QObject::tr("Summer");
    }

    if (evaluationName == storedNames.at(3))
    {
        return QObject::tr("Fall");
    }

    return evaluationName;
}

QString normalizedEvaluationName(
    const QString& evaluationName
    )
{
    const std::u16string storedName = evaluationName.toStdU16String();
    const std::u16string_view normalizedName =
        ClassMngr::Next::Application::normalizeStoredEvaluationName(
            storedName
            );
    return QString::fromStdU16String(std::u16string(normalizedName));
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
