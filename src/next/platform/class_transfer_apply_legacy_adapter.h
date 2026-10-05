#pragma once

#include "core/result.h"
#include "domain/models/class_transfer.h"
#include "next/application/class_transfer_apply_request.h"

#include <QObject>

#include <algorithm>
#include <string>

namespace ClassMngr::Next::Platform
{

namespace ClassTransferApplyLegacyDetail
{
template <typename TypedId>
[[nodiscard]] inline Result<TypedId> typedReviewId(int legacyId, const QString& description)
{
    if (legacyId <= 0)
        return std::unexpected(QObject::tr("The %1 contains an invalid destination ID.").arg(description));
    return *TypedId::fromString(std::to_string(legacyId));
}
}

[[nodiscard]] inline Result<Application::ClassTransferApplyCandidates> classTransferApplyCandidates(
    const ClassTransferPackage& package,
    const ClassImportPreview& preview,
    bool normalizeBlankTeacherKeys = false
    )
{
    using namespace Application;
    using ClassId = ClassMngr::Next::Domain::ClassId;
    using TeacherId = ClassMngr::Next::Domain::TeacherId;
    ClassTransferApplyCandidates request;

    for (int index = 0; index < package.classes.size(); ++index)
    {
        ClassTransferReviewClassCandidate candidate;
        candidate.packageClassIndex = index;
        const auto previewEntry = std::find_if(
            preview.classes.cbegin(),
            preview.classes.cend(),
            [index](const ClassImportClassPreview& item)
            {
                return item.packageClassIndex == index;
            }
            );
        if (previewEntry != preview.classes.cend())
        {
            candidate.matchingClassIds.reserve(
                static_cast<std::size_t>(previewEntry->matchingClassIds.size())
            );
            for (const int classId : previewEntry->matchingClassIds)
            {
                const Result<ClassId> typedId = ClassTransferApplyLegacyDetail::typedReviewId<ClassId>(
                    classId, QObject::tr("class import preview"));
                if (!typedId)
                {
                    return std::unexpected(typedId.error());
                }
                candidate.matchingClassIds.push_back(*typedId);
            }
        }
        request.classes.push_back(std::move(candidate));
    }

    for (const ClassTransferTeacher& transferTeacher : package.teachers)
    {
        ClassTransferReviewTeacherCandidate candidate;
        candidate.teacherKey = (normalizeBlankTeacherKeys && transferTeacher.key.trimmed().isEmpty()
            ? std::string{} : transferTeacher.key.toStdString());
        const auto previewEntry = std::find_if(
            preview.teachers.cbegin(),
            preview.teachers.cend(),
            [&transferTeacher](const ClassImportTeacherPreview& item)
            {
                return item.teacherKey == transferTeacher.key;
            }
            );
        if (previewEntry != preview.teachers.cend())
        {
            candidate.matchingTeacherIds.reserve(
                static_cast<std::size_t>(previewEntry->matchingTeacherIds.size())
            );
            for (const int teacherId : previewEntry->matchingTeacherIds)
            {
                const Result<TeacherId> typedId = ClassTransferApplyLegacyDetail::typedReviewId<TeacherId>(
                    teacherId, QObject::tr("teacher import preview"));
                if (!typedId)
                {
                    return std::unexpected(typedId.error());
                }
                candidate.matchingTeacherIds.push_back(*typedId);
            }
        }
        request.teachers.push_back(std::move(candidate));
    }

    return request;
}

[[nodiscard]] inline Application::ClassTransferApplyRequest classTransferApplyRequest(
    const ClassImportPlan& plan)
{
    using namespace Application;
    ClassTransferApplyRequest request;
    for (const auto& resolution : plan.classes)
    {
        auto action = ClassTransferReviewClassAction::Invalid;
        switch (resolution.action)
        {
        case ClassImportAction::Create: action = ClassTransferReviewClassAction::Create; break;
        case ClassImportAction::Replace: action = ClassTransferReviewClassAction::Replace; break;
        case ClassImportAction::Skip: action = ClassTransferReviewClassAction::Skip; break;
        }
        request.classes.push_back({resolution.packageClassIndex, action,
            resolution.targetClassId == -1 ? std::nullopt
                : Domain::ClassId::fromString(std::to_string(resolution.targetClassId))});
    }
    for (const auto& resolution : plan.teachers)
    {
        auto action = ClassTransferReviewTeacherAction::Invalid;
        switch (resolution.action)
        {
        case TeacherImportAction::Create: action = ClassTransferReviewTeacherAction::Create; break;
        case TeacherImportAction::KeepExisting: action = ClassTransferReviewTeacherAction::KeepExisting; break;
        case TeacherImportAction::ReplaceExisting: action = ClassTransferReviewTeacherAction::ReplaceExisting; break;
        }
        request.teachers.push_back({resolution.teacherKey.toStdString(), action,
            resolution.targetTeacherId == -1 ? std::nullopt
                : Domain::TeacherId::fromString(std::to_string(resolution.targetTeacherId))});
    }
    return request;
}

// Called after typed validation, at the legacy persistence implementation edge.
[[nodiscard]] inline ClassImportPlan validatedClassTransferLegacyPlan(
    const Application::ClassTransferApplyRequest& request)
{
    using namespace Application;
    ClassImportPlan plan;
    for (const auto& choice : request.classes)
    {
        const auto action = choice.action == ClassTransferReviewClassAction::Create
            ? ClassImportAction::Create : choice.action == ClassTransferReviewClassAction::Replace
                ? ClassImportAction::Replace : ClassImportAction::Skip;
        plan.classes.append({choice.packageClassIndex, action,
            choice.targetClassId ? *classTransferApplyDestinationId(*choice.targetClassId) : -1});
    }
    for (const auto& choice : request.teachers)
    {
        const auto action = choice.action == ClassTransferReviewTeacherAction::Create
            ? TeacherImportAction::Create : choice.action == ClassTransferReviewTeacherAction::KeepExisting
                ? TeacherImportAction::KeepExisting : TeacherImportAction::ReplaceExisting;
        plan.teachers.append({QString::fromStdString(choice.teacherKey), action,
            choice.targetTeacherId ? *classTransferApplyDestinationId(*choice.targetTeacherId) : -1});
    }
    return plan;
}

} // namespace ClassMngr::Next::Platform
