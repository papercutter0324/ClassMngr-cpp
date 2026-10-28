#pragma once

#include "next/application/schedule_testing_class_choices_query.h"
#include "next/application/testing_class_create.h"
#include "next/application/testing_class_delete.h"
#include "next/application/testing_class_details_read_query.h"
#include "next/application/testing_class_details_update.h"
#include "next/application/testing_teacher_choices_read_query.h"

#include <QString>

namespace ClassMngr::Next::Platform
{

[[nodiscard]] Application::ScheduleTestingClassChoicesReadResult
readTestingClassesAtPath(const QString& databasePath);

[[nodiscard]] Application::TestingClassDetailsReadResult
readTestingClassDetailsAtPath(
    const QString& databasePath,
    const Domain::ClassId& classId
    );

[[nodiscard]] Application::TestingTeacherChoicesReadResult
readTestingTeacherChoicesAtPath(const QString& databasePath);

[[nodiscard]] Application::TestingClassDetailsUpdateResult
updateTestingClassDetailsAtPath(
    const QString& databasePath,
    const Application::TestingClassDetailsUpdateRequest& request
    );

[[nodiscard]] Application::TestingClassCreateResult
createTestingClassAtPath(
    const QString& databasePath,
    const Application::TestingClassCreateRequest& request
    );

[[nodiscard]] Application::TestingClassDeleteResult
deleteTestingClassAtPath(
    const QString& databasePath,
    const Application::TestingClassDeleteRequest& request
    );

} // namespace ClassMngr::Next::Platform
