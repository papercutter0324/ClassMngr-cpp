#pragma once

#include "domain/models/teacher_import.h"

#include <QString>

#include <expected>

namespace ClassMngr::Domain
{

[[nodiscard]] std::expected<TeacherImportPlan, QString>
prepareTeacherImportPlan(const TeacherImportPlan& plan);

} // namespace ClassMngr::Domain
