#include "update_signature_verifier.h"

#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>

namespace
{
Status writeFile(
    const QString& path,
    const QByteArray& data
    )
{
    QFile file(path);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return std::unexpected(
            QStringLiteral("Unable to prepare signature verification file.")
            );
    }

    if (file.write(data) != data.size())
    {
        return std::unexpected(
            QStringLiteral("Unable to write signature verification file.")
            );
    }

    return {};
}

bool looksBase64Encoded(
    const QByteArray& data
    )
{
    const QByteArray trimmed =
        data.trimmed();

    if (trimmed.isEmpty())
    {
        return false;
    }

    static const QRegularExpression pattern(
        QStringLiteral(R"(^[A-Za-z0-9+/=\r\n\t ]+$)")
        );

    return pattern.match(
        QString::fromLatin1(trimmed)
        ).hasMatch();
}
}

Status UpdateSignatureVerifier::verifyDetachedSignature(
    const QByteArray& payload,
    const QByteArray& signature,
    const QString& publicKeyPem
    )
{
    if (payload.isEmpty())
    {
        return std::unexpected(
            QStringLiteral("Manifest payload is empty.")
            );
    }

    if (signature.trimmed().isEmpty())
    {
        return std::unexpected(
            QStringLiteral("Manifest signature is empty.")
            );
    }

    if (publicKeyPem.trimmed().isEmpty())
    {
        return std::unexpected(
            QStringLiteral("Update public key is not configured.")
            );
    }

    QTemporaryDir directory;

    if (!directory.isValid())
    {
        return std::unexpected(
            QStringLiteral("Unable to create signature verification workspace.")
            );
    }

    const QString payloadPath =
        directory.filePath(
            QStringLiteral("latest.json")
            );
    const QString signaturePath =
        directory.filePath(
            QStringLiteral("latest.sig")
            );
    const QString publicKeyPath =
        directory.filePath(
            QStringLiteral("public.pem")
            );

    if (auto status = writeFile(payloadPath, payload); !status)
    {
        return status;
    }

    if (
        auto status =
            writeFile(
                signaturePath,
                normalizedSignature(signature)
                );
        !status
        )
    {
        return status;
    }

    if (
        auto status =
            writeFile(
                publicKeyPath,
                publicKeyPem.toUtf8()
                );
        !status
        )
    {
        return status;
    }

    QProcess openssl;
    openssl.start(
        QStringLiteral("openssl"),
        {
            QStringLiteral("dgst"),
            QStringLiteral("-sha256"),
            QStringLiteral("-verify"),
            publicKeyPath,
            QStringLiteral("-signature"),
            signaturePath,
            payloadPath
        }
        );

    if (!openssl.waitForStarted())
    {
        return std::unexpected(
            QStringLiteral("Unable to start OpenSSL for update signature verification.")
            );
    }

    if (!openssl.waitForFinished(15000))
    {
        openssl.kill();
        openssl.waitForFinished();

        return std::unexpected(
            QStringLiteral("Update signature verification timed out.")
            );
    }

    if (openssl.exitStatus() != QProcess::NormalExit || openssl.exitCode() != 0)
    {
        const QString details =
            QString::fromUtf8(
                openssl.readAllStandardError()
                ).trimmed();

        return std::unexpected(
            details.isEmpty()
                ? QStringLiteral("Update manifest signature is invalid.")
                : QStringLiteral("Update manifest signature is invalid: %1")
                    .arg(details)
            );
    }

    return {};
}

QByteArray UpdateSignatureVerifier::normalizedSignature(
    const QByteArray& signature
    )
{
    if (!looksBase64Encoded(signature))
    {
        return signature;
    }

    const QByteArray decoded =
        QByteArray::fromBase64(
            signature
            );

    return decoded.isEmpty()
        ? signature
        : decoded;
}
