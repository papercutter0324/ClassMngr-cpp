#pragma once

#include "next/application/teacher_import_use_case.h"

#include <QSqlDatabase>

#include <memory>

[[nodiscard]] std::unique_ptr<
    ClassMngr::Next::Application::TeacherImportPersistencePort>
makeTeacherImportSqlPersistenceAdapter(QSqlDatabase& database);
