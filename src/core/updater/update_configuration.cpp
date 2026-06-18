#include "update_configuration.h"

#include "core/build_info.h"

UpdateConfiguration UpdateConfiguration::fromBuild()
{
    UpdateConfiguration configuration;
    configuration.manifestUrl =
        QUrl(
            QString::fromUtf8(BuildInfo::UpdateManifestUrl).trimmed()
            );
    configuration.signatureUrl =
        QUrl(
            QString::fromUtf8(BuildInfo::UpdateSignatureUrl).trimmed()
            );
    configuration.publicKeyPem =
        QString::fromUtf8(BuildInfo::UpdatePublicKeyPem).trimmed();
    configuration.requireSignature =
        BuildInfo::UpdateRequireSignature;
    configuration.checkOnStartup =
        BuildInfo::UpdateCheckOnStartup;

    return configuration;
}

bool UpdateConfiguration::hasManifestUrl() const
{
    return manifestUrl.isValid()
        && !manifestUrl.isRelative()
        && !manifestUrl.isEmpty();
}
