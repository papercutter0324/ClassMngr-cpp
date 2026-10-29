#include "next/application/document_catalog_projection.h"
#include "next/platform/resource_document_catalog_port.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_diagnostics.h"
#include "next/resources/resource_loader.h"
#include "next/ui/application_shell.h"
#include "next/ui/document_catalog_navigation.h"
#include "next/ui/document_viewer_page.h"
#include "next/ui/document_viewer_route.h"
#include "preferences_test_ports.h"

#include <QCoreApplication>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLineEdit>
#include <QPdfDocument>
#include <QPdfPageNavigator>
#include <QPdfView>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QTranslator>
#include <QUrl>
#include <QtTest/QtTest>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Ui;

namespace
{

QString resourceRoot()
{
    return QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
}

QString resourceCatalogPath()
{
    return QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG);
}

Domain::DocumentId id(const char* value)
{
    return *Domain::DocumentId::fromString(value);
}

Application::DocumentCatalogProjection catalogProjection(
    Resources::ResourceLoader& loader
    )
{
    const Platform::ResourceDocumentCatalogPort port(loader);
    const auto projection = port.projection(QStringLiteral("en_US"));
    if (!projection)
    {
        qFatal("Could not project document catalog: %s",
               projection.error().message.c_str());
    }
    return projection.value();
}

QTreeWidgetItem* findDocumentItem(
    QTreeWidget* tree,
    const Domain::DocumentId& documentId
    )
{
    if (!tree)
    {
        return nullptr;
    }
    QTreeWidgetItemIterator iterator(tree);
    while (*iterator)
    {
        if (DocumentCatalogNavigation::documentIdFor(*iterator)
            == std::optional<Domain::DocumentId>(documentId))
        {
            return *iterator;
        }
        ++iterator;
    }
    return nullptr;
}

QTreeWidgetItem* findCatalogFolder(QTreeWidget* tree)
{
    if (!tree)
    {
        return nullptr;
    }
    QTreeWidgetItemIterator iterator(tree);
    while (*iterator)
    {
        if ((*iterator)->data(
                0,
                static_cast<int>(DocumentCatalogItemRole::Kind)
                ).toInt() == static_cast<int>(DocumentCatalogItemKind::Folder))
        {
            return *iterator;
        }
        ++iterator;
    }
    return nullptr;
}

class EmptyTestPage final : public Page
{
public:
    explicit EmptyTestPage(const PageContext& context)
        : Page(context)
    {
    }
};

struct ViewerHarness final
{
    explicit ViewerHarness(
        Resources::ResourceLoader& loader,
        Application::DocumentCatalogProjection documentProjection
        )
        : projection(std::move(documentProjection))
    {
        shell = std::make_unique<ApplicationShell>(loader, preferences.model);
        shell->setDocumentCatalogProjection(projection);
        const auto registered = registerDocumentViewerPage(shell->pageHost());
        if (!registered)
        {
            qFatal("Could not register viewer page: %s",
                   qPrintable(registered.error().message));
        }
        const auto otherPageId = PageId::fromString(QStringLiteral("viewer_test_elsewhere"));
        const auto otherRegistered = shell->pageHost().registerPage(PageDescriptor{
            .id = *otherPageId,
            .label = QStringLiteral("Elsewhere"),
            .iconResourceId = std::nullopt,
            .requiredPermissions = {},
            .factory = [](const PageContext& context)
                -> PageOperationResult<std::unique_ptr<Page>> {
                return std::make_unique<EmptyTestPage>(context);
            }
        });
        if (!otherRegistered)
        {
            qFatal("Could not register test page: %s",
                   qPrintable(otherRegistered.error().message));
        }
        bindDocumentViewerSelection(*shell);
        shell->resize(1024, 720);
    }

    void select(const Domain::DocumentId& documentId)
    {
        auto* const tree = shell->findChild<QTreeWidget*>(
            QStringLiteral("classMngrNextNavigationTree")
            );
        auto* const item = findDocumentItem(tree, documentId);
        if (!item)
        {
            qFatal("Could not find document row %s", documentId.value().c_str());
        }
        tree->setCurrentItem(item);
        QCoreApplication::processEvents();
    }

    [[nodiscard]] DocumentViewerPage* viewer() const
    {
        return dynamic_cast<DocumentViewerPage*>(
            shell->pageHost().pageInstance(
                QString::fromLatin1(kDocumentViewerPageKey)
                )
            );
    }

    Test::MemoryPreferences preferences;
    Application::DocumentCatalogProjection projection;
    std::unique_ptr<ApplicationShell> shell;
};

bool copyFile(const QString& source, const QString& target);

QString modifiedCorruptPackage(QTemporaryDir& package)
{
    const QString outputCatalog = package.filePath(QStringLiteral("resource-catalog.json"));
    if (!QFile::copy(resourceCatalogPath(), outputCatalog))
    {
        return {};
    }
    const QString selectedBody = package.filePath(
        QStringLiteral("assets/documents/Guides/DYB Lesson Planning Guide.pdf")
        );
    if (!QDir().mkpath(QFileInfo(selectedBody).absolutePath())
        || !QFile::copy(
            QDir(resourceRoot()).filePath(
                QStringLiteral("assets/documents/Guides/DYB Lesson Planning Guide.pdf")
                ),
            selectedBody
            ))
    {
        return {};
    }
    QFile body(selectedBody);
    if (!body.open(QIODevice::ReadWrite))
    {
        return {};
    }
    QByteArray bytes = body.readAll();
    if (bytes.isEmpty())
    {
        return {};
    }
    bytes[0] = bytes.at(0) == 'X' ? 'Y' : 'X';
    body.resize(0);
    if (body.write(bytes) != bytes.size())
    {
        return {};
    }
    body.close();
    return outputCatalog;
}

QString modifiedCorruptOfficeExportPackage(QTemporaryDir& package)
{
    const QString outputCatalog = package.filePath(QStringLiteral("resource-catalog.json"));
    if (!QFile::copy(resourceCatalogPath(), outputCatalog))
    {
        return {};
    }
    const QString primaryRelative =
        QStringLiteral("assets/documents/Lesson Templates/SP+WR Template.pdf");
    const QString exportRelative =
        QStringLiteral("assets/documents/Lesson Templates/SP+WR Template.pptx");
    const QString primaryPath = package.filePath(primaryRelative);
    const QString exportPath = package.filePath(exportRelative);
    if (!copyFile(QDir(resourceRoot()).filePath(primaryRelative), primaryPath)
        || !copyFile(QDir(resourceRoot()).filePath(exportRelative), exportPath))
    {
        return {};
    }
    QFile exportAsset(exportPath);
    if (!exportAsset.open(QIODevice::ReadWrite))
    {
        return {};
    }
    QByteArray bytes = exportAsset.readAll();
    if (bytes.isEmpty())
    {
        return {};
    }
    bytes[0] = bytes.at(0) == 'X' ? 'Y' : 'X';
    exportAsset.resize(0);
    if (exportAsset.write(bytes) != bytes.size())
    {
        return {};
    }
    exportAsset.close();
    return outputCatalog;
}

bool copyFile(const QString& source, const QString& target)
{
    return QDir().mkpath(QFileInfo(target).absolutePath())
        && QFile::copy(source, target);
}

}  // namespace

class DocumentViewerPageTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void typedCatalogSelectionLoadsReadyPdfAndKeepsMetadataCapabilities();
    void replacementClosesDocumentBeforeReleasingStreamAndRequestsFreshContent();
    void closeTimeoutDestroysConsumersBeforeReleasingAndReopensFreshContent();
    void suspensionTimeoutReleasesContentAndLeavesReusableWrappers();
    void folderSelectionAndPageReleaseClearTheActiveSession();
    void unknownAndCorruptSelectionsShowErrorsWithoutRetainingContent();
    void pdfAndOfficeExportsAreAtomicFileBackedCopies();
    void copyCancellationAndCorruptSourcePreserveTheDestination();
    void openAfterSaveAndPrintCapabilityUseDeterministicHandlers();
    void koreanViewerCloseButtonUsesSharedActionTranslation();
    void pageNavigationClampsToTheOneBasedPageRange();
    void zoomEntryButtonsAndFitModesUpdateThePdfView();

private:
    std::unique_ptr<Resources::ResourceLoader> m_loader;
    Application::DocumentCatalogProjection m_projection;
    std::vector<Resources::ResourceDiagnostic> m_diagnostics;
};

void DocumentViewerPageTests::initTestCase()
{
    auto catalog = Resources::ResourceCatalog::load(
        resourceCatalogPath(),
        resourceRoot()
        );
    QVERIFY(catalog);
    m_loader = std::make_unique<Resources::ResourceLoader>(
        std::move(*catalog),
        [this](const Resources::ResourceDiagnostic& diagnostic) {
            m_diagnostics.push_back(diagnostic);
        }
        );
    m_projection = catalogProjection(*m_loader);
}

void DocumentViewerPageTests::
typedCatalogSelectionLoadsReadyPdfAndKeepsMetadataCapabilities()
{
    ViewerHarness harness(*m_loader, m_projection);
    harness.select(id("document_guides_lesson_planning"));
    auto* const viewer = harness.viewer();
    QVERIFY(viewer);
    QTRY_VERIFY_WITH_TIMEOUT(viewer->hasLoadedDocument(), 10000);
    QCOMPARE(viewer->documentStatus(), QPdfDocument::Status::Ready);
    QCOMPARE(viewer->contentSnapshot().phase(), Application::DocumentContentPhase::Ready);
    QVERIFY(viewer->hasActivePrimaryStream());
    QVERIFY(viewer->canPrint());
    QVERIFY(viewer->canSaveCopy());
    QVERIFY(viewer->findChildren<QPdfDocument*>().size() == 1);
    QCOMPARE(
        harness.shell->pageHost().activePageKey(),
        std::optional<QString>(QString::fromLatin1(kDocumentViewerPageKey))
        );
}

void DocumentViewerPageTests::
replacementClosesDocumentBeforeReleasingStreamAndRequestsFreshContent()
{
    ViewerHarness harness(*m_loader, m_projection);
    const Domain::DocumentId firstId = id("document_guides_lesson_planning");
    const Domain::DocumentId secondId = id("document_guides_powerpoint_shortcuts");
    harness.select(firstId);
    auto* const viewer = harness.viewer();
    QVERIFY(viewer);
    QTRY_VERIFY_WITH_TIMEOUT(viewer->hasLoadedDocument(), 10000);

    bool closeSawOpenStream = false;
    bool streamReleaseSawNullDocument = false;
    bool sessionReleaseSawClosedStream = false;
    QStringList lifecycle;
    viewer->setLifecycleObserver(
        [&](const QString& operation, const QString& phase) {
            lifecycle.append(operation + QLatin1Char(':') + phase);
            if (operation == QStringLiteral("qpdf-document-close")
                && phase == QStringLiteral("after"))
            {
                closeSawOpenStream = viewer->documentStatus() == QPdfDocument::Status::Null
                    && viewer->hasActivePrimaryStream();
            }
            if (operation == QStringLiteral("primary-stream-release")
                && phase == QStringLiteral("before"))
            {
                streamReleaseSawNullDocument =
                    viewer->documentStatus() == QPdfDocument::Status::Null
                    && viewer->hasActivePrimaryStream();
            }
            if (operation == QStringLiteral("content-session-release")
                && phase == QStringLiteral("before"))
            {
                sessionReleaseSawClosedStream = !viewer->hasActivePrimaryStream();
            }
        }
        );

    const std::size_t diagnosticsBefore = m_diagnostics.size();
    harness.select(secondId);
    QTRY_VERIFY_WITH_TIMEOUT(viewer->hasLoadedDocument(), 10000);
    QCOMPARE(viewer->contentSnapshot().phase(), Application::DocumentContentPhase::Ready);
    QVERIFY(closeSawOpenStream);
    QVERIFY(streamReleaseSawNullDocument);
    QVERIFY(sessionReleaseSawClosedStream);
    QVERIFY(lifecycle.indexOf(QStringLiteral("qpdf-document-close:after"))
        < lifecycle.indexOf(QStringLiteral("primary-stream-release:before")));
    QVERIFY(lifecycle.indexOf(QStringLiteral("primary-stream-release:after"))
        < lifecycle.indexOf(QStringLiteral("content-session-release:before")));

    const auto opens = std::count_if(
        m_diagnostics.cbegin() + static_cast<std::ptrdiff_t>(diagnosticsBefore),
        m_diagnostics.cend(),
        [](const Resources::ResourceDiagnostic& diagnostic) {
            return diagnostic.scope.kind() == Resources::ResourceScopeKind::ViewerSession
                && diagnostic.succeeded;
        }
        );
    QCOMPARE(opens, std::ptrdiff_t(1));
    QCOMPARE(viewer->findChildren<QPdfDocument*>().size(), 1);

    QVERIFY(viewer->closeDocument());
    QCOMPARE(viewer->documentStatus(), QPdfDocument::Status::Null);
    QVERIFY(!viewer->hasActivePrimaryStream());
    QCOMPARE(viewer->contentSnapshot().phase(), Application::DocumentContentPhase::Released);

    harness.select(firstId);
    QTRY_VERIFY_WITH_TIMEOUT(viewer->hasLoadedDocument(), 10000);
    QCOMPARE(viewer->contentSnapshot().phase(), Application::DocumentContentPhase::Ready);
    QCOMPARE(viewer->findChildren<QPdfDocument*>().size(), 1);
    QVERIFY(viewer->closeDocument());
    viewer->setLifecycleObserver({});
}

void DocumentViewerPageTests::
closeTimeoutDestroysConsumersBeforeReleasingAndReopensFreshContent()
{
    ViewerHarness harness(*m_loader, m_projection);
    const Domain::DocumentId selectedId = id("document_guides_lesson_planning");
    harness.select(selectedId);
    auto* const viewer = harness.viewer();
    QVERIFY(viewer);
    QTRY_VERIFY_WITH_TIMEOUT(viewer->hasLoadedDocument(), 10000);

    QStringList lifecycle;
    bool streamAliveDuringConsumerDestruction = false;
    viewer->setLifecycleObserver(
        [&](const QString& operation, const QString& phase) {
            lifecycle.append(operation + QLatin1Char(':') + phase);
            if (operation == QStringLiteral("qpdf-view-destroy")
                && phase == QStringLiteral("before"))
            {
                streamAliveDuringConsumerDestruction =
                    viewer->hasActivePrimaryStream();
            }
            if (operation == QStringLiteral("qpdf-document-destroy")
                && phase == QStringLiteral("before"))
            {
                streamAliveDuringConsumerDestruction =
                    streamAliveDuringConsumerDestruction
                    && viewer->hasActivePrimaryStream();
            }
        }
        );
    viewer->setCloseWaiterForTesting([] { return false; });
    const std::size_t diagnosticsBefore = m_diagnostics.size();

    QVERIFY(viewer->closeDocument());
    QVERIFY(streamAliveDuringConsumerDestruction);
    QVERIFY(lifecycle.indexOf(QStringLiteral("qpdf-view-destroy:after"))
        < lifecycle.indexOf(QStringLiteral("qpdf-document-destroy:before")));
    QVERIFY(lifecycle.indexOf(QStringLiteral("qpdf-document-destroy:after"))
        < lifecycle.indexOf(QStringLiteral("primary-stream-release:before")));
    QVERIFY(lifecycle.indexOf(QStringLiteral("primary-stream-release:after"))
        < lifecycle.indexOf(QStringLiteral("content-session-release:before")));
    QVERIFY(lifecycle.contains(QStringLiteral("qpdf-wrapper-recreate:after")));
    QCOMPARE(viewer->documentStatus(), QPdfDocument::Status::Null);
    QCOMPARE(viewer->findChildren<QPdfDocument*>().size(), 1);
    QCOMPARE(viewer->findChildren<QPdfView*>().size(), 1);
    QVERIFY(!viewer->hasActivePrimaryStream());
    QCOMPARE(viewer->contentSnapshot().phase(),
             Application::DocumentContentPhase::Released);
    QVERIFY(!viewer->contentSnapshot().reference().has_value());

    viewer->setCloseWaiterForTesting({});
    QVERIFY(viewer->openDocument(selectedId, m_projection));
    QTRY_VERIFY_WITH_TIMEOUT(viewer->hasLoadedDocument(), 10000);
    QCOMPARE(viewer->contentSnapshot().phase(),
             Application::DocumentContentPhase::Ready);
    QVERIFY(viewer->hasActivePrimaryStream());
    QCOMPARE(viewer->findChildren<QPdfDocument*>().size(), 1);
    const auto freshOpens = std::count_if(
        m_diagnostics.cbegin() + static_cast<std::ptrdiff_t>(diagnosticsBefore),
        m_diagnostics.cend(),
        [](const Resources::ResourceDiagnostic& diagnostic) {
            return diagnostic.scope.kind()
                    == Resources::ResourceScopeKind::ViewerSession
                && diagnostic.succeeded;
        }
        );
    QCOMPARE(freshOpens, std::ptrdiff_t(1));
    QVERIFY(viewer->closeDocument());
    viewer->setLifecycleObserver({});
}

void DocumentViewerPageTests::
suspensionTimeoutReleasesContentAndLeavesReusableWrappers()
{
    ViewerHarness harness(*m_loader, m_projection);
    const Domain::DocumentId selectedId = id("document_guides_lesson_planning");
    harness.select(selectedId);
    auto* const viewer = harness.viewer();
    QVERIFY(viewer);
    QTRY_VERIFY_WITH_TIMEOUT(viewer->hasLoadedDocument(), 10000);
    QStringList lifecycle;
    viewer->setLifecycleObserver(
        [&lifecycle](const QString& operation, const QString& phase) {
            lifecycle.append(operation + QLatin1Char(':') + phase);
        }
        );
    viewer->setCloseWaiterForTesting([] { return false; });

    const auto suspended = harness.shell->pageHost().activatePage(
        QStringLiteral("viewer_test_elsewhere")
        );
    QVERIFY(suspended);
    QVERIFY(!viewer->hasActivePrimaryStream());
    QCOMPARE(viewer->documentStatus(), QPdfDocument::Status::Null);
    QCOMPARE(viewer->contentSnapshot().phase(),
             Application::DocumentContentPhase::Released);
    QVERIFY(!viewer->contentSnapshot().reference().has_value());
    QVERIFY(lifecycle.indexOf(QStringLiteral("qpdf-view-destroy:after"))
        < lifecycle.indexOf(QStringLiteral("qpdf-document-destroy:before")));
    QVERIFY(lifecycle.indexOf(QStringLiteral("qpdf-document-destroy:after"))
        < lifecycle.indexOf(QStringLiteral("primary-stream-release:before")));
    QVERIFY(lifecycle.indexOf(QStringLiteral("primary-stream-release:after"))
        < lifecycle.indexOf(QStringLiteral("content-session-release:before")));
    QCOMPARE(viewer->findChildren<QPdfDocument*>().size(), 1);
    QCOMPARE(viewer->findChildren<QPdfView*>().size(), 1);

    QVERIFY(harness.shell->pageHost().activatePage(
        QString::fromLatin1(kDocumentViewerPageKey)
        ));
    viewer->setCloseWaiterForTesting({});
    QVERIFY(viewer->openDocument(selectedId, m_projection));
    QTRY_VERIFY_WITH_TIMEOUT(viewer->hasLoadedDocument(), 10000);
    QCOMPARE(viewer->contentSnapshot().phase(),
             Application::DocumentContentPhase::Ready);
    QVERIFY(viewer->hasActivePrimaryStream());
    QVERIFY(viewer->closeDocument());
    viewer->setLifecycleObserver({});
}

void DocumentViewerPageTests::folderSelectionAndPageReleaseClearTheActiveSession()
{
    QStringList releaseLifecycle;
    ViewerHarness harness(*m_loader, m_projection);
    harness.select(id("document_guides_lesson_planning"));
    auto* const viewer = harness.viewer();
    QVERIFY(viewer);
    QTRY_VERIFY_WITH_TIMEOUT(viewer->hasLoadedDocument(), 10000);

    auto* const tree = harness.shell->findChild<QTreeWidget*>(
        QStringLiteral("classMngrNextNavigationTree")
        );
    QVERIFY(tree);
    QTreeWidgetItem* const folder = findCatalogFolder(tree);
    QVERIFY(folder);
    tree->setCurrentItem(folder);
    QCoreApplication::processEvents();
    QVERIFY(!viewer->hasActivePrimaryStream());
    QCOMPARE(viewer->documentStatus(), QPdfDocument::Status::Null);
    QCOMPARE(viewer->contentSnapshot().phase(), Application::DocumentContentPhase::Released);

    harness.select(id("document_guides_lesson_planning"));
    QTRY_VERIFY_WITH_TIMEOUT(viewer->hasLoadedDocument(), 10000);
    viewer->setLifecycleObserver(
        [&releaseLifecycle](const QString& operation, const QString& phase) {
            releaseLifecycle.append(operation + QLatin1Char(':') + phase);
        }
        );
    const auto release = harness.shell->pageHost().releasePage(
        QString::fromLatin1(kDocumentViewerPageKey)
        );
    QVERIFY(release);
    QVERIFY(releaseLifecycle.indexOf(QStringLiteral("qpdf-document-close:after"))
        < releaseLifecycle.indexOf(QStringLiteral("primary-stream-release:before")));
    QVERIFY(releaseLifecycle.indexOf(QStringLiteral("primary-stream-release:after"))
        < releaseLifecycle.indexOf(QStringLiteral("content-session-release:before")));
    QVERIFY(!harness.shell->pageHost().hasPageInstance(
        QString::fromLatin1(kDocumentViewerPageKey)
        ));
    QCOMPARE(harness.shell->pageHost().activePageKey(), std::nullopt);
}

void DocumentViewerPageTests::
unknownAndCorruptSelectionsShowErrorsWithoutRetainingContent()
{
    {
        ViewerHarness harness(*m_loader, m_projection);
        harness.select(id("document_guides_lesson_planning"));
        auto* const viewer = harness.viewer();
        QVERIFY(viewer);
        QTRY_VERIFY_WITH_TIMEOUT(viewer->hasLoadedDocument(), 10000);

        QVERIFY(!viewer->openDocument(id("document_missing"), m_projection));
        QVERIFY(!viewer->hasActivePrimaryStream());
        QCOMPARE(viewer->documentStatus(), QPdfDocument::Status::Null);
        QCOMPARE(viewer->contentSnapshot().phase(), Application::DocumentContentPhase::Released);
        QVERIFY(!viewer->lastErrorMessage().isEmpty());
    }

    QTemporaryDir package;
    QVERIFY(package.isValid());
    const QString fixtureCatalog = modifiedCorruptPackage(package);
    QVERIFY(!fixtureCatalog.isEmpty());
    auto resourceCatalog = Resources::ResourceCatalog::load(
        fixtureCatalog,
        package.path()
        );
    QVERIFY(resourceCatalog);
    std::vector<Resources::ResourceDiagnostic> diagnostics;
    Resources::ResourceLoader corruptLoader(
        std::move(*resourceCatalog),
        [&diagnostics](const Resources::ResourceDiagnostic& diagnostic) {
            diagnostics.push_back(diagnostic);
        }
        );
    ViewerHarness corruptHarness(corruptLoader, m_projection);
    corruptHarness.select(id("document_guides_lesson_planning"));
    auto* const corruptViewer = corruptHarness.viewer();
    QVERIFY(corruptViewer);
    QVERIFY(!corruptViewer->hasLoadedDocument());
    QVERIFY(!corruptViewer->hasActivePrimaryStream());
    QCOMPARE(corruptViewer->documentStatus(), QPdfDocument::Status::Null);
    QCOMPARE(
        corruptViewer->contentSnapshot().phase(),
        Application::DocumentContentPhase::Released
        );
    QVERIFY(!corruptViewer->lastErrorMessage().isEmpty());
    QCOMPARE(diagnostics.size(), std::size_t(1));
    QVERIFY(!diagnostics.front().succeeded);
}

void DocumentViewerPageTests::pdfAndOfficeExportsAreAtomicFileBackedCopies()
{
    ViewerHarness harness(*m_loader, m_projection);
    const Domain::DocumentId pdfId = id("document_guides_lesson_planning");
    harness.select(pdfId);
    auto* const viewer = harness.viewer();
    QVERIFY(viewer);
    QTRY_VERIFY_WITH_TIMEOUT(viewer->hasLoadedDocument(), 10000);

    QTemporaryDir output;
    QVERIFY(output.isValid());
    const QString pdfCopy = output.filePath(QStringLiteral("guide-copy.pdf"));
    QString error;
    QVERIFY2(viewer->copySelectedExportTo(pdfCopy, &error), qPrintable(error));
    const QString pdfSource = QDir(resourceRoot()).filePath(
        QStringLiteral("assets/documents/Guides/DYB Lesson Planning Guide.pdf")
        );
    QFile expectedPdf(pdfSource);
    QFile actualPdf(pdfCopy);
    QVERIFY(expectedPdf.open(QIODevice::ReadOnly));
    QVERIFY(actualPdf.open(QIODevice::ReadOnly));
    QCOMPARE(actualPdf.readAll(), expectedPdf.readAll());

    harness.select(id("document_lesson_templates_sp_wr"));
    QTRY_VERIFY_WITH_TIMEOUT(viewer->hasLoadedDocument(), 10000);
    const QString officeCopy = output.filePath(QStringLiteral("template-copy.pptx"));
    QVERIFY2(viewer->copySelectedExportTo(officeCopy, &error), qPrintable(error));
    const QString officeSource = QDir(resourceRoot()).filePath(
        QStringLiteral("assets/documents/Lesson Templates/SP+WR Template.pptx")
        );
    QFile expectedOffice(officeSource);
    QFile actualOffice(officeCopy);
    QVERIFY(expectedOffice.open(QIODevice::ReadOnly));
    QVERIFY(actualOffice.open(QIODevice::ReadOnly));
    QCOMPARE(actualOffice.readAll(), expectedOffice.readAll());
}

void DocumentViewerPageTests::copyCancellationAndCorruptSourcePreserveTheDestination()
{
    ViewerHarness normalHarness(*m_loader, m_projection);
    normalHarness.select(id("document_guides_lesson_planning"));
    auto* const normalViewer = normalHarness.viewer();
    QVERIFY(normalViewer);
    QTRY_VERIFY_WITH_TIMEOUT(normalViewer->hasLoadedDocument(), 10000);
    QPushButton* const exportButton = normalViewer->findChild<QPushButton*>(
        QStringLiteral("documentViewerExport")
        );
    QVERIFY(exportButton);

    const std::size_t diagnosticsBeforeCancellation = m_diagnostics.size();
    QTimer::singleShot(0, qApp, [] {
        if (auto* const dialog = qobject_cast<QDialog*>(
                QApplication::activeModalWidget()
                ))
        {
            dialog->reject();
        }
    });
    exportButton->click();
    QCOMPARE(m_diagnostics.size(), diagnosticsBeforeCancellation);
    QVERIFY(normalViewer->hasLoadedDocument());

    QTemporaryDir corruptPackage;
    QVERIFY(corruptPackage.isValid());
    const QString fixtureCatalog = modifiedCorruptOfficeExportPackage(corruptPackage);
    QVERIFY(!fixtureCatalog.isEmpty());
    auto resourceCatalog = Resources::ResourceCatalog::load(
        fixtureCatalog,
        corruptPackage.path()
        );
    QVERIFY(resourceCatalog);
    std::vector<Resources::ResourceDiagnostic> diagnostics;
    Resources::ResourceLoader corruptLoader(
        std::move(*resourceCatalog),
        [&diagnostics](const Resources::ResourceDiagnostic& diagnostic) {
            diagnostics.push_back(diagnostic);
        }
        );
    ViewerHarness corruptHarness(corruptLoader, m_projection);
    corruptHarness.select(id("document_lesson_templates_sp_wr"));
    auto* const corruptExportViewer = corruptHarness.viewer();
    QVERIFY(corruptExportViewer);
    QTRY_VERIFY_WITH_TIMEOUT(corruptExportViewer->hasLoadedDocument(), 10000);

    QTemporaryDir output;
    QVERIFY(output.isValid());
    const QString targetPath = output.filePath(QStringLiteral("preserve-existing.pptx"));
    const QByteArray originalContents("keep this destination intact");
    QFile target(targetPath);
    QVERIFY(target.open(QIODevice::WriteOnly));
    QCOMPARE(target.write(originalContents), originalContents.size());
    target.close();

    QString error;
    QVERIFY(!corruptExportViewer->copySelectedExportTo(targetPath, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(target.open(QIODevice::ReadOnly));
    QCOMPARE(target.readAll(), originalContents);
}

void DocumentViewerPageTests::
openAfterSaveAndPrintCapabilityUseDeterministicHandlers()
{
    ViewerHarness harness(*m_loader, m_projection);
    const Domain::DocumentId printableId = id("document_guides_lesson_planning");
    harness.select(printableId);
    auto* const viewer = harness.viewer();
    QVERIFY(viewer);
    QTRY_VERIFY_WITH_TIMEOUT(viewer->hasLoadedDocument(), 10000);

    auto* const printButton = viewer->findChild<QPushButton*>(
        QStringLiteral("documentViewerPrint")
        );
    auto* const exportButton = viewer->findChild<QPushButton*>(
        QStringLiteral("documentViewerExport")
        );
    QVERIFY(printButton);
    QVERIFY(exportButton);
    int printCalls = 0;
    viewer->setPrintActionHandlerForTesting(
        [&printCalls](QPdfDocument* document, const QString&, int pageIndex) {
            QVERIFY(document);
            QCOMPARE(document->status(), QPdfDocument::Status::Ready);
            QCOMPARE(pageIndex, 0);
            ++printCalls;
        }
        );
    printButton->click();
    QCOMPARE(printCalls, 1);

    QTemporaryDir output;
    QVERIFY(output.isValid());
    const QString openedCopy = output.filePath(QStringLiteral("opened-copy"));
    const QString silentCopy = output.filePath(QStringLiteral("silent-copy"));
    int requestNumber = 0;
    int openCalls = 0;
    QUrl openedUrl;
    viewer->setSaveCopyActionsForTesting(
        [&](const QString&, const QString&) -> std::optional<DocumentViewerPage::SaveCopyRequest> {
            ++requestNumber;
            if (requestNumber == 1)
            {
                return DocumentViewerPage::SaveCopyRequest{
                    .targetPath = openedCopy,
                    .openAfterSaving = true
                };
            }
            return DocumentViewerPage::SaveCopyRequest{
                .targetPath = silentCopy,
                .openAfterSaving = false
            };
        },
        [&](const QUrl& url) {
            ++openCalls;
            openedUrl = url;
            return true;
        }
        );
    exportButton->click();
    QVERIFY(QFile::exists(openedCopy + QStringLiteral(".pdf")));
    QCOMPARE(openCalls, 1);
    QCOMPARE(openedUrl, QUrl::fromLocalFile(openedCopy + QStringLiteral(".pdf")));
    exportButton->click();
    QVERIFY(QFile::exists(silentCopy + QStringLiteral(".pdf")));
    QCOMPARE(openCalls, 1);

    harness.select(id("document_lesson_templates_sp_wr"));
    QTRY_VERIFY_WITH_TIMEOUT(viewer->hasLoadedDocument(), 10000);
    QVERIFY(!viewer->canPrint());
    printButton->click();
    QCOMPARE(printCalls, 1);
    QVERIFY(viewer->closeDocument());
}

void DocumentViewerPageTests::koreanViewerCloseButtonUsesSharedActionTranslation()
{
    QTranslator translator;
    QVERIFY(translator.load(QStringLiteral(CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY)
                            + QStringLiteral("/ClassMngr_ko_KR.qm")));
    QVERIFY(qApp->installTranslator(&translator));
    {
        ViewerHarness harness(*m_loader, m_projection);
        harness.select(id("document_guides_lesson_planning"));
        auto* const viewer = harness.viewer();
        QVERIFY(viewer);
        QPushButton* const closeButton = viewer->findChild<QPushButton*>(
            QStringLiteral("documentViewerClose")
            );
        QVERIFY(closeButton);
        QCOMPARE(closeButton->text(), QString::fromUtf8("닫기"));
        QVERIFY(viewer->closeDocument());
    }
    QVERIFY(qApp->removeTranslator(&translator));
}

void DocumentViewerPageTests::pageNavigationClampsToTheOneBasedPageRange()
{
    ViewerHarness harness(*m_loader, m_projection);
    harness.select(id("document_guides_lesson_planning"));
    auto* const viewer = harness.viewer();
    QVERIFY(viewer);
    QTRY_VERIFY_WITH_TIMEOUT(viewer->hasLoadedDocument(), 10000);
    QTRY_VERIFY_WITH_TIMEOUT(viewer->documentPageCount() > 0, 10000);

    QLineEdit* const pageInput = viewer->findChild<QLineEdit*>(
        QStringLiteral("documentViewerPageInput")
        );
    QVERIFY(pageInput);
    auto* const view = viewer->findChild<QPdfView*>(
        QStringLiteral("documentViewerPdfView")
        );
    QVERIFY(view);
    QVERIFY(view->pageNavigator());

    pageInput->setText(QStringLiteral("9999"));
    QVERIFY(QMetaObject::invokeMethod(pageInput, "editingFinished", Qt::DirectConnection));
    QCOMPARE(view->pageNavigator()->currentPage(), viewer->documentPageCount() - 1);
    pageInput->setText(QStringLiteral("0"));
    QVERIFY(QMetaObject::invokeMethod(pageInput, "editingFinished", Qt::DirectConnection));
    QCOMPARE(view->pageNavigator()->currentPage(), 0);
}

void DocumentViewerPageTests::zoomEntryButtonsAndFitModesUpdateThePdfView()
{
    ViewerHarness harness(*m_loader, m_projection);
    harness.shell->show();
    QCoreApplication::processEvents();
    harness.select(id("document_guides_lesson_planning"));
    auto* const viewer = harness.viewer();
    QVERIFY(viewer);
    QTRY_VERIFY_WITH_TIMEOUT(viewer->hasLoadedDocument(), 10000);

    auto* const view = viewer->findChild<QPdfView*>(
        QStringLiteral("documentViewerPdfView")
        );
    QVERIFY(view);
    QLineEdit* const zoomInput = viewer->findChild<QLineEdit*>(
        QStringLiteral("documentViewerZoomInput")
        );
    QVERIFY(zoomInput);
    QPushButton* const zoomIn = viewer->findChild<QPushButton*>(
        QStringLiteral("documentViewerZoomIn")
        );
    QPushButton* const zoomOut = viewer->findChild<QPushButton*>(
        QStringLiteral("documentViewerZoomOut")
        );
    QPushButton* const fitWidth = viewer->findChild<QPushButton*>(
        QStringLiteral("documentViewerFitWidth")
        );
    QPushButton* const fitPage = viewer->findChild<QPushButton*>(
        QStringLiteral("documentViewerFitPage")
        );
    QVERIFY(zoomIn);
    QVERIFY(zoomOut);
    QVERIFY(fitWidth);
    QVERIFY(fitPage);
    QVERIFY(view->viewport());

    zoomInput->setText(QStringLiteral("150%"));
    QVERIFY(QMetaObject::invokeMethod(zoomInput, "editingFinished", Qt::DirectConnection));
    QCOMPARE(qRound(view->zoomFactor() * 100.0), 150);
    zoomIn->click();
    QCOMPARE(qRound(view->zoomFactor() * 100.0), 180);
    zoomOut->click();
    QCOMPARE(qRound(view->zoomFactor() * 100.0), 150);

    zoomInput->setText(QStringLiteral("900%"));
    QVERIFY(QMetaObject::invokeMethod(zoomInput, "editingFinished", Qt::DirectConnection));
    QCOMPARE(qRound(view->zoomFactor() * 100.0), 300);
    zoomInput->setText(QStringLiteral("5%"));
    QVERIFY(QMetaObject::invokeMethod(zoomInput, "editingFinished", Qt::DirectConnection));
    QCOMPARE(qRound(view->zoomFactor() * 100.0), 25);

    fitWidth->click();
    const QSizeF pageSize = viewer->findChild<QPdfDocument*>()->pagePointSize(0);
    QVERIFY(pageSize.isValid());
    const QMargins margins = view->documentMargins();
    const qreal expectedWidthFactor = std::clamp(
        (view->viewport()->width() - margins.left() - margins.right())
            / (pageSize.width() * std::max(view->logicalDpiX() / 72.0, 0.01)),
        0.25,
        3.0
        );
    QCOMPARE(view->zoomMode(), QPdfView::ZoomMode::Custom);
    QVERIFY(qAbs(view->zoomFactor() - expectedWidthFactor) < 0.01);
    const qreal widthZoomFactor = view->zoomFactor();
    fitPage->click();
    const qreal expectedHeightFactor = std::clamp(
        (view->viewport()->height() - margins.top() - margins.bottom())
            / (pageSize.height() * std::max(view->logicalDpiY() / 72.0, 0.01)),
        0.25,
        3.0
        );
    const qreal expectedPageFactor = std::min(
        expectedWidthFactor,
        expectedHeightFactor
        );
    QCOMPARE(view->zoomMode(), QPdfView::ZoomMode::Custom);
    QVERIFY(qAbs(view->zoomFactor() - expectedPageFactor) < 0.01);
    QVERIFY(view->zoomFactor() <= widthZoomFactor + 0.01);

    QLineEdit* const pageInput = viewer->findChild<QLineEdit*>(
        QStringLiteral("documentViewerPageInput")
        );
    QVERIFY(pageInput);
    pageInput->setText(QStringLiteral("9999"));
    QVERIFY(QMetaObject::invokeMethod(pageInput, "editingFinished", Qt::DirectConnection));
    QCOMPARE(view->pageNavigator()->currentPage(), viewer->documentPageCount() - 1);
    QVERIFY(viewer->closeDocument());
    QVERIFY(viewer->openDocument(
        id("document_guides_lesson_planning"),
        m_projection
        ));
    QTRY_VERIFY_WITH_TIMEOUT(viewer->hasLoadedDocument(), 10000);
    QCOMPARE(view->zoomMode(), QPdfView::ZoomMode::Custom);
    QCOMPARE(view->zoomFactor(), qreal(1.0));
    QCOMPARE(view->pageNavigator()->currentPage(), 0);
}

QTEST_MAIN(DocumentViewerPageTests)

#include "document_viewer_page_tests.moc"
