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
        new QDialogButtonBox(
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

SidebarController::SidebarController(
    ApplicationServices* services,
    Sidebar* sidebar,
    PageManager* pages,
    QObject* parent
    )
    : QObject(parent)
    , m_services(services)
    , m_sidebar(sidebar)
    , m_pages(pages)
{
}

void SidebarController::connectActions(ActionRegistry& actions)
{
    m_actions =
        &actions;

    connect(
        actions.newClass,
        &QAction::triggered,
        this,
        &SidebarController::addClass
        );

    connect(
        actions.deleteClass,
        &QAction::triggered,
        this,
        &SidebarController::deleteClass
        );

    connect(
        actions.newTeacher,
        &QAction::triggered,
        this,
        &SidebarController::addTeacher
        );

    connect(
        actions.deleteTeacher,
        &QAction::triggered,
        this,
        &SidebarController::deleteTeacher
        );

    connect(
        actions.showAllKoreanTeachers,
        &QAction::toggled,
        this,
        [this](bool)
        {
            refreshTeacherSidebar();
        }
        );

    connect(
        m_sidebar,
        &Sidebar::addClassRequested,
        this,
        &SidebarController::addClass
        );

    connect(
        m_sidebar,
        &Sidebar::deleteClassRequested,
        this,
        &SidebarController::deleteClass
        );

    connect(
        m_sidebar,
        &Sidebar::addTeacherRequested,
        this,
        &SidebarController::addTeacher
        );

    connect(
        m_sidebar,
        &Sidebar::deleteTeacherRequested,
        this,
        &SidebarController::deleteTeacher
        );

    updateActionStates();
}

void SidebarController::refreshClassSidebar()
{
    if (!m_sidebar)
    {
        return;
    }

    m_sidebar->clearClasses();

    auto* ds =
        m_services
            ? m_services->dataService()
            : nullptr;

    if (!ds || !ds->isOpen())
    {
        updateActionStates();
        return;
    }

    QList<SidebarClassNode> classNodes;

    for (const Classroom& classroom : ds->getClasses())
    {
        auto classInfo =
            ds->loadClassInfo(
                classroom.id
                );

        Teacher teacher;

        if (classInfo.teacherId > 0)
        {
            teacher =
                ds->getTeacher(
                    classInfo.teacherId
                    );
        }

        QString displayName =
            SidebarNodeNaming::formatClassDisplayName(
                classInfo,
                teacher
                );

        SidebarClassNode node;
        node.classId =
            classroom.id;
        node.classInfo =
            classInfo;
        node.displayName =
            displayName;
        node.teacherKr =
            teacher.teacherKr.trimmed().isEmpty()
                ? classInfo.teacherKr
                : teacher.teacherKr;

        classNodes.append(
            node
            );
    }

    std::sort(
        classNodes.begin(),
        classNodes.end(),
        sidebarClassNodeLessThan
        );

    for (const SidebarClassNode& node : std::as_const(classNodes))
    {
        m_sidebar->addClassNode(
            node.displayName,
            node.classId
            );
    }

    updateActionStates();
}

void SidebarController::refreshTeacherSidebar()
{
    if (!m_sidebar)
    {
        return;
    }

    m_sidebar->clearTeachers();

    auto* ds =
        m_services
            ? m_services->dataService()
            : nullptr;

    if (!ds || !ds->isOpen())
    {
        updateActionStates();
        return;
    }

    const QList<Teacher> teachers =
        ds->getAllTeachers();

    QHash<int, Teacher> teachersById;

    for (const Teacher& teacher : teachers)
    {
        if (teacher.id > 0)
        {
            teachersById.insert(
                teacher.id,
                teacher
                );
        }
    }

    QSet<int> myTeacherIds;
    QList<Teacher> myTeachers;

    for (const Classroom& classroom : ds->getClasses())
    {
        const ClassInfo classInfo =
            ds->loadClassInfo(
                classroom.id
                );

        const int teacherId =
            classInfo.teacherId;

        if (
            teacherId <= 0
            || myTeacherIds.contains(teacherId)
            || !teachersById.contains(teacherId)
            )
        {
            continue;
        }

        myTeacherIds.insert(
            teacherId
            );
        myTeachers.append(
            teachersById.value(teacherId)
            );
    }

    for (const Teacher& teacher : sortedTeachers(myTeachers))
    {
        const QString displayName =
            SidebarNodeNaming::formatTeacherDisplayName(
                teacher
                );

        m_sidebar->addTeacherNode(
            displayName,
            teacher.id,
            true
            );
    }

    const bool showAllKoreanTeachers =
        m_actions && m_actions->showAllKoreanTeachers
            ? m_actions->showAllKoreanTeachers->isChecked()
            : SettingsManager::instance().showAllKoreanTeachers();

    m_sidebar->setAllKoreanTeachersVisible(
        showAllKoreanTeachers
        );

    if (showAllKoreanTeachers)
    {
        for (const Teacher& teacher : sortedTeachers(teachers))
        {
            const QString displayName =
                SidebarNodeNaming::formatTeacherDisplayName(
                    teacher
                    );

            m_sidebar->addTeacherNode(
                displayName,
                teacher.id,
                false
                );
        }
    }

    updateActionStates();
}

void SidebarController::refreshAllSidebars()
{
    refreshTeacherSidebar();
    refreshClassSidebar();
}

void SidebarController::handleClassInfoSaved(
    int classId
    )
{
    const QStringList selectedKeys =
        m_sidebar->selectedKeys();
    const int selectedClassId =
        m_sidebar->getSelectedClassId();

    refreshClassSidebar();

    m_sidebar->selectByKeys(
        selectedKeys,
        selectedClassId > 0
            ? selectedClassId
            : classId
        );
}

void SidebarController::handleTeacherSaved(
    int teacherId
    )
{
    refreshTeacherSidebar();
    refreshClassSidebar();

    m_sidebar->selectTeacher(
        teacherId
        );
}

Classroom SidebarController::getClassById(int classId) const
{
    auto* ds =
        openDataService(m_services);

    return ds
        ? ds->getClassById(classId)
        : Classroom();
}

Classroom SidebarController::getSelectedClass() const
{
    const int classId =
        m_sidebar->getSelectedClassId();

    if (classId <= 0)
    {
        return {};
    }

    return getClassById(classId);
}

void SidebarController::addClass()
{
    auto* ds =
        openDataService(m_services);

    if (!ds)
    {
        return;
    }

    if (!m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    int classId =
        ds->createClass("");

    refreshClassSidebar();

    Classroom classroom =
        ds->getClassById(
            classId
            );

    if (classroom.id == 0)
    {
        return;
    }

    m_pages->classesPage()->openClass(
        classroom.id,
        ClassesSection::Details
        );

    m_pages->showPage(
        PageType::Classes
        );

    m_sidebar->selectByKeys(
        {QStringLiteral("classes")}
        );
}

void SidebarController::deleteClass()
{
    int classId =
        m_sidebar->getSelectedClassId();

    if (classId <= 0)
    {
        classId =
            promptForClassToDelete();

        if (classId <= 0)
        {
            return;
        }
    }

    auto* ds =
        openDataService(m_services);

    if (!ds)
    {
        return;
    }

    const Classroom classroom =
        ds->getClassById(
            classId
            );

    if (!confirmDeleteClass(classroom))
    {
        return;
    }

    if (!m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    const QStringList selectedKeys =
        m_sidebar->selectedKeys();
    const int selectedClassId =
        m_sidebar->getSelectedClassId();

    ds->deleteClass(
        classroom.id
        );

    refreshClassSidebar();

    m_pages->classesPage()->loadClasses();

    if (selectedClassId == classroom.id)
    {
        m_sidebar->selectByKeys(
            {QStringLiteral("classes")}
            );
    }
    else
    {
        m_sidebar->selectByKeys(
            selectedKeys,
            selectedClassId
            );
    }
}

Teacher SidebarController::getTeacherById(int teacherId) const
{
    auto* ds =
        openDataService(m_services);

    return ds
        ? ds->getTeacher(teacherId)
        : Teacher();
}

void SidebarController::addTeacher()
{
    auto* ds =
        openDataService(m_services);

    if (!ds)
    {
        return;
    }

    Teacher newTeacher;

    int teacherId =
        ds->createTeacher(
            newTeacher
            );

    refreshTeacherSidebar();

    Teacher teacher =
        ds->getTeacher(
            teacherId
            );

    if (teacher.id == 0)
    {
        return;
    }

    m_sidebar->selectTeacher(
        teacherId
        );

    m_pages->teacherPage()->loadTeacher(
        teacher
        );

    m_pages->showPage(
        PageType::TeacherInfo
        );
}

void SidebarController::deleteTeacher()
{
    int teacherId =
        m_sidebar->getSelectedTeacherId();

    if (teacherId <= 0)
    {
        teacherId =
            promptForTeacherToDelete();

        if (teacherId <= 0)
        {
            return;
        }
    }

    auto* ds =
        openDataService(m_services);

    if (!ds)
    {
        return;
    }

    const Teacher teacher =
        ds->getTeacher(
            teacherId
            );

    if (teacher.id <= 0)
    {
        return;
    }

    if (!confirmDeleteTeacher(teacher))
    {
        return;
    }

    ds->deleteTeacher(
        teacher.id
        );

    refreshTeacherSidebar();
    refreshClassSidebar();
}

int SidebarController::promptForClassToDelete() const
{
    auto* ds =
        openDataService(m_services);

    if (!ds)
    {
        return -1;
    }

    QList<QPair<QString, int>> records;

    for (const Classroom& classroom : ds->getClasses())
    {
        if (classroom.id <= 0)
        {
            continue;
        }

        records.append(
            {
                classDisplayName(classroom),
                classroom.id
            }
            );
    }

    if (records.isEmpty())
    {
        return -1;
    }

    return chooseRecord(
        m_sidebar,
        tr("Delete Class"),
        tr("Which class would you like to delete?"),
        records
        );
}

int SidebarController::promptForTeacherToDelete() const
{
    auto* ds =
        openDataService(m_services);

    if (!ds)
    {
        return -1;
    }

    QList<QPair<QString, int>> records;

    for (const Teacher& teacher : ds->getAllTeachers())
    {
        if (teacher.id <= 0)
        {
            continue;
        }

        records.append(
            {
                SidebarNodeNaming::formatTeacherDisplayName(
                    teacher
                    ),
                teacher.id
            }
            );
    }

    if (records.isEmpty())
    {
        return -1;
    }

    return chooseRecord(
        m_sidebar,
        tr("Delete Teacher"),
        tr("Which teacher would you like to delete?"),
        records
        );
}

QString SidebarController::classDisplayName(
    const Classroom& classroom
    ) const
{
    auto* ds =
        openDataService(m_services);

    if (!ds)
    {
        return classroom.name.trimmed().isEmpty()
            ? tr("Class %1").arg(classroom.id)
            : classroom.name.trimmed();
    }

    const ClassInfo classInfo =
        ds->loadClassInfo(
            classroom.id
            );

    Teacher teacher;

    if (classInfo.teacherId > 0)
    {
        teacher =
            ds->getTeacher(
                classInfo.teacherId
                );
    }

    QString displayName =
        SidebarNodeNaming::formatClassDisplayName(
            classInfo,
            teacher
            )
            .trimmed();

    if (displayName.isEmpty())
    {
        displayName =
            classroom.name.trimmed();
    }

    if (displayName.isEmpty())
    {
        displayName =
            tr("Class %1")
                .arg(classroom.id);
    }

    return displayName;
}

bool SidebarController::confirmDeleteClass(
    const Classroom& classroom
    ) const
{
    const auto result =
        QMessageBox::question(
            m_sidebar,
            tr("Delete Class"),
            tr("Delete '%1'?")
                .arg(classDisplayName(classroom))
            );

    return result == QMessageBox::Yes;
}

bool SidebarController::confirmDeleteTeacher(
    const Teacher& teacher
    ) const
{
    const QString displayName =
        SidebarNodeNaming::formatTeacherDisplayName(
            teacher
            );

    const auto result =
        QMessageBox::question(
            m_sidebar,
            tr("Delete Teacher"),
            tr("Delete '%1'?")
                .arg(displayName)
            );

    return result == QMessageBox::Yes;
}

void SidebarController::updateActionStates()
{
    if (!m_actions)
    {
        return;
    }

    auto* ds =
        m_services
            ? m_services->dataService()
            : nullptr;

    if (!ds || !ds->isOpen())
    {
        if (m_actions->deleteClass)
        {
            m_actions->deleteClass->setEnabled(false);
        }

        if (m_actions->deleteTeacher)
        {
            m_actions->deleteTeacher->setEnabled(false);
        }

        return;
    }

    if (m_actions->deleteClass)
    {
        m_actions->deleteClass->setEnabled(
            !ds->getClasses().isEmpty()
            );
    }

    if (m_actions->deleteTeacher)
    {
        m_actions->deleteTeacher->setEnabled(
            !ds->getAllTeachers().isEmpty()
            );
    }
}
