#include "next/domain/document_content_reference.h"
#include "next/platform/document_content_resource_port.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_diagnostics.h"
#include "next/resources/resource_loader.h"

#include <QEventLoop>
#include <QIODevice>
#include <QPdfDocument>
#include <QPointer>
#include <QStringList>
#include <QTimer>
#include <QtTest/QtTest>

#include <optional>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

constexpr int kStatusTimeoutMs = 10000;
constexpr auto kSelectedPdfReference =
    "resource://documents/Guides/DYB Lesson Planning Guide.pdf";

bool waitForStatus(
    QPdfDocument& document,
    const QPdfDocument::Status expected,
    const int timeoutMs
    )
{
    if (document.status() == expected)
    {
        return true;
    }

    QEventLoop eventLoop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(
        &document,
        &QPdfDocument::statusChanged,
        &eventLoop,
        [&eventLoop, expected](const QPdfDocument::Status status) {
            if (status == expected)
            {
                eventLoop.quit();
            }
        }
        );
    QObject::connect(&timeout, &QTimer::timeout, &eventLoop, &QEventLoop::quit);
    timeout.start(timeoutMs);

    while (document.status() != expected && timeout.isActive())
    {
        eventLoop.exec(QEventLoop::ExcludeUserInputEvents);
    }

    return document.status() == expected;
}

QString resourceRoot()
{
    return QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
}

QString catalogPath()
{
    return QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG);
}

class PdfSessionOwner final
{
public:
    explicit PdfSessionOwner(QStringList* destructionEvents = nullptr) noexcept
        : m_destructionEvents(destructionEvents)
    {
    }

    PdfSessionOwner(const PdfSessionOwner&) = delete;
    PdfSessionOwner& operator=(const PdfSessionOwner&) = delete;

    ~PdfSessionOwner()
    {
        m_document.close();
        const bool reachedNull = waitForStatus(
            m_document,
            QPdfDocument::Status::Null,
            kStatusTimeoutMs
            );
        const bool streamStayedOpen = m_resource
            && m_resource->primaryBody.get().isOpen();

        if (m_destructionEvents && m_resource)
        {
            m_destructionEvents->append(
                reachedNull && streamStayedOpen
                    ? QStringLiteral("document-null-while-stream-open")
                    : QStringLiteral("document-close-order-failed")
                );
        }

        if (m_resource)
        {
            m_resource->primaryBody.get().close();
            m_resource.reset();
        }
    }

    void attach(Platform::DocumentContentResource resource)
    {
        m_resource.emplace(std::move(resource));
    }

    [[nodiscard]] QPdfDocument& document() noexcept
    {
        return m_document;
    }

    [[nodiscard]] QIODevice* streamDevice() noexcept
    {
        if (!m_resource)
        {
            return nullptr;
        }
        return &m_resource->primaryBody.get().device();
    }

    [[nodiscard]] bool streamIsOpen() const noexcept
    {
        return m_resource && m_resource->primaryBody.get().isOpen();
    }

    [[nodiscard]] const Resources::ResourceScope* streamScope() const noexcept
    {
        return m_resource ? &m_resource->primaryBody.scope() : nullptr;
    }

    void loadFromStream()
    {
        m_document.load(streamDevice());
    }

    [[nodiscard]] bool closeDocument()
    {
        m_document.close();
        return waitForStatus(
            m_document,
            QPdfDocument::Status::Null,
            kStatusTimeoutMs
            );
    }

    void closeStream() noexcept
    {
        if (m_resource)
        {
            m_resource->primaryBody.get().close();
        }
    }

    void releaseStream() noexcept
    {
        m_resource.reset();
    }

private:
    QPdfDocument m_document;
    std::optional<Platform::DocumentContentResource> m_resource;
    QStringList* m_destructionEvents = nullptr;
};

} // namespace

class NextPlatformQtPdfSessionReleaseTests final : public QObject
{
    Q_OBJECT

private slots:
    void closeAndReopenUsesFreshStreamAndSessionGeneration();
    void destroyingAnActiveOwnerClosesDocumentBeforeStream();
};

void NextPlatformQtPdfSessionReleaseTests::
closeAndReopenUsesFreshStreamAndSessionGeneration()
{
    auto catalog = Resources::ResourceCatalog::load(catalogPath(), resourceRoot());
    QVERIFY(catalog);

    std::vector<Resources::ResourceDiagnostic> diagnostics;
    Resources::ResourceLoader loader(
        std::move(*catalog),
        [&diagnostics](const Resources::ResourceDiagnostic& diagnostic) {
            diagnostics.push_back(diagnostic);
        }
        );
    Platform::DocumentContentResourcePort port(loader);
    PdfSessionOwner owner;

    const QString firstGeneration =
        QStringLiteral("DocumentViewer:QtPdfLifecycle:1");
    auto firstOpen = port.openPrimaryPdf(
        Domain::DocumentContentReference(kSelectedPdfReference),
        firstGeneration
        );
    QVERIFY(firstOpen);
    owner.attach(std::move(firstOpen.value()));

    QIODevice* const firstDevice = owner.streamDevice();
    QPointer<QIODevice> firstDeviceGuard(firstDevice);
    QVERIFY(firstDevice);
    QVERIFY(owner.streamIsOpen());
    QCOMPARE(owner.streamScope()->ownerLabel(), firstGeneration);

    owner.loadFromStream();
    QTRY_COMPARE_WITH_TIMEOUT(
        owner.document().status(),
        QPdfDocument::Status::Ready,
        kStatusTimeoutMs
        );
    QCOMPARE(owner.document().error(), QPdfDocument::Error::None);
    QTRY_VERIFY_WITH_TIMEOUT(
        owner.document().pageCount() > 0,
        kStatusTimeoutMs
        );
    const int firstPageCount = owner.document().pageCount();
    QVERIFY(owner.streamIsOpen());

    QVERIFY(owner.closeDocument());
    QCOMPARE(owner.document().status(), QPdfDocument::Status::Null);
    QVERIFY(owner.streamIsOpen());

    owner.closeStream();
    QVERIFY(!owner.streamIsOpen());
    owner.releaseStream();
    QVERIFY(firstDeviceGuard.isNull());

    const QString secondGeneration =
        QStringLiteral("DocumentViewer:QtPdfLifecycle:2");
    auto secondOpen = port.openPrimaryPdf(
        Domain::DocumentContentReference(kSelectedPdfReference),
        secondGeneration
        );
    QVERIFY(secondOpen);
    owner.attach(std::move(secondOpen.value()));

    QVERIFY(owner.streamDevice());
    QVERIFY(owner.streamIsOpen());
    QCOMPARE(owner.streamScope()->ownerLabel(), secondGeneration);
    owner.loadFromStream();
    QTRY_COMPARE_WITH_TIMEOUT(
        owner.document().status(),
        QPdfDocument::Status::Ready,
        kStatusTimeoutMs
        );
    QCOMPARE(owner.document().error(), QPdfDocument::Error::None);
    QTRY_COMPARE_WITH_TIMEOUT(
        owner.document().pageCount(),
        firstPageCount,
        kStatusTimeoutMs
        );
    QVERIFY(owner.streamIsOpen());

    QCOMPARE(diagnostics.size(), std::size_t(2));
    for (const Resources::ResourceDiagnostic& diagnostic : diagnostics)
    {
        QVERIFY(diagnostic.succeeded);
        QCOMPARE(
            diagnostic.resourceId.value(),
            QStringLiteral("document.body:Guides/DYB Lesson Planning Guide.pdf")
            );
        QCOMPARE(
            diagnostic.scope.kind(),
            Resources::ResourceScopeKind::ViewerSession
            );
    }
    QCOMPARE(diagnostics[0].scope.ownerLabel(), firstGeneration);
    QCOMPARE(diagnostics[1].scope.ownerLabel(), secondGeneration);

    QVERIFY(owner.closeDocument());
    QVERIFY(owner.streamIsOpen());
    owner.closeStream();
    owner.releaseStream();
}

void NextPlatformQtPdfSessionReleaseTests::
destroyingAnActiveOwnerClosesDocumentBeforeStream()
{
    auto catalog = Resources::ResourceCatalog::load(catalogPath(), resourceRoot());
    QVERIFY(catalog);

    Resources::ResourceLoader loader(std::move(*catalog));
    Platform::DocumentContentResourcePort port(loader);
    QStringList destructionEvents;

    {
        PdfSessionOwner owner(&destructionEvents);
        const QString generation =
            QStringLiteral("DocumentViewer:QtPdfLifecycle:destruction:1");
        auto opened = port.openPrimaryPdf(
            Domain::DocumentContentReference(kSelectedPdfReference),
            generation
            );
        QVERIFY(opened);
        owner.attach(std::move(opened.value()));

        QIODevice* const device = owner.streamDevice();
        QVERIFY(device);
        QVERIFY(device->isOpen());
        QObject::connect(
            device,
            &QIODevice::aboutToClose,
            [&destructionEvents]() {
                destructionEvents.append(QStringLiteral("stream-about-to-close"));
            }
            );

        owner.loadFromStream();
        QTRY_COMPARE_WITH_TIMEOUT(
            owner.document().status(),
            QPdfDocument::Status::Ready,
            kStatusTimeoutMs
            );
        QVERIFY(owner.document().pageCount() > 0);
        QVERIFY(device->isOpen());
    }

    const QStringList expectedDestructionEvents{
        QStringLiteral("document-null-while-stream-open"),
        QStringLiteral("stream-about-to-close")
    };
    QCOMPARE(destructionEvents, expectedDestructionEvents);
}

QTEST_GUILESS_MAIN(NextPlatformQtPdfSessionReleaseTests)

#include "next_platform_qtpdf_session_release_tests.moc"
