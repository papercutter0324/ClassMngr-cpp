#pragma once

#include <QString>
#include <QUrl>

class UpdateConfiguration
{
public:
    [[nodiscard]] static UpdateConfiguration fromBuild();

    [[nodiscard]] bool hasManifestUrl() const;

    QUrl manifestUrl;
    QUrl signatureUrl;
    QString publicKeyPem;
    bool requireSignature = true;
    bool checkOnStartup = false;
};
