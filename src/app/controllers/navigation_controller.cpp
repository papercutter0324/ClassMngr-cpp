#include "navigation_controller.h"

#include "core/resource_paths.h"
#include "domain/models/classroom.h"
#include "domain/models/teacher.h"

#include "data/data_service.h"

#include "features/classes/ui/classes_page.h"
#include "features/campus/ui/campus_dashboard_page.h"
#include "features/my_info/ui/calendar_page.h"
#include "features/my_info/ui/my_info_page.h"
#include "features/roster/ui/rosters_page.h"
#include "ui/shared/pages/pagemanager.h"
#include "features/speaking_eval/ui/speaking_eval_page.h"
#include "features/sub_prep/ui/sub_prep_page.h"
#include "features/teacher/ui/teacher_info_page.h"
#include "ui/shared/pages/pdf_viewer_page.h"

#include <QDir>

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

namespace DocumentActions
{
constexpr PdfViewerDocumentActions NoActions{
    false,
    false
};

constexpr PdfViewerDocumentActions ExportOnly{
    true,
    false
};

constexpr PdfViewerDocumentActions PrintOnly{
    false,
    true
};

constexpr PdfViewerDocumentActions ExportAndPrint{
    true,
    true
};
}

struct DocumentRoute
{
    QString directory;
    QString fileName;
    PdfViewerDocumentActions actions = DocumentActions::ExportAndPrint;
};

DocumentRoute documentRouteForKey(
    const QString& key
    )
{
    if (key == QStringLiteral("document_guides_lesson_planning"))
    {
        return {
            ResourcePaths::Files::guidesDirectory(),
            QStringLiteral("DYB Lesson Planning Guide.pdf")
        };
    }

    if (key == QStringLiteral("document_guides_powerpoint_shortcuts"))
    {
        return {
            ResourcePaths::Files::guidesDirectory(),
            QStringLiteral("PowerPoint Keyboard Shortcuts.pdf")
        };
    }

    if (key == QStringLiteral("document_lesson_templates_sp_wr"))
    {
        return {
            ResourcePaths::Files::lessonsDirectory(),
            QStringLiteral("SP+WR Template.pdf"),
            DocumentActions::ExportOnly
        };
    }

    if (key == QStringLiteral("document_lesson_templates_skill"))
    {
        return {
            ResourcePaths::Files::lessonsDirectory(),
            QStringLiteral("Skill Lesson Template.pdf"),
            DocumentActions::ExportOnly
        };
    }

    if (key == QStringLiteral("document_lesson_templates_student_led"))
    {
        return {
            ResourcePaths::Files::lessonsDirectory(),
            QStringLiteral("Student-Led Activity Template.pdf"),
            DocumentActions::ExportOnly
        };
    }

    if (key == QStringLiteral("document_lesson_templates_creo_writing"))
    {
        return {
            ResourcePaths::Files::lessonsDirectory(),
            QStringLiteral("CREO Creative Writing Lesson Template.pdf"),
            DocumentActions::ExportOnly
        };
    }

    if (key == QStringLiteral("document_lesson_templates_ms_essay"))
    {
        return {
            ResourcePaths::Files::lessonsDirectory(),
            QStringLiteral("Middle School OE Template.pdf"),
            DocumentActions::ExportOnly
        };
    }

    if (key == QStringLiteral("document_lesson_templates_theseus_paragraph"))
    {
        return {
            ResourcePaths::Files::lessonsDirectory(),
            QStringLiteral("Theseus Paragraph Writing Template.pdf"),
            DocumentActions::ExportOnly
        };
    }

    if (key == QStringLiteral("document_online_essay_topic_template"))
    {
        return {
            ResourcePaths::Files::essayDirectory(),
            QStringLiteral("Essay Topic Template for Students.pdf"),
            DocumentActions::ExportOnly
        };
    }

    if (key == QStringLiteral("document_online_essay_brainstorm"))
    {
        return {
            ResourcePaths::Files::essayDirectory(),
            QStringLiteral("Essay Writing Brainstorm Template.pdf"),
            DocumentActions::ExportOnly
        };
    }

    if (key == QStringLiteral("document_online_essay_theseus_explained"))
    {
        return {
            ResourcePaths::Files::essayDirectory(),
            QStringLiteral("Theseus Paragraph Writing Explained.pdf")
        };
    }

    if (key == QStringLiteral("document_speaking_evals_tips_one_on_one"))
    {
        return {
            ResourcePaths::Files::evaluationsDirectory(),
            QStringLiteral("Speaking Evaluations Explained - 1-on-1.pdf")
        };
    }

    if (key == QStringLiteral("document_speaking_evals_tips_presentations"))
    {
        return {
            ResourcePaths::Files::evaluationsDirectory(),
            QStringLiteral("Speaking Evaluations Explained - In-class Presentations.pdf")
        };
    }

    if (key == QStringLiteral("document_speaking_evals_tips_single_class"))
    {
        return {
            ResourcePaths::Files::evaluationsDirectory(),
            QStringLiteral("Speaking Evaluations Explained - One Class Tips.pdf")
        };
    }

    if (key == QStringLiteral("document_speaking_evals_topic_options"))
    {
        return {
            ResourcePaths::Files::evaluationsDirectory(),
            QStringLiteral("Speaking Evaluation Topic Options.pdf"),
            DocumentActions::ExportOnly
        };
    }

    if (key == QStringLiteral("document_speaking_evals_regular_template"))
    {
        return {
            ResourcePaths::Files::evaluationsDirectory(),
            QStringLiteral("SpeakingEvaluationTemplate-Full.pdf"),
            DocumentActions::ExportOnly
        };
    }

    if (key == QStringLiteral("document_speaking_evals_athena_songs_template"))
    {
        return {
            ResourcePaths::Files::evaluationsDirectory(),
            QStringLiteral("SpeakingEvaluationTemplate_Advanced-Full.pdf"),
            DocumentActions::ExportOnly
        };
    }

    if (key == QStringLiteral("document_speaking_evals_winner_certificates"))
    {
        return {
            ResourcePaths::Files::evaluationsDirectory(),
            QStringLiteral("CertificateTemplate-Full.pdf"),
            DocumentActions::ExportOnly
        };
    }

    if (key == QStringLiteral("document_book_reports_grading_rubric"))
    {
        return {
            ResourcePaths::Files::bookReportsDirectory(),
            QStringLiteral("Book Report Grading Rubric.pdf")
        };
    }

    if (key == QStringLiteral("document_book_reports_grading_rubric_40"))
    {
        return {
            ResourcePaths::Files::bookReportsDirectory(),
            QStringLiteral("Book Report Grading Rubric (Sentence Requirements Not Met).pdf")
        };
    }

    if (key == QStringLiteral("document_book_reports_student_info_handout"))
    {
        return {
            ResourcePaths::Files::bookReportsDirectory(),
            QStringLiteral("Book Report Grading.pdf")
        };
    }

    if (key == QStringLiteral("document_training_observation"))
    {
        return {
            ResourcePaths::Files::trainingDirectory(),
            QStringLiteral("Name Surname Observation 0 - By Name .pdf"),
            DocumentActions::ExportOnly
        };
    }

    if (key == QStringLiteral("document_training_reflection"))
    {
        return {
            ResourcePaths::Files::trainingDirectory(),
            QStringLiteral("Name Surname Reflection 0.pdf"),
            DocumentActions::ExportOnly
        };
    }

    if (key == QStringLiteral("document_training_final_reflection"))
    {
        return {
            ResourcePaths::Files::trainingDirectory(),
            QStringLiteral("Name Surname Reflection 8 (Final Reflection).pdf"),
            DocumentActions::ExportOnly
        };
    }

    if (key == QStringLiteral("document_vacation_sub_prep_applying"))
    {
        return {
            ResourcePaths::Files::vacationDirectory(),
            QStringLiteral("1 - How to Apply for Vacation.pdf"),
            DocumentActions::ExportOnly
        };
    }

    if (key == QStringLiteral("document_vacation_sub_prep_guidelines"))
    {
        return {
            ResourcePaths::Files::vacationDirectory(),
            QStringLiteral("2 - Vacation Guidelines.pdf"),
            DocumentActions::ExportOnly
        };
    }

    if (key == QStringLiteral("document_vacation_sub_prep_request_form"))
    {
        return {
            ResourcePaths::Files::vacationDirectory(),
            QStringLiteral("3 - Vacation Request Form.pdf"),
            DocumentActions::ExportAndPrint
        };
    }

    if (key == QStringLiteral("document_vacation_sub_prep_procedures"))
    {
        return {
            ResourcePaths::Files::vacationDirectory(),
            QStringLiteral("DYB Sub Procedures.pdf"),
            DocumentActions::ExportOnly
        };
    }

    if (key == QStringLiteral("document_vacation_sub_prep_checklist"))
    {
        return {
            ResourcePaths::Files::vacationDirectory(),
            QStringLiteral("DYB Sub Prep Checklist.pdf"),
            DocumentActions::ExportAndPrint
        };
    }

    if (key == QStringLiteral("document_vacation_sub_prep_template"))
    {
        return {
            ResourcePaths::Files::vacationDirectory(),
            QStringLiteral("DYB Sub Prep Template.pdf"),
            DocumentActions::ExportOnly
        };
    }

    return {};
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
                || !m_services->dataService()
                || !m_services->dataService()->isOpen()
                )
            {
                return;
            }

            const Classroom classroom =
                m_services
                    ->dataService()
                    ->getClassById(data.classId);

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
                || !m_services->dataService()
                || !m_services->dataService()->isOpen()
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

        if (data.routeKey == QStringLiteral("my_info_calendar"))
        {
            handleMyInfo(data);
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

        if (data.keys.first() == QStringLiteral("document"))
        {
            handleDocument(data);
            return;
        }

        if (data.keys.first() == QStringLiteral("speaking_evaluations"))
        {
            if (
                !m_services
                || !m_services->dataService()
                || !m_services->dataService()->isOpen()
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
    const DocumentRoute route =
        documentRouteForKey(
            data.routeKey
            );

    if (
        route.directory.trimmed().isEmpty()
        || route.fileName.trimmed().isEmpty()
        )
    {
        return;
    }

    if (!m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    const QString filePath =
        QDir(
            route.directory
            ).filePath(
                route.fileName
                );

    [[maybe_unused]] const bool loaded =
        m_pages->pdfViewerPage()
            ->loadPdf(
                filePath,
                route.actions
                );

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
        || !m_services->dataService()
        || !m_services->dataService()->isOpen()
        )
    {
        return;
    }

    const bool rootClick =
        data.keys.size() == 1
        && data.routeKey == QStringLiteral("my_info");

    const QString sectionKey =
        rootClick
            ? QStringLiteral("my_info_information")
            : data.routeKey;

    PageType targetPageType =
        PageType::MyInfo;
    QWidget* targetPage =
        m_pages->myInfoPage();
    QString selectedSectionKey =
        QStringLiteral("my_info_information");

    if (sectionKey == QStringLiteral("my_info_schedule"))
    {
        targetPageType =
            PageType::MyInfoSchedule;
        targetPage =
            m_pages->myInfoSchedulePage();
        selectedSectionKey =
            QStringLiteral("my_info_schedule");
    }
    else if (sectionKey == QStringLiteral("my_info_class_information"))
    {
        targetPageType =
            PageType::MyInfoClassInformation;
        targetPage =
            m_pages->myInfoClassInformationPage();
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

    if (rootClick)
    {
        m_sidebar->selectMyInfoSection(
            QStringLiteral("my_info_information")
            );
        m_pages->myInfoPage()->scrollToTop();
    }
    else
    {
        m_sidebar->selectMyInfoSection(
            selectedSectionKey
            );

        if (sectionKey == QStringLiteral("my_info_calendar"))
        {
            m_pages->calendarPage()->scrollToTop();
        }
        else if (sectionKey == QStringLiteral("my_info_information"))
        {
            m_pages->myInfoPage()->scrollToTop();
        }
    }
}

void NavigationController::handleRosters(
    const NavigationData& data
    )
{
    Q_UNUSED(data)

    if (
        !m_services
        || !m_services->dataService()
        || !m_services->dataService()->isOpen()
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
        || !m_services->dataService()
        || !m_services->dataService()->isOpen()
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
        || !m_services->dataService()
        || !m_services->dataService()->isOpen()
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
