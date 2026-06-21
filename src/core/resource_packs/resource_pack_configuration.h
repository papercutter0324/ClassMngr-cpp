#pragma once

#include <QString>
#include <QUrl>

class ResourcePackConfiguration
{
public:
    [[nodiscard]] static ResourcePackConfiguration fromBuild();

    [[nodiscard]] bool hasManifestUrl() const;

    QUrl manifestUrl;
    QUrl signatureUrl;
    QString publicKeyPem;
    bool requireSignature = true;
    bool checkOnStartup = true;
};
