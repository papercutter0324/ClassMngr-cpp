#include "update_signature_verifier.h"

#include <QObject>
#include <QRegularExpression>

#if defined(Q_OS_WIN)
#include <QCryptographicHash>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#else
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#endif

namespace
{
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

#if defined(Q_OS_WIN)
class LocalPublicKeyInfo
{
public:
    ~LocalPublicKeyInfo()
    {
        if (value)
        {
            LocalFree(value);
        }
    }

    CERT_PUBLIC_KEY_INFO* value = nullptr;
};

class BCryptKey
{
public:
    ~BCryptKey()
    {
        if (value)
        {
            BCryptDestroyKey(value);
        }
    }

    BCRYPT_KEY_HANDLE value = nullptr;
};

QByteArray decodePemPublicKey(
    const QString& publicKeyPem
    )
{
    QByteArray encoded;

    for (QByteArray line : publicKeyPem.toUtf8().split('\n'))
    {
        line = line.trimmed();

        if (line.isEmpty() || line.startsWith("-----"))
        {
            continue;
        }

        encoded.append(line);
    }

    return QByteArray::fromBase64(encoded);
}

Status verifyWithWindowsCrypto(
    const QByteArray& payload,
    const QByteArray& signature,
    const QString& publicKeyPem
    )
{
    const QByteArray publicKeyDer =
        decodePemPublicKey(publicKeyPem);

    if (publicKeyDer.isEmpty())
    {
        return std::unexpected(
            QObject::tr("Update public key is not valid PEM data.")
            );
    }

    LocalPublicKeyInfo publicKeyInfo;
    DWORD publicKeyInfoSize = 0;

    if (
        !CryptDecodeObjectEx(
            X509_ASN_ENCODING,
            X509_PUBLIC_KEY_INFO,
            reinterpret_cast<const BYTE*>(publicKeyDer.constData()),
            static_cast<DWORD>(publicKeyDer.size()),
            CRYPT_DECODE_ALLOC_FLAG,
            nullptr,
            &publicKeyInfo.value,
            &publicKeyInfoSize
            )
        )
    {
        return std::unexpected(
            QObject::tr(
                "Update public key must use PEM SubjectPublicKeyInfo format."
                )
            );
    }

    BCryptKey publicKey;

    if (
        !CryptImportPublicKeyInfoEx2(
            X509_ASN_ENCODING,
            publicKeyInfo.value,
            0,
            nullptr,
            &publicKey.value
            )
        )
    {
        return std::unexpected(
            QObject::tr("Unable to import the update public key.")
            );
    }

    QByteArray digest =
        QCryptographicHash::hash(
            payload,
            QCryptographicHash::Sha256
            );
    QByteArray signatureBytes =
        signature;
    BCRYPT_PKCS1_PADDING_INFO paddingInfo = {
        BCRYPT_SHA256_ALGORITHM
    };

    const NTSTATUS verificationStatus =
        BCryptVerifySignature(
            publicKey.value,
            &paddingInfo,
            reinterpret_cast<PUCHAR>(digest.data()),
            static_cast<ULONG>(digest.size()),
            reinterpret_cast<PUCHAR>(signatureBytes.data()),
            static_cast<ULONG>(signatureBytes.size()),
            BCRYPT_PAD_PKCS1
            );

    if (verificationStatus < 0)
    {
        return std::unexpected(
            QObject::tr("Update manifest signature is invalid.")
            );
    }

    return {};
}
#else
Status writeFile(
    const QString& path,
    const QByteArray& data
    )
{
    QFile file(path);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return std::unexpected(
            QObject::tr("Unable to prepare signature verification file.")
            );
    }

    if (file.write(data) != data.size())
    {
        return std::unexpected(
            QObject::tr("Unable to write signature verification file.")
            );
    }

    return {};
}

Status verifyWithOpenSsl(
    const QByteArray& payload,
    const QByteArray& signature,
    const QString& publicKeyPem
    )
{
    QTemporaryDir directory;

    if (!directory.isValid())
    {
        return std::unexpected(
            QObject::tr(
                "Unable to create signature verification workspace."
                )
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

    if (auto status = writeFile(signaturePath, signature); !status)
    {
        return status;
    }

    if (auto status = writeFile(publicKeyPath, publicKeyPem.toUtf8()); !status)
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
            QObject::tr(
                "Unable to start OpenSSL for update signature verification."
                )
            );
    }

    if (!openssl.waitForFinished(15000))
    {
        openssl.kill();
        openssl.waitForFinished();

        return std::unexpected(
            QObject::tr("Update signature verification timed out.")
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
                ? QObject::tr("Update manifest signature is invalid.")
                : QObject::tr("Update manifest signature is invalid: %1")
                    .arg(details)
            );
    }

    return {};
}
#endif
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
            QObject::tr("Manifest payload is empty.")
            );
    }

    if (signature.trimmed().isEmpty())
    {
        return std::unexpected(
            QObject::tr("Manifest signature is empty.")
            );
    }

    if (publicKeyPem.trimmed().isEmpty())
    {
        return std::unexpected(
            QObject::tr("Update public key is not configured.")
            );
    }

    const QByteArray signatureBytes =
        normalizedSignature(signature);

#if defined(Q_OS_WIN)
    return verifyWithWindowsCrypto(
        payload,
        signatureBytes,
        publicKeyPem
        );
#else
    return verifyWithOpenSsl(
        payload,
        signatureBytes,
        publicKeyPem
        );
#endif
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
