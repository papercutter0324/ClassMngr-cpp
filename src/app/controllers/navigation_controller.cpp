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

namespace
{
QString evaluationNameForKey(
    const QString& key
    )
{
    if (key == QStringLiteral("speaking_winter"))
    {
        return QStringLiteral("Winter");
    }

    if (key == QStringLiteral("speaking_speech_contest"))
    {
        return QStringLiteral("Speech Contest");
    }

    if (key == QStringLiteral("speaking_summer"))
    {
        return QStringLiteral("Summer");
    }

    if (key == QStringLiteral("speaking_fall"))
    {
        return QStringLiteral("Fall");
    }

    return QString();
}
}

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
    if (m_pages && m_pages->campusDashboard() && m_sidebar)
    {
        connect(
            m_pages->campusDashboard(),
            &CampusDashboardPage::sectionChanged,
            this,
            [this](const QString& sectionKey)
            {
                if (
                    !m_pages
                    || !m_sidebar
                    || m_pages->currentWidget() != m_pages->campusDashboard()
                    )
                {
                    return;
                }

                m_sidebar->selectCampusSection(sectionKey);
            }
            );
    }
}

void NavigationController::handleTeacher(
    const NavigationData& data
    )
{
    if (
        !m_services
        || !m_services->dataService()
        || !m_services->dataService()->isOpen()
        )
    {
        return;
    }

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
        if (!data.keys.isEmpty() && data.keys.first() == QStringLiteral("my_info"))
        {
            handleMyInfo(data);
            return;
        }

        if (!data.keys.isEmpty() && data.keys.first() == QStringLiteral("sub_prep"))
        {
            handleSubPrep(data);
            return;
        }

        if (!data.keys.isEmpty() && data.keys.first() == QStringLiteral("campus_info"))
        {
            handleCampus(data);
            return;
        }

        return;

    case NodeType::Page:
        if (data.path.isEmpty() || data.keys.isEmpty())
        {
            return;
        }

        if (data.keys.first() == QStringLiteral("my_info"))
        {
            handleMyInfo(data);
            return;
        }

        if (data.keys.first() == QStringLiteral("sub_prep"))
        {
            handleSubPrep(data);
            return;
        }

        if (data.keys.first() == QStringLiteral("campus_info"))
        {
            handleCampus(data);
            return;
        }

        if (data.classId > 0)
        {
            if (
                !m_services
                || !m_services->dataService()
                || !m_services->dataService()->isOpen()
                )
            {
                return;
            }

            if (data.routeKey == QStringLiteral("class_info"))
            {
                handleClass(data);
                return;
            }

            if (data.routeKey == QStringLiteral("class_roster"))
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

            if (data.routeKey == QStringLiteral("class_notes"))
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
                data.keys.contains(QStringLiteral("student_evaluations"))
                && !data.routeKey.trimmed().isEmpty()
                )
            {
                const QString evaluationName =
                    evaluationNameForKey(data.routeKey);

                if (evaluationName.trimmed().isEmpty())
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

                m_pages->speakingPage()
                    ->loadEvaluation(
                        classroom,
                        evaluationName
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
    if (
        !m_services
        || !m_services->dataService()
        || !m_services->dataService()->isOpen()
        )
    {
        return;
    }

    const bool alreadyShowingSubPrep =
        m_pages->currentWidget()
        == m_pages->subPrepPage();

    const QString sectionKey =
        data.keys.size() >= 2
            ? data.routeKey
            : QStringLiteral("sub_prep_important");

    SubPrepSection section =
        SubPrepSection::ImportantInformation;

    if (sectionKey == QStringLiteral("sub_prep_comments"))
    {
        section =
            SubPrepSection::SubComments;
    }

    const bool rootClick =
        data.path.size() == 1;

    if (!alreadyShowingSubPrep && !m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    if (rootClick)
    {
        m_sidebar->selectSubPrepSection(
            QStringLiteral("sub_prep_important")
            );
    }

    m_pages->showPage(PageType::SubPrep);
    if (rootClick)
    {
        m_pages->subPrepPage()->scrollToTop();
    }
    else
    {
        m_pages->subPrepPage()
            ->scrollToSection(section);
    }
}

void NavigationController::handleMyInfo(
    const NavigationData& data
    )
{
    if (
        !m_services
        || !m_services->dataService()
        || !m_services->dataService()->isOpen()
        )
    {
        return;
    }

    const bool alreadyShowingMyInfo =
        m_pages->currentWidget()
        == m_pages->myInfoPage();

    const QString sectionKey =
        data.keys.size() >= 2
            ? data.routeKey
            : QStringLiteral("my_info_information");

    MyInfoSection section =
        MyInfoSection::MyInformation;

    if (sectionKey == QStringLiteral("my_info_schedule"))
    {
        section =
            MyInfoSection::ClassSchedule;
    }
    else if (sectionKey == QStringLiteral("my_info_class_information"))
    {
        section =
            MyInfoSection::ClassInformation;
    }
    else if (sectionKey == QStringLiteral("my_info_calendar"))
    {
        section =
            MyInfoSection::MonthlyCalendar;
    }

    const bool rootClick =
        data.path.size() == 1;

    if (!alreadyShowingMyInfo && !m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    if (rootClick)
    {
        m_sidebar->selectMyInfoSection(
            QStringLiteral("my_info_information")
            );
    }

    m_pages->showPage(PageType::MyInfo);
    if (rootClick)
    {
        m_pages->myInfoPage()->scrollToTop();
    }
    else
    {
        m_pages->myInfoPage()
            ->scrollToSection(section);
    }
}

void NavigationController::handleCampus(
    const NavigationData& data
    )
{
    if (data.keys.isEmpty())
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
        const QString sectionKey =
            alreadyShowingCampus
                ? m_pages->campusDashboard()->currentSectionKey()
                : QStringLiteral("campus_information");

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
            sectionKey
            );

        return;
    }

    if (!alreadyShowingCampus && !m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    m_pages->showPage(PageType::CampusDashboard);

    const QString pageKey =
        data.keys.size() >= 2
            ? data.routeKey
            : QStringLiteral("campus_information");

    if (pageKey == QStringLiteral("campus_address"))
    {
        m_pages->campusDashboard()->showAddress();
        return;
    }

    if (pageKey == QStringLiteral("campus_directions"))
    {
        m_pages->campusDashboard()->showDirections();
        return;
    }

    if (pageKey == QStringLiteral("campus_information"))
    {
        m_pages->campusDashboard()->showInformation();
        return;
    }

    if (pageKey == QStringLiteral("campus_housing"))
    {
        m_pages->campusDashboard()->showHousing();
        return;
    }

    if (pageKey == QStringLiteral("campus_map"))
    {
        m_pages->campusDashboard()->showMap();
        return;
    }
}

void NavigationController::handleClass(
    const NavigationData& data
    )
{
    if (
        !m_services
        || !m_services->dataService()
        || !m_services->dataService()->isOpen()
        )
    {
        return;
    }

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
