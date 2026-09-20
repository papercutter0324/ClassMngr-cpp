#include "core/application_services.h"
#include "next/application/document_catalog_use_case.h"
#include "next/platform/application_services_document_catalog_port.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QtTest/QtTest>

#include <algorithm>
#include <type_traits>

using namespace ClassMngr::Next;

namespace
{

QString utf8(const std::string& value)
{
    return QString::fromUtf8(value);
}

QString relativePath(const DocumentAssetReference& asset)
{
    return QDir::fromNativeSeparators(
        QDir(asset.path).filePath(asset.fileName)
        );
}

} // namespace

class NextPlatformApplicationServicesDocumentCatalogPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void projectsEmbeddedCatalogAsBoundedLocalizedMetadata();
    void preservesDocumentFlagsAndOptionalExportReferences();
    void missingRouteIsReportedByUseCase();
    void boundaryExposesValuesWithoutLegacyPointers();
    void navigationUsesPortAndPreservesViewerLeaseFlow();
};

void NextPlatformApplicationServicesDocumentCatalogPortTests::
projectsEmbeddedCatalogAsBoundedLocalizedMetadata()
{
    ApplicationServices services;
    const DocumentCatalog* legacy = services.documentCatalog();
    QVERIFY(legacy);
    QVERIFY(!legacy->folders().isEmpty());
    QVERIFY(!legacy->documents().isEmpty());

    Platform::ApplicationServicesDocumentCatalogPort port(services);
    const auto result = port.projection(QStringLiteral("ko_KR"));

    QVERIFY(result);
    const auto& projection = result.value();
    QCOMPARE(projection.folders().size(),
             static_cast<std::size_t>(legacy->folders().size()));
    QCOMPARE(projection.documents().size(),
             static_cast<std::size_t>(legacy->documents().size()));
    const Application::DocumentCatalogProjectionInput input{
        projection.folders(), projection.documents()
    };
    QVERIFY(Application::DocumentCatalogProjection::validate(input));

    for (const DocumentFolderDefinition& source : legacy->folders())
    {
        const auto id = Domain::DocumentFolderId::fromString(
            source.id.toUtf8().toStdString()
            );
        QVERIFY(id);
        const auto projected = std::find_if(
            projection.folders().cbegin(),
            projection.folders().cend(),
            [&id](const Application::DocumentFolderMetadata& folder)
            {
                return folder.id == *id;
            }
            );
        QVERIFY(projected != projection.folders().cend());
        QCOMPARE(utf8(projected->path), source.path);
        QCOMPARE(utf8(projected->parentPath), source.parentPath);
        QCOMPARE(utf8(projected->key), source.id);
        QCOMPARE(utf8(projected->displayName),
                 source.sidebarNames.forLocale(QStringLiteral("ko_KR")));
        QCOMPARE(projected->order, source.order);
        QVERIFY(projected->id.value().size()
                <= Application::kDocumentCatalogMaxIdentifierLength);
    }
}

void NextPlatformApplicationServicesDocumentCatalogPortTests::
preservesDocumentFlagsAndOptionalExportReferences()
{
    ApplicationServices services;
    const DocumentCatalog* legacy = services.documentCatalog();
    QVERIFY(legacy);
    Platform::ApplicationServicesDocumentCatalogPort port(services);
    const auto result = port.projection(QStringLiteral("en_US"));
    QVERIFY(result);

    for (const DocumentDefinition& source : legacy->documents())
    {
        const auto id = Domain::DocumentId::fromString(
            source.id.toUtf8().toStdString()
            );
        QVERIFY(id);
        const auto projected = std::find_if(
            result.value().documents().cbegin(),
            result.value().documents().cend(),
            [&id](const Application::DocumentEntryMetadata& document)
            {
                return document.id == *id;
            }
            );
        QVERIFY(projected != result.value().documents().cend());

        const auto folder = std::find_if(
            legacy->folders().cbegin(),
            legacy->folders().cend(),
            [&source](const DocumentFolderDefinition& candidate)
            {
                return candidate.path == source.pdf.path;
            }
            );
        QVERIFY(folder != legacy->folders().cend());
        QCOMPARE(utf8(projected->folderId.value()), folder->id);
        QCOMPARE(utf8(projected->path), relativePath(source.pdf));
        QCOMPARE(utf8(projected->key), source.id);
        QCOMPARE(utf8(projected->displayName),
                 source.sidebarNames.forLocale(QStringLiteral("en_US")));
        QCOMPARE(projected->printable, source.printingEnabled);

        const bool expectedExportable =
            source.exportingEnabled && source.exportFile.has_value();
        QCOMPARE(projected->exportable, expectedExportable);
        QCOMPARE(projected->exportReference.has_value(), expectedExportable);
        QCOMPARE(
            utf8(projected->contentReference.value()),
            QStringLiteral("resource://documents/") + relativePath(source.pdf)
            );
        QVERIFY(projected->contentReference.value().size()
                <= Application::kDocumentCatalogMaxReferenceLength);

        if (expectedExportable)
        {
            QCOMPARE(
                utf8(projected->exportReference->value()),
                QStringLiteral("resource://documents/")
                    + relativePath(*source.exportFile)
                );
        }
    }
}

void NextPlatformApplicationServicesDocumentCatalogPortTests::
missingRouteIsReportedByUseCase()
{
    ApplicationServices services;
    Platform::ApplicationServicesDocumentCatalogPort port(services);
    const auto projection = port.projection(QStringLiteral("en_US"));
    QVERIFY(projection);
    Application::DocumentContentSession session;
    const Application::DocumentCatalogUseCase useCase(
        projection.value(), session
        );
    const auto missing = Domain::DocumentId::fromString(
        "document_that_does_not_exist"
        );
    QVERIFY(missing);

    const auto result = useCase.resolveDocument(*missing);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
}

void NextPlatformApplicationServicesDocumentCatalogPortTests::
boundaryExposesValuesWithoutLegacyPointers()
{
    using Port = Platform::ApplicationServicesDocumentCatalogPort;
    using Result = decltype(
        std::declval<const Port&>().projection(std::declval<const QString&>())
        );
    static_assert(std::is_same_v<
        Result,
        Domain::Result<Application::DocumentCatalogProjection>
        >);
    static_assert(!std::is_copy_constructible_v<Port>);
    static_assert(!std::is_move_constructible_v<Port>);
    QVERIFY(true);
}

void NextPlatformApplicationServicesDocumentCatalogPortTests::
navigationUsesPortAndPreservesViewerLeaseFlow()
{
    const QString sourcePath = QFINDTESTDATA(
        "../src/app/controllers/navigation_controller.cpp"
        );
    QVERIFY2(!sourcePath.isEmpty(), "NavigationController source was not found.");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("ApplicationServicesDocumentCatalogPort"));
    QVERIFY(contents.contains("DocumentCatalogUseCase"));
    QVERIFY(contents.contains("DocumentContentSession"));
    QVERIFY(contents.contains("resolveDocument"));
    QVERIFY(!contents.contains("m_services->documentCatalog()"));
    QVERIFY(!contents.contains("const DocumentDefinition* document"));
    QVERIFY(contents.contains("confirmCurrentPageCanLeave"));
    QVERIFY(contents.contains("m_documentContentResourcePort.resolve"));
    QVERIFY(!contents.contains("ResourcePaths::Documents::acquire"));
    QVERIFY(contents.contains("PdfViewerDocumentDescriptor"));
    QVERIFY(contents.contains("viewer->loadPdf"));
    QVERIFY(contents.contains("PageType::PdfViewer"));
}

QTEST_APPLESS_MAIN(NextPlatformApplicationServicesDocumentCatalogPortTests)

#include "next_platform_application_services_document_catalog_port_tests.moc"
