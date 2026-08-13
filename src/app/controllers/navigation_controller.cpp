#include "navigation_controller.h"

#include "app/services/feature_services.h"
#include "domain/models/classroom.h"
#include "domain/models/teacher.h"

#include "features/classes/ui/classes_page.h"
#include "features/campus/ui/campus_dashboard_page.h"
#include "features/calendar/ui/calendar_page.h"
#include "features/my_info/ui/my_classes_page.h"
#include "features/my_info/ui/personal_details_page.h"
#include "features/roster/ui/rosters_page.h"
#include "features/schedule/ui/schedule_page.h"
#include "ui/shared/pages/pagemanager.h"
#include "features/speaking_eval/ui/speaking_eval_page.h"
#include "features/sub_prep/ui/sub_prep_page.h"
#include "features/teacher/ui/teacher_info_page.h"
#include "features/teacher/ui/staff_directory_page.h"
#include "ui/shared/pages/pdf_viewer_page.h"

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
    auto* teachers =
        m_services
            ? m_services->teacherService()
            : nullptr;

    if (!teachers || !teachers->isAvailable())
    {
        return;
    }

    Teacher teacher =
        teachers->teacher(data.teacherId);

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

        if (!data.keys.isEmpty() && data.keys.first() == QStringLiteral("document"))
        {
            handleDocument(data);
            return;
        }

        return;

    case NodeType::Page:
        if (data.path.isEmpty() || data.keys.isEmpty())
        {
            return;
        }

        if (
            data.routeKey == QStringLiteral("native_english_teachers")
            || data.routeKey == QStringLiteral("gs_team")
            )
        {
            if (
                !m_services
                || !m_services->teacherService()->isAvailable()
                || !m_pages->confirmCurrentPageCanLeave()
                )
            {
                return;
            }

            StaffDirectoryPage* page =
                data.routeKey == QStringLiteral("native_english_teachers")
                    ? m_pages->nativeEnglishTeachersPage()
                    : m_pages->gsTeamPage();
            if (!page || !page->loadDirectory())
            {
                return;
            }
            m_pages->showPage(
                data.routeKey == QStringLiteral("native_english_teachers")
                    ? PageType::NativeEnglishTeachers
                    : PageType::GsTeam
                );
            return;
        }

        if (data.routeKey == QStringLiteral("my_info_class_roster"))
        {
            handleRosters(data);
            return;
        }

        if (
            data.classId > 0
            && data.keys.contains(QStringLiteral("student_evaluations"))
            && !data.routeKey.trimmed().isEmpty()
            )
        {
            const QString evaluationName =
                evaluationNameForKey(data.routeKey);

            if (
                evaluationName.trimmed().isEmpty()
                || !m_services
                || !m_services->classService()->isAvailable()
                )
            {
                return;
            }

            const Classroom classroom =
                m_services
                    ->classService()
                    ->classroom(data.classId);

            if (
                classroom.id <= 0
                || !m_pages->confirmCurrentPageCanLeave()
                )
            {
                return;
            }

            m_pages->speakingPage()->loadEvaluation(
                classroom,
                evaluationName
                );
            m_pages->showPage(PageType::SpeakingEval);
            return;
        }

        if (data.classId > 0)
        {
            if (data.routeKey == QStringLiteral("class_details"))
            {
                handleClass(data);
                return;
            }

            if (data.routeKey == QStringLiteral("class_roster"))
            {
                handleRoster(data);
                return;
            }

            if (data.routeKey == QStringLiteral("class_notes"))
            {
                handleNotes(data);
                return;
            }
        }

        if (data.routeKey == QStringLiteral("classes"))
        {
            if (
                !m_services
                || !m_services->classService()->isAvailable()
                )
            {
                return;
            }

            const bool alreadyShowingClasses =
                m_pages->currentWidget()
                == m_pages->classesPage();

            if (
                !alreadyShowingClasses
                && !m_pages->confirmCurrentPageCanLeave()
                )
            {
                return;
            }

            if (!m_pages->classesPage()->loadClasses())
            {
                return;
            }

            m_pages->showPage(PageType::Classes);
            return;
        }

        if (
            data.routeKey == QStringLiteral("my_info_information")
            || data.routeKey == QStringLiteral("my_info_schedule")
            || data.routeKey == QStringLiteral("my_info_calendar")
            )
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

        if (data.keys.first() == QStringLiteral("document"))
        {
            handleDocument(data);
            return;
        }

        if (data.keys.first() == QStringLiteral("speaking_evaluations"))
        {
            if (
                !m_services
                || !m_services->speakingEvaluationService()->isAvailable()
                || !m_pages->confirmCurrentPageCanLeave()
                )
            {
                return;
            }

            m_pages->speakingPage()->loadEvaluations();
            m_pages->showPage(PageType::SpeakingEval);
            return;
        }

        return;

    default:
        return;
    }
}

void NavigationController::handleDocument(
    const NavigationData& data
    )
{
    if (!m_services || !m_pages)
    {
        return;
    }

    const DocumentCatalog* catalog =
        m_services->documentCatalog();

    const DocumentDefinition* document =
        catalog
            ? catalog->document(data.routeKey)
            : nullptr;

    if (!document)
    {
        return;
    }

    if (!m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    PdfViewerDocumentDescriptor descriptor;
    descriptor.pdfFilePath =
        document->pdf.absoluteFilePath;
    descriptor.printEnabled =
        document->printingEnabled;
    descriptor.exportEnabled =
        document->exportingEnabled
        && document->exportFile.has_value();

    if (document->exportFile)
    {
        descriptor.exportFilePath =
            document->exportFile->absoluteFilePath;
        descriptor.exportFileName =
            document->exportFile->fileName;
    }

    [[maybe_unused]] const bool loaded =
        m_pages->pdfViewerPage()->loadPdf(descriptor);

    m_pages->showPage(
        PageType::PdfViewer
        );
}

void NavigationController::handleSubPrep(
    const NavigationData& data
    )
{
    if (
        !m_services
        || !m_services->hasOpenDatabase()
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

    if (
        sectionKey == QStringLiteral("sub_prep_notes")
        || sectionKey == QStringLiteral("sub_prep_comments")
        )
    {
        section =
            SubPrepSection::SubNotes;
    }

    const bool rootClick =
        data.path.size() == 1;

    if (!alreadyShowingSubPrep && !m_pages->confirmCurrentPageCanLeave())
    {
        return;
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
        || !m_services->hasOpenDatabase()
        )
    {
        return;
    }

    const QString sectionKey =
        data.routeKey;

    PageType targetPageType =
        PageType::PersonalDetails;
    QWidget* targetPage =
        m_pages->personalDetailsPage();
    QString selectedSectionKey =
        QStringLiteral("my_info_information");

    if (sectionKey == QStringLiteral("my_info_schedule"))
    {
        targetPageType =
            PageType::MySchedule;
        targetPage =
            m_pages->mySchedulePage();
        selectedSectionKey =
            QStringLiteral("my_info_schedule");
    }
    else if (sectionKey == QStringLiteral("my_info_class_information"))
    {
        targetPageType =
            PageType::MyClasses;
        targetPage =
            m_pages->myClassesPage();
        selectedSectionKey =
            QStringLiteral("my_info_class_information");
    }
    else if (sectionKey == QStringLiteral("my_info_calendar"))
    {
        targetPageType =
            PageType::Calendar;
        targetPage =
            m_pages->calendarPage();
        selectedSectionKey =
            QStringLiteral("my_info_calendar");
    }

    const bool alreadyShowingTarget =
        m_pages->currentWidget()
        == targetPage;

    if (!alreadyShowingTarget && !m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    m_pages->showPage(targetPageType);

    m_sidebar->selectMyInfoSection(
        selectedSectionKey
        );

    if (sectionKey == QStringLiteral("my_info_calendar"))
    {
        m_pages->calendarPage()->scrollToTop();
    }
    else if (sectionKey == QStringLiteral("my_info_information"))
    {
        m_pages->personalDetailsPage()->scrollToTop();
    }
}

void NavigationController::handleRosters(
    const NavigationData& data
    )
{
    Q_UNUSED(data)

    if (
        !m_services
        || !m_services->rosterService()->isAvailable()
        || !m_pages->confirmCurrentPageCanLeave()
        )
    {
        return;
    }

    m_pages->rostersPage()
        ->loadRosters();

    m_pages->showPage(PageType::Rosters);

    m_sidebar->selectByKeys(
        {QStringLiteral("my_info_class_roster")}
        );
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

void NavigationController::handleRoster(
    const NavigationData& data
    )
{
    if (
        !m_services
        || !m_services->classService()->isAvailable()
        || data.classId <= 0
        )
    {
        return;
    }

    const bool alreadyShowingClasses =
        m_pages->currentWidget()
        == m_pages->classesPage();

    if (
        !alreadyShowingClasses
        && !m_pages->confirmCurrentPageCanLeave()
        )
    {
        return;
    }

    if (
        m_pages->classesPage()->openClass(
            data.classId,
            ClassesSection::Roster
            )
        )
    {
        m_pages->showPage(PageType::Classes);
    }
}

void NavigationController::handleNotes(
    const NavigationData& data
    )
{
    if (
        !m_services
        || !m_services->classService()->isAvailable()
        || data.classId <= 0
        )
    {
        return;
    }

    const bool alreadyShowingClasses =
        m_pages->currentWidget()
        == m_pages->classesPage();

    if (
        !alreadyShowingClasses
        && !m_pages->confirmCurrentPageCanLeave()
        )
    {
        return;
    }

    if (
        m_pages->classesPage()->openClass(
            data.classId,
            ClassesSection::Notes
            )
        )
    {
        m_pages->showPage(PageType::Classes);
    }
}

void NavigationController::handleClass(
    const NavigationData& data
    )
{
    if (
        !m_services
        || !m_services->classService()->isAvailable()
        )
    {
        return;
    }

    if (data.classId <= 0)
    {
        return;
    }

    const bool alreadyShowingClasses =
        m_pages->currentWidget()
        == m_pages->classesPage();

    if (
        !alreadyShowingClasses
        && !m_pages->confirmCurrentPageCanLeave()
        )
    {
        return;
    }

    if (
        m_pages->classesPage()->openClass(
            data.classId,
            ClassesSection::Details
            )
        )
    {
        m_pages->showPage(PageType::Classes);
    }
}
