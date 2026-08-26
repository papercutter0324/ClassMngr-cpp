#include "core/application_services.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/pages/pdf_viewer_page.h"

#include <QFileInfo>
#include <QtTest>

#include <algorithm>

class PageManagerTests : public QObject
{
    Q_OBJECT

private slots:
    void heavyPagesAreDeferredAndReused();
    void registeredPagesAreCreatedOnFirstUse();
    void leavingPdfViewerReleasesTheDocument();
    void lifecycleReportsUncreatedHiddenAndCurrentPages();
};

void PageManagerTests::heavyPagesAreDeferredAndReused()
{
    MemoryUsageDiagnostics::enable();
    MemoryUsageDiagnostics::history().clear();
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
    const QList<MemoryUsageHistoryEntry>& events =
        MemoryUsageDiagnostics::history().entries();
    QVERIFY(std::any_of(
        events.cbegin(),
        events.cend(),
        [](const MemoryUsageHistoryEntry& entry)
        {
            return entry.kind == MemoryUsageHistoryEntryKind::Event
                && entry.eventType == QStringLiteral("timing")
                && entry.eventDetail.contains(
                    QStringLiteral("page-construction")
                    );
        }
        ));
    QVERIFY(std::any_of(
        events.cbegin(),
        events.cend(),
        [](const MemoryUsageHistoryEntry& entry)
        {
            return entry.kind == MemoryUsageHistoryEntryKind::Event
                && entry.eventType == QStringLiteral("timing")
                && entry.eventDetail.contains(
                    QStringLiteral("page-activation")
                    );
        }
        ));
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

void PageManagerTests::leavingPdfViewerReleasesTheDocument()
{
    MemoryUsageDiagnostics::enable();
    MemoryUsageDiagnostics::history().clear();
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
    const MemoryBreakdownEntry loadedAttribution =
        viewer->memoryBreakdown().constFirst();
    QCOMPARE(loadedAttribution.itemCount, quint64(1));
    QVERIFY(loadedAttribution.retainedBytes > 0);
    QVERIFY(pages.outputCapabilities().printEnabled);
    QVERIFY(pages.outputCapabilities().saveAsEnabled);

    pages.showPage(PageType::MyWorkspace);

    QVERIFY(!viewer->hasLoadedDocument());
    QVERIFY(viewer->currentFilePath().isEmpty());
    const MemoryBreakdownEntry releasedAttribution =
        viewer->memoryBreakdown().constFirst();
    QCOMPARE(releasedAttribution.itemCount, quint64(0));
    QCOMPARE(releasedAttribution.retainedBytes, quint64(0));
    QVERIFY(!pages.outputCapabilities().printEnabled);
    QVERIFY(!pages.outputCapabilities().saveAsEnabled);

    pages.showPage(PageType::PdfViewer);
    QCOMPARE(pages.pdfViewerPage(), viewer);
    QVERIFY(!viewer->hasLoadedDocument());
    const QList<MemoryUsageHistoryEntry>& events =
        MemoryUsageDiagnostics::history().entries();
    QVERIFY(std::any_of(
        events.cbegin(),
        events.cend(),
        [](const MemoryUsageHistoryEntry& entry)
        {
            return entry.kind == MemoryUsageHistoryEntryKind::Event
                && entry.eventType == QStringLiteral("timing")
                && entry.eventDetail.contains(QStringLiteral("pdf-open"));
        }
        ));
    QVERIFY(std::any_of(
        events.cbegin(),
        events.cend(),
        [](const MemoryUsageHistoryEntry& entry)
        {
            return entry.kind == MemoryUsageHistoryEntryKind::Event
                && entry.eventType == QStringLiteral("timing")
                && entry.eventDetail.contains(QStringLiteral("pdf-release"));
        }
        ));
}

void PageManagerTests::lifecycleReportsUncreatedHiddenAndCurrentPages()
{
    ApplicationServices services;
    PageManager pages;
    pages.initialize(&services, false);

    const auto findPage = [](const QList<PageLifecycleEntry>& entries,
                             const QString& identifier)
    {
        return std::find_if(
            entries.cbegin(),
            entries.cend(),
            [&identifier](const PageLifecycleEntry& entry)
            {
                return entry.pageIdentifier == identifier;
            }
            );
    };

    const QList<PageLifecycleEntry> initial = pages.pageLifecycle();
    QVERIFY(findPage(initial, QStringLiteral("calendar")) == initial.cend());

    const auto workspace = findPage(initial, QStringLiteral("my-workspace"));
    QVERIFY(workspace != initial.cend());
    QCOMPARE(workspace->state, PageLifecycleState::Current);
    QVERIFY(workspace->createdAt.isValid());
    QVERIFY(workspace->lastActivatedAt.isValid());

    pages.showPage(PageType::PdfViewer);
    const QList<PageLifecycleEntry> afterNavigation = pages.pageLifecycle();
    const auto hiddenWorkspace = findPage(
        afterNavigation,
        QStringLiteral("my-workspace")
        );
    QVERIFY(hiddenWorkspace != afterNavigation.cend());
    QCOMPARE(hiddenWorkspace->state, PageLifecycleState::Hidden);
    const auto viewer = findPage(afterNavigation, QStringLiteral("pdf-viewer"));
    QVERIFY(viewer != afterNavigation.cend());
    QCOMPARE(viewer->state, PageLifecycleState::Current);
    QVERIFY(viewer->createdAt.isValid());
}

QTEST_MAIN(PageManagerTests)

#include "pagemanager_tests.moc"
