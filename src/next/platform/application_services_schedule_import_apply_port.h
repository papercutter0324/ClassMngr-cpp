#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "domain/models/schedule_import.h"
#include "next/application/schedule_import_apply_use_case.h"

#include <QString>
#include <charconv>

namespace ClassMngr::Next::Platform
{

template <typename Id>
[[nodiscard]] inline std::optional<Id> applyId(int value)
{
    return value > 0 ? Id::fromString(std::to_string(value)) : std::nullopt;
}

template <typename Id>
[[nodiscard]] inline int legacyApplyId(const std::optional<Id>& value)
{
    if (!value) return -1;
    int parsed = -1;
    const auto& text = value->value();
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), parsed);
    return error == std::errc{} && end == text.data() + text.size() && parsed > 0
        ? parsed : -1;
}

[[nodiscard]] inline Application::ScheduleImportReviewTeacherAction applyTeacherAction(
    ScheduleImportTeacherAction action)
{
    using Action = Application::ScheduleImportReviewTeacherAction;
    switch (action)
    {
    case ScheduleImportTeacherAction::Reuse: return Action::Reuse;
    case ScheduleImportTeacherAction::UpdateRoom: return Action::UpdateRoom;
    case ScheduleImportTeacherAction::Create: return Action::Create;
    case ScheduleImportTeacherAction::Skip: return Action::Skip;
    }
    return Action::Invalid;
}

[[nodiscard]] inline Application::ScheduleImportReviewClassAction applyClassAction(
    ScheduleImportClassAction action)
{
    using Action = Application::ScheduleImportReviewClassAction;
    switch (action)
    {
    case ScheduleImportClassAction::UpdateExisting: return Action::UpdateExisting;
    case ScheduleImportClassAction::CreateNew: return Action::CreateNew;
    case ScheduleImportClassAction::Skip: return Action::Skip;
    }
    return Action::Invalid;
}

[[nodiscard]] inline ScheduleImportTeacherAction legacyTeacherAction(
    Application::ScheduleImportReviewTeacherAction action)
{
    switch (action)
    {
    case Application::ScheduleImportReviewTeacherAction::Reuse: return ScheduleImportTeacherAction::Reuse;
    case Application::ScheduleImportReviewTeacherAction::UpdateRoom: return ScheduleImportTeacherAction::UpdateRoom;
    case Application::ScheduleImportReviewTeacherAction::Skip: return ScheduleImportTeacherAction::Skip;
    default: return ScheduleImportTeacherAction::Create;
    }
}

[[nodiscard]] inline ScheduleImportClassAction legacyClassAction(
    Application::ScheduleImportReviewClassAction action)
{
    switch (action)
    {
    case Application::ScheduleImportReviewClassAction::UpdateExisting: return ScheduleImportClassAction::UpdateExisting;
    case Application::ScheduleImportReviewClassAction::Skip: return ScheduleImportClassAction::Skip;
    default: return ScheduleImportClassAction::CreateNew;
    }
}

[[nodiscard]] inline Application::ScheduleImportApplyRequest
scheduleImportApplyRequest(const ScheduleImportPlan& plan)
{
    using namespace Application;
    ScheduleImportApplyRequest request;
    request.intensiveSchedule = plan.kind == ScheduleImportKind::Intensive;
    request.intensiveMode = plan.intensiveMode == ScheduleImportIntensiveMode::ReplaceWithNew
        ? ScheduleImportPlanIntensiveMode::ReplaceWithNew
        : ScheduleImportPlanIntensiveMode::UpdateExisting;
    request.selectedUserName = plan.selectedUserName.toStdU16String();
    request.saveProfileNameIfBlank = plan.saveProfileNameIfBlank;
    request.updateProfileName = plan.updateProfileName;
    request.diagnosticsAcknowledged = plan.unknownCellsAcknowledged;
    for (const auto& candidate : plan.candidates)
    {
        ScheduleImportApplyCandidate item;
        item.teacherKey = candidate.teacherKey.toStdU16String();
        item.teacherName = candidate.teacherKr.toStdU16String();
        for (const auto& room : candidate.rooms) item.rooms.push_back(room.trimmed().toStdU16String());
        for (const auto& color : candidate.importedColors) item.importedColors.push_back(color.toStdU16String());
        item.grade = candidate.classGrade.toStdU16String();
        item.level = candidate.classLevel.toStdU16String();
        for (const auto& time : candidate.times)
        {
            item.times.push_back({time.day.toStdU16String(),
                                  time.startTime.toStdU16String(),
                                  time.endTime.toStdU16String()});
        }
        for (const auto& cell : candidate.sourceCells) item.sourceCells.push_back(cell.toStdU16String());
        item.meetingPatternError = candidate.meetingPatternError.toStdU16String();
        request.candidates.push_back(std::move(item));
    }
    for (const auto& slot : plan.intensiveSlotStates)
    {
        request.intensiveSlotStates.push_back({slot.day.toStdU16String(),
            slot.startTime.toStdU16String(), slot.state.toStdU16String()});
    }
    for (const auto& diagnostic : plan.diagnostics)
    {
        request.diagnostics.push_back({diagnostic.sheetName.toStdU16String(),
            diagnostic.userName.toStdU16String(), diagnostic.cellReference.toStdU16String(),
            diagnostic.value.toStdU16String(), diagnostic.message.toStdU16String()});
    }
    for (const auto& teacher : plan.teachers)
    {
        request.teachers.push_back({teacher.teacherKey.toStdU16String(),
            applyTeacherAction(teacher.action),
            applyId<Domain::TeacherId>(teacher.targetTeacherId),
            teacher.selectedRoom.trimmed().toStdU16String()});
    }
    for (const auto& candidateClass : plan.classes)
    {
        request.classes.push_back({candidateClass.candidateIndex,
            applyClassAction(candidateClass.action),
            applyId<Domain::ClassId>(candidateClass.targetClassId),
            candidateClass.classColor.trimmed().toStdU16String(),
            candidateClass.fontColor.trimmed().toStdU16String()});
    }
    return request;
}

[[nodiscard]] inline ScheduleImportPlan legacyScheduleImportPlan(
    const Application::ScheduleImportApplyRequest& request)
{
    ScheduleImportPlan plan;
    plan.kind = request.intensiveSchedule ? ScheduleImportKind::Intensive : ScheduleImportKind::Normal;
    plan.intensiveMode = request.intensiveMode == Application::ScheduleImportPlanIntensiveMode::ReplaceWithNew
        ? ScheduleImportIntensiveMode::ReplaceWithNew : ScheduleImportIntensiveMode::UpdateExisting;
    plan.selectedUserName = QString::fromStdU16String(request.selectedUserName);
    plan.saveProfileNameIfBlank = request.saveProfileNameIfBlank;
    plan.updateProfileName = request.updateProfileName;
    plan.unknownCellsAcknowledged = request.diagnosticsAcknowledged;
    for (const auto& item : request.candidates)
    {
        ScheduleImportClassCandidate candidate;
        candidate.teacherKey = QString::fromStdU16String(item.teacherKey);
        candidate.teacherKr = QString::fromStdU16String(item.teacherName);
        for (const auto& room : item.rooms) candidate.rooms.append(QString::fromStdU16String(room));
        for (const auto& color : item.importedColors) candidate.importedColors.append(QString::fromStdU16String(color));
        candidate.classGrade = QString::fromStdU16String(item.grade);
        candidate.classLevel = QString::fromStdU16String(item.level);
        for (const auto& time : item.times) candidate.times.append({
            QString::fromStdU16String(time.day), QString::fromStdU16String(time.startTime),
            QString::fromStdU16String(time.endTime)});
        for (const auto& cell : item.sourceCells) candidate.sourceCells.append(QString::fromStdU16String(cell));
        candidate.meetingPatternError = QString::fromStdU16String(item.meetingPatternError);
        plan.candidates.append(std::move(candidate));
    }
    for (const auto& slot : request.intensiveSlotStates)
        plan.intensiveSlotStates.append({QString::fromStdU16String(slot.day),
            QString::fromStdU16String(slot.startTime), QString::fromStdU16String(slot.state)});
    for (const auto& diagnostic : request.diagnostics)
        plan.diagnostics.append({QString::fromStdU16String(diagnostic.sheetName),
            QString::fromStdU16String(diagnostic.userName), QString::fromStdU16String(diagnostic.cellReference),
            QString::fromStdU16String(diagnostic.value), QString::fromStdU16String(diagnostic.message)});
    for (const auto& teacher : request.teachers)
        plan.teachers.append({QString::fromStdU16String(teacher.teacherKey),
            legacyTeacherAction(teacher.action), legacyApplyId(teacher.targetTeacherId),
            QString::fromStdU16String(teacher.selectedRoom)});
    for (const auto& item : request.classes)
        plan.classes.append({item.candidateIndex, legacyClassAction(item.action),
            legacyApplyId(item.targetClassId), QString::fromStdU16String(item.classColor),
            QString::fromStdU16String(item.fontColor)});
    return plan;
}

[[nodiscard]] inline Application::ScheduleImportApplyResult scheduleImportApplyResult(
    const Result<ScheduleImportSummary>& result)
{
    if (!result)
        return std::unexpected(Application::ScheduleImportApplyFailure{
            result.error().toStdU16String(), std::nullopt});
    return Application::ScheduleImportApplySummary{
        result->teachersCreated, result->teachersUpdated, result->classesCreated,
        result->classesUpdated, result->classesSkipped, result->schedulesCleared,
        result->ignoredCells, result->profileNameUpdated};
}

class ApplicationServicesScheduleImportApplyPort final
    : public Application::ScheduleImportApplyWritePort
{
public:
    explicit ApplicationServicesScheduleImportApplyPort(ApplicationServices* services)
        : m_services(services) {}

    [[nodiscard]] Application::ScheduleImportApplyResult applyScheduleImport(
        const Application::ScheduleImportApplyRequest& request) const override
    {
        ScheduleService* service = m_services ? m_services->scheduleService() : nullptr;
        if (!service || !service->isAvailable())
            return std::unexpected(Application::ScheduleImportApplyFailure{
                u"No Teacher Profile is open.", std::nullopt});
        return scheduleImportApplyResult(
            service->importSchedule(legacyScheduleImportPlan(request)));
    }

private:
    ApplicationServices* m_services;
};

} // namespace ClassMngr::Next::Platform
