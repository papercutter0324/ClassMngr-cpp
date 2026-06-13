#include "sidebar_controller.h"

#include "core/application_services.h"
#include "services/dataservice.h"

#include "models/class_info.h"
#include "models/classroom.h"
#include "models/teacher.h"

#include "ui/actions/action_registry.h"
#include "ui/pages/pagemanager.h"
#include "ui/pages/class/class_info_page.h"
#include "ui/pages/teacher/teacher_info_page.h"
#include "ui/widgets/sidebar/sidebar.h"

#include "utils/sidebar_node_naming.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
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
        new QComboBox(&dialog);

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
    auto* ds = m_services->dataService();

    const auto classes = ds->getClasses();

    m_sidebar->clearClasses();

    for (const Classroom& classroom : classes)
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

        m_sidebar->addClassNode(
            displayName,
            classroom.id
            );
    }

    updateActionStates();
}

void SidebarController::refreshTeacherSidebar()
{
    auto* ds = m_services->dataService();

    const auto teachers =
        ds->getAllTeachers();

    m_sidebar->clearTeachers();

    for (const auto& teacher : teachers)
    {
        const QString displayName =
            SidebarNodeNaming::formatTeacherDisplayName(
                teacher
                );

        m_sidebar->addTeacherNode(
            displayName,
            teacher.id
            );
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
    refreshClassSidebar();

    m_sidebar->selectClass(
        classId
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
    return m_services
        ->dataService()
        ->getClassById(classId);
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
    auto* ds = m_services->dataService();

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

    m_pages->classInfoPage()->loadClass(
        classroom
        );

    m_pages->showPage(
        PageType::ClassInfo
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

    auto* ds = m_services->dataService();

    const Classroom classroom =
        ds->getClassById(
            classId
            );

    if (!confirmDeleteClass(classroom))
    {
        return;
    }

    ds->deleteClass(
        classroom.id
        );

    refreshClassSidebar();
}

Teacher SidebarController::getTeacherById(int teacherId) const
{
    return m_services
        ->dataService()
        ->getTeacher(teacherId);
}

void SidebarController::addTeacher()
{
    auto* ds = m_services->dataService();

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

    auto* ds = m_services->dataService();

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
        m_services
            ? m_services->dataService()
            : nullptr;

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
        m_services
            ? m_services->dataService()
            : nullptr;

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
        m_services
            ? m_services->dataService()
            : nullptr;

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
    if (
        !m_actions
        || !m_services
        || !m_services->dataService()
        )
    {
        return;
    }

    auto* ds =
        m_services->dataService();

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
