#include "sidebar_controller_p.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "app/services/feature_services.h"
#include "next/application/classes_list_read_query.h"
#include "next/application/initial_setup_teacher_choices_read_query.h"
#include "next/application/selected_class_subtitle_batch_read_query.h"
#include "next/application/selected_class_subtitle_read_query.h"
#include "next/domain/domain_types.h"
#include "next/platform/application_services_classes_list_read_port.h"
#include "next/platform/application_services_initial_setup_teacher_choices_read_port.h"
#include "next/platform/application_services_selected_class_subtitle_batch_read_port.h"
#include "next/platform/application_services_selected_class_subtitle_read_port.h"

#include <QCoreApplication>

#include <charconv>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace SidebarControllerPrivate;

namespace
{
QString classesListErrorMessage(const std::string& message)
{
    return QString::fromUtf8(
        message.data(),
        static_cast<qsizetype>(message.size())
        );
}

QString teacherChoicesErrorMessage(const std::string& message)
{
    return QString::fromUtf8(
        message.data(),
        static_cast<qsizetype>(message.size())
        );
}

QString classNumberDisplayName(const int classId)
{
    return QCoreApplication::translate(
        "SidebarController",
        "Class %1"
        ).arg(classId);
}

QString storedClassDisplayName(const Classroom& classroom)
{
    const QString name = classroom.name.trimmed();
    return name.isEmpty()
        ? classNumberDisplayName(classroom.id)
        : name;
}

QString formattedClassDisplayName(
    const Classroom& classroom,
    const ClassMngr::Next::Application::SelectedClassSubtitleReadSnapshot*
        subtitle
    )
{
    ClassInfo classInfo;
    Teacher teacher;

    if (subtitle && subtitle->classFields)
    {
        const auto& fields = subtitle->classFields.value();
        classInfo.classGrade = QString::fromStdU16String(
            fields.classGrade
            );
        classInfo.classLevel = QString::fromStdU16String(
            fields.classLevel
            );
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

        if (subtitle->assignedTeacher && subtitle->assignedTeacher.value())
        {
            const auto& teacherFields =
                subtitle->assignedTeacher.value().value();
            teacher.teacherKr = QString::fromStdU16String(
                teacherFields.teacherKr
                );
            teacher.teacherEn = QString::fromStdU16String(
                teacherFields.teacherEn
                );
            teacher.preferredRomanization = QString::fromStdU16String(
                teacherFields.preferredRomanization
                );
            teacher.preferredName = QString::fromStdU16String(
                teacherFields.preferredName
                );
        }
    }

    QString displayName =
        SidebarNodeNaming::formatClassDisplayName(
            classInfo,
            teacher
            )
            .trimmed();

    if (displayName.isEmpty())
    {
        displayName = classroom.name.trimmed();
    }

    if (displayName.isEmpty())
    {
        displayName = classNumberDisplayName(classroom.id);
    }

    return displayName;
}
}

int SidebarController::promptForClassToDelete() const
{
    auto* classes =
        openClassService(m_services);

    if (!classes)
    {
        return -1;
    }

    auto* teachers =
        openTeacherService(m_services);
    const bool canReadSubtitles = classes && teachers;

    QList<Classroom> classrooms;
    std::vector<ClassMngr::Next::Domain::ClassId> classIds;
    ClassMngr::Next::Platform::
        ApplicationServicesClassesListReadPort readPort(m_services);
    const ClassMngr::Next::Application::ClassesListReadQuery query(readPort);
    const auto loadedClasses = query.execute();
    if (!loadedClasses)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Delete Class"),
            tr("Classes could not be loaded."),
            classesListErrorMessage(loadedClasses.error().message)
            );
        return -1;
    }

    for (const auto& entry : loadedClasses.value().classes)
    {
        int classId = 0;
        const std::string& classIdValue = entry.classId.value();
        const auto [end, conversionError] = std::from_chars(
            classIdValue.data(),
            classIdValue.data() + classIdValue.size(),
            classId
            );
        if (conversionError != std::errc{}
            || end != classIdValue.data() + classIdValue.size()
            || classId <= 0
            || std::to_string(classId) != classIdValue)
        {
            DialogServices::showWarning(
                m_sidebar,
                tr("Delete Class"),
                tr("Classes could not be loaded."),
                classesListErrorMessage(
                    "The classes list contains an invalid class identifier."
                    )
                );
            return -1;
        }

        Classroom classroom;
        classroom.id = classId;
        classroom.name = QString::fromStdU16String(entry.className);
        if (classroom.id <= 0)
        {
            continue;
        }

        const auto typedClassId =
            ClassMngr::Next::Domain::ClassId::fromString(classIdValue);
        if (!typedClassId)
        {
            DialogServices::showWarning(
                m_sidebar,
                tr("Delete Class"),
                tr("Classes could not be loaded."),
                classesListErrorMessage(
                    "The classes list contains an invalid class identifier."
                    )
                );
            return -1;
        }

        classIds.push_back(*typedClassId);
        classrooms.append(std::move(classroom));
    }

    if (classrooms.isEmpty())
    {
        return -1;
    }

    QList<QPair<QString, int>> records;
    if (!canReadSubtitles)
    {
        for (const Classroom& classroom : classrooms)
        {
            records.append(
                {
                    storedClassDisplayName(classroom),
                    classroom.id
                }
                );
        }
    }
    else
    {
        std::vector<
            ClassMngr::Next::Application::SelectedClassSubtitleReadSnapshot
            > subtitles;
        if (!classIds.empty())
        {
            ClassMngr::Next::Platform::
                ApplicationServicesSelectedClassSubtitleBatchReadPort
                    subtitleReadPort(m_services);
            const ClassMngr::Next::Application::
                SelectedClassSubtitleBatchReadQuery subtitleQuery(
                    subtitleReadPort
                    );
            const auto loadedSubtitles = subtitleQuery.execute(classIds);
            if (loadedSubtitles)
            {
                subtitles = std::move(loadedSubtitles.value());
            }
        }

        for (std::size_t index = 0; index < classrooms.size(); ++index)
        {
            const Classroom& classroom = classrooms.at(
                static_cast<qsizetype>(index)
                );
            const auto* subtitle = index < subtitles.size()
                ? &subtitles[index]
                : nullptr;
            records.append(
                {
                    formattedClassDisplayName(classroom, subtitle),
                    classroom.id
                }
                );
        }
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
    auto* teachers =
        openTeacherService(m_services);

    if (!teachers)
    {
        return -1;
    }

    QList<QPair<QString, int>> records;
    ClassMngr::Next::Platform::
        ApplicationServicesInitialSetupTeacherChoicesReadPort readPort(
            m_services
            );
    const ClassMngr::Next::Application::
        InitialSetupTeacherChoicesReadQuery query(readPort);
    const auto loadedTeachers = query.execute();
    if (!loadedTeachers)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Delete Teacher"),
            tr("Teachers could not be loaded."),
            teacherChoicesErrorMessage(loadedTeachers.error().message)
            );
        return -1;
    }

    for (const auto& choice : loadedTeachers.value().teachers)
    {
        int teacherId = 0;
        const std::string& teacherIdValue = choice.teacherId.value();
        const auto [end, conversionError] = std::from_chars(
            teacherIdValue.data(),
            teacherIdValue.data() + teacherIdValue.size(),
            teacherId
            );
        if (conversionError != std::errc{}
            || end != teacherIdValue.data() + teacherIdValue.size()
            || teacherId <= 0
            || std::to_string(teacherId) != teacherIdValue)
        {
            continue;
        }

        Teacher teacher;
        teacher.id = teacherId;
        teacher.teacherKr = QString::fromStdU16String(choice.teacherKr);
        teacher.teacherEn = QString::fromStdU16String(choice.teacherEn);
        teacher.preferredRomanization = QString::fromStdU16String(
            choice.preferredRomanization
            );
        teacher.preferredName = QString::fromStdU16String(
            choice.preferredName
            );

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
    auto* classes =
        openClassService(m_services);
    auto* teachers =
        openTeacherService(m_services);

    if (!classes || !teachers)
    {
        return storedClassDisplayName(classroom);
    }

    const auto classId = ClassMngr::Next::Domain::ClassId::fromString(
        std::to_string(classroom.id)
        );
    if (classId)
    {
        ClassMngr::Next::Platform::
            ApplicationServicesSelectedClassSubtitleReadPort readPort(
                m_services
                );
        const ClassMngr::Next::Application::SelectedClassSubtitleReadQuery
            query(readPort);
        const auto loadedSubtitle = query.execute(*classId);
        return formattedClassDisplayName(
            classroom,
            loadedSubtitle ? &loadedSubtitle.value() : nullptr
            );
    }

    return formattedClassDisplayName(classroom, nullptr);
}

bool SidebarController::confirmDeleteClass(
    const Classroom& classroom
    ) const
{
    return DialogServices::confirm(
        m_sidebar,
        tr("Delete Class"),
        tr("Delete '%1'?")
            .arg(classDisplayName(classroom)),
        tr("Delete"),
        tr("Cancel"),
        true
        ) == PromptChoice::Destructive;
}

bool SidebarController::confirmDeleteTeacher(
    const Teacher& teacher
    ) const
{
    const QString displayName =
        SidebarNodeNaming::formatTeacherDisplayName(
            teacher
            );

    return DialogServices::confirm(
        m_sidebar,
        tr("Delete Teacher"),
        tr("Delete '%1'?")
            .arg(displayName),
        tr("Delete"),
        tr("Cancel"),
        true
        ) == PromptChoice::Destructive;
}

void SidebarController::updateActionStates()
{
    if (!m_actions)
    {
        return;
    }

    ClassMngr::Next::Platform::
        ApplicationServicesClassesListReadPort classesReadPort(m_services);
    const ClassMngr::Next::Application::ClassesListReadQuery classesQuery(
        classesReadPort
        );
    const auto loadedClasses = classesQuery.execute();

    ClassMngr::Next::Platform::
        ApplicationServicesInitialSetupTeacherChoicesReadPort teachersReadPort(
            m_services
            );
    const ClassMngr::Next::Application::
        InitialSetupTeacherChoicesReadQuery teachersQuery(teachersReadPort);
    const auto loadedTeachers = teachersQuery.execute();

    updateActionStates(
        loadedClasses && !loadedClasses.value().classes.empty(),
        loadedTeachers && !loadedTeachers.value().teachers.empty()
        );
}

void SidebarController::updateActionStates(
    bool hasClasses,
    bool hasTeachers
    )
{
    if (!m_actions)
    {
        return;
    }

    const bool servicesAvailable =
        m_services && m_services->hasOpenDatabase();

    if (m_actions->importClasses)
    {
        m_actions->importClasses->setEnabled(servicesAvailable);
    }

    if (m_actions->importTeachers)
    {
        m_actions->importTeachers->setEnabled(servicesAvailable);
    }

    if (m_actions->upcomingBirthdays)
    {
        m_actions->upcomingBirthdays->setEnabled(servicesAvailable);
    }

    if (m_actions->exportClasses)
    {
        m_actions->exportClasses->setEnabled(
            servicesAvailable && hasClasses
            );
    }

    if (m_actions->deleteClass)
    {
        m_actions->deleteClass->setEnabled(
            servicesAvailable && hasClasses
            );
    }

    if (m_actions->deleteTeacher)
    {
        m_actions->deleteTeacher->setEnabled(
            servicesAvailable && hasTeachers
            );
    }
}
