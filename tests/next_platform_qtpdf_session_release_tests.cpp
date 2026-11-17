#include "next/platform/qt_pdf_viewer_session.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_diagnostics.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPdfView>
#include <QPdfPageNavigator>
#include <QSet>
#include <QPointer>
#include <QStringList>
#include <QTemporaryDir>
#include <QVBoxLayout>
#include <QWidget>
#include <QtTest/QtTest>

#include <memory>
#include <vector>

using namespace ClassMngr::Next;

namespace
{
constexpr auto kReference = "resource://documents/Guides/DYB Lesson Planning Guide.pdf";
constexpr auto kBodyId = "document.body:Guides/DYB Lesson Planning Guide.pdf";

class FixedDiagnosticMemory final : public ProcessMemorySnapshotProvider
{
public:
    ProcessMemorySnapshot snapshot() const override
    {
        ProcessMemorySnapshot memory;
        memory.platform = QStringLiteral("windows");
        memory.isAvailable = true;
        memory.workingSetBytes = 4096;
        memory.workingSetBytesAvailable = true;
        memory.capturedAt = QDateTime::fromString(
            QStringLiteral("2026-11-17T12:00:00.000Z"), Qt::ISODateWithMs);
        return memory;
    }
};

std::unique_ptr<Resources::ResourceLoader> loader(
    Resources::ResourceDiagnosticsSink sink = {})
{
    auto catalog = Resources::ResourceCatalog::load(
        QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
        QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT));
    if (!catalog) { return {}; }
    return std::make_unique<Resources::ResourceLoader>(std::move(*catalog), std::move(sink));
}

}

class NextPlatformQtPdfSessionReleaseTests final : public QObject
{
    Q_OBJECT
private slots:
    void structuredDiagnosticsCoverReleaseReopenAndFinalTeardown();
    void navigationDetailsAreBoundedAndReleaseReportsSuppressedSelections();
    void constructionIsLazy();
    void closeAndReplacementUseFreshDocumentAndStreamGeneration();
    void failedOpenKeepsAnEmptyBoundViewAndCanReopen();
    void destroyingAnActiveOwnerClosesConsumersBeforeStream();
    void timeoutDestroysViewThenDocumentThenStreamAndReopens();
    void queuedStatusesCannotReachReplacementOrClosedSession();
    void failedFilenameLoadErrorIsDeferredAndCanReleaseSafely();
    void synchronousPrintBorrowProtectsTheActiveDocument();
};

void NextPlatformQtPdfSessionReleaseTests::structuredDiagnosticsCoverReleaseReopenAndFinalTeardown()
{
    auto resources = loader();
    QVERIFY(resources);
    QWidget parent;
    QVector<QJsonObject> events;
    auto diagnostics = std::make_shared<ProcessMemoryDiagnostics>(nullptr,
        [&](const QByteArray& bytes) { events.append(QJsonDocument::fromJson(bytes).object()); });
    {
        Platform::QtPdfViewerSession session(*resources, &parent, diagnostics);
        QVERIFY(session.open(Domain::DocumentContentReference(kReference),
                             QStringLiteral("private-label-must-not-be-logged")));
        QTRY_VERIFY_WITH_TIMEOUT(session.isReady(), 10000);
        session.view()->pageNavigator()->jump(1, {}, 1.0);
        QVERIFY(session.release());
        QVERIFY(session.open(Domain::DocumentContentReference(kReference),
                             QStringLiteral("private-reopen-label")));
        QTRY_VERIFY_WITH_TIMEOUT(session.isReady(), 10000);
    }
    QStringList phases;
    QSet<qint64> loadedGenerations;
    bool statusReady = false;
    bool pageSelection = false;
    int retiredLoadedDocuments = 0;
    for (const auto& event : events)
    {
        const QString name = event.value(QStringLiteral("event")).toString();
        const auto details = event.value(QStringLiteral("details")).toObject();
        const QString phase = details.value(QStringLiteral("phase")).toString();
        phases.append(name + QLatin1Char(':') + phase);
        if (name == QStringLiteral("qpdf-document-load") && phase == QStringLiteral("after"))
        {
            loadedGenerations.insert(details.value(QStringLiteral("generation")).toInteger());
            QVERIFY(details.value(QStringLiteral("active_stream")).toBool());
        }
        if (name == QStringLiteral("qpdf-document-status") && phase == QStringLiteral("published"))
            statusReady |= details.value(QStringLiteral("status")).toInt()
                == static_cast<int>(QPdfDocument::Status::Ready);
        if (name == QStringLiteral("qpdf-view-page-selection"))
        {
            pageSelection = true;
            QVERIFY(details.value(QStringLiteral("render_observation")).toString()
                .contains(QStringLiteral("completion unavailable")));
        }
        if (name == QStringLiteral("qpdf-document-destroy") && phase == QStringLiteral("after")
            && details.value(QStringLiteral("retired_page_count_before_close")).toInt() > 0)
        {
            ++retiredLoadedDocuments;
            QVERIFY(loadedGenerations.contains(
                details.value(QStringLiteral("retired_generation")).toInteger()));
            QVERIFY(!details.value(QStringLiteral("retired_document_present")).toBool());
        }
        QVERIFY(!event.value(QStringLiteral("memory")).toObject().isEmpty());
        QVERIFY(!QJsonDocument(event).toJson().contains("private-label"));
        QVERIFY(!QJsonDocument(event).toJson().contains("private-reopen"));
    }
    QCOMPARE(loadedGenerations.size(), 2);
    QCOMPARE(retiredLoadedDocuments, 2);
    QVERIFY(statusReady);
    QVERIFY(pageSelection);
    const int finalView = phases.lastIndexOf(QStringLiteral("qpdf-view-destroy:after"));
    const int finalDocument = phases.lastIndexOf(QStringLiteral("qpdf-document-destroy:after"));
    const int finalStream = phases.lastIndexOf(QStringLiteral("primary-stream-release:after"));
    QVERIFY(finalView >= 0);
    QVERIFY(finalView < finalDocument);
    QVERIFY(finalDocument < finalStream);
    QCOMPARE(phases.back(), QStringLiteral("qpdf-session-release:after"));
    const auto final = events.back().value(QStringLiteral("details")).toObject();
    QVERIFY(!final.value(QStringLiteral("active_stream")).toBool());
    QVERIFY(!final.value(QStringLiteral("document_present")).toBool());
    QVERIFY(!final.value(QStringLiteral("view_present")).toBool());
}

void NextPlatformQtPdfSessionReleaseTests::navigationDetailsAreBoundedAndReleaseReportsSuppressedSelections()
{
    auto resources = loader();
    QVERIFY(resources);
    QWidget parent;
    QVector<QJsonObject> events;
    auto diagnostics = std::make_shared<ProcessMemoryDiagnostics>(std::make_unique<FixedDiagnosticMemory>(),
        [&](const QByteArray& bytes) { events.append(QJsonDocument::fromJson(bytes).object()); });
    Platform::QtPdfViewerSession session(*resources, &parent, diagnostics);
    QVERIFY(session.open(Domain::DocumentContentReference(kReference), QStringLiteral("Viewer:bounded-navigation")));
    QTRY_VERIFY_WITH_TIMEOUT(session.isReady(), 10000);
    QVERIFY(session.pageCount() > 1);
    auto* const navigator = session.view()->pageNavigator();
    QVERIFY(navigator);
    QCOMPARE(navigator->currentPage(), 0);
    const qint64 initialSelections = events.back().value(QStringLiteral("details")).toObject()
        .value(QStringLiteral("page_selection_count")).toInteger();
    const bool initialBackAvailable = navigator->backAvailable();
    const bool initialForwardAvailable = navigator->forwardAvailable();
    QSignalSpy changedPages(navigator, &QPdfPageNavigator::currentPageChanged);
    for (int index = 0; index < 40; ++index)
    {
        const int page = index % 2 == 0 ? 1 : 0;
        // Exercise changing observations without adding jump-history entries
        // or requesting a rendering scroll for each counter observation.
        navigator->update(page, {}, 1.0);
        QCOMPARE(navigator->currentPage(), page);
    }
    QCOMPARE(changedPages.size(), 40);
    QCOMPARE(navigator->backAvailable(), initialBackAvailable);
    QCOMPARE(navigator->forwardAvailable(), initialForwardAvailable);
    QVERIFY(session.release());

    int selectionDetails = 0;
    QJsonObject released;
    for (const auto& event : events)
    {
        QCOMPARE(event.value(QStringLiteral("platform")).toString(), QStringLiteral("windows"));
        QCOMPARE(event.value(QStringLiteral("memory")).toObject()
            .value(QStringLiteral("working_set_bytes")).toInteger(), 4096);
        const auto details = event.value(QStringLiteral("details")).toObject();
        if (event.value(QStringLiteral("event")) == QStringLiteral("qpdf-view-page-selection"))
        {
            ++selectionDetails;
            QCOMPARE(details.value(QStringLiteral("page_selection_count")).toInt(), selectionDetails);
            QCOMPARE(details.value(QStringLiteral("page_selection_details_suppressed")).toInt(), 0);
        }
        if (event.value(QStringLiteral("event")) == QStringLiteral("qpdf-generation-release")
            && details.value(QStringLiteral("phase")) == QStringLiteral("after"))
            released = details;
    }
    QCOMPARE(selectionDetails, 32);
    QCOMPARE(released.value(QStringLiteral("page_selection_detail_limit")).toInt(), 32);
    QCOMPARE(released.value(QStringLiteral("page_selection_count")).toInteger(), initialSelections + 40);
    QCOMPARE(released.value(QStringLiteral("page_selection_details_suppressed")).toInteger(),
             initialSelections + 8);
}

void NextPlatformQtPdfSessionReleaseTests::constructionIsLazy()
{
    int requests = 0;
    auto resources = loader([&](const auto&) { ++requests; });
    QVERIFY(resources);
    QWidget parent;
    Platform::QtPdfViewerSession session(*resources, &parent);
    QCOMPARE(requests, 0);
    QCOMPARE(session.status(), QPdfDocument::Status::Null);
    QCOMPARE(session.pageCount(), 0);
    QVERIFY(!session.hasActiveStream());
    QVERIFY(!session.isReady());
}

void NextPlatformQtPdfSessionReleaseTests::closeAndReplacementUseFreshDocumentAndStreamGeneration()
{
    std::vector<Resources::ResourceDiagnostic> diagnostics;
    auto resources = loader([&](const auto& value) { diagnostics.push_back(value); });
    QVERIFY(resources);
    QWidget parent;
    QVBoxLayout layout(&parent);
    QPointer<QPdfDocument> retiring;
    int bindings = 0;
    Platform::QtPdfViewerSession session(*resources, &parent);
    QPointer<QPdfView> view(session.view());
    layout.addWidget(view);
    view->setZoomFactor(1.25);
    view->setPageSpacing(13);
    retiring = view->document();
    connect(view, &QPdfView::documentChanged, &session, [&](QPdfDocument* document) {
        QVERIFY(document);
        QVERIFY(retiring);
        QVERIFY(document != retiring.data());
        QCOMPARE(retiring->status(), QPdfDocument::Status::Null);
        QCOMPARE(document->status(), QPdfDocument::Status::Null);
        ++bindings;
    });
    QVERIFY(session.open(Domain::DocumentContentReference(kReference), QStringLiteral("Viewer:1")));
    QCOMPARE(bindings, 1);
    QTRY_VERIFY_WITH_TIMEOUT(session.isReady(), 10000);
    const int pages = session.pageCount();
    QPointer<QPdfDocument> first(session.findChild<QPdfDocument*>());
    QVERIFY(first);
    retiring = first;
    QCOMPARE(session.error(), QPdfDocument::Error::None);
    QVERIFY(session.pagePointSize(0).isValid());
    bool nullWithStreamOpen = false;
    int documentsRetired = 0;
    session.setLifecycleObserver([&](const QString& operation, const QString& phase) {
        if (operation == QStringLiteral("qpdf-document-close") && phase == QStringLiteral("after"))
        {
            nullWithStreamOpen = session.status() == QPdfDocument::Status::Null && session.hasActiveStream();
        }
        if (operation == QStringLiteral("qpdf-document-destroy") && phase == QStringLiteral("before"))
        {
            QVERIFY(retiring);
            QCOMPARE(session.view(), view.data());
            QVERIFY(view->document());
            QVERIFY(view->document() != retiring.data());
            QCOMPARE(view->document()->status(), QPdfDocument::Status::Null);
            ++documentsRetired;
        }
    });
    // open() also owns replacement cleanup; no external close is needed.
    QVERIFY(session.open(Domain::DocumentContentReference(kReference), QStringLiteral("Viewer:2")));
    QVERIFY(first.isNull());
    QCOMPARE(bindings, 2);
    QCOMPARE(documentsRetired, 1);
    QCOMPARE(session.view(), view.data());
    QCOMPARE(layout.indexOf(view), 0);
    QCOMPARE(view->zoomFactor(), 1.25);
    QCOMPARE(view->pageSpacing(), 13);
    QVERIFY(nullWithStreamOpen);
    QTRY_VERIFY_WITH_TIMEOUT(session.isReady(), 10000);
    QCOMPARE(session.pageCount(), pages);
    QCOMPARE(diagnostics.size(), std::size_t(2));
    for (const auto& diagnostic : diagnostics)
    {
        QVERIFY(diagnostic.succeeded);
        QCOMPARE(diagnostic.resourceId.value(), QString::fromLatin1(kBodyId));
        QCOMPARE(diagnostic.scope.kind(), Resources::ResourceScopeKind::ViewerSession);
    }
    QCOMPARE(diagnostics[0].scope.ownerLabel(), QStringLiteral("Viewer:1"));
    QCOMPARE(diagnostics[1].scope.ownerLabel(), QStringLiteral("Viewer:2"));
    retiring = view->document();
    QVERIFY(session.release());
    QCOMPARE(bindings, 3);
    QCOMPARE(documentsRetired, 2);
    QVERIFY(retiring.isNull());
    QVERIFY(!session.hasActiveStream());
    QCOMPARE(session.status(), QPdfDocument::Status::Null);
    retiring = view->document();
    QVERIFY(session.release());
    QCOMPARE(bindings, 4);
    QCOMPARE(documentsRetired, 3);
    QVERIFY(retiring.isNull());
    QCOMPARE(session.view(), view.data());
    QCOMPARE(layout.indexOf(view), 0);
    session.setLifecycleObserver({});
}

void NextPlatformQtPdfSessionReleaseTests::failedOpenKeepsAnEmptyBoundViewAndCanReopen()
{
    auto resources = loader();
    QVERIFY(resources);
    QWidget parent;
    Platform::QtPdfViewerSession session(*resources, &parent);
    QVERIFY(session.open(Domain::DocumentContentReference(kReference), QStringLiteral("Viewer:before-failure")));
    QTRY_VERIFY_WITH_TIMEOUT(session.isReady(), 10000);
    QPointer<QPdfView> view(session.view());
    QPointer<QPdfDocument> document(view->document());
    QVERIFY(!session.open(Domain::DocumentContentReference(
        "resource://documents/Guides/missing.pdf"), QStringLiteral("Viewer:missing")));
    QVERIFY(document.isNull());
    QCOMPARE(session.view(), view.data());
    QVERIFY(view->document());
    QCOMPARE(view->document()->status(), QPdfDocument::Status::Null);
    QVERIFY(!session.hasActiveStream());
    QVERIFY(session.open(Domain::DocumentContentReference(kReference), QStringLiteral("Viewer:after-failure")));
    QTRY_VERIFY_WITH_TIMEOUT(session.isReady(), 10000);
    QCOMPARE(session.view(), view.data());
}

void NextPlatformQtPdfSessionReleaseTests::destroyingAnActiveOwnerClosesConsumersBeforeStream()
{
    auto resources = loader();
    QVERIFY(resources);
    QWidget parent;
    QStringList events;
    bool nullWithStreamOpen = false;
    QPointer<QPdfDocument> document;
    QPointer<QPdfView> view;
    {
        Platform::QtPdfViewerSession session(*resources, &parent);
        QVERIFY(session.open(Domain::DocumentContentReference(kReference), QStringLiteral("Viewer:destruction")));
        QTRY_VERIFY_WITH_TIMEOUT(session.isReady(), 10000);
        document = session.findChild<QPdfDocument*>();
        view = session.view();
        session.setLifecycleObserver([&](const QString& operation, const QString& phase) {
            events.append(operation + QLatin1Char(':') + phase);
            if (operation == QStringLiteral("qpdf-document-close") && phase == QStringLiteral("after"))
            {
                nullWithStreamOpen = session.status() == QPdfDocument::Status::Null && session.hasActiveStream();
            }
        });
    }
    QVERIFY(nullWithStreamOpen);
    QVERIFY(document.isNull());
    QVERIFY(view.isNull());
    QVERIFY(events.indexOf(QStringLiteral("qpdf-view-destroy:after"))
        < events.indexOf(QStringLiteral("qpdf-document-destroy:before")));
    QVERIFY(events.indexOf(QStringLiteral("qpdf-document-destroy:after"))
        < events.indexOf(QStringLiteral("primary-stream-release:before")));
    QCOMPARE(events.count(QStringLiteral("primary-stream-release:after")), 1);
}

void NextPlatformQtPdfSessionReleaseTests::timeoutDestroysViewThenDocumentThenStreamAndReopens()
{
    auto resources = loader();
    QVERIFY(resources);
    QWidget parent;
    Platform::QtPdfViewerSession session(*resources, &parent);
    QVERIFY(session.open(Domain::DocumentContentReference(kReference), QStringLiteral("Viewer:timeout")));
    QTRY_VERIFY_WITH_TIMEOUT(session.isReady(), 10000);
    QPointer<QPdfView> view(session.view());
    QPointer<QPdfDocument> document(session.findChild<QPdfDocument*>());
    QStringList events;
    bool consumersDestroyedWhileStreamOpen = true;
    session.setLifecycleObserver([&](const QString& operation, const QString& phase) {
        events.append(operation + QLatin1Char(':') + phase);
        if (operation.endsWith(QStringLiteral("-destroy")) && phase == QStringLiteral("after"))
        {
            consumersDestroyedWhileStreamOpen &= session.hasActiveStream();
        }
    });
    session.setCloseWaiterForTesting([] { return false; });
    QVERIFY(session.release());
    QVERIFY(consumersDestroyedWhileStreamOpen);
    QVERIFY(view.isNull());
    QVERIFY(document.isNull());
    QVERIFY(events.indexOf(QStringLiteral("qpdf-view-destroy:after"))
        < events.indexOf(QStringLiteral("qpdf-document-destroy:before")));
    QVERIFY(events.indexOf(QStringLiteral("qpdf-document-destroy:after"))
        < events.indexOf(QStringLiteral("primary-stream-release:before")));
    QVERIFY(!session.hasActiveStream());
    QVERIFY(session.view());
    session.setCloseWaiterForTesting({});
    session.setLifecycleObserver({});
    QVERIFY(session.open(Domain::DocumentContentReference(kReference), QStringLiteral("Viewer:reopen")));
    QTRY_VERIFY_WITH_TIMEOUT(session.isReady(), 10000);
}

void NextPlatformQtPdfSessionReleaseTests::queuedStatusesCannotReachReplacementOrClosedSession()
{
    auto resources = loader();
    QVERIFY(resources);
    QWidget parent;
    Platform::QtPdfViewerSession session(*resources, &parent);
    int readyEvents = 0;
    session.setStatusHandler([&](QPdfDocument::Status status) {
        if (status == QPdfDocument::Status::Ready) { ++readyEvents; }
    });
    QVERIFY(session.open(Domain::DocumentContentReference(kReference), QStringLiteral("Viewer:stale:1")));
    QTRY_VERIFY_WITH_TIMEOUT(session.isReady(), 10000);
    QVERIFY(session.open(Domain::DocumentContentReference(kReference), QStringLiteral("Viewer:stale:2")));
    QTRY_VERIFY_WITH_TIMEOUT(session.isReady(), 10000);
    QCOMPARE(readyEvents, 2);
    // Queued Ready from generation 1 must not publish as generation 2, even
    // though both documents currently have the same Ready status.
    QCoreApplication::processEvents();
    QCOMPARE(readyEvents, 2);
    QVERIFY(session.release());
    QCoreApplication::processEvents();
    QCOMPARE(readyEvents, 2);
    QCOMPARE(session.status(), QPdfDocument::Status::Null);
}

void NextPlatformQtPdfSessionReleaseTests::failedFilenameLoadErrorIsDeferredAndCanReleaseSafely()
{
    QTemporaryDir missingFileDirectory;
    QVERIFY(missingFileDirectory.isValid());
    const QString missingPath = missingFileDirectory.filePath(QStringLiteral("missing.pdf"));
    QVERIFY(!QFileInfo::exists(missingPath));
    auto resources = loader();
    QVERIFY(resources);
    QWidget parent;
    bool loadReturned = false;
    bool errorHandled = false;
    bool callbackAfterOpenReturned = false;
    bool streamActiveDuringCallback = false;
    bool expectedFileNotFoundError = false;
    bool releaseSucceeded = false;
    bool failedDocumentDestroyed = false;
    bool replacementDocumentBound = false;
    bool streamReleasedDuringCallback = false;
    QPdfDocument::Status statusAfterCallbackRelease = QPdfDocument::Status::Error;
    bool closeCompletedWithErrorAndStream = false;
    bool retiredDocumentDestroyedWithStream = false;
    QStringList lifecycleEvents;
    Platform::QtPdfViewerSession session(*resources, &parent);
    session.setLifecycleObserver([&](const QString& operation, const QString& phase) {
        lifecycleEvents.append(operation + QLatin1Char(':') + phase);
        if (operation == QStringLiteral("qpdf-document-close")
            && phase == QStringLiteral("after"))
        {
            closeCompletedWithErrorAndStream = session.status() == QPdfDocument::Status::Error
                && session.hasActiveStream();
        }
        if (operation == QStringLiteral("qpdf-document-destroy")
            && phase == QStringLiteral("before"))
        {
            retiredDocumentDestroyedWithStream = session.hasActiveStream();
        }
    });
    session.setDocumentLoadForTesting([missingPath](QPdfDocument& document) {
        document.load(missingPath);
    });
    session.setStatusHandler([&](QPdfDocument::Status status) {
        if (status != QPdfDocument::Status::Error) { return; }
        callbackAfterOpenReturned = loadReturned;
        streamActiveDuringCallback = session.hasActiveStream();
        expectedFileNotFoundError = status == session.status()
            && session.error() == QPdfDocument::Error::FileNotFound;
        QPointer<QPdfDocument> failedDocument(session.view()->document());
        releaseSucceeded = session.release();
        failedDocumentDestroyed = failedDocument.isNull();
        replacementDocumentBound = session.view() && session.view()->document()
            && session.view()->document()->status() == QPdfDocument::Status::Null;
        streamReleasedDuringCallback = !session.hasActiveStream();
        statusAfterCallbackRelease = session.status();
        errorHandled = true;
    });
    QVERIFY(session.open(Domain::DocumentContentReference(kReference),
                         QStringLiteral("Viewer:missing-file-error")));
    QVERIFY(!errorHandled);
    loadReturned = true;
    QTRY_VERIFY_WITH_TIMEOUT(errorHandled, 10000);
    QVERIFY(callbackAfterOpenReturned);
    QVERIFY(streamActiveDuringCallback);
    QVERIFY(expectedFileNotFoundError);
    QVERIFY(releaseSucceeded);
    QVERIFY(failedDocumentDestroyed);
    QVERIFY(replacementDocumentBound);
    QVERIFY(streamReleasedDuringCallback);
    QCOMPARE(statusAfterCallbackRelease, QPdfDocument::Status::Null);
    QVERIFY(closeCompletedWithErrorAndStream);
    QVERIFY(retiredDocumentDestroyedWithStream);
    QVERIFY(!lifecycleEvents.contains(QStringLiteral("qpdf-document-close:timeout")));
    const int closeAfter = lifecycleEvents.indexOf(QStringLiteral("qpdf-document-close:after"));
    const int destroyBefore = lifecycleEvents.indexOf(
        QStringLiteral("qpdf-document-destroy:before"), closeAfter + 1);
    const int destroyAfter = lifecycleEvents.indexOf(
        QStringLiteral("qpdf-document-destroy:after"), destroyBefore + 1);
    const int streamReleaseBefore = lifecycleEvents.indexOf(
        QStringLiteral("primary-stream-release:before"), destroyAfter + 1);
    const int streamReleaseAfter = lifecycleEvents.indexOf(
        QStringLiteral("primary-stream-release:after"), streamReleaseBefore + 1);
    QVERIFY(closeAfter >= 0);
    QVERIFY(closeAfter < destroyBefore);
    QVERIFY(destroyBefore < destroyAfter);
    QVERIFY(destroyAfter < streamReleaseBefore);
    QVERIFY(streamReleaseBefore < streamReleaseAfter);
    QVERIFY(!session.hasActiveStream());
    QCOMPARE(session.status(), QPdfDocument::Status::Null);
}

void NextPlatformQtPdfSessionReleaseTests::synchronousPrintBorrowProtectsTheActiveDocument()
{
    auto resources = loader();
    QVERIFY(resources);
    QWidget parent;
    Platform::QtPdfViewerSession session(*resources, &parent);
    QVERIFY(session.open(Domain::DocumentContentReference(kReference), QStringLiteral("Viewer:print")));
    QTRY_VERIFY_WITH_TIMEOUT(session.isReady(), 10000);
    bool called = false;
    session.withReadyDocument([&](QPdfDocument* document) {
        called = true;
        QPointer<QPdfDocument> guard(document);
        QVERIFY(!session.release());
        QVERIFY(!session.open(Domain::DocumentContentReference(kReference), QStringLiteral("Viewer:busy")));
        QCoreApplication::processEvents();
        QVERIFY(guard);
        QVERIFY(session.hasActiveStream());
        QCOMPARE(document->status(), QPdfDocument::Status::Ready);
    });
    QVERIFY(called);
    QVERIFY(session.release());
}

QTEST_MAIN(NextPlatformQtPdfSessionReleaseTests)
#include "next_platform_qtpdf_session_release_tests.moc"
