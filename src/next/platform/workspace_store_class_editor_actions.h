#pragma once

#include "next/application/class_co_teacher_assignment_use_case.h"
#include "next/application/class_co_teacher_page_read_port.h"
#include "next/application/class_co_teacher_teacher_choices_read_port.h"
#include "next/application/class_create.h"
#include "next/application/class_delete.h"
#include "next/application/class_details_page_read_port.h"
#include "next/application/class_details_save_workflow.h"

#include <QString>

namespace ClassMngr::Next::Platform
{

[[nodiscard]] Application::ClassDetailsPageReadResult
readClassDetailsAtPath(
    const QString& databasePath,
    const Domain::ClassId& classId
    );

[[nodiscard]] Application::ClassCoTeacherPageReadResult
readClassCoTeacherAtPath(
    const QString& databasePath,
    const Domain::ClassId& classId
    );

[[nodiscard]] Application::ClassCoTeacherTeacherChoicesResult
readClassCoTeacherChoicesAtPath(const QString& databasePath);

[[nodiscard]] Domain::Result<Application::ClassCoTeacherTeacherChoice>
readClassCoTeacherTeacherDetailsAtPath(
    const QString& databasePath,
    const Domain::TeacherId& teacherId
    );

[[nodiscard]] Application::ClassDetailsSaveWorkflowResult
saveClassDetailsAtPath(
    const QString& databasePath,
    const Application::ClassDetailsSaveWorkflowRequest& request
    );

[[nodiscard]] Domain::Result<void> assignClassCoTeacherAtPath(
    const QString& databasePath,
    const Application::ClassCoTeacherAssignmentRequest& request
    );

[[nodiscard]] Application::ClassCreateResult
createClassAtPath(const QString& databasePath);

[[nodiscard]] Application::ClassDeleteResult deleteClassAtPath(
    const QString& databasePath,
    const Application::ClassDeleteRequest& request
    );

} // namespace ClassMngr::Next::Platform
