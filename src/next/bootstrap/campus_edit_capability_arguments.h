#pragma once

#include "core/appsettings.h"
#include "next/application/campus_edit_capability.h"

#include <QString>
#include <QStringList>

namespace ClassMngr::Next::Bootstrap
{

// Keep the presence-based legacy command-line contract at the bootstrap
// boundary and share the same mapping with focused composition tests.
[[nodiscard]] inline Application::CampusEditCapability
campusEditCapabilityForArguments(const QStringList& arguments)
{
    const QString adminFlag = QString::fromLatin1(
        AppSettings::AdminModeArgument
        );
    return arguments.contains(adminFlag)
        ? Application::CampusEditCapability::enabled()
        : Application::CampusEditCapability::readOnly();
}

} // namespace ClassMngr::Next::Bootstrap
