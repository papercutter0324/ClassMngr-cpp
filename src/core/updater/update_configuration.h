#pragma once

#include <QUrl>

class UpdateConfiguration
{
public:
    [[nodiscard]] static UpdateConfiguration fromBuild();

    [[nodiscard]] bool hasReleasesApiUrl() const;

    QUrl releasesApiUrl;
    bool checkOnStartup = true;
};
