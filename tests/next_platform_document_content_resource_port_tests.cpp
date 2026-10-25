#include "next/domain/document_content_reference.h"
#include "next/platform/document_content_resource_port.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_diagnostics.h"
#include "next/resources/resource_loader.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <initializer_list>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using ClassMngr::Next::Domain::DocumentContentReference;
using ClassMngr::Next::Platform::DocumentContentResource;
using ClassMngr::Next::Platform::DocumentContentResourcePort;

namespace
{

QString resourceRoot()
{
    return QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
}

QString catalogPath()
{
    return QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG);
}

bool copyFile(const QString& source, const QString& destination)
{
    if (!QDir().mkpath(QFileInfo(destination).absolutePath()))
    {
        return false;
    }
    QFile::remove(destination);
    return QFile::copy(source, destination);
}

DocumentContentReference reference(const char* value)
{
    return DocumentContentReference(value);
}

DocumentContentReference reference(std::string value)
{
    return DocumentContentReference(std::move(value));
}

QString modifiedCatalog(
    QTemporaryDir& directory,
    const QString& resourceId,
    const std::initializer_list<std::pair<QString, QString>>& replacements
    )
{
    QFile source(catalogPath());
    if (!source.open(QIODevice::ReadOnly))
    {
        return {};
    }
    const QJsonDocument parsed = QJsonDocument::fromJson(source.readAll());
    if (!parsed.isObject())
    {
        return {};
    }

    QJsonObject root = parsed.object();
    QJsonArray records = root.value(QStringLiteral("records")).toArray();
    bool updated = false;
    for (qsizetype index = 0; index < records.size(); ++index)
    {
        QJsonObject record = records.at(index).toObject();
        if (record.value(QStringLiteral("logical_id")).toString() == resourceId)
        {
            for (const auto& [field, replacement] : replacements)
            {
                record.insert(field, replacement);
            }
            records.replace(index, record);
            updated = true;
            break;
        }
    }
    if (!updated)
    {
        return {};
    }
    root.insert(QStringLiteral("records"), records);

    const QString outputPath = directory.filePath(QStringLiteral("resource-catalog.json"));
    QFile output(outputPath);
    if (!output.open(QIODevice::WriteOnly)
        || output.write(QJsonDocument(root).toJson()) < 0)
    {
        return {};
    }
    return outputPath;
}

QString modifiedCatalog(
    QTemporaryDir& directory,
    const QString& resourceId,
    const QString& field,
    const QString& replacement
    )
{
    return modifiedCatalog(
        directory,
        resourceId,
        {{field, replacement}}
        );
}

} // namespace

class NextPlatformDocumentContentResourcePortTests final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsMalformedAndUnsafeReferencesBeforeLoading();
    void opensOnlyTheSelectedPdfWithViewerSessionScope();
    void rejectsNonPdfAndMalformedCatalogDescriptors();
    void missingSelectedBodyFailsWithoutFallback();
    void sameSizeCorruptSelectedBodyFailsWithoutFallback();
    void boundaryIsMoveOnlyAndUsesLoaderOwnedStream();
};

void NextPlatformDocumentContentResourcePortTests::
rejectsMalformedAndUnsafeReferencesBeforeLoading()
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
    DocumentContentResourcePort port(loader);

    std::string embeddedNul = "resource://documents/Guides/file.pdf";
    embeddedNul.push_back('\0');
    embeddedNul.append("suffix");
    std::string invalidUtf8 = "resource://documents/Guides/";
    invalidUtf8.append("\xED\xA0\x80", 3);
    std::string oversizedReference = "resource://documents/";
    oversizedReference.append(
        Application::kDocumentContentMaxReferenceLength,
        'x'
        );
    const std::vector<DocumentContentReference> invalidReferences{
        reference(""),
        reference("   "),
        reference("documents/Guides/file.pdf"),
        reference("resource://exports/Guides/file.pdf"),
        reference("resource://documents/"),
        reference("resource://documents/../Guides/file.pdf"),
        reference("resource://documents/Guides/../../file.pdf"),
        reference("resource://documents/Guides\\..\\file.pdf"),
        reference("resource://documents//Guides/file.pdf"),
        reference("resource://documents/Guides/file.pdf?download=1"),
        reference(std::move(embeddedNul)),
        reference(std::move(invalidUtf8)),
        reference(std::move(oversizedReference))
    };

    // Catalog parsing and loader construction inspect metadata only. Invalid
    // references are rejected before ResourceLoader receives a body request.
    QVERIFY(diagnostics.empty());
    for (const DocumentContentReference& invalid : invalidReferences)
    {
        const auto result = port.openPrimaryPdf(
            invalid,
            QStringLiteral("DocumentViewer:invalid")
            );
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }
    QVERIFY(diagnostics.empty());
}

void NextPlatformDocumentContentResourcePortTests::
opensOnlyTheSelectedPdfWithViewerSessionScope()
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
    DocumentContentResourcePort port(loader);
    QVERIFY(diagnostics.empty());

    auto result = port.openPrimaryPdf(
        reference("resource://documents/Guides/DYB Lesson Planning Guide.pdf"),
        QStringLiteral("DocumentViewer:document_guides_lesson_planning")
        );
    QVERIFY(result);
    QVERIFY(result.value().primaryBody.get().isOpen());
    QVERIFY(result.value().primaryBody.get().device().isOpen());
    QCOMPARE(
        result.value().primaryBody.scope().kind(),
        Resources::ResourceScopeKind::ViewerSession
        );
    QCOMPARE(
        result.value().primaryBody.scope().ownerLabel(),
        QStringLiteral("DocumentViewer:document_guides_lesson_planning")
        );

    // The only loader request is the chosen PDF body. No catalog or optional
    // export body is requested by primary document selection.
    QCOMPARE(diagnostics.size(), std::size_t(1));
    QCOMPARE(
        diagnostics.front().resourceId.value(),
        QStringLiteral("document.body:Guides/DYB Lesson Planning Guide.pdf")
        );
    QCOMPARE(
        diagnostics.front().scope.kind(),
        Resources::ResourceScopeKind::ViewerSession
        );
    QVERIFY(diagnostics.front().succeeded);

    QIODevice* const borrowedDevice =
        &result.value().primaryBody.get().device();
    QVERIFY(borrowedDevice->isOpen());
    result.value().primaryBody.get().close();
    QVERIFY(!borrowedDevice->isOpen());
}

void NextPlatformDocumentContentResourcePortTests::
rejectsNonPdfAndMalformedCatalogDescriptors()
{
    auto originalCatalog = Resources::ResourceCatalog::load(
        catalogPath(),
        resourceRoot()
        );
    QVERIFY(originalCatalog);
    std::vector<Resources::ResourceDiagnostic> diagnostics;
    Resources::ResourceLoader originalLoader(
        std::move(*originalCatalog),
        [&diagnostics](const Resources::ResourceDiagnostic& diagnostic) {
            diagnostics.push_back(diagnostic);
        }
        );
    DocumentContentResourcePort originalPort(originalLoader);

    const auto nonPdf = originalPort.openPrimaryPdf(
        reference("resource://documents/Lesson Templates/SP+WR Template.pptx"),
        QStringLiteral("DocumentViewer:non-pdf")
        );
    QVERIFY(!nonPdf);
    QCOMPARE(nonPdf.error().code, Domain::ErrorCode::Validation);
    QVERIFY(diagnostics.empty());

    const QString bodyId = QStringLiteral(
        "document.body:Guides/DYB Lesson Planning Guide.pdf"
        );
    for (const auto& [field, wrongValue] : {
             std::pair{QStringLiteral("category"), QStringLiteral("templates")},
             std::pair{QStringLiteral("resource_kind"), QStringLiteral("document_export")}
         })
    {
        QTemporaryDir fixture;
        QVERIFY(fixture.isValid());
        const QString fixtureCatalog = modifiedCatalog(
            fixture,
            bodyId,
            {{field, wrongValue}}
            );
        QVERIFY(!fixtureCatalog.isEmpty());

        auto modified = Resources::ResourceCatalog::load(
            fixtureCatalog,
            resourceRoot()
            );
        QVERIFY(modified);
        Resources::ResourceLoader modifiedLoader(
            std::move(*modified),
            [&diagnostics](const Resources::ResourceDiagnostic& diagnostic) {
                diagnostics.push_back(diagnostic);
            }
            );
        DocumentContentResourcePort modifiedPort(modifiedLoader);
        const auto rejected = modifiedPort.openPrimaryPdf(
            reference("resource://documents/Guides/DYB Lesson Planning Guide.pdf"),
            QStringLiteral("DocumentViewer:invalid-record")
            );
        QVERIFY(!rejected);
        QCOMPARE(rejected.error().code, Domain::ErrorCode::Validation);
        QVERIFY(diagnostics.empty());
    }

    {
        QTemporaryDir fixture;
        QVERIFY(fixture.isValid());
        const QString fixtureCatalog = modifiedCatalog(
            fixture,
            bodyId,
            {{QStringLiteral("expected_type"), QStringLiteral("pptx")}}
            );
        QVERIFY(!fixtureCatalog.isEmpty());

        QFile fixtureFile(fixtureCatalog);
        QVERIFY(fixtureFile.open(QIODevice::ReadOnly));
        const QJsonDocument fixtureDocument =
            QJsonDocument::fromJson(fixtureFile.readAll());
        QVERIFY(fixtureDocument.isObject());
        bool foundBody = false;
        for (const QJsonValue& value :
             fixtureDocument.object().value(QStringLiteral("records")).toArray())
        {
            const QJsonObject record = value.toObject();
            if (record.value(QStringLiteral("logical_id")).toString() != bodyId)
            {
                continue;
            }
            QCOMPARE(
                record.value(QStringLiteral("package_relative_path")).toString(),
                QStringLiteral("assets/documents/Guides/DYB Lesson Planning Guide.pdf")
                );
            QCOMPARE(
                record.value(QStringLiteral("expected_type")).toString(),
                QStringLiteral("pptx")
                );
            foundBody = true;
            break;
        }
        QVERIFY(foundBody);

        const auto modified = Resources::ResourceCatalog::load(
            fixtureCatalog,
            resourceRoot()
            );
        QVERIFY(!modified);
        QCOMPARE(modified.error().code, Resources::ResourceErrorCode::InvalidCatalog);
        // The package-type invariant rejects this descriptor before a loader
        // or port can issue a document-body request.
        QVERIFY(diagnostics.empty());
    }

    {
        QTemporaryDir fixture;
        QVERIFY(fixture.isValid());
        const QString fixtureCatalog = modifiedCatalog(
            fixture,
            bodyId,
            {{QStringLiteral("package_relative_path"),
              QStringLiteral("assets/documents/Guides/PortValidationMismatch.pdf")}}
            );
        QVERIFY(!fixtureCatalog.isEmpty());

        auto modified = Resources::ResourceCatalog::load(
            fixtureCatalog,
            resourceRoot()
            );
        QVERIFY(modified);
        const auto id = Resources::ResourceId::fromString(bodyId);
        QVERIFY(id);
        const Resources::ResourceDescriptor* const descriptor =
            modified->find(*id);
        QVERIFY(descriptor);
        QCOMPARE(descriptor->expectedTypeName(), QStringLiteral("pdf"));
        QCOMPARE(
            descriptor->packageRelativePath(),
            QStringLiteral("assets/documents/Guides/PortValidationMismatch.pdf")
            );
        Resources::ResourceLoader modifiedLoader(
            std::move(*modified),
            [&diagnostics](const Resources::ResourceDiagnostic& diagnostic) {
                diagnostics.push_back(diagnostic);
            }
            );
        DocumentContentResourcePort modifiedPort(modifiedLoader);
        const auto rejected = modifiedPort.openPrimaryPdf(
            reference("resource://documents/Guides/DYB Lesson Planning Guide.pdf"),
            QStringLiteral("DocumentViewer:wrong-package-path")
            );
        QVERIFY(!rejected);
        QCOMPARE(rejected.error().code, Domain::ErrorCode::Validation);
        QVERIFY(diagnostics.empty());
    }
}

void NextPlatformDocumentContentResourcePortTests::
missingSelectedBodyFailsWithoutFallback()
{
    QTemporaryDir package;
    QVERIFY(package.isValid());
    const QString fixtureCatalog = package.filePath(QStringLiteral("resource-catalog.json"));
    QVERIFY(QFile::copy(catalogPath(), fixtureCatalog));

    auto catalog = Resources::ResourceCatalog::load(
        fixtureCatalog,
        package.path()
        );
    QVERIFY(catalog);

    std::vector<Resources::ResourceDiagnostic> diagnostics;
    Resources::ResourceLoader loader(
        std::move(*catalog),
        [&diagnostics](const Resources::ResourceDiagnostic& diagnostic) {
            diagnostics.push_back(diagnostic);
        }
        );
    DocumentContentResourcePort port(loader);

    const auto missing = port.openPrimaryPdf(
        reference("resource://documents/Guides/DYB Lesson Planning Guide.pdf"),
        QStringLiteral("DocumentViewer:missing-package-body")
        );
    QVERIFY(!missing);
    QCOMPARE(missing.error().code, Domain::ErrorCode::NotFound);
    QCOMPARE(diagnostics.size(), std::size_t(1));
    QVERIFY(!diagnostics.front().succeeded);
    QCOMPARE(
        diagnostics.front().errorCode,
        std::optional<Resources::ResourceErrorCode>(
            Resources::ResourceErrorCode::MissingFile
            )
        );
    QVERIFY(!QFileInfo::exists(
        package.filePath(QStringLiteral("assets/documents/Guides/DYB Lesson Planning Guide.pdf"))
        ));
}

void NextPlatformDocumentContentResourcePortTests::
sameSizeCorruptSelectedBodyFailsWithoutFallback()
{
    QTemporaryDir package;
    QVERIFY(package.isValid());

    const QString fixtureCatalog = package.filePath(QStringLiteral("resource-catalog.json"));
    QVERIFY(QFile::copy(catalogPath(), fixtureCatalog));

    const QString selectedRelative =
        QStringLiteral("Guides/DYB Lesson Planning Guide.pdf");
    const QString alternateRelative =
        QStringLiteral("Guides/PowerPoint Keyboard Shortcuts.pdf");
    const QString selectedPackagePath =
        package.filePath(QStringLiteral("assets/documents/") + selectedRelative);
    const QString alternatePackagePath =
        package.filePath(QStringLiteral("assets/documents/") + alternateRelative);
    QVERIFY(copyFile(
        QDir(resourceRoot()).filePath(QStringLiteral("assets/documents/") + selectedRelative),
        selectedPackagePath
        ));
    QVERIFY(copyFile(
        QDir(resourceRoot()).filePath(QStringLiteral("assets/documents/") + alternateRelative),
        alternatePackagePath
        ));

    QFile selectedFile(selectedPackagePath);
    QVERIFY(selectedFile.open(QIODevice::ReadOnly));
    const QByteArray originalBytes = selectedFile.readAll();
    QVERIFY(!originalBytes.isEmpty());
    selectedFile.close();

    QByteArray corruptedBytes = originalBytes;
    corruptedBytes[0] = corruptedBytes.at(0) == 'X' ? 'Y' : 'X';
    QCOMPARE(corruptedBytes.size(), originalBytes.size());
    QVERIFY(corruptedBytes != originalBytes);
    QVERIFY(selectedFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(selectedFile.write(corruptedBytes), qint64(corruptedBytes.size()));
    selectedFile.close();
    QCOMPARE(QFileInfo(selectedPackagePath).size(), qint64(originalBytes.size()));
    QVERIFY(QFileInfo::exists(alternatePackagePath));

    auto catalog = Resources::ResourceCatalog::load(fixtureCatalog, package.path());
    QVERIFY(catalog);
    const auto selectedId = Resources::ResourceId::fromString(
        QStringLiteral("document.body:Guides/DYB Lesson Planning Guide.pdf")
        );
    const auto alternateId = Resources::ResourceId::fromString(
        QStringLiteral("document.body:Guides/PowerPoint Keyboard Shortcuts.pdf")
        );
    QVERIFY(selectedId);
    QVERIFY(alternateId);
    const Resources::ResourceDescriptor* const selectedDescriptor =
        catalog->find(*selectedId);
    QVERIFY(selectedDescriptor);
    QVERIFY(selectedDescriptor->expectedBytes().has_value());
    QCOMPARE(*selectedDescriptor->expectedBytes(), qint64(originalBytes.size()));
    QVERIFY(selectedDescriptor->expectedSha256().has_value());
    QVERIFY(QCryptographicHash::hash(corruptedBytes, QCryptographicHash::Sha256)
        != *selectedDescriptor->expectedSha256());
    QVERIFY(catalog->find(*alternateId));

    std::vector<Resources::ResourceDiagnostic> diagnostics;
    Resources::ResourceLoader loader(
        std::move(*catalog),
        [&diagnostics](const Resources::ResourceDiagnostic& diagnostic) {
            diagnostics.push_back(diagnostic);
        }
        );
    DocumentContentResourcePort port(loader);

    const auto corrupted = port.openPrimaryPdf(
        reference("resource://documents/Guides/DYB Lesson Planning Guide.pdf"),
        QStringLiteral("DocumentViewer:same-size-corruption")
        );
    QVERIFY(!corrupted);
    QCOMPARE(corrupted.error().code, Domain::ErrorCode::Validation);
    QVERIFY(!corrupted.error().recoverable);
    QCOMPARE(diagnostics.size(), std::size_t(1));
    QCOMPARE(
        diagnostics.front().resourceId.value(),
        QStringLiteral("document.body:Guides/DYB Lesson Planning Guide.pdf")
        );
    QCOMPARE(
        diagnostics.front().scope.kind(),
        Resources::ResourceScopeKind::ViewerSession
        );
    QVERIFY(!diagnostics.front().succeeded);
    QCOMPARE(
        diagnostics.front().errorCode,
        std::optional<Resources::ResourceErrorCode>(
            Resources::ResourceErrorCode::CorruptResource
            )
        );
}

void NextPlatformDocumentContentResourcePortTests::
boundaryIsMoveOnlyAndUsesLoaderOwnedStream()
{
    using Port = DocumentContentResourcePort;
    using Result = decltype(
        std::declval<const Port&>().openPrimaryPdf(
            std::declval<const DocumentContentReference&>(),
            std::declval<QString>()
            )
        );

    static_assert(std::is_same_v<Result, Domain::Result<DocumentContentResource>>);
    static_assert(!std::is_copy_constructible_v<Port>);
    static_assert(!std::is_move_constructible_v<Port>);
    static_assert(!std::is_copy_constructible_v<DocumentContentResource>);
    static_assert(std::is_move_constructible_v<DocumentContentResource>);
    static_assert(!std::is_copy_constructible_v<
        Resources::ScopedResource<Resources::ResourceFileStream>>);
    QVERIFY(true);
}

QTEST_GUILESS_MAIN(NextPlatformDocumentContentResourcePortTests)

#include "next_platform_document_content_resource_port_tests.moc"
