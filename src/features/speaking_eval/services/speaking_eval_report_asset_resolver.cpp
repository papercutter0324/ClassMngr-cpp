#include "speaking_eval_report_asset_resolver.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QObject>
#include <QSaveFile>

namespace SpeakingEvalReportAssetResolver
{
bool copyResourceToFile(
    const QString& resourcePath,
    const QString& targetPath,
    QString* errorMessage
    )
{
    QFile source(resourcePath);
    if (!source.open(QIODevice::ReadOnly))
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr(
                "The PowerPoint template could not be opened."
                );
        }
        return false;
    }

    QSaveFile target(targetPath);
    if (!target.open(QIODevice::WriteOnly)
        || target.write(source.readAll()) < 0
        || !target.commit())
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr(
                "A temporary PowerPoint template could not be created."
                );
        }
        return false;
    }

    return true;
}

bool copyFileReplacing(
    const QString& sourcePath,
    const QString& targetPath,
    const QString& failureMessage,
    QString* errorMessage
    )
{
    if ((QFileInfo::exists(targetPath) && !QFile::remove(targetPath))
        || !QFile::copy(sourcePath, targetPath))
    {
        if (errorMessage)
        {
            *errorMessage = failureMessage;
        }
        return false;
    }

    return true;
}

bool prepareSignatureImage(
    const QByteArray& signatureImage,
    const QString& directory,
    QString* signaturePath,
    QString* errorMessage
    )
{
    if (!signaturePath)
    {
        return false;
    }

    signaturePath->clear();
    if (signatureImage.isEmpty())
    {
        return true;
    }

    QImage signature;
    if (!signature.loadFromData(signatureImage) || signature.isNull())
    {
        return true;
    }

    const QString path = QDir(directory).filePath(
        QStringLiteral("signature.png")
        );
    if (!signature.save(path, "PNG"))
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr(
                "The signature image could not be prepared for the report."
                );
        }
        return false;
    }

    *signaturePath = path;
    return true;
}
}
