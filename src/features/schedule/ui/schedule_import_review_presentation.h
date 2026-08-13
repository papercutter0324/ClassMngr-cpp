#pragma once

#include "domain/models/schedule_import.h"
#include "features/schedule/ui/schedule_view_model.h"

#include <QString>
#include <QStringList>

class ClassService;
class QColor;
class TeacherService;
struct ScheduleDisplayState;
struct Teacher;

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

[[nodiscard]] QString classLabel(
    ClassService* classService,
    TeacherService* teacherService,
    int classId,
    ScheduleImportKind kind
    );

[[nodiscard]] QString classDifferences(
    ClassService* classService,
    TeacherService* teacherService,
    const ScheduleImportClassCandidate& candidate,
    int targetClassId,
    ScheduleImportKind kind,
    const QString& classColor,
    const QColor& changesColor,
    const QColor& changesHeadingColor
    );

[[nodiscard]] QString teacherLabel(
    const Teacher& teacher
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
