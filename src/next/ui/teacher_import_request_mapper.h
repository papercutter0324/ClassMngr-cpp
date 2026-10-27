#pragma once

#include "domain/models/teacher_import.h"
#include "next/application/teacher_import_use_case.h"

#include <expected>
#include <vector>

struct TeacherImportPreview;

namespace ClassMngr::Next::Ui
{

[[nodiscard]] std::expected<
    Application::TeacherImportUseCaseRequest,
    Application::TeacherImportReviewIssue
    > teacherImportPreviewToUseCaseRequest(
        const TeacherImportPreview& preview,
        const std::vector<Application::TeacherImportGroupDecision>& decisions
        );

}  // namespace ClassMngr::Next::Ui
