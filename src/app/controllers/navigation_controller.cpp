#include "navigation_controller.h"

#include "domain/models/classroom.h"
#include "domain/models/teacher.h"

#include "data/data_service.h"

#include "features/classes/ui/class_info_page.h"
#include "features/classes/ui/class_notes_page.h"
#include "features/campus/ui/campus_dashboard_page.h"
#include "features/my_info/ui/my_info_page.h"
#include "ui/shared/pages/pagemanager.h"
#include "features/roster/ui/roster_page.h"
#include "features/speaking_eval/ui/speaking_eval_page.h"
#include "features/sub_prep/ui/sub_prep_page.h"
#include "features/teacher/ui/teacher_info_page.h"

NavigationController::NavigationController(
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

void NavigationController::handleTeacher(
    const NavigationData& data
    )
{
    Teacher teacher =
        m_services
            ->dataService()
            ->getTeacher(
                data.teacherId
                );

    if (teacher.id < 0)
    {
        return;
    }

    if (!m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    m_pages->teacherPage()
        ->loadTeacher(
            teacher
            );

    m_pages->showPage(
        PageType::TeacherInfo
        );
}

void NavigationController::handleNavigation(
    const NavigationData& data
    )
{
    switch (data.type)
    {
    case NodeType::Teacher:
        handleTeacher(data);
        return;

    case NodeType::Class:
        handleClass(data);
        return;

    case NodeType::Root:
        if (!data.path.isEmpty() && data.path.first() == tr("My Info"))
        {
            handleMyInfo(data);
            return;
        }

        if (!data.path.isEmpty() && data.path.first() == tr("Sub Prep"))
        {
            handleSubPrep(data);
            return;
        }

        return;

    case NodeType::Page:
        if (data.path.isEmpty())
        {
            return;
        }

        if (data.path.first() == tr("My Info"))
        {
            handleMyInfo(data);
            return;
        }

        if (data.path.first() == tr("Sub Prep"))
        {
            handleSubPrep(data);
            return;
        }

        if (data.path.first() == tr("Campus Directory"))
        {
            handleCampus(data);
            return;
        }

        if (data.classId > 0)
        {
            const QString pageName =
                data.path.last();

            if (pageName == tr("Class Info"))
            {
                handleClass(data);
                return;
            }

            if (pageName == tr("Class Roster"))
            {
                Classroom classroom =
                    m_services
                        ->dataService()
                        ->getClassById(data.classId);

                if (classroom.id < 0)
                {
                    return;
                }

                if (!m_pages->confirmCurrentPageCanLeave())
                {
                    return;
                }

                m_pages->rosterPage()
                    ->loadClass(classroom);

                m_pages->showPage(PageType::Roster);
                return;
            }

            if (pageName == tr("Class Notes"))
            {
                Classroom classroom =
                    m_services
                        ->dataService()
                        ->getClassById(data.classId);

                if (classroom.id < 0)
                {
                    return;
                }

                if (!m_pages->confirmCurrentPageCanLeave())
                {
                    return;
                }

                m_pages->classNotesPage()
                    ->loadClass(classroom);

                m_pages->showPage(PageType::ClassNotes);
                return;
            }

            if (
                data.path.contains(tr("Student Evaluations"))
                && !pageName.trimmed().isEmpty()
                )
            {
                Classroom classroom =
                    m_services
                        ->dataService()
                        ->getClassById(data.classId);

                if (classroom.id < 0)
                {
                    return;
                }

                if (!m_pages->confirmCurrentPageCanLeave())
                {
                    return;
                }

                m_pages->speakingPage()
                    ->loadEvaluation(
                        classroom,
                        pageName
                        );

                m_pages->showPage(PageType::SpeakingEval);
                return;
            }

            return;
        }

        return;

    default:
        return;
    }
}

void NavigationController::handleSubPrep(
    const NavigationData& data
    )
{
    const bool alreadyShowingSubPrep =
        m_pages->currentWidget()
        == m_pages->subPrepPage();

    const QString sectionName =
        data.path.size() >= 2
            ? data.path.last()
            : tr("Important Information");

    SubPrepSection section =
        SubPrepSection::ImportantInformation;

    if (sectionName == tr("Class Information"))
    {
        section =
            SubPrepSection::ClassInformation;
    }
    else if (sectionName == tr("Sub Comments"))
    {
        section =
            SubPrepSection::SubComments;
    }

    const bool rootClick =
        data.path.size() == 1;

    if (rootClick && alreadyShowingSubPrep)
    {
        return;
    }

    if (!alreadyShowingSubPrep && !m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    if (rootClick)
    {
        m_sidebar->selectSubPrepSection(
            tr("Important Information")
            );
    }

    m_pages->showPage(PageType::SubPrep);
    m_pages->subPrepPage()
        ->scrollToSection(section);
}

void NavigationController::handleMyInfo(
    const NavigationData& data
    )
{
    const bool alreadyShowingMyInfo =
        m_pages->currentWidget()
        == m_pages->myInfoPage();

    const QString sectionName =
        data.path.size() >= 2
            ? data.path.last()
            : tr("My Information");

    MyInfoSection section =
        MyInfoSection::MyInformation;

    if (sectionName == tr("Class Schedule"))
    {
        section =
            MyInfoSection::ClassSchedule;
    }
    else if (sectionName == tr("Monthly Calendar"))
    {
        section =
            MyInfoSection::MonthlyCalendar;
    }

    const bool rootClick =
        data.path.size() == 1;

    if (rootClick && alreadyShowingMyInfo)
    {
        return;
    }

    if (!alreadyShowingMyInfo && !m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    if (rootClick)
    {
        m_sidebar->selectMyInfoSection(
            tr("My Information")
            );
    }

    m_pages->showPage(PageType::MyInfo);
    m_pages->myInfoPage()
        ->scrollToSection(section);
}

void NavigationController::handleCampus(
    const NavigationData& data
    )
{
    if (data.path.isEmpty())
    {
        return;
    }

    const bool alreadyShowingCampus =
        m_pages->currentWidget()
        == m_pages->campusDashboard();

    const bool rootClick =
        data.path.size() == 1;

    if (rootClick)
    {
        const QString sectionName =
            alreadyShowingCampus
                ? m_pages->campusDashboard()->currentSectionName()
                : tr("Information");

        if (!alreadyShowingCampus)
        {
            if (!m_pages->confirmCurrentPageCanLeave())
            {
                return;
            }

            m_pages->showPage(PageType::CampusDashboard);
            m_pages->campusDashboard()->showInformation();
        }

        m_sidebar->selectCampusSection(
            sectionName
            );

        return;
    }

    if (!m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    m_pages->showPage(PageType::CampusDashboard);

    const QString pageName =
        data.path.last();

    if (pageName == tr("Directions"))
    {
        m_pages->campusDashboard()->showDirections();
        return;
    }

    if (pageName == tr("Information"))
    {
        m_pages->campusDashboard()->showInformation();
        return;
    }

    if (pageName == tr("Housing"))
    {
        m_pages->campusDashboard()->showHousing();
        return;
    }

    if (pageName == tr("Map"))
    {
        m_pages->campusDashboard()->showMap();
        return;
    }
}

void NavigationController::handleClass(
    const NavigationData& data
    )
{
    if (data.classId <= 0)
    {
        return;
    }

    Classroom classroom =
        m_services
            ->dataService()
            ->getClassById(data.classId);

    if (classroom.id < 0)
    {
        return;
    }

    if (!m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    m_pages->classInfoPage()
        ->loadClass(classroom);

    m_pages->showPage(PageType::ClassInfo);
}
