#pragma once

#include "next/application/roster_save_use_case.h"
#include "next/application/roster_window_read_query.h"

#include <QString>

namespace ClassMngr::Next::Platform
{

[[nodiscard]] Application::RosterWindowReadResult readRosterWindowAtPath(
    const QString& databasePath,
    const Domain::ClassId& classId
    );

[[nodiscard]] Domain::Result<void> saveRosterAtPath(
    const QString& databasePath,
    const Application::RosterSaveRequest& request
    );

[[nodiscard]] Domain::Result<bool> rosterColumnHasTailDataAtPath(
    const QString& databasePath,
    const Domain::ClassId& classId,
    const std::u16string& columnName
    );

} // namespace ClassMngr::Next::Platform
