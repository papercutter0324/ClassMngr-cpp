#include "ui/shared/pages/pdf_viewer_page.h"

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
#include <QLabel>
#include <QPdfView>
#include <QPixmap>
#include <QThread>
#include <QTranslator>

#include <memory>
#include <utility>

namespace
{

constexpr QSize Viewport{1024, 720};
const QString kDocumentId = QStringLiteral("document_guides_lesson_planning");

QString readText(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

bool waitUntilReady(PdfViewerPage& viewer)
{
    QElapsedTimer timer;
    timer.start();
    while (!viewer.hasLoadedDocument() && timer.elapsed() < 10000)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
        QThread::msleep(10);
    }
    QCoreApplication::processEvents();
    return viewer.hasLoadedDocument();
}

void settlePdfRendering(PdfViewerPage& viewer)
{
    if (QPdfView* const view = viewer.findChild<QPdfView*>(
            QStringLiteral("pdfViewerView")
            ))
    {
        view->viewport()->update();
        QCoreApplication::processEvents();
        QThread::msleep(500);
        QCoreApplication::processEvents();
        view->viewport()->repaint();
        QCoreApplication::processEvents();
    }
}

QString errorText(const PdfViewerPage& viewer)
{
    for (const QLabel* const label : viewer.findChildren<QLabel*>())
    {
        if (label->text().contains(QStringLiteral("Failed to load PDF")))
        {
            return label->text();
        }
    }
    return {};
}

bool saveStage(
    PdfViewerPage& viewer,
    const QString& outputDirectory,
    const QString& locale,
    const QString& theme,
    const QString& stage,
    QJsonArray* captures,
    QString* error
    )
{
    const QString fileName = QStringLiteral("legacy_document_viewer_%1_%2_%3.png")
                                 .arg(locale, theme, stage);
    const QPixmap pixmap = viewer.grab();
    if (pixmap.size() != Viewport
        || !pixmap.save(QDir(outputDirectory).filePath(fileName), "PNG"))
    {
        if (error)
        {
            *error = QStringLiteral("Could not save Legacy %1 capture.").arg(stage);
        }
        return false;
    }
    captures->append(QJsonObject{
        {QStringLiteral("implementation"), QStringLiteral("legacy-PdfViewerPage")},
        {QStringLiteral("locale"), locale},
        {QStringLiteral("theme"), theme},
        {QStringLiteral("state"), stage},
        {QStringLiteral("file"), fileName},
        {QStringLiteral("width"), pixmap.width()},
        {QStringLiteral("height"), pixmap.height()},
        {QStringLiteral("selectedDocumentId"),
         stage == QStringLiteral("error") ? QStringLiteral("missing-c43-viewer-selection")
                                           : kDocumentId},
        {QStringLiteral("errorMessage"), errorText(viewer)}
    });
    return true;
}

bool captureCombination(
    QApplication& application,
    const QString& locale,
    const QString& theme,
    const QString& outputDirectory,
    QJsonArray* captures,
    QString* error
    )
{
    const QString resourceRoot = QString::fromUtf8(
        CLASSMNGR_NEXT_TEST_RESOURCE_ROOT
        );
    const QString stylesheet = readText(QDir(resourceRoot).filePath(
        QStringLiteral("assets/styles/%1.qss").arg(theme)
        ));
    if (stylesheet.isEmpty())
    {
        if (error)
        {
            *error = QStringLiteral("Could not read packaged %1 theme.").arg(theme);
        }
        return false;
    }
    application.setStyleSheet(stylesheet);

    const QString pdfPath = QDir(resourceRoot).filePath(
        QStringLiteral("assets/documents/Guides/DYB Lesson Planning Guide.pdf")
        );
    PdfViewerPage viewer;
    viewer.resize(Viewport);
    viewer.show();
    QCoreApplication::processEvents();

    PdfViewerDocumentDescriptor descriptor;
    descriptor.pdfFilePath = pdfPath;
    descriptor.exportEnabled = true;
    descriptor.exportFilePath = pdfPath;
    descriptor.exportFileName = QStringLiteral("DYB Lesson Planning Guide.pdf");
    descriptor.printEnabled = true;
    if (!viewer.loadPdf(std::move(descriptor)) || !waitUntilReady(viewer))
    {
        if (error)
        {
            *error = QStringLiteral("Legacy viewer did not reach Ready for the packaged PDF.");
        }
        return false;
    }
    settlePdfRendering(viewer);
    if (!saveStage(viewer, outputDirectory, locale, theme,
                   QStringLiteral("ready"), captures, error))
    {
        return false;
    }

    PdfViewerDocumentDescriptor missing;
    missing.pdfFilePath = QDir(outputDirectory).filePath(
        QStringLiteral("missing-c43-viewer-selection.pdf")
        );
    if (viewer.loadPdf(std::move(missing)))
    {
        if (error)
        {
            *error = QStringLiteral("Legacy viewer unexpectedly loaded a missing PDF.");
        }
        return false;
    }
    QCoreApplication::processEvents();
    if (!saveStage(viewer, outputDirectory, locale, theme,
                   QStringLiteral("error"), captures, error))
    {
        return false;
    }

    viewer.releaseDocument();
    QCoreApplication::processEvents();
    if (!saveStage(viewer, outputDirectory, locale, theme,
                   QStringLiteral("closed"), captures, error))
    {
        return false;
    }

    PdfViewerDocumentDescriptor reopened;
    reopened.pdfFilePath = pdfPath;
    reopened.exportEnabled = true;
    reopened.exportFilePath = pdfPath;
    reopened.exportFileName = QStringLiteral("DYB Lesson Planning Guide.pdf");
    reopened.printEnabled = true;
    if (!viewer.loadPdf(std::move(reopened)) || !waitUntilReady(viewer))
    {
        if (error)
        {
            *error = QStringLiteral("Legacy viewer did not reopen the packaged PDF.");
        }
        return false;
    }
    settlePdfRendering(viewer);
    if (!saveStage(viewer, outputDirectory, locale, theme,
                   QStringLiteral("reopened"), captures, error))
    {
        return false;
    }
    viewer.releaseDocument();
    return true;
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    QApplication::setApplicationName(QStringLiteral("ClassMngr Legacy viewer capture"));
    if (argc != 2)
    {
        qCritical("Usage: ClassMngrNextLegacyDocumentViewerVisualCapture OUTPUT_DIR");
        return 2;
    }
    const QString outputDirectory = QString::fromLocal8Bit(argv[1]);
    if (!QDir().mkpath(outputDirectory))
    {
        qCritical("Could not create Legacy viewer evidence directory");
        return 2;
    }
    const QString resourceRoot = QString::fromUtf8(
        CLASSMNGR_NEXT_TEST_RESOURCE_ROOT
        );
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
    for (const QString& locale : {
             QStringLiteral("en_US"),
             QStringLiteral("ko_KR")
         })
    {
        std::unique_ptr<QTranslator> translator;
        if (locale == QStringLiteral("ko_KR"))
        {
            translator = std::make_unique<QTranslator>();
            const QString path = QDir(QString::fromUtf8(
                CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY
                )).filePath(QStringLiteral("ClassMngr_ko_KR.qm"));
            if (!translator->load(path)
                || !application.installTranslator(translator.get()))
            {
                qCritical().noquote() << "Could not load Korean translation:" << path;
                return 1;
            }
        }
        for (const QString& theme : {
                 QStringLiteral("light"),
                 QStringLiteral("dark")
             })
        {
            QString error;
            if (!captureCombination(
                    application,
                    locale,
                    theme,
                    outputDirectory,
                    &captures,
                    &error
                    ))
            {
                qCritical().noquote() << "Legacy viewer capture failed:" << error;
                return 1;
            }
        }
        if (translator)
        {
            application.removeTranslator(translator.get());
        }
    }
    if (captures.size() != 16)
    {
        qCritical() << "Expected 16 Legacy viewer captures, observed" << captures.size();
        return 1;
    }
    const QJsonObject manifest{
        {QStringLiteral("taskId"), QStringLiteral("phase7_c43_document_viewer_implementation_01")},
        {QStringLiteral("implementation"), QStringLiteral("Legacy PdfViewerPage")},
        {QStringLiteral("qtVersion"), QStringLiteral(QT_VERSION_STR)},
        {QStringLiteral("qtPlatformPlugin"), QStringLiteral("offscreen")},
        {QStringLiteral("style"), QStringLiteral("Fusion with packaged light/dark stylesheet")},
        {QStringLiteral("viewport"), QJsonObject{
             {QStringLiteral("width"), Viewport.width()},
             {QStringLiteral("height"), Viewport.height()}}},
        {QStringLiteral("catalogSelectionId"), kDocumentId},
        {QStringLiteral("errorState"), QStringLiteral("Missing file path, matching the Next missing-selection error state by outcome; Legacy is given a missing path through its PDF page API.")},
        {QStringLiteral("captures"), captures},
        {QStringLiteral("comparisonLimits"), QJsonArray{
             QStringLiteral("This harness shows the production Legacy PdfViewerPage directly at 1024x720, not the full Legacy MainWindow. Next captures show the Next shell plus its viewer page."),
             QStringLiteral("Legacy print/export controls live in application actions and no in-page Close button exists; Next includes Print, Export, and Close in its viewer action row."),
             QStringLiteral("The paired captures document structure and state only; they do not claim pixel parity.")
        }}
    };
    QFile manifestFile(QDir(outputDirectory).filePath(
        QStringLiteral("legacy_document_viewer_capture.json")
        ));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || manifestFile.write(QJsonDocument(manifest).toJson(
               QJsonDocument::Indented
               )) < 0)
    {
        qCritical("Could not write Legacy viewer capture manifest");
        return 1;
    }
    return 0;
}
