#pragma once

#include "core/result.h"

#include <QString>

class UpdateInstaller
{
public:
    [[nodiscard]] static Status launch(
        const QString& filePath
        );

    [[nodiscard]] static Status revealInFolder(
        const QString& filePath
        );
};
