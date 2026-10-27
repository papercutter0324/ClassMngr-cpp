#pragma once

#include "next/application/file_dialog_port.h"
#include "next/application/workspace_contracts.h"

namespace ClassMngr::Next::Application
{

class WorkspaceOpenProfilePathPolicyPort
{
public:
    virtual ~WorkspaceOpenProfilePathPolicyPort() = default;

    [[nodiscard]] virtual Domain::Result<WorkspaceLocation>
    normalizeOpenProfilePath(const FileDialogPath& path) const = 0;
};

} // namespace ClassMngr::Next::Application
