#include "resource_pack_configuration.h"

#include "core/build_info.h"

ResourcePackConfiguration ResourcePackConfiguration::fromBuild()
{
    ResourcePackConfiguration configuration;
    configuration.manifestUrl =
        QUrl(
            QString::fromUtf8(
                BuildInfo::ResourcePackManifestUrl
                ).trimmed()
            );
    configuration.signatureUrl =
        QUrl(
            QString::fromUtf8(
                BuildInfo::ResourcePackSignatureUrl
                ).trimmed()
            );
    configuration.publicKeyPem =
        QString::fromUtf8(
            BuildInfo::ResourcePackPublicKeyPem
            ).trimmed();

    if (configuration.publicKeyPem.isEmpty())
    {
        configuration.publicKeyPem =
            QString::fromUtf8(
                BuildInfo::UpdatePublicKeyPem
                ).trimmed();
    }

    configuration.requireSignature =
        BuildInfo::ResourcePackRequireSignature;
    configuration.checkOnStartup =
        BuildInfo::ResourcePackCheckOnStartup;

    return configuration;
}

bool ResourcePackConfiguration::hasManifestUrl() const
{
    return manifestUrl.isValid()
        && !manifestUrl.isRelative()
        && !manifestUrl.isEmpty();
}
