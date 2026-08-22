#include "core/application_services.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/pages/pdf_viewer_page.h"

#include <QFileInfo>
#include <QtTest>

class PageManagerTests : public QObject
{
    Q_OBJECT

private slots:
    void heavyPagesAreDeferredAndReused();
    void leavingPdfViewerReleasesTheDocument();
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

    QVERIFY(pages.isCurrentPage(PageType::PersonalDetails));
    QVERIFY(!pages.isPageInstantiated(PageType::Calendar));
    QVERIFY(!pages.isPageInstantiated(PageType::Classes));
    QVERIFY(!pages.isPageInstantiated(PageType::CampusDashboard));
    QVERIFY(!pages.isPageInstantiated(PageType::PdfViewer));

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
    QVERIFY(!pages.isPageInstantiated(PageType::Calendar));
    QVERIFY(!pages.isPageInstantiated(PageType::Classes));
    QVERIFY(!pages.isPageInstantiated(PageType::CampusDashboard));

    pages.showPage(PageType::PersonalDetails);
    pages.showPage(PageType::PdfViewer);

    QCOMPARE(pages.pdfViewerPage(), firstViewer);
    QCOMPARE(pdfPageCreations, 1);
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

    pages.showPage(PageType::PersonalDetails);

    QVERIFY(!viewer->hasLoadedDocument());
    QVERIFY(viewer->currentFilePath().isEmpty());
    QVERIFY(!pages.outputCapabilities().printEnabled);
    QVERIFY(!pages.outputCapabilities().saveAsEnabled);

    pages.showPage(PageType::PdfViewer);
    QCOMPARE(pages.pdfViewerPage(), viewer);
    QVERIFY(!viewer->hasLoadedDocument());
}

QTEST_MAIN(PageManagerTests)

#include "pagemanager_tests.moc"
