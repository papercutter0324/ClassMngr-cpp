#include "next/domain/document_content_reference.h"
#include "next/platform/document_export_resource_port.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_diagnostics.h"
#include "next/resources/resource_loader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
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
using ClassMngr::Next::Platform::DocumentExportResourcePort;
using ClassMngr::Next::Resources::ResourceFileStream;
using ClassMngr::Next::Resources::ScopedResource;

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

DocumentContentReference reference(const char* value)
{
    return DocumentContentReference(value);
}

DocumentContentReference reference(std::string value)
{
    return DocumentContentReference(std::move(value));
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

QString modifiedCatalog(
    QTemporaryDir& directory,
    const QString& resourceId,
    const std::vector<std::pair<QString, QString>>& replacements
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
        if (record.value(QStringLiteral("logical_id")).toString() != resourceId)
        {
            continue;
        }
        for (const auto& [field, replacement] : replacements)
        {
            record.insert(field, replacement);
        }
        records.replace(index, record);
        updated = true;
        break;
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

} // namespace

class NextPlatformDocumentExportResourcePortTests final : public QObject
{
    Q_OBJECT

private slots:
    void opensOnlySelectedPptxWithOperationScope();
    void opensCatalogedDocxExport();
    void rejectsInvalidNonOfficeAndMissingInputsWithoutFallback();
    void rejectsMismatchedDocumentBodyDescriptorsBeforeLoading();
    void missingSelectedBodyFailsWithoutFallback();
    void boundaryIsMoveOnlyAndUsesLoaderOwnedStream();
};

void NextPlatformDocumentExportResourcePortTests::
opensOnlySelectedPptxWithOperationScope()
{
    QTemporaryDir package;
    QVERIFY(package.isValid());
    QVERIFY(copyFile(
        catalogPath(),
        package.filePath(QStringLiteral("resource-catalog.json"))
        ));

    const QString selectedRelative =
        QStringLiteral("Lesson Templates/SP+WR Template.pptx");
    const QString selectedPackagePath =
        package.filePath(QStringLiteral("assets/documents/") + selectedRelative);
    QVERIFY(copyFile(
        QDir(resourceRoot()).filePath(QStringLiteral("assets/documents/") + selectedRelative),
        selectedPackagePath
        ));
    QVERIFY(QFileInfo::exists(selectedPackagePath));
    QVERIFY(!QFileInfo::exists(package.filePath(
        QStringLiteral("assets/documents/Lesson Templates/Skill Lesson Template.pptx")
        )));
    QVERIFY(!QFileInfo::exists(package.filePath(
        QStringLiteral(
            "assets/documents/Speaking Evaluations/Speaking Evaluation Topic Options.docx"
            )
        )));

    auto catalog = Resources::ResourceCatalog::load(
        package.filePath(QStringLiteral("resource-catalog.json")),
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
    QVERIFY(diagnostics.empty());

    DocumentExportResourcePort port(loader);
    QVERIFY(diagnostics.empty());

    QPointer<QIODevice> streamDevice;
    {
        auto result = port.openExport(
            reference("resource://documents/Lesson Templates/SP+WR Template.pptx"),
            QStringLiteral("SpeakingEvaluation:export-selected-template")
            );
        QVERIFY(result);
        QVERIFY(result.value().get().isOpen());
        QVERIFY(result.value().get().device().isOpen());
        QCOMPARE(
            result.value().scope().kind(),
            Resources::ResourceScopeKind::Operation
            );
        QCOMPARE(
            result.value().scope().ownerLabel(),
            QStringLiteral("SpeakingEvaluation:export-selected-template")
            );

        QCOMPARE(diagnostics.size(), std::size_t(1));
        QCOMPARE(
            diagnostics.front().resourceId.value(),
            QStringLiteral("document.body:Lesson Templates/SP+WR Template.pptx")
            );
        QCOMPARE(
            diagnostics.front().scope.kind(),
            Resources::ResourceScopeKind::Operation
            );
        QCOMPARE(
            diagnostics.front().scope.ownerLabel(),
            QStringLiteral("SpeakingEvaluation:export-selected-template")
            );
        QVERIFY(diagnostics.front().succeeded);

        streamDevice = &result.value().get().device();
        QVERIFY(streamDevice);
        QVERIFY(streamDevice->isOpen());
        result.value().get().close();
        QVERIFY(streamDevice);
        QVERIFY(!streamDevice->isOpen());
    }
    QVERIFY(streamDevice.isNull());
}

void NextPlatformDocumentExportResourcePortTests::opensCatalogedDocxExport()
{
    QTemporaryDir package;
    QVERIFY(package.isValid());
    QVERIFY(copyFile(
        catalogPath(),
        package.filePath(QStringLiteral("resource-catalog.json"))
        ));

    const QString selectedRelative =
        QStringLiteral("Speaking Evaluations/Speaking Evaluation Topic Options.docx");
    QVERIFY(copyFile(
        QDir(resourceRoot()).filePath(QStringLiteral("assets/documents/") + selectedRelative),
        package.filePath(QStringLiteral("assets/documents/") + selectedRelative)
        ));

    auto catalog = Resources::ResourceCatalog::load(
        package.filePath(QStringLiteral("resource-catalog.json")),
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
    DocumentExportResourcePort port(loader);
    QVERIFY(diagnostics.empty());

    auto result = port.openExport(
        reference(
            "resource://documents/Speaking Evaluations/Speaking Evaluation Topic Options.docx"
            ),
        QStringLiteral("DocumentExport:open-topic-options")
        );
    QVERIFY(result);
    QVERIFY(result.value().get().isOpen());
    QCOMPARE(diagnostics.size(), std::size_t(1));
    QCOMPARE(
        diagnostics.front().resourceId.value(),
        QStringLiteral("document.body:Speaking Evaluations/Speaking Evaluation Topic Options.docx")
        );
    QVERIFY(diagnostics.front().succeeded);
}

void NextPlatformDocumentExportResourcePortTests::
rejectsInvalidNonOfficeAndMissingInputsWithoutFallback()
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
    DocumentExportResourcePort port(loader);

    std::string embeddedNul =
        "resource://documents/Lesson Templates/SP+WR Template.pptx";
    embeddedNul.push_back('\0');
    embeddedNul.append("suffix");
    std::string invalidUtf8 = "resource://documents/Lesson Templates/";
    invalidUtf8.append("\xED\xA0\x80", 3);
    std::string oversizedReference = "resource://documents/";
    oversizedReference.append(
        Application::kDocumentContentMaxReferenceLength,
        'x'
        );
    const std::vector<DocumentContentReference> invalidReferences{
        reference(""),
        reference("documents/Lesson Templates/SP+WR Template.pptx"),
        reference("resource://exports/Lesson Templates/SP+WR Template.pptx"),
        reference("resource://documents/"),
        reference("resource://documents/../Lesson Templates/SP+WR Template.pptx"),
        reference("resource://documents/Lesson Templates/../../other.pptx"),
        reference("resource://documents/Lesson Templates\\..\\other.pptx"),
        reference("resource://documents/Lesson Templates//SP+WR Template.pptx"),
        reference("resource://documents/Lesson Templates/SP+WR Template.pptx?download=1"),
        reference(std::move(embeddedNul)),
        reference(std::move(invalidUtf8)),
        reference(std::move(oversizedReference))
    };

    QVERIFY(diagnostics.empty());
    for (const DocumentContentReference& invalid : invalidReferences)
    {
        const auto result = port.openExport(
            invalid,
            QStringLiteral("DocumentExport:invalid-reference")
            );
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }

    const auto nonOffice = port.openExport(
        reference("resource://documents/Guides/DYB Lesson Planning Guide.pdf"),
        QStringLiteral("DocumentExport:non-office")
        );
    QVERIFY(!nonOffice);
    QCOMPARE(nonOffice.error().code, Domain::ErrorCode::Validation);

    const auto missingCatalogRecord = port.openExport(
        reference("resource://documents/Unlisted/Other Template.pptx"),
        QStringLiteral("DocumentExport:missing-catalog-entry")
        );
    QVERIFY(!missingCatalogRecord);
    QCOMPARE(missingCatalogRecord.error().code, Domain::ErrorCode::NotFound);

    const auto missingOwner = port.openExport(
        reference("resource://documents/Lesson Templates/SP+WR Template.pptx"),
        QStringLiteral("   ")
        );
    QVERIFY(!missingOwner);
    QCOMPARE(missingOwner.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(diagnostics.empty());
}

void NextPlatformDocumentExportResourcePortTests::
rejectsMismatchedDocumentBodyDescriptorsBeforeLoading()
{
    const QString bodyId = QStringLiteral(
        "document.body:Lesson Templates/SP+WR Template.pptx"
        );
    const std::vector<std::vector<std::pair<QString, QString>>> replacements{
        {{QStringLiteral("category"), QStringLiteral("templates")}},
        {{QStringLiteral("resource_kind"), QStringLiteral("document_export")}},
        {{QStringLiteral("package_relative_path"),
          QStringLiteral("assets/documents/Lesson Templates/Other Template.pptx")}}
    };
    for (const auto& descriptorReplacements : replacements)
    {
        QTemporaryDir fixture;
        QVERIFY(fixture.isValid());
        const QString fixtureCatalog = modifiedCatalog(
            fixture,
            bodyId,
            descriptorReplacements
            );
        QVERIFY(!fixtureCatalog.isEmpty());

        auto modified = Resources::ResourceCatalog::load(fixtureCatalog, resourceRoot());
        QVERIFY(modified);
        std::vector<Resources::ResourceDiagnostic> diagnostics;
        Resources::ResourceLoader loader(
            std::move(*modified),
            [&diagnostics](const Resources::ResourceDiagnostic& diagnostic) {
                diagnostics.push_back(diagnostic);
            }
            );
        DocumentExportResourcePort port(loader);
        const auto result = port.openExport(
            reference("resource://documents/Lesson Templates/SP+WR Template.pptx"),
            QStringLiteral("DocumentExport:invalid-record")
            );
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
        QVERIFY(diagnostics.empty());
    }
}

void NextPlatformDocumentExportResourcePortTests::missingSelectedBodyFailsWithoutFallback()
{
    QTemporaryDir package;
    QVERIFY(package.isValid());
    QVERIFY(copyFile(
        catalogPath(),
        package.filePath(QStringLiteral("resource-catalog.json"))
        ));

    // Keep a different Office body available to show that a missing selected
    // export is not silently replaced by another template.
    const QString alternateRelative =
        QStringLiteral("Lesson Templates/Skill Lesson Template.pptx");
    QVERIFY(copyFile(
        QDir(resourceRoot()).filePath(QStringLiteral("assets/documents/") + alternateRelative),
        package.filePath(QStringLiteral("assets/documents/") + alternateRelative)
        ));
    QVERIFY(!QFileInfo::exists(package.filePath(
        QStringLiteral("assets/documents/Lesson Templates/SP+WR Template.pptx")
        )));

    auto catalog = Resources::ResourceCatalog::load(
        package.filePath(QStringLiteral("resource-catalog.json")),
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
    DocumentExportResourcePort port(loader);
    QVERIFY(diagnostics.empty());

    const auto missing = port.openExport(
        reference("resource://documents/Lesson Templates/SP+WR Template.pptx"),
        QStringLiteral("DocumentExport:missing-selected-template")
        );
    QVERIFY(!missing);
    QCOMPARE(missing.error().code, Domain::ErrorCode::NotFound);
    QCOMPARE(diagnostics.size(), std::size_t(1));
    QCOMPARE(
        diagnostics.front().resourceId.value(),
        QStringLiteral("document.body:Lesson Templates/SP+WR Template.pptx")
        );
    QVERIFY(!diagnostics.front().succeeded);
    QCOMPARE(
        diagnostics.front().errorCode,
        std::optional<Resources::ResourceErrorCode>(
            Resources::ResourceErrorCode::MissingFile
            )
        );
}

void NextPlatformDocumentExportResourcePortTests::
boundaryIsMoveOnlyAndUsesLoaderOwnedStream()
{
    using Port = DocumentExportResourcePort;
    using Handle = ScopedResource<ResourceFileStream>;
    using Result = decltype(
        std::declval<const Port&>().openExport(
            std::declval<const DocumentContentReference&>(),
            std::declval<QString>()
            )
        );

    static_assert(std::is_same_v<Result, Domain::Result<Handle>>);
    static_assert(!std::is_copy_constructible_v<Port>);
    static_assert(!std::is_move_constructible_v<Port>);
    static_assert(!std::is_copy_constructible_v<Handle>);
    static_assert(std::is_move_constructible_v<Handle>);
    static_assert(!std::is_copy_constructible_v<ResourceFileStream>);
    static_assert(std::is_move_constructible_v<ResourceFileStream>);
    QVERIFY(true);
}

QTEST_GUILESS_MAIN(NextPlatformDocumentExportResourcePortTests)

#include "next_platform_document_export_resource_port_tests.moc"
