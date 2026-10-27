#pragma once

#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

// Exposes only the currently active page's in-memory draft lifecycle. The
// backing workspace is managed separately by WorkspaceCoordinator.
class ActivePageChangesPort
{
public:
    virtual ~ActivePageChangesPort() = default;

    [[nodiscard]] virtual bool hasUnsavedChanges() const noexcept = 0;
    [[nodiscard]] virtual Domain::Result<void> saveChanges() = 0;
    [[nodiscard]] virtual Domain::Result<void> discardChanges() = 0;
};

} // namespace ClassMngr::Next::Application
