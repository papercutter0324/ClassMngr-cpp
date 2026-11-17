#pragma once

#include "next/resources/resource_diagnostics.h"

#include <QByteArray>
#include <QJsonObject>

namespace ClassMngr::Next::Bootstrap
{

[[nodiscard]] QJsonObject qtResourceDiagnosticObject(
    const Resources::ResourceDiagnostic& diagnostic
    );
[[nodiscard]] QByteArray qtResourceDiagnosticRecord(
    const Resources::ResourceDiagnostic& diagnostic
    );
[[nodiscard]] Resources::ResourceDiagnosticsSink
makeQtResourceDiagnosticsSink();

}  // namespace ClassMngr::Next::Bootstrap
