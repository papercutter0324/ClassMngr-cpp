#pragma once

#include "core/result.h"

#include <QByteArray>
#include <QString>

class UpdateSignatureVerifier
{
public:
    [[nodiscard]] static Status verifyDetachedSignature(
        const QByteArray& payload,
        const QByteArray& signature,
        const QString& publicKeyPem
        );

private:
    [[nodiscard]] static QByteArray normalizedSignature(
        const QByteArray& signature
        );
};
