#pragma once

#include "next/application/workspace_creation_lifecycle.h"

#include <QString>

namespace ClassMngr::App::Controllers
{

[[nodiscard]] QString newTeacherProfileCreationFailureMessage(
    const ClassMngr::Next::Application::WorkspaceCreationLifecycleResult&
        result,
    const QString& selectedPath
    );

} // namespace ClassMngr::App::Controllers
