#include "pdf_viewer_page_p.h"

QString PdfViewerPage::exportSourcePath() const
{
    if (m_currentFilePath.trimmed().isEmpty())
    {
        return QString();
    }

    const QFileInfo pdfInfo(m_currentFilePath);
    const QDir directory(
        pdfInfo.absolutePath()
        );

    if (!directory.exists())
    {
        return m_currentFilePath;
    }

    QList<QFileInfo> alternatives;

    const QFileInfoList files =
        directory.entryInfoList(
            QDir::Files | QDir::NoDotAndDotDot,
            QDir::Name
            );

    for (const QFileInfo& fileInfo : files)
    {
        if (
            fileInfo.completeBaseName().compare(
                pdfInfo.completeBaseName(),
                Qt::CaseInsensitive
                ) != 0
            )
        {
            continue;
        }

        if (
            fileInfo.suffix().compare(
                QStringLiteral("pdf"),
                Qt::CaseInsensitive
                ) == 0
            )
        {
            continue;
        }

        alternatives.append(fileInfo);
    }

    if (alternatives.isEmpty())
    {
        return m_currentFilePath;
    }

    std::sort(
        alternatives.begin(),
        alternatives.end(),
        [](const QFileInfo& left, const QFileInfo& right)
        {
            const int leftRank =
                exportSuffixRank(
                    left.suffix()
                    );
            const int rightRank =
                exportSuffixRank(
                    right.suffix()
                    );

            if (leftRank != rightRank)
            {
                return leftRank < rightRank;
            }

            return left.fileName().localeAwareCompare(
                right.fileName()
                ) < 0;
        }
        );

    return alternatives.first().absoluteFilePath();
}

bool PdfViewerPage::copyFileTo(
    const QString& sourcePath,
    const QString& targetPath,
    QString* errorMessage
    ) const
{
    QFile sourceFile(sourcePath);

    if (!sourceFile.open(QIODevice::ReadOnly))
    {
        if (errorMessage)
        {
            *errorMessage =
                tr("Unable to read the source file:\n%1")
                    .arg(sourcePath);
        }

        return false;
    }

    const QFileInfo targetInfo(targetPath);

    if (
        !targetInfo.absolutePath().isEmpty()
        && !QDir().mkpath(targetInfo.absolutePath())
        )
    {
        if (errorMessage)
        {
            *errorMessage =
                tr("Unable to create the destination folder:\n%1")
                    .arg(targetInfo.absolutePath());
        }

        return false;
    }

    QSaveFile targetFile(targetPath);

    if (!targetFile.open(QIODevice::WriteOnly))
    {
        if (errorMessage)
        {
            *errorMessage =
                tr("Unable to write the destination file:\n%1")
                    .arg(targetPath);
        }

        return false;
    }

    while (!sourceFile.atEnd())
    {
        const QByteArray chunk =
            sourceFile.read(CopyBufferSize);

        if (
            chunk.isEmpty()
            && sourceFile.error() != QFile::NoError
            )
        {
            targetFile.cancelWriting();

            if (errorMessage)
            {
                *errorMessage =
                    tr("Unable to read the source file:\n%1")
                        .arg(sourcePath);
            }

            return false;
        }

        if (targetFile.write(chunk) != chunk.size())
        {
            targetFile.cancelWriting();

            if (errorMessage)
            {
                *errorMessage =
                    tr("Unable to write the destination file:\n%1")
                        .arg(targetPath);
            }

            return false;
        }
    }

    if (!targetFile.commit())
    {
        if (errorMessage)
        {
            *errorMessage =
                tr("Unable to save the file:\n%1")
                    .arg(targetPath);
        }

        return false;
    }

    return true;
}

