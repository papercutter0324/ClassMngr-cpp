#include "core/process_memory_snapshot.h"
#include "next/platform/resource_document_catalog_port.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_diagnostics.h"
#include "next/resources/resource_loader.h"
#include "next/ui/application_shell.h"
#include "next/ui/document_catalog_navigation.h"
#include "preferences_test_ports.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPixmap>
#include <QTranslator>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

constexpr QSize Viewport{1024, 720};

QString readText(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QJsonObject memoryJson(const ProcessMemorySnapshot& snapshot)
{
    return {
        {QStringLiteral("available"), snapshot.isAvailable},
        {QStringLiteral("platform"), snapshot.platform},
        {QStringLiteral("workingSetBytes"),
         static_cast<double>(snapshot.workingSetBytes)},
        {QStringLiteral("privateUsageBytes"),
         static_cast<double>(snapshot.privateUsageBytes)},
        {QStringLiteral("capturedAt"),
         snapshot.capturedAt.toString(Qt::ISODateWithMs)}
    };
}

QTreeWidgetItem* findDocument(
    QTreeWidget* tree,
    const QString& documentId
    )
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
                static_cast<int>(Ui::DocumentCatalogItemRole::DocumentId)
                ).toString() == documentId)
        {
            return *iterator;
        }
        ++iterator;
    }
    return nullptr;
}

std::size_t countKind(
    QTreeWidget* tree,
    const Ui::DocumentCatalogItemKind kind
    )
{
    std::size_t count = 0;
    QTreeWidgetItemIterator iterator(tree);
    while (*iterator)
    {
        if ((*iterator)->data(
                0,
                static_cast<int>(Ui::DocumentCatalogItemRole::Kind)
                ).toInt() == static_cast<int>(kind))
        {
            ++count;
        }
        ++iterator;
    }
    return count;
}

bool saveCatalogCapture(
    Resources::ResourceLoader& loader,
    Application::DocumentCatalogProjection projection,
    const QString& locale,
    const QString& theme,
    const QString& outputDirectory,
    const std::optional<ProcessMemorySnapshot>& beforeShell,
    QJsonObject* memoryMeasurement,
    QJsonObject* captureRecord,
    QString* error
    )
{
    const QString themePath = QDir(QString::fromUtf8(
        CLASSMNGR_NEXT_TEST_RESOURCE_ROOT
        )).filePath(QStringLiteral("assets/styles/%1.qss").arg(theme));
    const QString stylesheet = readText(themePath);
    if (stylesheet.isEmpty())
    {
        if (error)
        {
            *error = QStringLiteral("Could not read packaged theme: %1")
                         .arg(themePath);
        }
        return false;
    }
    qApp->setStyleSheet(stylesheet);

    const int folderCount = static_cast<int>(projection.folders().size());
    const int documentCount = static_cast<int>(projection.documents().size());
    const QString fileName = QStringLiteral("document_catalog_%1_%2.png")
                                 .arg(locale, theme);
    const QString outputPath = QDir(outputDirectory).filePath(fileName);

    ProcessMemorySnapshot renderedMemory;
    ProcessMemorySnapshot releasedMemory;
    bool saved = false;
    {
        Test::MemoryPreferences preferences;
        Ui::ApplicationShell shell(loader, preferences.model);
        shell.setDocumentCatalogProjection(std::move(projection));
        shell.resize(Viewport);
        shell.show();
        QCoreApplication::processEvents();

        QTreeWidget* const tree = shell.findChild<QTreeWidget*>(
            QStringLiteral("classMngrNextNavigationTree")
            );
        QTreeWidgetItem* const selected = findDocument(
            tree,
            QStringLiteral("document_guides_lesson_planning")
            );
        if (!tree || !selected)
        {
            if (error)
            {
                *error = QStringLiteral(
                    "The packaged catalog did not expose the expected stable document ID."
                    );
            }
            return false;
        }
        tree->setCurrentItem(selected);
        QCoreApplication::processEvents();

        const QPixmap pixmap = shell.grab();
        saved = pixmap.size() == Viewport && pixmap.save(outputPath, "PNG");
        renderedMemory = PlatformProcessMemorySnapshotProvider().snapshot();
        if (!saved)
        {
            if (error)
            {
                *error = QStringLiteral("Could not save a %1x%2 catalog capture.")
                             .arg(Viewport.width())
                             .arg(Viewport.height());
            }
            return false;
        }

        if (memoryMeasurement && beforeShell)
        {
            *memoryMeasurement = {
                {QStringLiteral("metricSource"),
                 QStringLiteral("PlatformProcessMemorySnapshotProvider (whole-process resident working set and private usage)")},
                {QStringLiteral("beforeMetadataProjection"),
                 memoryJson(*beforeShell)},
                {QStringLiteral("afterCatalogRender"), memoryJson(renderedMemory)},
                {QStringLiteral("metadataFolderCount"), folderCount},
                {QStringLiteral("metadataDocumentCount"), documentCount},
                {QStringLiteral("documentBodyResourceDiagnostics"), 0},
                {QStringLiteral("qtPdfDocumentInstances"), 0},
                {QStringLiteral("lifecycleInterpretation"),
                 QStringLiteral("Catalog construction and sidebar rendering retain localized metadata and typed resource references only. The capture path does not construct a QPdfDocument or request a document body.")}
            };
        }

        *captureRecord = {
            {QStringLiteral("locale"), locale},
            {QStringLiteral("theme"), theme},
            {QStringLiteral("file"), fileName},
            {QStringLiteral("width"), pixmap.width()},
            {QStringLiteral("height"), pixmap.height()},
            {QStringLiteral("folderRows"),
             static_cast<int>(countKind(tree, Ui::DocumentCatalogItemKind::Folder))},
            {QStringLiteral("documentRows"),
             static_cast<int>(countKind(tree, Ui::DocumentCatalogItemKind::Document))},
            {QStringLiteral("selectedDocumentId"), QStringLiteral("document_guides_lesson_planning")},
            {QStringLiteral("selectedDocumentBodyLoaded"), false}
        };
    }
    releasedMemory = PlatformProcessMemorySnapshotProvider().snapshot();
    if (memoryMeasurement && !memoryMeasurement->isEmpty())
    {
        memoryMeasurement->insert(
            QStringLiteral("afterShellAndCatalogRelease"),
            memoryJson(releasedMemory)
            );
        memoryMeasurement->insert(
            QStringLiteral("wholeProcessMemoryMayRemainHigh"),
            true
            );
    }
    return saved;
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    QApplication::setApplicationName(QStringLiteral("ClassMngr Next catalog capture"));
    if (argc != 2)
    {
        qCritical("Usage: ClassMngrNextDocumentCatalogVisualCapture OUTPUT_DIR");
        return 2;
    }

    const QString outputDirectory = QString::fromLocal8Bit(argv[1]);
    if (!QDir().mkpath(outputDirectory))
    {
        qCritical("Could not create document catalog evidence directory");
        return 2;
    }

    const QString resourceRoot = QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
    const QString catalogPath = QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG);
    auto catalog = Resources::ResourceCatalog::load(catalogPath, resourceRoot);
    if (!catalog)
    {
        qCritical().noquote() << "Could not load Next resource catalog:"
                              << catalog.error().message;
        return 1;
    }

    std::vector<Resources::ResourceDiagnostic> diagnostics;
    Resources::ResourceLoader loader(
        std::move(*catalog),
        [&diagnostics](const Resources::ResourceDiagnostic& diagnostic) {
            diagnostics.push_back(diagnostic);
        }
        );
    const Platform::ResourceDocumentCatalogPort catalogPort(loader);

    for (const QString& font : {
             QStringLiteral("Inter.ttc"),
             QStringLiteral("PretendardVariable.ttf")
         })
    {
        const QString fontPath = QDir(resourceRoot).filePath(
            QStringLiteral("assets/fonts/") + font
            );
        if (QFile::exists(fontPath))
        {
            static_cast<void>(QFontDatabase::addApplicationFont(fontPath));
        }
    }

    QJsonArray captures;
    QJsonObject memoryMeasurement;
    std::optional<ProcessMemorySnapshot> beforeMetadata;
    PlatformProcessMemorySnapshotProvider memoryProvider;
    for (const QString& locale : {
             QStringLiteral("en_US"),
             QStringLiteral("ko_KR")
         })
    {
        if (locale == QStringLiteral("ko_KR"))
        {
            const QString translationPath = QDir(QString::fromUtf8(
                CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY
                )).filePath(QStringLiteral("ClassMngr_ko_KR.qm"));
            auto translator = std::make_unique<QTranslator>();
            if (!translator->load(translationPath)
                || !application.installTranslator(translator.get()))
            {
                qCritical().noquote() << "Could not load Korean translation:"
                                      << translationPath;
                return 1;
            }

            const auto projection = catalogPort.projection(locale);
            if (!projection)
            {
                qCritical().noquote() << "Could not project Korean document catalog:"
                                      << QString::fromStdString(projection.error().message);
                return 1;
            }
            for (const QString& theme : {
                     QStringLiteral("light"),
                     QStringLiteral("dark")
                 })
            {
                QJsonObject record;
                QString error;
                if (!saveCatalogCapture(
                        loader,
                        projection.value(),
                        locale,
                        theme,
                        outputDirectory,
                        std::nullopt,
                        nullptr,
                        &record,
                        &error
                        ))
                {
                    qCritical().noquote() << "Korean catalog capture failed:"
                                          << error;
                    return 1;
                }
                captures.append(record);
            }
            application.removeTranslator(translator.get());
        }
        else
        {
            beforeMetadata = memoryProvider.snapshot();
            const auto projection = catalogPort.projection(locale);
            if (!projection)
            {
                qCritical().noquote() << "Could not project English document catalog:"
                                      << QString::fromStdString(projection.error().message);
                return 1;
            }
            for (const QString& theme : {
                     QStringLiteral("light"),
                     QStringLiteral("dark")
                 })
            {
                QJsonObject record;
                QJsonObject measurement;
                QString error;
                const bool sampled = theme == QStringLiteral("light");
                if (!saveCatalogCapture(
                        loader,
                        projection.value(),
                        locale,
                        theme,
                        outputDirectory,
                        sampled ? beforeMetadata : std::nullopt,
                        sampled ? &measurement : nullptr,
                        &record,
                        &error
                        ))
                {
                    qCritical().noquote() << "English catalog capture failed:"
                                          << error;
                    return 1;
                }
                if (sampled)
                {
                    memoryMeasurement = std::move(measurement);
                }
                captures.append(record);
            }
        }
    }

    const std::size_t bodyDiagnostics = static_cast<std::size_t>(std::count_if(
        diagnostics.cbegin(),
        diagnostics.cend(),
        [](const Resources::ResourceDiagnostic& diagnostic) {
            return diagnostic.resourceId.value()
                != QStringLiteral("document.catalog-metadata");
        }
        ));
    if (bodyDiagnostics != 0 || diagnostics.size() != 2)
    {
        qCritical() << "Unexpected document body resource diagnostics:"
                    << bodyDiagnostics << "total resource events="
                    << diagnostics.size();
        return 1;
    }

    QJsonArray diagnosticIds;
    for (const Resources::ResourceDiagnostic& diagnostic : diagnostics)
    {
        diagnosticIds.append(diagnostic.resourceId.value());
    }
    QJsonObject manifest{
        {QStringLiteral("purpose"), QStringLiteral("Next Document catalog sidebar captures and metadata-only lifecycle sample")},
        {QStringLiteral("viewport"), QJsonObject{
             {QStringLiteral("width"), Viewport.width()},
             {QStringLiteral("height"), Viewport.height()}}},
        {QStringLiteral("qtVersion"), QStringLiteral(QT_VERSION_STR)},
        {QStringLiteral("qtPlatformPlugin"), QStringLiteral("offscreen")},
        {QStringLiteral("style"), QStringLiteral("Fusion with packaged light/dark stylesheet")},
        {QStringLiteral("catalogSource"), QStringLiteral("ResourceDocumentCatalogPort projection of packaged documents.json metadata")},
        {QStringLiteral("documentBodyResourceDiagnostics"), static_cast<int>(bodyDiagnostics)},
        {QStringLiteral("resourceDiagnosticIds"), diagnosticIds},
        {QStringLiteral("qtPdfDocumentInstances"), 0},
        {QStringLiteral("qtPdfLinkage"), QStringLiteral("The capture target does not link Qt6::Pdf or Qt6::PdfWidgets, and the catalog/navigation call path does not construct QPdfDocument.")},
        {QStringLiteral("memoryMeasurement"), memoryMeasurement},
        {QStringLiteral("captures"), captures}
    };

    QFile manifestFile(QDir(outputDirectory).filePath(
        QStringLiteral("document_catalog_capture.json")
        ));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || manifestFile.write(QJsonDocument(manifest).toJson(
               QJsonDocument::Indented
               )) < 0)
    {
        qCritical("Could not write document catalog capture manifest");
        return 1;
    }
    qInfo().noquote() << "Captured Next Document catalog evidence in"
                      << outputDirectory;
    return 0;
}
