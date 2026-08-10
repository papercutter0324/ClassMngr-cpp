#include "sidebar_controller.h"

#include "core/application_services.h"
#include "core/settingsmanager.h"
#include "data/data_service.h"

#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/teacher.h"

#include "ui/shared/actions/action_registry.h"
#include "ui/shared/pages/pagemanager.h"
#include "features/classes/config/class_info_config.h"
#include "features/classes/ui/classes_page.h"
#include "features/teacher/ui/teacher_info_page.h"
#include "ui/shared/widgets/sidebar/sidebar.h"
#include "ui/shared/widgets/text_fit_dialog_button_box.h"

#include "core/utils/sidebar_node_naming.h"

#include <algorithm>

#include <QComboBox>
#include "ui/shared/widgets/no_wheel_combobox.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QHash>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QTime>
#include <QVBoxLayout>

namespace
{
constexpr int UnknownSidebarOrder = 1'000'000;

struct SidebarClassNode
{
    int classId = -1;
    ClassInfo classInfo;
    QString displayName;
    QString teacherKr;
};

DataService* openDataService(
    ApplicationServices* services
    )
{
    auto* dataService =
        services
            ? services->dataService()
            : nullptr;

    return dataService && dataService->isOpen()
        ? dataService
        : nullptr;
}

int gradeOrder(
    const QString& grade
    )
{
    const int index =
        ClassInfoConfig::Grades.indexOf(
            grade.trimmed()
            );

    return index >= 0
        ? index
        : UnknownSidebarOrder;
}

int levelOrder(
    const ClassInfo& classInfo
    )
{
    const QStringList levels =
        ClassInfoConfig::levelsForGrade(
            classInfo.classGrade.trimmed()
            );

    const int index =
        levels.indexOf(
            classInfo.classLevel.trimmed()
            );

    return index >= 0
        ? index
        : UnknownSidebarOrder;
}

QString dayCode(
    const QString& day
    )
{
    if (day == QStringLiteral("Monday"))
    {
        return QStringLiteral("Mon");
    }
    if (day == QStringLiteral("Tuesday"))
    {
        return QStringLiteral("Tues");
    }
    if (day == QStringLiteral("Wednesday"))
    {
        return QStringLiteral("Wed");
    }
    if (day == QStringLiteral("Thursday"))
    {
        return QStringLiteral("Thurs");
    }
    if (day == QStringLiteral("Friday"))
    {
        return QStringLiteral("Fri");
    }
    if (day == QStringLiteral("Saturday"))
    {
        return QStringLiteral("Sat");
    }
    if (day == QStringLiteral("Sunday"))
    {
        return QStringLiteral("Sun");
    }

    return day.trimmed();
}

int dayOrder(
    const QString& day
    )
{
    const int index =
        ClassInfoConfig::Days.indexOf(
            day.trimmed()
            );

    return index >= 0
        ? index
        : UnknownSidebarOrder;
}

QString dayPattern(
    QStringList days
    )
{
    days.removeDuplicates();

    std::sort(
        days.begin(),
        days.end(),
        [](const QString& left, const QString& right)
        {
            return dayOrder(left) < dayOrder(right);
        }
        );

    QStringList codes;

    for (const QString& day : std::as_const(days))
    {
        const QString code =
            dayCode(day);

        if (!code.isEmpty())
        {
            codes.append(
                code
                );
        }
    }

    if (codes == QStringList{QStringLiteral("Mon"), QStringLiteral("Wed")})
    {
        return QStringLiteral("M/W");
    }
    if (codes == QStringList{QStringLiteral("Mon"), QStringLiteral("Fri")})
    {
        return QStringLiteral("M/F");
    }
    if (codes == QStringList{QStringLiteral("Wed"), QStringLiteral("Fri")})
    {
        return QStringLiteral("W/F");
    }
    if (
        codes == QStringList{
            QStringLiteral("Mon"),
            QStringLiteral("Wed"),
            QStringLiteral("Fri")
        }
        )
    {
        return QStringLiteral("M/W/F");
    }
    if (codes == QStringList{QStringLiteral("Tues"), QStringLiteral("Thurs")})
    {
        return QStringLiteral("T/Th");
    }

    return codes.join(
        QStringLiteral("/")
        );
}

int dayPatternOrder(
    const QString& pattern
    )
{
    static const QStringList patterns{
        QStringLiteral("M/W"),
        QStringLiteral("M/F"),
        QStringLiteral("W/F"),
        QStringLiteral("M/W/F"),
        QStringLiteral("T/Th"),
        QStringLiteral("Mon"),
        QStringLiteral("Tues"),
        QStringLiteral("Wed"),
        QStringLiteral("Thurs"),
        QStringLiteral("Fri")
    };

    const int index =
        patterns.indexOf(
            pattern
            );

    return index >= 0
        ? index
        : UnknownSidebarOrder;
}

int timeOrder(
    const QString& value
    )
{
    const QString trimmed =
        value.trimmed();

    if (trimmed.isEmpty())
    {
        return UnknownSidebarOrder;
    }

    const QStringList formats{
        QStringLiteral("h:mm AP"),
        QStringLiteral("h:mmAP"),
        QStringLiteral("hh:mm AP"),
        QStringLiteral("hh:mmAP"),
        QStringLiteral("H:mm"),
        QStringLiteral("HH:mm"),
        QStringLiteral("H:mm:ss"),
        QStringLiteral("HH:mm:ss")
    };

    for (const QString& format : formats)
    {
        const QTime time =
            QTime::fromString(
                trimmed,
                format
                );

        if (time.isValid())
        {
            return (time.hour() * 60) + time.minute();
        }
    }

    return UnknownSidebarOrder;
}

const QList<ClassTime>& sidebarScheduleTimes(
    const ClassInfo& classInfo
    )
{
    return classInfo.classTimes.isEmpty()
        ? classInfo.intensiveTimes
        : classInfo.classTimes;
}

int scheduleDayPatternOrder(
    const ClassInfo& classInfo
    )
{
    QStringList days;

    for (const ClassTime& time : sidebarScheduleTimes(classInfo))
    {
        days.append(
            time.day.trimmed()
            );
    }

    return dayPatternOrder(
        dayPattern(days)
        );
}

int scheduleTimeOrder(
    const ClassInfo& classInfo
    )
{
    int result =
        UnknownSidebarOrder;

    for (const ClassTime& time : sidebarScheduleTimes(classInfo))
    {
        result =
            std::min(
                result,
                timeOrder(time.startTime)
                );
    }

    return result;
}

int compareTeacherKr(
    const SidebarClassNode& left,
    const SidebarClassNode& right
    )
{
    const bool leftHasTeacher =
        !left.teacherKr.trimmed().isEmpty();
    const bool rightHasTeacher =
        !right.teacherKr.trimmed().isEmpty();

    if (leftHasTeacher != rightHasTeacher)
    {
        return leftHasTeacher
            ? -1
            : 1;
    }

    return QString::localeAwareCompare(
        left.teacherKr.trimmed(),
        right.teacherKr.trimmed()
        );
}

bool sidebarClassNodeLessThan(
    const SidebarClassNode& left,
    const SidebarClassNode& right
    )
{
    const int leftGradeOrder =
        gradeOrder(
            left.classInfo.classGrade
            );
    const int rightGradeOrder =
        gradeOrder(
            right.classInfo.classGrade
            );

    if (leftGradeOrder != rightGradeOrder)
    {
        return leftGradeOrder < rightGradeOrder;
    }

    const int teacherComparison =
        compareTeacherKr(
            left,
            right
            );

    if (teacherComparison != 0)
    {
        return teacherComparison < 0;
    }

    const int leftScheduleDayOrder =
        scheduleDayPatternOrder(
            left.classInfo
            );
    const int rightScheduleDayOrder =
        scheduleDayPatternOrder(
            right.classInfo
            );

    if (leftScheduleDayOrder != rightScheduleDayOrder)
    {
        return leftScheduleDayOrder < rightScheduleDayOrder;
    }

    const int leftScheduleTimeOrder =
        scheduleTimeOrder(
            left.classInfo
            );
    const int rightScheduleTimeOrder =
        scheduleTimeOrder(
            right.classInfo
            );

    if (leftScheduleTimeOrder != rightScheduleTimeOrder)
    {
        return leftScheduleTimeOrder < rightScheduleTimeOrder;
    }

    const int leftLevelOrder =
        levelOrder(
            left.classInfo
            );
    const int rightLevelOrder =
        levelOrder(
            right.classInfo
            );

    if (leftLevelOrder != rightLevelOrder)
    {
        return leftLevelOrder < rightLevelOrder;
    }

    const int displayComparison =
        QString::localeAwareCompare(
            left.displayName,
            right.displayName
            );

    if (displayComparison != 0)
    {
        return displayComparison < 0;
    }

    return left.classId < right.classId;
}

bool teacherSidebarLessThan(
    const Teacher& left,
    const Teacher& right
    )
{
    return SidebarNodeNaming::teacherDisplayLessThan(
        left,
        right
        );
}

QList<Teacher> sortedTeachers(
    QList<Teacher> teachers
    )
{
    teachers.erase(
        std::remove_if(
            teachers.begin(),
            teachers.end(),
            [](const Teacher& teacher)
            {
                return teacher.id <= 0;
            }
            ),
        teachers.end()
        );

    std::sort(
        teachers.begin(),
        teachers.end(),
        teacherSidebarLessThan
        );

    return teachers;
}

int chooseRecord(
    QWidget* parent,
    const QString& title,
    const QString& prompt,
    const QList<QPair<QString, int>>& records
    )
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);

    auto* layout =
        new QVBoxLayout(&dialog);

    auto* label =
        new QLabel(
            prompt,
            &dialog
            );

    auto* combo =
        new NoWheelComboBox(&dialog);

    combo->addItem(
        QString(),
        -1
        );

    for (const auto& record : records)
    {
        combo->addItem(
            record.first,
            record.second
            );
    }

    auto* buttons =
        new TextFitDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            &dialog
            );

    auto* okButton =
        buttons->button(
            QDialogButtonBox::Ok
            );

    if (okButton)
    {
        okButton->setEnabled(false);
    }

    QObject::connect(
        combo,
        &QComboBox::currentIndexChanged,
        &dialog,
        [combo, okButton]
        {
            if (!okButton)
            {
                return;
            }

            okButton->setEnabled(
                combo->currentData().toInt() > 0
                );
        }
        );

    QObject::connect(
        buttons,
        &QDialogButtonBox::accepted,
        &dialog,
        &QDialog::accept
        );

    QObject::connect(
        buttons,
        &QDialogButtonBox::rejected,
        &dialog,
        &QDialog::reject
        );

    layout->addWidget(label);
    layout->addWidget(combo);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
    {
        return -1;
    }

    return combo->currentData().toInt();
}
}

