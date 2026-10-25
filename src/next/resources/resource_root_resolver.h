#pragma once

#include "resource_error.h"

#include <QString>

namespace ClassMngr::Next::Resources
{

struct ResourcePackagePaths final
{
    QString resourceRoot;
    QString catalogFilePath;
};

class ResourceRootResolver final
{
public:
    [[nodiscard]] static ResourceResult<ResourcePackagePaths> resolve();

    [[nodiscard]] static ResourceResult<ResourcePackagePaths> resolveFromApplicationDirectory(
        const QString& applicationDirectory
        );
};

}
