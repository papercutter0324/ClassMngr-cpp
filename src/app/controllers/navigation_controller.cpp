#include "navigation_controller.h"

#include "app/services/feature_services.h"
#include "domain/models/classroom.h"
#include "domain/models/teacher.h"

#include "features/classes/ui/classes_page.h"
#include "features/campus/ui/campus_dashboard_page.h"
#include "features/calendar/ui/calendar_page.h"
#include "features/my_info/ui/my_classes_page.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "features/my_info/ui/personal_details_page.h"
#include "features/schedule/ui/schedule_page.h"
#include "ui/shared/pages/pagemanager.h"
#include "features/sub_prep/ui/sub_prep_page.h"
#include "features/teacher/ui/teacher_info_page.h"
#include "features/teacher/ui/staff_directory_page.h"
#include "core/resource_paths.h"
#include "next/application/document_catalog_use_case.h"
#include "next/platform/application_services_document_catalog_port.h"
#include "ui/shared/pages/pdf_viewer_page.h"

#include <QFileInfo>
#include <QLocale>

#include <string_view>
#include <utility>
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
    if (m_pages && m_sidebar)
    {
        connect(
            m_pages,
            &PageManager::pageCreated,
            this,
            [this](PageType type, BasePage* page)
            {
                if (type != PageType::CampusDashboard)
                {
                    return;
                }

                auto* campus = qobject_cast<CampusDashboardPage*>(page);

                if (!campus)
                {
                    return;
                }

                connect(
                    campus,
                    &CampusDashboardPage::sectionChanged,
                    this,
                    [this](const QString& sectionKey)
                    {
                        if (
                            !m_pages
                            || !m_sidebar
                            || !m_pages->isCurrentPage(
                                PageType::CampusDashboard
                                )
                            )
                        {
                            return;
                        }

                        m_sidebar->selectCampusSection(sectionKey);
                    }
                    );
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

    const Result<Teacher> teacher =
        teachers->teacher(data.teacherId);

    if (!teacher)
    {
        return;
    }

    if (!m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    auto* page = m_pages->ensureTeacherPage();

    if (!page)
    {
        return;
    }

    page->loadTeacher(
        *teacher
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
                    ? m_pages->ensureNativeEnglishTeachersPage()
                    : m_pages->ensureGsTeamPage();
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

            if (
                !m_pages->confirmCurrentPageCanLeave()
                )
            {
                return;
            }

            if (auto* classes = m_pages->ensureClassesPage(); classes
                && classes->openEvaluation(
                    data.classId,
                    evaluationName
                    ))
            {
                m_pages->showPage(PageType::Classes);
            }
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
                m_pages->isCurrentPage(PageType::Classes);

            if (
                !alreadyShowingClasses
                && !m_pages->confirmCurrentPageCanLeave()
                )
            {
                return;
            }

            auto* classes = m_pages->ensureClassesPage();

            if (!classes || !classes->loadClasses())
            {
                return;
            }

            m_pages->showPage(PageType::Classes);
            return;
        }

        if (
            data.routeKey == QStringLiteral("my_workspace")
            || data.routeKey == QStringLiteral("my_info_information")
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

    const QByteArray routeKey = data.routeKey.toUtf8();
    if (data.routeKey.trimmed().isEmpty()
        || static_cast<std::size_t>(routeKey.size())
            > ClassMngr::Next::Application::kDocumentCatalogMaxIdentifierLength)
    {
        return;
    }

    const auto documentId =
        ClassMngr::Next::Domain::DocumentId::fromString(
            routeKey.toStdString()
            );
    if (!documentId)
    {
        return;
    }

    ClassMngr::Next::Platform::ApplicationServicesDocumentCatalogPort port(
        *m_services
        );
    const auto projection = port.projection(QLocale().name());
    if (!projection)
    {
        return;
    }

    ClassMngr::Next::Application::DocumentContentSession contentSession;
    const ClassMngr::Next::Application::DocumentCatalogUseCase catalogUseCase(
        projection.value(),
        contentSession
        );
    const auto document = catalogUseCase.resolveDocument(*documentId);
    if (!document)
    {
        return;
    }

    if (!m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    auto lease = ResourcePaths::Documents::acquire();
    if (!lease)
    {
        return;
    }

    PdfViewerDocumentDescriptor descriptor;
    descriptor.pdfFilePath =
        ResourcePaths::Documents::filePath(
            *lease,
            QString::fromUtf8(document.value().path)
            );
    descriptor.printEnabled =
        document.value().printable;
    descriptor.exportEnabled =
        document.value().exportable
        && document.value().exportReference.has_value();

    if (document.value().exportReference)
    {
        constexpr std::string_view prefix = "resource://documents/";
        const std::string& reference =
            document.value().exportReference->value();
        if (!reference.starts_with(prefix))
        {
            return;
        }
        const QString exportRelativePath = QString::fromUtf8(
            reference.substr(prefix.size())
            );
        descriptor.exportFilePath =
            ResourcePaths::Documents::filePath(
                *lease,
                exportRelativePath
                );
        descriptor.exportFileName =
            QFileInfo(exportRelativePath).fileName();
    }

    descriptor.resourceLease = std::move(*lease);

    if (auto* viewer = m_pages->ensurePdfViewerPage())
    {
        [[maybe_unused]] const bool loaded = viewer->loadPdf(std::move(descriptor));
    }

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

    if (sectionKey == QStringLiteral("my_info_class_information"))
    {
        const bool alreadyShowingClasses =
            m_pages->isCurrentPage(PageType::MyClasses);

        if (!alreadyShowingClasses && !m_pages->confirmCurrentPageCanLeave())
        {
            return;
        }

        m_pages->showPage(PageType::MyClasses);
        return;
    }

    WorkspaceTab tab = WorkspaceTab::Schedule;

    if (sectionKey == QStringLiteral("my_info_information"))
    {
        tab = WorkspaceTab::Details;
    }
    else if (sectionKey == QStringLiteral("my_info_calendar"))
    {
        tab = WorkspaceTab::Calendar;
    }

    const bool alreadyShowingWorkspace =
        m_pages->isCurrentPage(PageType::MyWorkspace);

    if (!alreadyShowingWorkspace && !m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    m_pages->showPage(PageType::MyWorkspace);

    auto* workspace = m_pages->myWorkspacePage();

    if (!workspace)
    {
        return;
    }

    workspace->openTab(tab);

    m_sidebar->selectMyInfoSection(
        QStringLiteral("my_workspace")
        );

    if (tab == WorkspaceTab::Calendar)
    {
        if (auto* calendar = workspace->calendarPage())
        {
            calendar->scrollToTop();
        }
    }
    else if (tab == WorkspaceTab::Details)
    {
        workspace->personalDetailsPage()->scrollToTop();
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
        m_pages->isCurrentPage(PageType::CampusDashboard);

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

            if (auto* campus = m_pages->campusDashboard())
            {
                campus->showInformation();
            }
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

    auto* campus = m_pages->campusDashboard();

    if (!campus)
    {
        return;
    }

    const QString pageKey =
        data.keys.size() >= 2
            ? data.routeKey
            : QStringLiteral("campus_information");

    if (pageKey == QStringLiteral("campus_address"))
    {
        campus->showAddress();
        return;
    }

    if (pageKey == QStringLiteral("campus_directions"))
    {
        campus->showDirections();
        return;
    }

    if (pageKey == QStringLiteral("campus_information"))
    {
        campus->showInformation();
        return;
    }

    if (pageKey == QStringLiteral("campus_housing"))
    {
        campus->showHousing();
        return;
    }

    if (pageKey == QStringLiteral("campus_map"))
    {
        campus->showMap();
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
        m_pages->isCurrentPage(PageType::Classes);

    if (
        !alreadyShowingClasses
        && !m_pages->confirmCurrentPageCanLeave()
        )
    {
        return;
    }

    if (auto* classes = m_pages->ensureClassesPage(); classes
        && classes->openClass(
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
        m_pages->isCurrentPage(PageType::Classes);

    if (
        !alreadyShowingClasses
        && !m_pages->confirmCurrentPageCanLeave()
        )
    {
        return;
    }

    if (auto* classes = m_pages->ensureClassesPage(); classes
        && classes->openClass(
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
        m_pages->isCurrentPage(PageType::Classes);

    if (
        !alreadyShowingClasses
        && !m_pages->confirmCurrentPageCanLeave()
        )
    {
        return;
    }

    if (auto* classes = m_pages->ensureClassesPage(); classes
        && classes->openClass(
            data.classId,
            ClassesSection::Details
            )
        )
    {
        m_pages->showPage(PageType::Classes);
    }
}
