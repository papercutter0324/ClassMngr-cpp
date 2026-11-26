#include "core/process_memory_snapshot.h"
#include "core/fontmanager.h"
#include "next/platform/resource_document_catalog_port.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_diagnostics.h"
#include "next/resources/resource_loader.h"
#include "next/ui/application_shell.h"
#include "next/ui/document_catalog_navigation.h"
#include "preferences_test_ports.h"
#include "font_family_availability_test_support.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetrics>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPixmap>
#include <QTranslator>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

constexpr QSize Viewport{1024, 720};
constexpr auto SelectedDocumentId = "document_guides_lesson_planning";
constexpr int SidebarStableKeyRole = Qt::UserRole + 4;

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

QTreeWidgetItem* findSidebarItemByKey(
    QTreeWidget* tree,
    const QString& key
    )
{
    if (!tree)
    {
        return nullptr;
    }

    QTreeWidgetItemIterator iterator(tree);
    while (*iterator)
    {
        if ((*iterator)->data(0, SidebarStableKeyRole).toString() == key)
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

struct CatalogRowCounts final
{
    int folders = 0;
    int documents = 0;
};

QString fromUtf8(const std::string& text)
{
    return QString::fromUtf8(
        text.data(), static_cast<qsizetype>(text.size()));
}

CatalogRowCounts countLegacyCatalogRows(
    QTreeWidget* tree,
    const Application::DocumentCatalogProjection& projection
    )
{
    CatalogRowCounts counts;
    for (const auto& folder : projection.folders())
    {
        if (findSidebarItemByKey(tree, fromUtf8(folder.key.value())))
        {
            ++counts.folders;
        }
    }
    for (const auto& document : projection.documents())
    {
        if (findSidebarItemByKey(tree, fromUtf8(document.key.value())))
        {
            ++counts.documents;
        }
    }
    return counts;
}

QJsonObject fontMetadata(const QWidget& widget)
{
    const QFont font = widget.font();
    const QFontInfo resolved(font);
    const QFontMetrics metrics(font);
    return {
        {QStringLiteral("requestedFontFamilies"),
         QJsonArray::fromStringList(font.families())},
        {QStringLiteral("resolvedFontFamily"), resolved.family()},
        {QStringLiteral("hangulSyllableU+AC00InFontMetrics"),
         metrics.inFont(QChar(0xAC00))}
    };
}

QJsonObject captureRecord(
    const QString& surface,
    const QString& locale,
    const QString& theme,
    const QString& fileName,
    const QSize& imageSize,
    const CatalogRowCounts& rowCounts,
    const Application::DocumentCatalogProjection& projection,
    const QWidget& fontWidget,
    const QString& selectedStableId
    )
{
    const int expectedFolders = static_cast<int>(projection.folders().size());
    const int expectedDocuments = static_cast<int>(projection.documents().size());
    QJsonObject record{
        {QStringLiteral("surface"), surface},
        {QStringLiteral("locale"), locale},
        {QStringLiteral("theme"), theme},
        {QStringLiteral("file"), fileName},
        {QStringLiteral("width"), imageSize.width()},
        {QStringLiteral("height"), imageSize.height()},
        {QStringLiteral("folderRows"), rowCounts.folders},
        {QStringLiteral("documentRows"), rowCounts.documents},
        {QStringLiteral("expectedFolderRows"), expectedFolders},
        {QStringLiteral("expectedDocumentRows"), expectedDocuments},
        {QStringLiteral("rowCountsVerified"),
         rowCounts.folders == expectedFolders
             && rowCounts.documents == expectedDocuments},
        {QStringLiteral("selectedDocumentId"), selectedStableId},
        {QStringLiteral("selectedDocumentBodyLoaded"), false}
    };
    const QJsonObject font = fontMetadata(fontWidget);
    for (auto it = font.cbegin(); it != font.cend(); ++it)
    {
        record.insert(it.key(), it.value());
    }
    return record;
}

bool saveCatalogCapture(
    Resources::ResourceLoader& loader,
    Application::DocumentCatalogProjection projection,
    const QString& locale,
    const QString& theme,
    const QString& outputDirectory,
    const std::optional<ProcessMemorySnapshot>& beforeShell,
    QJsonObject* memoryMeasurement,
    QJsonArray* captures,
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
    QJsonObject record;
    bool saved = false;
    {
        Test::MemoryPreferences preferences;
        Ui::ApplicationShell shell(
            loader,
            preferences.model,
            Test::makeMemoryWindowGeometryPort(),
            Test::emptyFontFamilyAvailabilityPort()
            );
        shell.setDocumentCatalogProjection(std::move(projection));
        shell.resize(Viewport);
        shell.show();
        QCoreApplication::processEvents();

        QTreeWidget* const tree = shell.findChild<QTreeWidget*>(
            QStringLiteral("classMngrNextNavigationTree")
            );
        QTreeWidgetItem* const selected = findDocument(
            tree,
            QString::fromLatin1(SelectedDocumentId)
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

        const CatalogRowCounts rows{
            static_cast<int>(countKind(
                tree, Ui::DocumentCatalogItemKind::Folder)),
            static_cast<int>(countKind(
                tree, Ui::DocumentCatalogItemKind::Document))
        };
        if (rows.folders != folderCount || rows.documents != documentCount
            || selected->data(
                   0,
                   static_cast<int>(Ui::DocumentCatalogItemRole::DocumentId)
                   ).toString() != QString::fromLatin1(SelectedDocumentId))
        {
            if (error)
            {
                *error = QStringLiteral(
                    "Next shell rows or selected stable ID did not match the projection.");
            }
            return false;
        }

        record = captureRecord(
            QStringLiteral("next-full-shell"),
            locale,
            theme,
            fileName,
            pixmap.size(),
            rows,
            *shell.documentCatalogProjection(),
            *tree,
            QString::fromLatin1(SelectedDocumentId)
            );
        record.insert(QStringLiteral("bundledFontManagerApplied"), false);

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
                 QStringLiteral("Catalog construction and full-shell rendering retain localized metadata and typed resource references only. The capture path does not construct a QPdfDocument or request a document body.")}
            };
        }
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

    if (captures)
    {
        captures->append(record);
    }
    return saved;
}

bool saveSidebarPairCaptures(
    Resources::ResourceLoader& loader,
    const Application::DocumentCatalogProjection& projection,
    const QString& locale,
    const QString& theme,
    const QString& outputDirectory,
    QJsonArray* captures,
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

    QApplication& application = *qApp;
    FontManager::loadFonts();
    FontManager::applyGlobalFont(application, locale);

    const int folderCount = static_cast<int>(projection.folders().size());
    const int documentCount = static_cast<int>(projection.documents().size());
    const QString nextFileName = QStringLiteral(
        "document_catalog_next_sidebar_%1_%2.png").arg(locale, theme);
    const QString legacyFileName = QStringLiteral(
        "document_catalog_legacy_sidebar_%1_%2.png").arg(locale, theme);
    const QString nextPath = QDir(outputDirectory).filePath(nextFileName);
    const QString legacyPath = QDir(outputDirectory).filePath(legacyFileName);

    QSize nextTreeSize;
    {
        Test::MemoryPreferences preferences;
        Ui::ApplicationShell shell(
            loader,
            preferences.model,
            Test::makeMemoryWindowGeometryPort(),
            Test::emptyFontFamilyAvailabilityPort()
            );
        shell.setDocumentCatalogProjection(projection);
        shell.resize(Viewport);
        shell.show();
        QCoreApplication::processEvents();

        QTreeWidget* const tree = shell.findChild<QTreeWidget*>(
            QStringLiteral("classMngrNextNavigationTree")
            );
        QTreeWidgetItem* const selected = findDocument(
            tree,
            QString::fromLatin1(SelectedDocumentId)
            );
        if (!tree || !selected)
        {
            if (error)
            {
                *error = QStringLiteral(
                    "The Next navigation tree did not expose the expected stable document ID.");
            }
            return false;
        }
        tree->setCurrentItem(selected);
        tree->scrollToItem(selected);
        QCoreApplication::processEvents();

        const CatalogRowCounts rows{
            static_cast<int>(countKind(
                tree, Ui::DocumentCatalogItemKind::Folder)),
            static_cast<int>(countKind(
                tree, Ui::DocumentCatalogItemKind::Document))
        };
        if (rows.folders != folderCount || rows.documents != documentCount
            || selected->data(
                   0,
                   static_cast<int>(Ui::DocumentCatalogItemRole::DocumentId)
                   ).toString() != QString::fromLatin1(SelectedDocumentId))
        {
            if (error)
            {
                *error = QStringLiteral(
                    "Next navigation rows or selected stable ID did not match the projection.");
            }
            return false;
        }

        const QJsonObject nextFont = fontMetadata(*tree);
        if (nextFont.value(QStringLiteral("resolvedFontFamily")).toString().isEmpty()
            || !nextFont.value(
                    QStringLiteral("hangulSyllableU+AC00InFontMetrics")).toBool())
        {
            if (error)
            {
                *error = QStringLiteral(
                    "Next navigation font did not resolve bundled font/Hangul metadata.");
            }
            return false;
        }

        const QPixmap pixmap = tree->grab();
        if (pixmap.isNull() || !pixmap.save(nextPath, "PNG"))
        {
            if (error)
            {
                *error = QStringLiteral(
                    "Could not save the Next navigation-tree widget capture.");
            }
            return false;
        }
        nextTreeSize = tree->size();

        if (captures)
        {
            QJsonObject record = captureRecord(
                QStringLiteral("next-navigation-tree"),
                locale,
                theme,
                nextFileName,
                pixmap.size(),
                rows,
                projection,
                *tree,
                QString::fromLatin1(SelectedDocumentId)
                );
            record.insert(QStringLiteral("bundledFontManagerApplied"), true);
            captures->append(record);
        }
    }

    Sidebar legacySidebar;
    legacySidebar.setDocumentCatalog(projection, locale);
    QTreeWidget* const legacyTree = legacySidebar.findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree"));
    QTreeWidgetItem* const legacySelected = findSidebarItemByKey(
        legacyTree,
        QString::fromLatin1(SelectedDocumentId)
        );
    if (!legacyTree || !legacySelected)
    {
        if (error)
        {
            *error = QStringLiteral(
                "Legacy Sidebar did not expose the expected stable document key.");
        }
        return false;
    }
    for (QTreeWidgetItem* ancestor = legacySelected->parent();
         ancestor;
         ancestor = ancestor->parent())
    {
        ancestor->setExpanded(true);
    }
    legacyTree->setCurrentItem(legacySelected);
    legacyTree->scrollToItem(legacySelected);

    QLayout* const legacyLayout = legacySidebar.layout();
    if (!legacyLayout)
    {
        if (error)
        {
            *error = QStringLiteral("Legacy Sidebar has no capture layout.");
        }
        return false;
    }
    const QMargins margins = legacyLayout->contentsMargins();
    legacySidebar.resize(
        nextTreeSize.width() + margins.left() + margins.right(),
        nextTreeSize.height() + margins.top() + margins.bottom()
        );
    legacySidebar.show();
    QCoreApplication::processEvents();
    if (legacyTree->size() != nextTreeSize)
    {
        if (error)
        {
            *error = QStringLiteral(
                "Legacy and Next navigation-tree widget regions did not match in size.");
        }
        return false;
    }

    const CatalogRowCounts legacyRows = countLegacyCatalogRows(
        legacyTree, projection);
    if (legacyRows.folders != folderCount
        || legacyRows.documents != documentCount
        || legacyTree->currentItem() != legacySelected
        || legacySelected->data(0, SidebarStableKeyRole).toString()
            != QString::fromLatin1(SelectedDocumentId))
    {
        if (error)
        {
            *error = QStringLiteral(
                "Legacy Sidebar rows or selected stable ID did not match the projection.");
        }
        return false;
    }

    const QJsonObject legacyFont = fontMetadata(*legacyTree);
    if (legacyFont.value(QStringLiteral("resolvedFontFamily")).toString().isEmpty()
        || !legacyFont.value(
                QStringLiteral("hangulSyllableU+AC00InFontMetrics")).toBool())
    {
        if (error)
        {
            *error = QStringLiteral(
                "Legacy Sidebar font did not resolve bundled font/Hangul metadata.");
        }
        return false;
    }

    const QPixmap legacyPixmap = legacyTree->grab();
    if (legacyPixmap.isNull() || !legacyPixmap.save(legacyPath, "PNG"))
    {
        if (error)
        {
            *error = QStringLiteral("Could not save the Legacy Sidebar tree capture.");
        }
        return false;
    }
    if (captures)
    {
        QJsonObject record = captureRecord(
            QStringLiteral("legacy-sidebar-tree"),
            locale,
            theme,
            legacyFileName,
            legacyPixmap.size(),
            legacyRows,
            projection,
            *legacyTree,
            QString::fromLatin1(SelectedDocumentId)
            );
        record.insert(QStringLiteral("bundledFontManagerApplied"), true);
        captures->append(record);
    }
    return true;
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

    QJsonArray fullShellCaptures;
    QJsonObject memoryMeasurement;
    std::optional<ProcessMemorySnapshot> beforeMetadata;
    PlatformProcessMemorySnapshotProvider memoryProvider;
    std::vector<std::pair<QString, Application::DocumentCatalogProjection>>
        projections;
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
                QString error;
                if (!saveCatalogCapture(
                        loader,
                        projection.value(),
                        locale,
                        theme,
                        outputDirectory,
                        std::nullopt,
                        nullptr,
                        &fullShellCaptures,
                        &error
                        ))
                {
                    qCritical().noquote() << "Korean catalog capture failed:"
                                          << error;
                    return 1;
                }
            }
            projections.emplace_back(locale, projection.value());
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
                        &fullShellCaptures,
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
            }
            projections.emplace_back(locale, projection.value());
        }
    }

    QJsonArray captures = fullShellCaptures;
    for (const auto& [locale, projection] : projections)
    {
        std::unique_ptr<QTranslator> translator;
        if (locale == QStringLiteral("ko_KR"))
        {
            const QString translationPath = QDir(QString::fromUtf8(
                CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY
                )).filePath(QStringLiteral("ClassMngr_ko_KR.qm"));
            translator = std::make_unique<QTranslator>();
            if (!translator->load(translationPath)
                || !application.installTranslator(translator.get()))
            {
                qCritical().noquote() << "Could not reload Korean translation:"
                                      << translationPath;
                return 1;
            }
        }

        for (const QString& theme : {
                 QStringLiteral("light"),
                 QStringLiteral("dark")
             })
        {
            QString error;
            if (!saveSidebarPairCaptures(
                    loader,
                    projection,
                    locale,
                    theme,
                    outputDirectory,
                    &captures,
                    &error
                    ))
            {
                qCritical().noquote() << "Sidebar pair capture failed for"
                                      << locale << theme << ":" << error;
                return 1;
            }
        }
        if (translator)
        {
            application.removeTranslator(translator.get());
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
        {QStringLiteral("purpose"), QStringLiteral("Next full-shell captures plus paired Next navigation-tree and Legacy Sidebar captures for the packaged Document Catalog")},
        {QStringLiteral("viewport"), QJsonObject{
             {QStringLiteral("width"), Viewport.width()},
             {QStringLiteral("height"), Viewport.height()}}},
        {QStringLiteral("qtVersion"), QStringLiteral(QT_VERSION_STR)},
        {QStringLiteral("qtPlatformPlugin"), QStringLiteral("offscreen")},
        {QStringLiteral("style"), QStringLiteral("Fusion with packaged light/dark stylesheet")},
        {QStringLiteral("localeThemeOrder"), QJsonArray{
             QStringLiteral("en_US/light"),
             QStringLiteral("en_US/dark"),
             QStringLiteral("ko_KR/light"),
             QStringLiteral("ko_KR/dark")}},
        {QStringLiteral("captureCount"), static_cast<int>(captures.size())},
        {QStringLiteral("pairedSidebarCaptureCount"), 8},
        {QStringLiteral("selectedStableDocumentId"),
         QString::fromLatin1(SelectedDocumentId)},
        {QStringLiteral("pairedTreeWidgetRegionsMatchSize"), true},
        {QStringLiteral("pairedTreesShareProjectionAndThemeStyles"), true},
        {QStringLiteral("pairedTreesUseProductionBundledFonts"), true},
        {QStringLiteral("fontMetadataSource"),
         QStringLiteral("QFontInfo and QFontMetrics from each captured tree widget; the four retained full-shell rows record their pre-existing active font state")},
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
    if (captures.size() != 12)
    {
        qCritical() << "Expected 12 Document Catalog capture records; found"
                    << captures.size();
        return 1;
    }

    qInfo().noquote() << "Captured 12 Document catalog evidence images in"
                      << outputDirectory;
    return 0;
}
