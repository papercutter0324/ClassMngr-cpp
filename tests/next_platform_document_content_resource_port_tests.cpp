#include "core/resource_packs/resource_pack_manager.h"
#include "next/platform/document_content_resource_port.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <optional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;
using ClassMngr::Next::Platform::DocumentContentResource;
using ClassMngr::Next::Platform::DocumentContentResourcePort;

namespace
{

QString baselineDirectory()
{
    return QStringLiteral(CLASSMNGR_RESOURCE_PACK_DIR);
}

std::unique_ptr<ResourcePackManager> makeManager(
    QTemporaryDir& storage
    )
{
    return std::make_unique<ResourcePackManager>(
        storage.path(),
        baselineDirectory()
        );
}

DocumentContentReference reference(
    const char* value
    )
{
    return DocumentContentReference(value);
}

} // namespace

class NextPlatformDocumentContentResourcePortTests final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsMalformedAndTraversalReferencesWithoutMounting();
    void resolvesPrimaryAndExportWithOneLease();
    void reportsMissingResourcesWithoutLeakingLease();
    void releaseUnmountsAfterResolvedValueIsDestroyed();
    void viewerClosesDocumentBeforeReleasingContentBoundary();
    void boundaryIsMoveOnlyAndTyped();
};

void NextPlatformDocumentContentResourcePortTests::
rejectsMalformedAndTraversalReferencesWithoutMounting()
{
    QTemporaryDir storage;
    QVERIFY(storage.isValid());
    auto manager = makeManager(storage);
    DocumentContentResourcePort port(*manager);

    const std::vector<DocumentContentReference> invalidReferences{
        reference(""),
        reference("   "),
        reference("documents/Guides/file.pdf"),
        reference("resource://exports/Guides/file.pdf"),
        reference("resource://documents/"),
        reference("resource://documents/../Guides/file.pdf"),
        reference("resource://documents/Guides/../../file.pdf"),
        reference("resource://documents/Guides\\..\\file.pdf"),
        reference("resource://documents//Guides/file.pdf")
    };

    for (const DocumentContentReference& invalid : invalidReferences)
    {
        const auto result = port.resolve(invalid);
        QVERIFY(!result);
        QCOMPARE(result.error().code, ErrorCode::InvalidInput);
        QVERIFY(!manager->isMounted(QStringLiteral("documents")));
    }

    const auto invalidExport = port.resolve(
        reference("resource://documents/Guides/DYB Lesson Planning Guide.pdf"),
        std::optional<DocumentContentReference>(
            reference("resource://exports/Guides/file.pptx")
            )
        );
    QVERIFY(!invalidExport);
    QCOMPARE(invalidExport.error().code, ErrorCode::InvalidInput);
    QVERIFY(!manager->isMounted(QStringLiteral("documents")));
}

void NextPlatformDocumentContentResourcePortTests::
resolvesPrimaryAndExportWithOneLease()
{
    QTemporaryDir storage;
    QVERIFY(storage.isValid());
    auto manager = makeManager(storage);
    DocumentContentResourcePort port(*manager);

    auto result = port.resolve(
        reference("resource://documents/Guides/DYB Lesson Planning Guide.pdf"),
        std::optional<DocumentContentReference>(
            reference("resource://documents/Lesson Templates/SP+WR Template.pptx")
            )
        );

    QVERIFY(result);
    QVERIFY(result.value().resourceLease.isValid());
    QVERIFY(QFile::exists(result.value().primaryPath));
    QVERIFY(result.value().exportPath.has_value());
    QVERIFY(QFile::exists(*result.value().exportPath));
    QVERIFY(manager->isMounted(QStringLiteral("documents")));

    // Resetting the returned lease must unmount immediately. If resolution
    // retained a second internal lease, this would remain mounted.
    result.value().resourceLease.reset();
    QVERIFY(!manager->isMounted(QStringLiteral("documents")));
}

void NextPlatformDocumentContentResourcePortTests::
reportsMissingResourcesWithoutLeakingLease()
{
    QTemporaryDir storage;
    QVERIFY(storage.isValid());
    auto manager = makeManager(storage);
    DocumentContentResourcePort port(*manager);

    const auto missingPrimary = port.resolve(
        reference("resource://documents/Guides/missing.pdf")
        );
    QVERIFY(!missingPrimary);
    QCOMPARE(missingPrimary.error().code, ErrorCode::NotFound);
    QVERIFY(!manager->isMounted(QStringLiteral("documents")));

    const auto missingExport = port.resolve(
        reference("resource://documents/Guides/DYB Lesson Planning Guide.pdf"),
        std::optional<DocumentContentReference>(
            reference("resource://documents/Guides/missing.pptx")
            )
        );
    QVERIFY(!missingExport);
    QCOMPARE(missingExport.error().code, ErrorCode::NotFound);
    QVERIFY(!manager->isMounted(QStringLiteral("documents")));
}

void NextPlatformDocumentContentResourcePortTests::
releaseUnmountsAfterResolvedValueIsDestroyed()
{
    QTemporaryDir storage;
    QVERIFY(storage.isValid());
    auto manager = makeManager(storage);
    DocumentContentResourcePort port(*manager);
    QString primaryPath;

    {
        const auto result = port.resolve(
            reference("resource://documents/Guides/DYB Lesson Planning Guide.pdf")
            );
        QVERIFY(result);
        primaryPath = result.value().primaryPath;
        QVERIFY(manager->isMounted(QStringLiteral("documents")));
    }

    QVERIFY(!manager->isMounted(QStringLiteral("documents")));
    QVERIFY(!QFile::exists(primaryPath));
}

void NextPlatformDocumentContentResourcePortTests::
viewerClosesDocumentBeforeReleasingContentBoundary()
{
    const QString sourcePath = QFINDTESTDATA(
        "../src/ui/shared/pages/pdf_viewer_page.cpp"
        );
    QVERIFY2(!sourcePath.isEmpty(), "PdfViewerPage source was not found.");

    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();
    const int closePosition = contents.indexOf(
        "m_document->close();",
        contents.indexOf("void PdfViewerPage::releaseDocument()")
        );
    const int releasePosition = contents.indexOf(
        "releaseDocumentContentSession();",
        closePosition
        );

    QVERIFY(closePosition >= 0);
    QVERIFY(releasePosition > closePosition);
}

void NextPlatformDocumentContentResourcePortTests::boundaryIsMoveOnlyAndTyped()
{
    using Port = DocumentContentResourcePort;
    using Result = decltype(
        std::declval<const Port&>().resolve(
            std::declval<const DocumentContentReference&>()
            )
        );

    static_assert(std::is_same_v<
        Result,
        Domain::Result<DocumentContentResource>
        >);
    static_assert(!std::is_copy_constructible_v<Port>);
    static_assert(!std::is_move_constructible_v<Port>);
    static_assert(!std::is_copy_constructible_v<DocumentContentResource>);
    static_assert(std::is_move_constructible_v<DocumentContentResource>);
    QVERIFY(true);
}

QTEST_APPLESS_MAIN(NextPlatformDocumentContentResourcePortTests)

#include "next_platform_document_content_resource_port_tests.moc"
