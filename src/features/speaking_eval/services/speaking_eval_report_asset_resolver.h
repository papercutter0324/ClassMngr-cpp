#pragma once

#include <QByteArray>
#include <QString>

namespace SpeakingEvalReportAssetResolver
{
[[nodiscard]] bool copyResourceToFile(
    const QString& resourcePath,
    const QString& targetPath,
    QString* errorMessage
    );

[[nodiscard]] bool copyFileReplacing(
    const QString& sourcePath,
    const QString& targetPath,
    const QString& failureMessage,
    QString* errorMessage
    );

[[nodiscard]] bool prepareSignatureImage(
    const QByteArray& signatureImage,
    const QString& directory,
    QString* signaturePath,
    QString* errorMessage
    );
}
