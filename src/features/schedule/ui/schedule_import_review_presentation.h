#pragma once

#include "domain/models/schedule_import.h"
#include "features/schedule/ui/schedule_view_model.h"
#include "next/application/schedule_import_state_snapshot.h"

#include <QString>
#include <QStringList>

class QColor;
struct ScheduleDisplayState;

namespace ScheduleImportReviewPresentation
{
[[nodiscard]] QString compactMeetingText(
    const QList<ClassTime>& times
    );

[[nodiscard]] QString importedClassConflictLabel(
    const ScheduleImportClassCandidate& candidate
    );

[[nodiscard]] QStringList projectedScheduleConflicts(
    const ScheduleImportUserBlock& user
    );

// Data-only review variants consume the Application snapshot directly and do
// not issue additional legacy-service reads while the review is refreshing.
[[nodiscard]] QString classLabel(
    const ClassMngr::Next::Application::
        ScheduleImportStateReadClassSnapshot& classroom,
    const ClassMngr::Next::Application::
        ScheduleImportStateReadTeacherSnapshot* teacher,
    ScheduleImportKind kind
    );

[[nodiscard]] QString classDifferences(
    const ScheduleImportClassCandidate& candidate,
    const ClassMngr::Next::Application::
        ScheduleImportStateReadClassSnapshot* existing,
    const ClassMngr::Next::Application::
        ScheduleImportStateReadTeacherSnapshot* existingTeacher,
    int targetClassId,
    ScheduleImportKind kind,
    const QString& classColor,
    const QColor& changesColor,
    const QColor& changesHeadingColor
    );

[[nodiscard]] QString teacherLabel(
    const ClassMngr::Next::Application::
        ScheduleImportStateReadTeacherSnapshot& teacher
    );

[[nodiscard]] bool importedClassLess(
    const ScheduleImportClassCandidate& left,
    const ScheduleImportClassCandidate& right
    );

[[nodiscard]] ScheduleViewModel previewModel(
    const ScheduleImportUserBlock& user,
    bool useIntensive,
    const ScheduleDisplayState& displayState
    );
}
