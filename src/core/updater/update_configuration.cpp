#include "update_configuration.h"

#include "core/build_info.h"

UpdateConfiguration UpdateConfiguration::fromBuild()
{
    UpdateConfiguration configuration;
    configuration.releasesApiUrl =
        QUrl(
            QString::fromUtf8(BuildInfo::UpdateApiUrl).trimmed()
            );
    configuration.checkOnStartup =
        BuildInfo::UpdateCheckOnStartup;

    return configuration;
}

bool UpdateConfiguration::hasReleasesApiUrl() const
{
    return releasesApiUrl.isValid()
        && !releasesApiUrl.isRelative()
        && !releasesApiUrl.isEmpty();
}
