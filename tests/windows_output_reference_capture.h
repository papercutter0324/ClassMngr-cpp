#pragma once

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPdfDocument>

#include <algorithm>
#include <cmath>

namespace WindowsOutputReferenceCapture
{
inline constexpr char OutputRootEnvironmentVariable[] =
    "CLASSMNGR_WINDOWS_OUTPUT_REFERENCE_DIR";

struct OutputDirectory
{
    bool enabled = false;
    QString path;
    QString error;
};

struct PdfCapture
{
    QString pdfFileName;
    qint64 pdfBytes = 0;
    int pageCount = 0;
    QJsonArray renderedPages;
};

inline bool prepareOutputDirectory(
    const char* environmentVariable,
    const QString& directoryName,
    OutputDirectory* output
    )
{
    if (!output)
    {
        return false;
    }

    *output = {};
    if (!qEnvironmentVariableIsSet(environmentVariable))
    {
        return true;
    }

    output->enabled = true;
    const QString configuredRoot =
        qEnvironmentVariable(environmentVariable).trimmed();
    if (configuredRoot.isEmpty())
    {
        output->error =
            QStringLiteral("%1 is set but contains no output path.")
                .arg(QString::fromLatin1(environmentVariable));
        return false;
    }
    if (!QDir::isAbsolutePath(configuredRoot))
    {
        output->error =
            QStringLiteral("%1 must be an absolute directory path: %2")
                .arg(
                    QString::fromLatin1(environmentVariable),
                    configuredRoot
                    );
        return false;
    }
    if (directoryName.isEmpty()
        || directoryName == QStringLiteral(".")
        || directoryName == QStringLiteral("..")
        || directoryName.contains(QLatin1Char('/'))
        || directoryName.contains(QLatin1Char('\\')))
    {
        output->error = QStringLiteral("Invalid output directory name.");
        return false;
    }

    const QString absoluteRoot =
        QDir::cleanPath(QFileInfo(configuredRoot).absoluteFilePath());
    if (QDir(absoluteRoot).isRoot())
    {
        output->error =
            QStringLiteral("The filesystem root cannot hold test evidence.");
        return false;
    }

    const QFileInfo rootInfo(absoluteRoot);
    if (rootInfo.isSymLink()
        || (rootInfo.exists() && !rootInfo.isDir()))
    {
        output->error =
            QStringLiteral("The configured output root is not a plain directory: %1")
                .arg(absoluteRoot);
        return false;
    }
    if (!QDir().mkpath(absoluteRoot))
    {
        output->error =
            QStringLiteral("Unable to create test evidence root: %1")
                .arg(absoluteRoot);
        return false;
    }

    output->path =
        QDir(absoluteRoot).filePath(directoryName);
    const QFileInfo outputInfo(output->path);
    if (outputInfo.isSymLink()
        || (outputInfo.exists() && !outputInfo.isDir()))
    {
        output->error =
            QStringLiteral("The evidence destination is not a plain directory: %1")
                .arg(output->path);
        return false;
    }
    if (outputInfo.exists())
    {
        const QStringList existingEntries =
            QDir(output->path).entryList(
                QDir::AllEntries
                    | QDir::Hidden
                    | QDir::System
                    | QDir::NoDotAndDotDot
                );
        if (!existingEntries.isEmpty())
        {
            output->error =
                QStringLiteral(
                    "The evidence destination is non-empty; refusing to overwrite it: %1"
                    )
                    .arg(output->path);
            return false;
        }
    }
    else if (!QDir().mkpath(output->path))
    {
        output->error =
            QStringLiteral("Unable to create test evidence directory: %1")
                .arg(output->path);
        return false;
    }

    return true;
}

inline bool writeNewFile(
    const QString& path,
    const QByteArray& contents,
    QString* errorMessage
    )
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly))
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("Unable to create %1: %2")
                    .arg(path, file.errorString());
        }
        return false;
    }

    if (file.write(contents) != contents.size() || !file.flush()
        || file.error() != QFile::NoError)
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("Unable to finish writing %1: %2")
                    .arg(path, file.errorString());
        }
        return false;
    }
    return true;
}

inline bool capturePdf(
    const QString& sourcePdfPath,
    const QString& outputDirectory,
    const QString& fileStem,
    QPdfDocument& document,
    PdfCapture* capture,
    QString* errorMessage,
    int maximumRenderedPages = -1
    )
{
    if (!capture)
    {
        return false;
    }
    *capture = {};

    if (QFileInfo(fileStem).fileName() != fileStem
        || fileStem.isEmpty()
        || fileStem == QStringLiteral(".")
        || fileStem == QStringLiteral("..")
        || fileStem.contains(QLatin1Char('\\')))
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Invalid PDF evidence filename stem.");
        }
        return false;
    }

    const QFileInfo sourceInfo(sourcePdfPath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()
        || sourceInfo.size() <= 0
        || document.status() != QPdfDocument::Status::Ready
        || document.pageCount() <= 0)
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("The source PDF is not a readable loaded document: %1")
                    .arg(sourcePdfPath);
        }
        return false;
    }

    const int pagesToRender =
        maximumRenderedPages < 0
            ? document.pageCount()
            : maximumRenderedPages;
    if (pagesToRender <= 0 || pagesToRender > document.pageCount())
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("Invalid rendered page count for %1.")
                    .arg(sourcePdfPath);
        }
        return false;
    }

    capture->pdfFileName = fileStem + QStringLiteral(".pdf");
    capture->pdfBytes = sourceInfo.size();
    capture->pageCount = document.pageCount();

    const QString pdfOutputPath =
        QDir(outputDirectory).filePath(capture->pdfFileName);
    if (QFileInfo::exists(pdfOutputPath))
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("Refusing to replace existing evidence: %1")
                    .arg(pdfOutputPath);
        }
        return false;
    }
    if (!QFile::copy(sourcePdfPath, pdfOutputPath))
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("Unable to retain PDF %1 as %2")
                    .arg(sourcePdfPath, pdfOutputPath);
        }
        return false;
    }

    QPdfDocument retainedPdf;
    if (retainedPdf.load(pdfOutputPath) != QPdfDocument::Error::None
        || retainedPdf.status() != QPdfDocument::Status::Ready
        || retainedPdf.pageCount() != document.pageCount())
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("The retained PDF could not be reopened: %1")
                    .arg(pdfOutputPath);
        }
        return false;
    }
    retainedPdf.close();

    for (int pageIndex = 0; pageIndex < pagesToRender; ++pageIndex)
    {
        const QSizeF pagePointSize =
            document.pagePointSize(pageIndex);
        if (pagePointSize.width() <= 0.0
            || pagePointSize.height() <= 0.0
            || !std::isfinite(pagePointSize.width())
            || !std::isfinite(pagePointSize.height()))
        {
            if (errorMessage)
            {
                *errorMessage =
                    QStringLiteral("PDF page %1 has invalid dimensions: %2")
                        .arg(pageIndex + 1)
                        .arg(sourcePdfPath);
            }
            return false;
        }

        constexpr qreal PixelsPerPoint = 2.0;
        const QSize requestedSize(
            std::max(1, qRound(pagePointSize.width() * PixelsPerPoint)),
            std::max(1, qRound(pagePointSize.height() * PixelsPerPoint))
            );
        const QImage image =
            document.render(pageIndex, requestedSize);
        if (image.isNull())
        {
            if (errorMessage)
            {
                *errorMessage =
                    QStringLiteral("Unable to render PDF page %1: %2")
                        .arg(pageIndex + 1)
                        .arg(sourcePdfPath);
            }
            return false;
        }

        QImage opaqueImage(image.size(), QImage::Format_RGB32);
        if (opaqueImage.isNull())
        {
            if (errorMessage)
            {
                *errorMessage =
                    QStringLiteral("Unable to allocate the PDF page image: %1")
                        .arg(sourcePdfPath);
            }
            return false;
        }
        opaqueImage.fill(Qt::white);
        QPainter painter(&opaqueImage);
        if (!painter.isActive())
        {
            if (errorMessage)
            {
                *errorMessage =
                    QStringLiteral("Unable to composite the PDF page image: %1")
                        .arg(sourcePdfPath);
            }
            return false;
        }
        painter.drawImage(QPoint(0, 0), image);
        painter.end();

        const QString imageFileName =
            QStringLiteral("%1-page-%2.png")
                .arg(fileStem)
                .arg(pageIndex + 1, 2, 10, QLatin1Char('0'));
        QByteArray imageBytes;
        QBuffer imageBuffer(&imageBytes);
        if (!imageBuffer.open(QIODevice::WriteOnly)
            || !opaqueImage.save(&imageBuffer, "PNG"))
        {
            if (errorMessage)
            {
                *errorMessage =
                    QStringLiteral("Unable to encode rendered PDF page %1 as PNG.")
                        .arg(pageIndex + 1);
            }
            return false;
        }

        const QString imagePath =
            QDir(outputDirectory).filePath(imageFileName);
        if (!writeNewFile(imagePath, imageBytes, errorMessage))
        {
            return false;
        }
        const QImage retainedImage(imagePath);
        if (retainedImage.isNull()
            || retainedImage.size() != image.size()
            || retainedImage.hasAlphaChannel())
        {
            if (errorMessage)
            {
                *errorMessage =
                    QStringLiteral(
                        "The retained PNG is invalid or has an alpha channel: %1"
                        )
                        .arg(imagePath);
            }
            return false;
        }

        capture->renderedPages.append(
            QJsonObject{
                {QStringLiteral("page"), pageIndex + 1},
                {QStringLiteral("png"), imageFileName},
                {QStringLiteral("pngBytes"), imageBytes.size()},
                {QStringLiteral("pixelWidth"), image.width()},
                {QStringLiteral("pixelHeight"), image.height()},
                {QStringLiteral("pageWidthPoints"), pagePointSize.width()},
                {QStringLiteral("pageHeightPoints"), pagePointSize.height()}
            }
            );
    }

    return true;
}

inline QJsonObject pdfManifestEntry(
    const PdfCapture& capture,
    const QString& statusAfterClose
    )
{
    return {
        {QStringLiteral("pdf"), capture.pdfFileName},
        {QStringLiteral("pdfBytes"), capture.pdfBytes},
        {QStringLiteral("pageCount"), capture.pageCount},
        {QStringLiteral("renderedPages"), capture.renderedPages},
        {QStringLiteral("statusAfterClose"), statusAfterClose}
    };
}

inline bool writeManifest(
    const QString& outputDirectory,
    const QJsonObject& manifest,
    QString* errorMessage
)
{
    const QString manifestPath =
        QDir(outputDirectory).filePath(QStringLiteral("manifest.json"));
    const QByteArray manifestBytes =
        QJsonDocument(manifest).toJson(QJsonDocument::Indented);
    if (!writeNewFile(manifestPath, manifestBytes, errorMessage))
    {
        return false;
    }

    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("Unable to read back %1: %2")
                    .arg(manifestPath, manifestFile.errorString());
        }
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument retainedManifest =
        QJsonDocument::fromJson(manifestFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError
        || !retainedManifest.isObject()
        || retainedManifest.object() != manifest)
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("The retained manifest is invalid: %1")
                    .arg(manifestPath);
        }
        return false;
    }
    return true;
}
}
