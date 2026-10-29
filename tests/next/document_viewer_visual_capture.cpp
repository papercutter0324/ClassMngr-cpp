#include "core/process_memory_snapshot.h"
#include "next/domain/domain_types.h"
#include "next/platform/resource_document_catalog_port.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_diagnostics.h"
#include "next/resources/resource_loader.h"
#include "next/ui/application_shell.h"
#include "next/ui/document_catalog_navigation.h"
#include "next/ui/document_viewer_page.h"
#include "next/ui/document_viewer_route.h"
#include "preferences_test_ports.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPixmap>
#include <QPdfDocument>
#include <QPdfView>
#include <QThread>
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
const Domain::DocumentId kSelectedDocumentId =
    *Domain::DocumentId::fromString("document_guides_lesson_planning");
const Domain::DocumentId kMissingDocumentId =
    *Domain::DocumentId::fromString("document_missing_capture_selection");

QString readText(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QString documentIdText(const Domain::DocumentId& documentId)
{
    return QString::fromStdString(documentId.value());
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
        {QStringLiteral("handleCount"), static_cast<int>(snapshot.handleCount)},
        {QStringLiteral("threadCount"), static_cast<int>(snapshot.threadCount)},
        {QStringLiteral("capturedAt"),
         snapshot.capturedAt.toString(Qt::ISODateWithMs)}
    };
}

QString phaseText(const Application::DocumentContentPhase phase)
{
    using Application::DocumentContentPhase;
    switch (phase)
    {
    case DocumentContentPhase::Idle: return QStringLiteral("Idle");
    case DocumentContentPhase::Requested: return QStringLiteral("Requested");
    case DocumentContentPhase::Loading: return QStringLiteral("Loading");
    case DocumentContentPhase::Ready: return QStringLiteral("Ready");
    case DocumentContentPhase::Failed: return QStringLiteral("Failed");
    case DocumentContentPhase::Released: return QStringLiteral("Released");
    }
    return QStringLiteral("Unknown");
}

QString pdfStatusText(const QPdfDocument::Status status)
{
    switch (status)
    {
    case QPdfDocument::Status::Null: return QStringLiteral("Null");
    case QPdfDocument::Status::Loading: return QStringLiteral("Loading");
    case QPdfDocument::Status::Unloading: return QStringLiteral("Unloading");
    case QPdfDocument::Status::Ready: return QStringLiteral("Ready");
    case QPdfDocument::Status::Error: return QStringLiteral("Error");
    }
    return QStringLiteral("Unknown");
}

QTreeWidgetItem* findDocument(
    QTreeWidget* tree,
    const Domain::DocumentId& documentId
    )
{
    if (!tree)
    {
        return nullptr;
    }
    QTreeWidgetItemIterator iterator(tree);
    while (*iterator)
    {
        if (Ui::DocumentCatalogNavigation::documentIdFor(*iterator)
            == std::optional<Domain::DocumentId>(documentId))
        {
            return *iterator;
        }
        ++iterator;
    }
    return nullptr;
}

std::size_t bodyDiagnosticCount(
    const std::vector<Resources::ResourceDiagnostic>& diagnostics
    )
{
    return static_cast<std::size_t>(std::count_if(
        diagnostics.cbegin(),
        diagnostics.cend(),
        [](const Resources::ResourceDiagnostic& diagnostic) {
            return diagnostic.resourceId.value().startsWith(
                QStringLiteral("document.body:")
                );
        }
        ));
}

QJsonObject lifecycleState(
    const ProcessMemorySnapshot& memory,
    const Ui::DocumentViewerPage* viewer
    )
{
    QJsonObject state{
        {QStringLiteral("processMemory"), memoryJson(memory)},
        {QStringLiteral("activePrimaryStreams"),
         viewer && viewer->hasActivePrimaryStream() ? 1 : 0},
        {QStringLiteral("contentSessionPhase"),
         viewer ? phaseText(viewer->contentSnapshot().phase())
                : QStringLiteral("absent")},
        {QStringLiteral("qpdfDocumentStatus"),
         viewer ? pdfStatusText(viewer->documentStatus())
                : QStringLiteral("absent")},
        {QStringLiteral("qpdfDocumentInstances"),
         viewer ? viewer->findChildren<QPdfDocument*>().size() : 0},
        {QStringLiteral("activeResourceHandles"),
         viewer && viewer->hasActivePrimaryStream() ? 1 : 0}
    };
    return state;
}

bool saveStage(
    Ui::ApplicationShell& shell,
    const QString& outputDirectory,
    const QString& locale,
    const QString& theme,
    const QString& stage,
    const QString& selectedDocumentId,
    const QString& errorMessage,
    const QJsonObject& lifecycle,
    QJsonObject* record,
    QString* error
    )
{
    const QString fileName = QStringLiteral("document_viewer_%1_%2_%3.png")
                                 .arg(locale, theme, stage);
    const QPixmap pixmap = shell.grab();
    const QString path = QDir(outputDirectory).filePath(fileName);
    if (pixmap.size() != Viewport || !pixmap.save(path, "PNG"))
    {
        if (error)
        {
            *error = QStringLiteral("Could not save the %1 viewer capture.")
                         .arg(stage);
        }
        return false;
    }
    *record = {
        {QStringLiteral("locale"), locale},
        {QStringLiteral("theme"), theme},
        {QStringLiteral("state"), stage},
        {QStringLiteral("file"), fileName},
        {QStringLiteral("width"), pixmap.width()},
        {QStringLiteral("height"), pixmap.height()},
        {QStringLiteral("selectedDocumentId"), selectedDocumentId},
        {QStringLiteral("errorMessage"), errorMessage},
        {QStringLiteral("lifecycle"), lifecycle}
    };
    return true;
}

bool waitUntilReady(Ui::DocumentViewerPage* viewer)
{
    if (!viewer)
    {
        return false;
    }
    QElapsedTimer timer;
    timer.start();
    while (!viewer->hasLoadedDocument() && timer.elapsed() < 10000)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
        QThread::msleep(10);
    }
    QCoreApplication::processEvents();
    return viewer->hasLoadedDocument();
}

void settlePdfRendering(Ui::DocumentViewerPage* viewer)
{
    if (QPdfView* const view = viewer
            ? viewer->findChild<QPdfView*>(QStringLiteral("documentViewerPdfView"))
            : nullptr)
    {
        if (view->viewport())
        {
            view->viewport()->update();
            QCoreApplication::processEvents();
            QThread::msleep(500);
            QCoreApplication::processEvents();
            view->viewport()->repaint();
            QCoreApplication::processEvents();
        }
    }
}

bool captureCombination(
    QApplication& application,
    Resources::ResourceLoader& loader,
    const Application::DocumentCatalogProjection& projection,
    const QString& locale,
    const QString& theme,
    const QString& outputDirectory,
    std::vector<Resources::ResourceDiagnostic>& diagnostics,
    QJsonArray* captures,
    QJsonObject* measurement,
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
    application.setStyleSheet(stylesheet);

    PlatformProcessMemorySnapshotProvider memoryProvider;
    const ProcessMemorySnapshot baselineMemory = memoryProvider.snapshot();
    const std::size_t bodyDiagnosticsAtStart = bodyDiagnosticCount(diagnostics);
    ProcessMemorySnapshot openedMemory;
    ProcessMemorySnapshot releasedMemory;
    ProcessMemorySnapshot reopenedMemory;
    QJsonArray lifecycleStages;
    QJsonObject record;
    QJsonObject baselineLifecycle = lifecycleState(baselineMemory, nullptr);
    baselineLifecycle.insert(
        QStringLiteral("documentBodyOpenDiagnosticsThisCombination"),
        0
        );
    lifecycleStages.append(QJsonObject{
        {QStringLiteral("stage"), QStringLiteral("baseline")},
        {QStringLiteral("state"), baselineLifecycle}
    });

    {
        Test::MemoryPreferences preferences;
        Ui::ApplicationShell shell(loader, preferences.model);
        shell.setDocumentCatalogProjection(projection);
        const auto registration = Ui::registerDocumentViewerPage(shell.pageHost());
        if (!registration)
        {
            if (error)
            {
                *error = QStringLiteral("Could not register document viewer: %1")
                             .arg(registration.error().message);
            }
            return false;
        }
        Ui::bindDocumentViewerSelection(shell);
        shell.resize(Viewport);
        shell.show();
        QCoreApplication::processEvents();

        QTreeWidget* const tree = shell.findChild<QTreeWidget*>(
            QStringLiteral("classMngrNextNavigationTree")
            );
        QTreeWidgetItem* const selected = findDocument(tree, kSelectedDocumentId);
        if (!tree || !selected)
        {
            if (error)
            {
                *error = QStringLiteral("The packaged catalog lacks the expected document row.");
            }
            return false;
        }
        tree->setCurrentItem(selected);
        QCoreApplication::processEvents();

        auto* viewer = dynamic_cast<Ui::DocumentViewerPage*>(
            shell.pageHost().pageInstance(
                QString::fromLatin1(Ui::kDocumentViewerPageKey)
                )
            );
        if (!waitUntilReady(viewer))
        {
            if (error)
            {
                *error = QStringLiteral("The selected packaged PDF did not reach Ready.");
            }
            return false;
        }
        settlePdfRendering(viewer);
        openedMemory = memoryProvider.snapshot();
        QJsonObject readyLifecycle = lifecycleState(
            openedMemory,
            viewer
            );
        readyLifecycle.insert(
            QStringLiteral("documentBodyOpenDiagnosticsThisCombination"),
            static_cast<int>(bodyDiagnosticCount(diagnostics) - bodyDiagnosticsAtStart)
            );
        lifecycleStages.append(QJsonObject{
            {QStringLiteral("stage"), QStringLiteral("open")},
            {QStringLiteral("state"), readyLifecycle}
        });
        if (!saveStage(
                shell,
                outputDirectory,
                locale,
                theme,
                QStringLiteral("ready"),
                documentIdText(kSelectedDocumentId),
                {},
                readyLifecycle,
                &record,
                error
                ))
        {
            return false;
        }
        captures->append(record);

        if (!viewer->openDocument(kMissingDocumentId, projection))
        {
            QCoreApplication::processEvents();
        }
        if (viewer->lastErrorMessage().isEmpty()
            || viewer->documentStatus() != QPdfDocument::Status::Null
            || viewer->hasActivePrimaryStream())
        {
            if (error)
            {
                *error = QStringLiteral("Missing document selection did not release viewer content and show an error.");
            }
            return false;
        }
        const ProcessMemorySnapshot errorMemory = memoryProvider.snapshot();
        QJsonObject errorLifecycle = lifecycleState(errorMemory, viewer);
        errorLifecycle.insert(
            QStringLiteral("documentBodyOpenDiagnosticsThisCombination"),
            static_cast<int>(bodyDiagnosticCount(diagnostics) - bodyDiagnosticsAtStart)
            );
        lifecycleStages.append(QJsonObject{
            {QStringLiteral("stage"), QStringLiteral("error")},
            {QStringLiteral("state"), errorLifecycle}
        });
        if (!saveStage(
                shell,
                outputDirectory,
                locale,
                theme,
                QStringLiteral("error"),
                documentIdText(kMissingDocumentId),
                viewer->lastErrorMessage(),
                errorLifecycle,
                &record,
                error
                ))
        {
            return false;
        }
        captures->append(record);

        if (!viewer->closeDocument())
        {
            if (error)
            {
                *error = QStringLiteral("The viewer did not close after the error state.");
            }
            return false;
        }
        releasedMemory = memoryProvider.snapshot();
        QJsonObject closedLifecycle = lifecycleState(releasedMemory, viewer);
        closedLifecycle.insert(
            QStringLiteral("documentBodyOpenDiagnosticsThisCombination"),
            static_cast<int>(bodyDiagnosticCount(diagnostics) - bodyDiagnosticsAtStart)
            );
        lifecycleStages.append(QJsonObject{
            {QStringLiteral("stage"), QStringLiteral("release")},
            {QStringLiteral("state"), closedLifecycle}
        });
        if (!saveStage(
                shell,
                outputDirectory,
                locale,
                theme,
                QStringLiteral("closed"),
                {},
                {},
                closedLifecycle,
                &record,
                error
                ))
        {
            return false;
        }
        captures->append(record);

        // A null selection clears the current tree row so choosing the same
        // typed ID again exercises the production selection callback.
        tree->setCurrentItem(nullptr);
        QCoreApplication::processEvents();
        tree->setCurrentItem(selected);
        QCoreApplication::processEvents();
        viewer = dynamic_cast<Ui::DocumentViewerPage*>(
            shell.pageHost().pageInstance(
                QString::fromLatin1(Ui::kDocumentViewerPageKey)
                )
            );
        if (!waitUntilReady(viewer))
        {
            if (error)
            {
                *error = QStringLiteral("Reselecting the same document did not open a fresh ready session.");
            }
            return false;
        }
        settlePdfRendering(viewer);
        reopenedMemory = memoryProvider.snapshot();
        QJsonObject reopenedLifecycle = lifecycleState(
            reopenedMemory,
            viewer
            );
        reopenedLifecycle.insert(
            QStringLiteral("documentBodyOpenDiagnosticsThisCombination"),
            static_cast<int>(bodyDiagnosticCount(diagnostics) - bodyDiagnosticsAtStart)
            );
        lifecycleStages.append(QJsonObject{
            {QStringLiteral("stage"), QStringLiteral("reopen")},
            {QStringLiteral("state"), reopenedLifecycle}
        });
        if (!saveStage(
                shell,
                outputDirectory,
                locale,
                theme,
                QStringLiteral("reopened"),
                documentIdText(kSelectedDocumentId),
                {},
                reopenedLifecycle,
                &record,
                error
                ))
        {
            return false;
        }
        captures->append(record);

    }

    const ProcessMemorySnapshot afterPageReleaseMemory = memoryProvider.snapshot();
    QJsonObject afterPageReleaseLifecycle = lifecycleState(
        afterPageReleaseMemory,
        nullptr
        );
    afterPageReleaseLifecycle.insert(
        QStringLiteral("documentBodyOpenDiagnosticsThisCombination"),
        static_cast<int>(bodyDiagnosticCount(diagnostics) - bodyDiagnosticsAtStart)
        );
    lifecycleStages.append(QJsonObject{
        {QStringLiteral("stage"), QStringLiteral("pageRelease")},
        {QStringLiteral("state"), afterPageReleaseLifecycle}
    });
    *measurement = {
        {QStringLiteral("locale"), locale},
        {QStringLiteral("theme"), theme},
        {QStringLiteral("metricSource"), QStringLiteral("PlatformProcessMemorySnapshotProvider (whole-process working set/private usage plus process handles and threads)")},
        {QStringLiteral("stages"), lifecycleStages}
    };

    return true;
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    QApplication::setApplicationName(QStringLiteral("ClassMngr Next document viewer capture"));
    if (argc != 2)
    {
        qCritical("Usage: ClassMngrNextDocumentViewerVisualCapture OUTPUT_DIR");
        return 2;
    }

    const QString outputDirectory = QString::fromLocal8Bit(argv[1]);
    if (!QDir().mkpath(outputDirectory))
    {
        qCritical("Could not create document viewer evidence directory");
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
    QJsonArray measurements;
    std::vector<QTranslator*> installedTranslators;
    for (const QString& locale : {
             QStringLiteral("en_US"),
             QStringLiteral("ko_KR")
         })
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
                qCritical().noquote() << "Could not load Korean translation:"
                                      << translationPath;
                return 1;
            }
            installedTranslators.push_back(translator.get());
        }

        const auto projection = catalogPort.projection(locale);
        if (!projection)
        {
            qCritical().noquote() << "Could not project document catalog:"
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
            if (!captureCombination(
                    application,
                    loader,
                    projection.value(),
                    locale,
                    theme,
                    outputDirectory,
                    diagnostics,
                    &captures,
                    &measurement,
                    &error
                    ))
            {
                qCritical().noquote() << "Document viewer capture failed:"
                                      << error;
                return 1;
            }
            measurements.append(measurement);
        }

        if (translator)
        {
            application.removeTranslator(translator.get());
            installedTranslators.erase(
                std::remove(installedTranslators.begin(), installedTranslators.end(),
                            translator.get()),
                installedTranslators.end()
                );
        }
    }

    const std::size_t totalBodyDiagnostics = bodyDiagnosticCount(diagnostics);
    if (totalBodyDiagnostics != 8)
    {
        qCritical() << "Expected exactly two selected-body opens per visual capture combination; observed"
                    << totalBodyDiagnostics;
        return 1;
    }
    QJsonObject manifest{
        {QStringLiteral("purpose"), QStringLiteral("Paired Next and Legacy document viewer ready/error/closed/reopened captures plus Next process/resource lifecycle observations")},
        {QStringLiteral("taskId"), QStringLiteral("phase7_c43_document_viewer_implementation_01")},
        {QStringLiteral("viewport"), QJsonObject{
             {QStringLiteral("width"), Viewport.width()},
             {QStringLiteral("height"), Viewport.height()}}},
        {QStringLiteral("qtVersion"), QStringLiteral(QT_VERSION_STR)},
        {QStringLiteral("qtPlatformPlugin"), QStringLiteral("offscreen")},
        {QStringLiteral("style"), QStringLiteral("Fusion with packaged light/dark stylesheet")},
        {QStringLiteral("catalogSource"), QStringLiteral("ResourceDocumentCatalogPort projection of packaged documents.json metadata")},
        {QStringLiteral("selectionRoute"), QStringLiteral("ApplicationShell typed DocumentId callback bound with bindDocumentViewerSelection; reopen reselects the same tree row after clearing it")},
        {QStringLiteral("errorState"), QStringLiteral("Unknown stable DocumentId; the prior QPdfDocument is closed and its stream/session are released before the missing selection is reported")},
        {QStringLiteral("documentBodyOpenDiagnostics"), static_cast<int>(totalBodyDiagnostics)},
        {QStringLiteral("activeResourceCountMeaning"), QStringLiteral("One active primary file-backed stream while open/reopened, zero after error/close; ResourceLoader exposes no global active-stream counter, so this is the viewer-owned handle count.")},
        {QStringLiteral("measurements"), measurements},
        {QStringLiteral("captures"), captures},
        {QStringLiteral("legacyCompanionManifest"), QStringLiteral("legacy_document_viewer_capture.json")},
        {QStringLiteral("comparison"), QStringLiteral("Next captures show the full Next application shell and viewer; the companion Legacy capture target instantiates production PdfViewerPage at the same 1024x720 widget viewport. Legacy keeps print/export in application actions and has no in-page Close button; Next puts Print, Export, and Close in its viewer action row. These are structural observations from matched page-state captures, not pixel-parity claims." )},
        {QStringLiteral("captureLimits"), QJsonArray{
             QStringLiteral("Qt 6.12.0 offscreen/Fusion capture at 1024x720; no native window manager, system font rasterizer, printer, or file dialog is exercised."),
             QStringLiteral("Whole-process memory and process handle/thread counts include Qt and allocator state; they are observations without a threshold."),
             QStringLiteral("Legacy companion captures instantiate PdfViewerPage directly at the same widget viewport; they do not claim full-window or pixel parity with the Next application shell."),
             QStringLiteral("Print and export buttons show capability state; the printer and interactive save dialog are not invoked by this capture harness.")
        }}
    };

    QFile manifestFile(QDir(outputDirectory).filePath(
        QStringLiteral("document_viewer_capture.json")
        ));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || manifestFile.write(QJsonDocument(manifest).toJson(
               QJsonDocument::Indented
               )) < 0)
    {
        qCritical("Could not write document viewer capture manifest");
        return 1;
    }
    qInfo().noquote() << "Captured Next Document viewer evidence in"
                      << outputDirectory;
    return 0;
}
