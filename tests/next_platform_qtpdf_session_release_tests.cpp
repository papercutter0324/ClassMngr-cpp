#include "next/platform/qt_pdf_viewer_session.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_diagnostics.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPdfView>
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
constexpr auto kBodyPath = "assets/documents/Guides/DYB Lesson Planning Guide.pdf";
constexpr auto kBodyId = "document.body:Guides/DYB Lesson Planning Guide.pdf";

std::unique_ptr<Resources::ResourceLoader> loader(
    Resources::ResourceDiagnosticsSink sink = {})
{
    auto catalog = Resources::ResourceCatalog::load(
        QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
        QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT));
    if (!catalog) { return {}; }
    return std::make_unique<Resources::ResourceLoader>(std::move(*catalog), std::move(sink));
}

// Pass resource integrity validation while deliberately failing PDF parsing.
QString malformedPackage(QTemporaryDir& package)
{
    QFile source(QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG));
    if (!source.open(QIODevice::ReadOnly)) { return {}; }
    QJsonObject root = QJsonDocument::fromJson(source.readAll()).object();
    QJsonArray records = root.value(QStringLiteral("records")).toArray();
    const QByteArray bytes("%PDF-1.7\nThis PDF has no catalog or pages.\n%%EOF\n");
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    for (qsizetype i = 0; i < records.size(); ++i)
    {
        auto record = records.at(i).toObject();
        if (record.value(QStringLiteral("logical_id")).toString() != QString::fromLatin1(kBodyId)) { continue; }
        auto provenance = record.value(QStringLiteral("provenance")).toObject();
        for (const auto& key : {QStringLiteral("source"), QStringLiteral("baked")})
        {
            auto entry = provenance.value(key).toObject();
            entry.insert(QStringLiteral("bytes"), bytes.size());
            entry.insert(QStringLiteral("sha256"), hash);
            provenance.insert(key, entry);
        }
        record.insert(QStringLiteral("provenance"), provenance);
        records.replace(i, record);
    }
    root.insert(QStringLiteral("records"), records);
    const QString path = package.filePath(QString::fromLatin1(kBodyPath));
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) { return {}; }
    QFile body(path);
    if (!body.open(QIODevice::WriteOnly) || body.write(bytes) != bytes.size()) { return {}; }
    body.close();
    const QString catalogPath = package.filePath(QStringLiteral("resource-catalog.json"));
    QFile output(catalogPath);
    if (!output.open(QIODevice::WriteOnly) || output.write(QJsonDocument(root).toJson()) <= 0) { return {}; }
    return catalogPath;
}
}

class NextPlatformQtPdfSessionReleaseTests final : public QObject
{
    Q_OBJECT
private slots:
    void constructionIsLazy();
    void closeAndReplacementUseFreshDocumentAndStreamGeneration();
    void failedOpenKeepsAnEmptyBoundViewAndCanReopen();
    void destroyingAnActiveOwnerClosesConsumersBeforeStream();
    void timeoutDestroysViewThenDocumentThenStreamAndReopens();
    void queuedStatusesCannotReachReplacementOrClosedSession();
    void malformedPdfErrorIsDeferredAndCanReleaseSafely();
    void synchronousPrintBorrowProtectsTheActiveDocument();
};

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

void NextPlatformQtPdfSessionReleaseTests::malformedPdfErrorIsDeferredAndCanReleaseSafely()
{
    QTemporaryDir package;
    QVERIFY(package.isValid());
    const QString path = malformedPackage(package);
    QVERIFY(!path.isEmpty());
    auto catalog = Resources::ResourceCatalog::load(path, package.path());
    QVERIFY(catalog);
    Resources::ResourceLoader resources(std::move(*catalog));
    QWidget parent;
    Platform::QtPdfViewerSession session(resources, &parent);
    bool loadReturned = false;
    bool errorHandled = false;
    session.setStatusHandler([&](QPdfDocument::Status status) {
        if (status != QPdfDocument::Status::Error) { return; }
        QVERIFY(loadReturned);
        QVERIFY(session.hasActiveStream());
        QVERIFY(session.error() != QPdfDocument::Error::None);
        QVERIFY(session.release());
        errorHandled = true;
    });
    QVERIFY(session.open(Domain::DocumentContentReference(kReference), QStringLiteral("Viewer:invalid")));
    loadReturned = true;
    QTRY_VERIFY_WITH_TIMEOUT(errorHandled, 10000);
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
