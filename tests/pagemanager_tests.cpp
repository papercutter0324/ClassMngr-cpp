#include "core/application_services.h"
#include "features/classes/ui/classes_page.h"
#include "features/calendar/ui/calendar_page.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "features/schedule/ui/schedule_widget.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/pages/pdf_viewer_page.h"

#include <QFileInfo>
#include <QtTest>

class PageManagerTests : public QObject
{
    Q_OBJECT

private slots:
    void heavyPagesAreDeferredAndReused();
    void pageWidgetDescendantCountsAreOnDemandValues();
    void scheduleWidgetsAreCreatedOnlyForOpenedScheduleViews();
    void registeredPagesAreCreatedOnFirstUse();
    void preparingCalendarDoesNotActivateItsHiddenTab();
    void leavingPdfViewerReleasesTheDocument();
    void referencedPdfLoadTracksReadyAndRelease();
    void referencedPdfFailureCanStartANewGeneration();
    void referencedPdfReplacementUsesTheCurrentGeneration();
};

void PageManagerTests::heavyPagesAreDeferredAndReused()
{
    ApplicationServices services;
    PageManager pages;

    int pdfPageCreations = 0;

    connect(
        &pages,
        &PageManager::pageCreated,
        &pages,
        [&pdfPageCreations](PageType type, BasePage*)
        {
            if (type == PageType::PdfViewer)
            {
                ++pdfPageCreations;
            }
        }
        );

    pages.initialize(&services, false);

    QVERIFY(pages.isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages.registeredPageCount(), 11);
    QCOMPARE(pages.instantiatedPageCount(), 1);

    for (const PageType type : {
             PageType::MyClasses,
             PageType::Schedule,
             PageType::Classes,
             PageType::TestingClasses,
             PageType::TeacherInfo,
             PageType::NativeEnglishTeachers,
             PageType::GsTeam,
             PageType::CampusDashboard,
             PageType::SubPrep,
             PageType::PdfViewer
         })
    {
        QVERIFY(!pages.isPageInstantiated(type));
    }

    pages.setDatabaseOpen(false);
    pages.clearDatabaseState();
    pages.refreshAll();
    pages.retranslatePages();

    pages.showPage(PageType::PdfViewer);

    auto* firstViewer = pages.pdfViewerPage();

    QVERIFY(firstViewer);
    QVERIFY(pages.isCurrentPage(PageType::PdfViewer));
    QVERIFY(pages.isPageInstantiated(PageType::PdfViewer));
    QCOMPARE(pdfPageCreations, 1);
    QVERIFY(!pages.isPageInstantiated(PageType::Classes));
    QVERIFY(!pages.isPageInstantiated(PageType::CampusDashboard));

    pages.showPage(PageType::MyWorkspace);
    pages.showPage(PageType::PdfViewer);

    QCOMPARE(pages.pdfViewerPage(), firstViewer);
    QCOMPARE(pdfPageCreations, 1);
}

void PageManagerTests::pageWidgetDescendantCountsAreOnDemandValues()
{
    ApplicationServices services;
    PageManager pages;
    pages.initialize(&services, false);

    const auto countForPage =
        [](const QList<PageWidgetDescendantCount>& counts,
           const QString& pageKey)
        {
            for (const PageWidgetDescendantCount& count : counts)
            {
                if (count.pageKey == pageKey)
                {
                    return count.descendantWidgetCount;
                }
            }
            return -1;
        };

    const QString workspaceKey =
        PageManager::pageTypeIdentifier(PageType::MyWorkspace);
    const QString classesKey =
        PageManager::pageTypeIdentifier(PageType::Classes);
    const int registeredPageCount = pages.registeredPageCount();
    const QList<PageType> allPageTypes{
        PageType::MyWorkspace,
        PageType::MyClasses,
        PageType::Schedule,
        PageType::Classes,
        PageType::TestingClasses,
        PageType::TeacherInfo,
        PageType::NativeEnglishTeachers,
        PageType::GsTeam,
        PageType::CampusDashboard,
        PageType::SubPrep,
        PageType::PdfViewer
    };
    const auto pageKeysFor =
        [](
            const QList<PageWidgetDescendantCount>& counts
            )
        {
            QList<QString> reportedKeys;
            for (const PageWidgetDescendantCount& count : counts)
            {
                reportedKeys.append(count.pageKey);
            }
            return reportedKeys;
        };
    const auto instantiatedPageKeys =
        [&pages, &allPageTypes]()
        {
            QList<QString> instantiatedKeys;
            for (const PageType type : allPageTypes)
            {
                if (pages.isPageInstantiated(type))
                {
                    instantiatedKeys.append(
                        PageManager::pageTypeIdentifier(type)
                        );
                }
            }
            return instantiatedKeys;
        };

    const QList<PageWidgetDescendantCount> initialCounts =
        pages.instantiatedPageWidgetDescendantCounts();
    QCOMPARE(pageKeysFor(initialCounts), instantiatedPageKeys());
    QCOMPARE(initialCounts.size(), 1);
    QCOMPARE(countForPage(initialCounts, classesKey), -1);
    QCOMPARE(pages.instantiatedPageCount(), 1);
    QVERIFY(pages.isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(
        countForPage(initialCounts, workspaceKey),
        pages.myWorkspacePage()->findChildren<QWidget*>().size()
        );

    auto* hiddenContainer = new QWidget(pages.myWorkspacePage());
    new QWidget(hiddenContainer);
    hiddenContainer->hide();
    const QList<PageWidgetDescendantCount> withTemporaryWidget =
        pages.instantiatedPageWidgetDescendantCounts();
    QCOMPARE(pageKeysFor(withTemporaryWidget), instantiatedPageKeys());
    const int temporaryWidgetCount =
        countForPage(withTemporaryWidget, workspaceKey);
    QCOMPARE(
        temporaryWidgetCount,
        countForPage(initialCounts, workspaceKey) + 2
        );
    delete hiddenContainer;

    const QList<PageWidgetDescendantCount> afterSynchronousDelete =
        pages.instantiatedPageWidgetDescendantCounts();
    QCOMPARE(
        countForPage(afterSynchronousDelete, workspaceKey),
        countForPage(initialCounts, workspaceKey)
        );
    // A prior report remains an integer/key snapshot after the widget dies.
    QCOMPARE(
        countForPage(withTemporaryWidget, workspaceKey),
        temporaryWidgetCount
        );
    QCOMPARE(pages.instantiatedPageCount(), 1);
    QCOMPARE(pages.registeredPageCount(), registeredPageCount);
    QVERIFY(pages.isCurrentPage(PageType::MyWorkspace));

    pages.showPage(PageType::Classes);
    BasePage* classesPage = pages.classesPage();
    QVERIFY(classesPage);
    const QList<PageWidgetDescendantCount> withClassesPage =
        pages.instantiatedPageWidgetDescendantCounts();
    QCOMPARE(pageKeysFor(withClassesPage), instantiatedPageKeys());
    QCOMPARE(withClassesPage.size(), 2);
    QCOMPARE(
        countForPage(withClassesPage, classesKey),
        classesPage->findChildren<QWidget*>().size()
        );
    QCOMPARE(pages.instantiatedPageCount(), 2);
    QVERIFY(pages.isCurrentPage(PageType::Classes));

    pages.showPage(PageType::MyWorkspace);
    const QList<PageWidgetDescendantCount> afterLeave =
        pages.instantiatedPageWidgetDescendantCounts();
    QCOMPARE(pageKeysFor(afterLeave), instantiatedPageKeys());
    QCOMPARE(afterLeave.size(), 2);
    QCOMPARE(pages.instantiatedPageCount(), 2);
    QVERIFY(pages.isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(
        countForPage(afterLeave, classesKey),
        classesPage->findChildren<QWidget*>().size()
        );

    pages.showPage(PageType::Classes);
    QCOMPARE(pages.instantiatedPageCount(), 2);
    QVERIFY(pages.isCurrentPage(PageType::Classes));
    QCOMPARE(
        countForPage(pages.instantiatedPageWidgetDescendantCounts(), classesKey),
        classesPage->findChildren<QWidget*>().size()
        );
}

void PageManagerTests::scheduleWidgetsAreCreatedOnlyForOpenedScheduleViews()
{
    ApplicationServices services;
    PageManager pages;
    pages.initialize(&services, false);

    QCOMPARE(pages.findChildren<ScheduleWidget*>().size(), 1);

    pages.showPage(PageType::Schedule);
    QCOMPARE(pages.findChildren<ScheduleWidget*>().size(), 2);

    pages.showPage(PageType::MyWorkspace);
    QCOMPARE(pages.findChildren<ScheduleWidget*>().size(), 2);

    pages.showPage(PageType::SubPrep);
    QCOMPARE(pages.findChildren<ScheduleWidget*>().size(), 3);
}

void PageManagerTests::registeredPagesAreCreatedOnFirstUse()
{
    ApplicationServices services;
    PageManager pages;
    pages.initialize(&services, false);
    pages.setDatabaseOpen(false);

    const QList<PageType> deferredPages{
        PageType::MyClasses,
        PageType::Schedule,
        PageType::Classes,
        PageType::TestingClasses,
        PageType::TeacherInfo,
        PageType::NativeEnglishTeachers,
        PageType::GsTeam,
        PageType::CampusDashboard,
        PageType::SubPrep,
        PageType::PdfViewer
    };

    for (const PageType type : deferredPages)
    {
        QVERIFY(!pages.isPageInstantiated(type));

        pages.showPage(type);

        QVERIFY(pages.isPageInstantiated(type));
        QVERIFY(pages.isCurrentPage(type));
    }

    QCOMPARE(
        pages.instantiatedPageCount(),
        pages.registeredPageCount()
        );
}

void PageManagerTests::preparingCalendarDoesNotActivateItsHiddenTab()
{
    ApplicationServices services;
    PageManager pages;
    pages.initialize(&services, false);
    pages.setDatabaseOpen(true);

    auto* workspace = pages.myWorkspacePage();
    QVERIFY(workspace);
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Schedule);

    CalendarPage* calendar = pages.ensureCalendarPage();
    QVERIFY(calendar);
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Schedule);
    QVERIFY(calendar->needsRefresh());
}

void PageManagerTests::leavingPdfViewerReleasesTheDocument()
{
    ApplicationServices services;
    PageManager pages;
    pages.initialize(&services, false);

    const QString pdfPath =
        QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral(
            "/resources/assets/documents/Guides/DYB Lesson Planning Guide.pdf"
            );
    QVERIFY(QFileInfo::exists(pdfPath));

    pages.showPage(PageType::PdfViewer);
    auto* viewer = pages.pdfViewerPage();
    QVERIFY(viewer);
    QVERIFY(
        viewer->loadPdf(
            {
                .pdfFilePath = pdfPath,
                .exportEnabled = true,
                .exportFilePath = pdfPath,
                .exportFileName = QStringLiteral("lesson-planning-guide.pdf"),
                .printEnabled = true
            }
            )
        );
    QTRY_VERIFY_WITH_TIMEOUT(viewer->hasLoadedDocument(), 5000);
    QVERIFY(pages.outputCapabilities().printEnabled);
    QVERIFY(pages.outputCapabilities().saveAsEnabled);

    pages.showPage(PageType::MyWorkspace);

    QVERIFY(!viewer->hasLoadedDocument());
    QVERIFY(viewer->currentFilePath().isEmpty());
    QVERIFY(!pages.outputCapabilities().printEnabled);
    QVERIFY(!pages.outputCapabilities().saveAsEnabled);

    pages.showPage(PageType::PdfViewer);
    QCOMPARE(pages.pdfViewerPage(), viewer);
    QVERIFY(!viewer->hasLoadedDocument());
}

void PageManagerTests::referencedPdfLoadTracksReadyAndRelease()
{
    using ClassMngr::Next::Application::DocumentContentPhase;
    using ClassMngr::Next::Application::DocumentContentReference;

    ApplicationServices services;
    PageManager pages;
    pages.initialize(&services, false);

    const QString pdfPath =
        QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral(
            "/resources/assets/documents/Guides/DYB Lesson Planning Guide.pdf"
            );
    QVERIFY(QFileInfo::exists(pdfPath));

    pages.showPage(PageType::PdfViewer);
    auto* viewer = pages.pdfViewerPage();
    QVERIFY(viewer);
    QCOMPARE(
        viewer->documentContentSnapshot().phase(),
        DocumentContentPhase::Idle
        );

    QVERIFY(
        viewer->loadPdf(
            {
                .pdfFilePath = pdfPath,
                .contentReference = DocumentContentReference(
                    "resource://documents/lesson-planning.pdf"
                    )
            }
            )
        );
    const auto phaseAfterLoad =
        viewer->documentContentSnapshot().phase();
    QVERIFY(
        phaseAfterLoad == DocumentContentPhase::Loading
        || phaseAfterLoad == DocumentContentPhase::Ready
        );
    QTRY_COMPARE_WITH_TIMEOUT(
        viewer->documentContentSnapshot().phase(),
        DocumentContentPhase::Ready,
        5000
        );
    QVERIFY(viewer->hasLoadedDocument());

    pages.showPage(PageType::MyWorkspace);

    const auto released = viewer->documentContentSnapshot();
    QCOMPARE(released.phase(), DocumentContentPhase::Released);
    QVERIFY(!released.contentReference().has_value());
    QVERIFY(!released.error().has_value());
}

void PageManagerTests::referencedPdfFailureCanStartANewGeneration()
{
    using ClassMngr::Next::Application::DocumentContentPhase;
    using ClassMngr::Next::Application::DocumentContentReference;
    using ClassMngr::Next::Domain::ErrorCode;

    ApplicationServices services;
    PageManager pages;
    pages.initialize(&services, false);
    pages.showPage(PageType::PdfViewer);

    auto* viewer = pages.pdfViewerPage();
    QVERIFY(viewer);

    const DocumentContentReference failedReference(
        "resource://documents/missing.pdf"
        );
    QVERIFY(
        !viewer->loadPdf(
            {
                .pdfFilePath = QStringLiteral(
                    CLASSMNGR_SOURCE_DIR "/missing-document.pdf"
                    ),
                .contentReference = failedReference
            }
            )
        );

    const auto failed = viewer->documentContentSnapshot();
    QCOMPARE(failed.phase(), DocumentContentPhase::Failed);
    QCOMPARE(failed.contentReference(), failedReference);
    QVERIFY(failed.error().has_value());
    QCOMPARE(failed.error()->code, ErrorCode::Technical);
    QVERIFY(!failed.error()->message.empty());

    const QString pdfPath =
        QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral(
            "/resources/assets/documents/Guides/DYB Lesson Planning Guide.pdf"
            );
    const DocumentContentReference replacementReference(
        "resource://documents/replacement.pdf"
        );
    QVERIFY(
        viewer->loadPdf(
            {
                .pdfFilePath = pdfPath,
                .contentReference = replacementReference
            }
            )
        );
    QCOMPARE(
        viewer->documentContentSnapshot().contentReference(),
        replacementReference
        );
    QVERIFY(!viewer->documentContentSnapshot().error().has_value());
    QTRY_COMPARE_WITH_TIMEOUT(
        viewer->documentContentSnapshot().phase(),
        DocumentContentPhase::Ready,
        5000
        );
}

void PageManagerTests::referencedPdfReplacementUsesTheCurrentGeneration()
{
    using ClassMngr::Next::Application::DocumentContentPhase;
    using ClassMngr::Next::Application::DocumentContentReference;

    ApplicationServices services;
    PageManager pages;
    pages.initialize(&services, false);
    pages.showPage(PageType::PdfViewer);

    auto* viewer = pages.pdfViewerPage();
    QVERIFY(viewer);

    const QString pdfPath =
        QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral(
            "/resources/assets/documents/Guides/DYB Lesson Planning Guide.pdf"
            );
    QVERIFY(
        viewer->loadPdf(
            {
                .pdfFilePath = pdfPath,
                .contentReference = DocumentContentReference(
                    "resource://documents/first.pdf"
                    )
            }
            )
        );

    const DocumentContentReference replacementReference(
        "resource://documents/second.pdf"
        );
    QVERIFY(
        viewer->loadPdf(
            {
                .pdfFilePath = pdfPath,
                .contentReference = replacementReference
            }
            )
        );
    QTRY_COMPARE_WITH_TIMEOUT(
        viewer->documentContentSnapshot().phase(),
        DocumentContentPhase::Ready,
        5000
        );
    QCOMPARE(
        viewer->documentContentSnapshot().contentReference(),
        replacementReference
        );

    viewer->releaseDocument();
    QCOMPARE(
        viewer->documentContentSnapshot().phase(),
        DocumentContentPhase::Released
        );

    const DocumentContentReference reopenedReference(
        "resource://documents/third.pdf"
        );
    QVERIFY(
        viewer->loadPdf(
            {
                .pdfFilePath = pdfPath,
                .contentReference = reopenedReference
            }
            )
        );
    QTRY_COMPARE_WITH_TIMEOUT(
        viewer->documentContentSnapshot().phase(),
        DocumentContentPhase::Ready,
        5000
        );
    QCOMPARE(
        viewer->documentContentSnapshot().contentReference(),
        reopenedReference
        );
}

QTEST_MAIN(PageManagerTests)

#include "pagemanager_tests.moc"
