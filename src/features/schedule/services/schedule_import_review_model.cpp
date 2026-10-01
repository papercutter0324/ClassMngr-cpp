#include "schedule_import_review_model.h"

#include "next/domain/domain_types.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>

namespace
{
using namespace ClassMngr::Next;

template <typename Id>
std::optional<Id> targetId(const int value)
{
    return value > 0 ? Id::fromString(std::to_string(value)) : std::nullopt;
}

Application::ScheduleImportReviewTeacherAction applyTeacherAction(
    const ScheduleImportTeacherAction action
    )
{
    using Action = Application::ScheduleImportReviewTeacherAction;
    switch (action)
    {
    case ScheduleImportTeacherAction::Reuse:
        return Action::Reuse;
    case ScheduleImportTeacherAction::UpdateRoom:
        return Action::UpdateRoom;
    case ScheduleImportTeacherAction::Create:
        return Action::Create;
    case ScheduleImportTeacherAction::Skip:
        return Action::Skip;
    }
    return Action::Invalid;
}

Application::ScheduleImportReviewClassAction applyClassAction(
    const ScheduleImportClassAction action
    )
{
    using Action = Application::ScheduleImportReviewClassAction;
    switch (action)
    {
    case ScheduleImportClassAction::UpdateExisting:
        return Action::UpdateExisting;
    case ScheduleImportClassAction::CreateNew:
        return Action::CreateNew;
    case ScheduleImportClassAction::Skip:
        return Action::Skip;
    }
    return Action::Invalid;
}
}

Application::ScheduleImportApplyRequest
ScheduleImportReviewModel::buildApplyRequest(
    const ScheduleImportReviewContext& context,
    const QList<ScheduleImportTeacherResolution>& teachers,
    const QList<ScheduleImportClassResolution>& classes
    )
{
    using namespace Application;
    ScheduleImportApplyRequest request;
    request.intensiveSchedule = context.kind == ScheduleImportKind::Intensive;
    switch (context.intensiveMode)
    {
    case ScheduleImportIntensiveMode::UpdateExisting:
        request.intensiveMode = ScheduleImportPlanIntensiveMode::UpdateExisting;
        break;
    case ScheduleImportIntensiveMode::ReplaceWithNew:
        request.intensiveMode = ScheduleImportPlanIntensiveMode::ReplaceWithNew;
        break;
    default:
        request.intensiveMode = ScheduleImportPlanIntensiveMode::Invalid;
        break;
    }
    request.selectedUserName = context.selectedUserName.toStdU16String();
    request.saveProfileNameIfBlank = context.saveProfileNameIfBlank;
    request.updateProfileName = context.updateProfileName;
    request.diagnosticsAcknowledged = context.unknownCellsAcknowledged;

    request.candidates.reserve(
        static_cast<std::size_t>(context.candidates.size())
        );
    for (const ScheduleImportClassCandidate& candidate : context.candidates)
    {
        ScheduleImportApplyCandidate item;
        item.teacherKey = candidate.teacherKey.toStdU16String();
        item.teacherName = candidate.teacherKr.toStdU16String();
        for (const QString& room : candidate.rooms)
        {
            item.rooms.push_back(room.trimmed().toStdU16String());
        }
        for (const QString& color : candidate.importedColors)
        {
            item.importedColors.push_back(color.toStdU16String());
        }
        item.grade = candidate.classGrade.toStdU16String();
        item.level = candidate.classLevel.toStdU16String();
        for (const ClassTime& time : candidate.times)
        {
            item.times.push_back({
                time.day.toStdU16String(),
                time.startTime.toStdU16String(),
                time.endTime.toStdU16String()
            });
        }
        for (const QString& cell : candidate.sourceCells)
        {
            item.sourceCells.push_back(cell.toStdU16String());
        }
        item.meetingPatternError = candidate.meetingPatternError.toStdU16String();
        request.candidates.push_back(std::move(item));
    }
    for (const IntensiveSlotState& slot : context.intensiveSlotStates)
    {
        request.intensiveSlotStates.push_back({
            slot.day.toStdU16String(),
            slot.startTime.toStdU16String(),
            slot.state.toStdU16String()
        });
    }
    for (const ScheduleImportDiagnostic& diagnostic : context.diagnostics)
    {
        request.diagnostics.push_back({
            diagnostic.sheetName.toStdU16String(),
            diagnostic.userName.toStdU16String(),
            diagnostic.cellReference.toStdU16String(),
            diagnostic.value.toStdU16String(),
            diagnostic.message.toStdU16String()
        });
    }
    for (const ScheduleImportTeacherResolution& teacher : teachers)
    {
        request.teachers.push_back({
            teacher.teacherKey.toStdU16String(),
            applyTeacherAction(teacher.action),
            targetId<Domain::TeacherId>(teacher.targetTeacherId),
            teacher.selectedRoom.trimmed().toStdU16String()
        });
    }
    for (const ScheduleImportClassResolution& candidateClass : classes)
    {
        request.classes.push_back({
            candidateClass.candidateIndex,
            applyClassAction(candidateClass.action),
            targetId<Domain::ClassId>(candidateClass.targetClassId),
            candidateClass.classColor.trimmed().toStdU16String(),
            candidateClass.fontColor.trimmed().toStdU16String()
        });
    }
    return request;
}
