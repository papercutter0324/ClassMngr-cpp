#include "next/platform/resource_document_catalog_port.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_diagnostics.h"
#include "next/resources/resource_loader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <algorithm>
#include <iostream>
#include <string_view>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

bool fail(const QString& message)
{
    std::cerr << message.toStdString() << '\n';
    return false;
}

int failMain(const QString& message)
{
    std::cerr << message.toStdString() << '\n';
    return 1;
}

bool copyFile(const QString& source, const QString& destination)
{
    if (!QDir().mkpath(QFileInfo(destination).absolutePath()))
    {
        return fail(QStringLiteral("cannot create fixture directory for ") + destination);
    }
    QFile::remove(destination);
    if (!QFile::copy(source, destination))
    {
        return fail(QStringLiteral("cannot copy fixture file ") + source);
    }
    return true;
}

bool verifyProjection(
    const Application::DocumentCatalogProjection& projection,
    const std::string_view expectedGuidesName,
    const std::string_view expectedLessonPlanningName
    )
{
    if (projection.folders().size() != 7 || projection.documents().size() != 30)
    {
        return fail(QStringLiteral("catalog projection did not contain 7 folders and 30 documents"));
    }

    const auto guides = std::find_if(
        projection.folders().begin(),
        projection.folders().end(),
        [](const auto& folder) {
            return folder.id.value() == "document_guides";
        }
        );
    if (guides == projection.folders().end()
        || guides->path.value() != "Guides"
        || guides->parentPath.value() != ""
        || guides->order.rawValue() != 10
        || guides->displayName != expectedGuidesName)
    {
        return fail(QStringLiteral("Guides folder metadata or localization was not preserved"));
    }

    const auto lessonPlanning = std::find_if(
        projection.documents().begin(),
        projection.documents().end(),
        [](const auto& document) {
            return document.id.value() == "document_guides_lesson_planning";
        }
        );
    if (lessonPlanning == projection.documents().end()
        || lessonPlanning->folderId.value() != "document_guides"
        || lessonPlanning->path.value() != "Guides/DYB Lesson Planning Guide.pdf"
        || lessonPlanning->displayName != expectedLessonPlanningName
        || !lessonPlanning->printable
        || !lessonPlanning->exportable
        || lessonPlanning->contentReference.value()
            != "resource://documents/Guides/DYB Lesson Planning Guide.pdf"
        || !lessonPlanning->exportReference
        || lessonPlanning->exportReference->value()
            != "resource://documents/Guides/DYB Lesson Planning Guide.pdf")
    {
        return fail(QStringLiteral("lesson planning metadata or references were not preserved"));
    }

    const auto firstTemplate = std::find_if(
        projection.documents().begin(),
        projection.documents().end(),
        [](const auto& document) {
            return document.id.value() == "document_lesson_templates_sp_wr";
        }
        );
    if (firstTemplate == projection.documents().end()
        || firstTemplate->printable
        || !firstTemplate->exportable
        || firstTemplate->contentReference.value()
            != "resource://documents/Lesson Templates/SP+WR Template.pdf"
        || !firstTemplate->exportReference
        || firstTemplate->exportReference->value()
            != "resource://documents/Lesson Templates/SP+WR Template.pptx")
    {
        return fail(QStringLiteral("template flags or separate export reference were not preserved"));
    }

    if (projection.documents().front().id.value()
            != "document_guides_lesson_planning"
        || projection.documents().front().order.rawValue() != 10)
    {
        return fail(QStringLiteral("document metadata order was not preserved"));
    }
    return true;
}

} // namespace

int main()
{
    QTemporaryDir packageDirectory;
    if (!packageDirectory.isValid())
    {
        return failMain(QStringLiteral("cannot create isolated resource package"));
    }

    const QString packageRoot = packageDirectory.path();
    const QString generatedRoot = QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
    const QString generatedCatalog = QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG);
    const QString fixtureCatalog = QDir(packageRoot).filePath(
        QStringLiteral("resource-catalog.json")
        );
    const QString fixtureMetadata = QDir(packageRoot).filePath(
        QStringLiteral("assets/documents/documents.json")
        );
    const QString generatedMetadata = QDir(generatedRoot).filePath(
        QStringLiteral("assets/documents/documents.json")
        );
    if (!copyFile(generatedCatalog, fixtureCatalog)
        || !copyFile(generatedMetadata, fixtureMetadata))
    {
        return 1;
    }

    const QString omittedBody = QDir(packageRoot).filePath(
        QStringLiteral("assets/documents/Guides/DYB Lesson Planning Guide.pdf")
        );
    if (QFileInfo::exists(omittedBody))
    {
        return failMain(QStringLiteral("fixture unexpectedly contains a document body"));
    }

    auto catalog = Resources::ResourceCatalog::load(fixtureCatalog, packageRoot);
    if (!catalog)
    {
        return failMain(QStringLiteral("generated catalog manifest was rejected: ")
            + catalog.error().message);
    }

    std::vector<Resources::ResourceDiagnostic> diagnostics;
    Resources::ResourceLoader observedLoader(
        *catalog,
        [&diagnostics](const Resources::ResourceDiagnostic& diagnostic) {
            diagnostics.push_back(diagnostic);
        }
        );
    Platform::ResourceDocumentCatalogPort observedPort(observedLoader);
    const auto localized = observedPort.projection(QStringLiteral("ko-KR"));
    if (!localized)
    {
        return failMain(QStringLiteral("document catalog metadata did not project: ")
            + QString::fromStdString(localized.error().message));
    }
    if (!verifyProjection(localized.value(), "안내서", "수업 계획"))
    {
        return 1;
    }

    const auto metadataId = Resources::ResourceId::fromString(
        QStringLiteral("document.catalog-metadata")
        );
    if (diagnostics.size() != 1 || !metadataId
        || diagnostics.front().resourceId != *metadataId
        || diagnostics.front().scope.kind() != Resources::ResourceScopeKind::Startup
        || diagnostics.front().scope.ownerLabel()
            != QStringLiteral("DocumentCatalogMetadata")
        || !diagnostics.front().succeeded
        || std::any_of(
            diagnostics.begin(),
            diagnostics.end(),
            [](const Resources::ResourceDiagnostic& diagnostic) {
                return diagnostic.resourceId.value().startsWith(
                    QStringLiteral("document.body:")
                    );
            }
            ))
    {
        return failMain(QStringLiteral("resource diagnostics did not record exactly one Startup metadata request"));
    }

    Resources::ResourceLoader fallbackLoader(*catalog);
    Platform::ResourceDocumentCatalogPort fallbackPort(fallbackLoader);
    const auto fallback = fallbackPort.projection(QStringLiteral("fr-CA"));
    if (!fallback || !verifyProjection(fallback.value(), "Guides", "Lesson Planning"))
    {
        return failMain(QStringLiteral("default locale fallback was not preserved"));
    }

    return 0;
}
