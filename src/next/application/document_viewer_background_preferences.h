#pragma once

#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

// The application boundary carries the typed document-viewer background.
// Persistence and legacy integer conversion belong to the platform adapter.
enum class DocumentViewerBackground
{
    Default = 0,
    White = 1,
    Black = 2
};

class DocumentViewerBackgroundPreferencesPort
{
public:
    virtual ~DocumentViewerBackgroundPreferencesPort() = default;

    [[nodiscard]] virtual DocumentViewerBackground read() const = 0;

    [[nodiscard]] virtual Domain::Result<void> write(
        DocumentViewerBackground background
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
