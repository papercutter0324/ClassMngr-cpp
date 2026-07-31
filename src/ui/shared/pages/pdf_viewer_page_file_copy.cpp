#include "pdf_viewer_page_p.h"

QString PdfViewerPage::exportSourcePath() const
{
    if (
        !m_documentDescriptor.exportEnabled
        || m_documentDescriptor.exportFilePath.trimmed().isEmpty()
        )
    {
        return QString();
    }

    return m_documentDescriptor.exportFilePath;
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

