#include "schedule_import_plan_validator.h"

#include "domain/rules/schedule_import_rules.h"
#include "features/teacher/import/teacher_import_name_utils.h"
#include "next/application/schedule_import_review_decisions.h"
#include "next/domain/course.h"
#include "next/domain/domain_types.h"

#include <QObject>
#include <QRegularExpression>

#include <cstddef>
#include <optional>
#include <set>
#include <string>

namespace
{
using ClassMngr::Next::Application::
    ScheduleImportReviewClassAction;
using ClassMngr::Next::Application::
    ScheduleImportReviewClassResolution;
using ClassMngr::Next::Application::
    ScheduleImportReviewDecisionIssue;
using ClassMngr::Next::Application::
    ScheduleImportReviewDecisionIssueCode;
using ClassMngr::Next::Application::
    ScheduleImportReviewDecisionRequest;
using ClassMngr::Next::Application::
    ScheduleImportReviewTeacherAction;
using ClassMngr::Next::Application::
    ScheduleImportReviewTeacherResolution;

std::optional<ClassMngr::Next::Domain::ClassId> decisionTargetId(
    int legacyTargetId
    )
{
    if (legacyTargetId <= 0)
    {
        return std::nullopt;
    }
    return ClassMngr::Next::Domain::ClassId::fromString(
        std::to_string(legacyTargetId)
        );
}

bool validCourse(const QString& grade, const QString& level)
{
    return ClassMngr::Next::Domain::Course::fromNames(
        grade.toStdString(),
        level.toStdString()
        ).has_value();
}

QString normalizedHexColor(const QString& value)
{
    static const QRegularExpression expression(
        QStringLiteral("^#[0-9A-Fa-f]{6}$")
        );
    const QString color = value.trimmed();
    return expression.match(color).hasMatch()
        ? color.toUpper()
        : QString();
}

ScheduleImportReviewTeacherAction decisionAction(
    ScheduleImportTeacherAction action
    )
{
    switch (action)
    {
    case ScheduleImportTeacherAction::Reuse:
        return ScheduleImportReviewTeacherAction::Reuse;
    case ScheduleImportTeacherAction::UpdateRoom:
        return ScheduleImportReviewTeacherAction::UpdateRoom;
    case ScheduleImportTeacherAction::Create:
        return ScheduleImportReviewTeacherAction::Create;
    case ScheduleImportTeacherAction::Skip:
        return ScheduleImportReviewTeacherAction::Skip;
    }
    return ScheduleImportReviewTeacherAction::Invalid;
}

ScheduleImportReviewClassAction decisionAction(
    ScheduleImportClassAction action
    )
{
    switch (action)
    {
    case ScheduleImportClassAction::UpdateExisting:
        return ScheduleImportReviewClassAction::UpdateExisting;
    case ScheduleImportClassAction::CreateNew:
        return ScheduleImportReviewClassAction::CreateNew;
    case ScheduleImportClassAction::Skip:
        return ScheduleImportReviewClassAction::Skip;
    }
    return ScheduleImportReviewClassAction::Invalid;
}

QString decisionFailure(
    const ScheduleImportReviewDecisionIssue& issue,
    const ScheduleImportReviewDecisionRequest& request
    )
{
    using Code = ScheduleImportReviewDecisionIssueCode;
    const auto genericIncompleteResolutionMessage = []()
    {
        return QObject::tr(
            "Every imported teacher and class requires a resolution."
            );
    };
    switch (issue.code)
    {
    case Code::InvalidTeacherAction:
    case Code::EmptyTeacherKey:
    case Code::DuplicateTeacherResolution:
        return QObject::tr(
            "The teacher import plan contains an invalid or duplicate resolution."
            );
    case Code::UnknownTeacherResolution:
    case Code::MissingClassResolution:
        if (issue.code == Code::MissingClassResolution)
        {
            return genericIncompleteResolutionMessage();
        }
        break;
    case Code::MissingTeacherResolution:
        break;
    case Code::MissingTeacherRoom:
    case Code::ForeignTeacherRoom:
        return QObject::tr(
            "Choose one of the imported rooms for every unresolved Korean teacher."
            );
    case Code::InvalidClassAction:
    case Code::CandidateIndexOutOfRange:
    case Code::DuplicateClassResolution:
        return QObject::tr(
            "The class import plan contains an invalid or duplicate resolution."
            );
    case Code::UpdateClassMissingTarget:
    case Code::DuplicateUpdatedClassTarget:
        return QObject::tr(
            "Each updated class must have a unique existing target."
            );
    case Code::CreateNewClassHasTarget:
        return QObject::tr(
            "A newly created class cannot have an existing target."
            );
    case Code::DuplicateSkippedClassTarget:
        return QObject::tr(
            "Each imported class must resolve to a unique existing target."
            );
    case Code::ActiveClassAssignedToSkippedTeacher:
        return QObject::tr(
            "Classes assigned to a skipped Korean teacher must also be skipped."
            );
    }

    std::set<std::string> candidateTeacherKeys;
    for (const auto& candidate : request.candidates)
    {
        candidateTeacherKeys.insert(candidate.teacherKey);
    }
    std::set<std::string> resolvedTeacherKeys;
    for (const auto& resolution : request.teachers)
    {
        if (!resolution.teacherKey.empty())
        {
            resolvedTeacherKeys.insert(resolution.teacherKey);
        }
    }
    if (resolvedTeacherKeys.size() != candidateTeacherKeys.size())
    {
        return genericIncompleteResolutionMessage();
    }
    return QObject::tr(
        "Every imported teacher requires a matching resolution."
        );
}
}

Result<ValidatedScheduleImportPlan> ScheduleImportPlanValidator::validate(
    const ScheduleImportPlan& plan
    )
{
    if (
        plan.kind == ScheduleImportKind::Intensive
        && plan.intensiveMode != ScheduleImportIntensiveMode::UpdateExisting
        && plan.intensiveMode != ScheduleImportIntensiveMode::ReplaceWithNew
        )
    {
        return std::unexpected(
            QObject::tr(
                "Choose how the existing intensive schedule should be handled."
                )
            );
    }

    if (!plan.diagnostics.isEmpty() && !plan.unknownCellsAcknowledged)
    {
        return std::unexpected(
            QObject::tr(
                "Unrecognized timetable cells must be acknowledged before importing."
                )
            );
    }

    ScheduleImportReviewDecisionRequest decisions;
    decisions.candidates.reserve(
        static_cast<std::size_t>(plan.candidates.size())
        );
    for (const ScheduleImportClassCandidate& candidate : plan.candidates)
    {
        ClassMngr::Next::Application::ScheduleImportReviewDecisionCandidate
            decisionCandidate;
        decisionCandidate.teacherKey = candidate.teacherKey.toStdString();
        decisionCandidate.importedRooms.reserve(
            static_cast<std::size_t>(candidate.rooms.size())
            );
        for (const QString& room : candidate.rooms)
        {
            const QString normalizedRoom = room.trimmed();
            if (!normalizedRoom.isEmpty())
            {
                decisionCandidate.importedRooms.push_back(
                    normalizedRoom.toStdString()
                    );
            }
        }
        decisions.candidates.push_back(std::move(decisionCandidate));
    }

    decisions.teachers.reserve(
        static_cast<std::size_t>(plan.teachers.size())
        );
    for (const ScheduleImportTeacherResolution& resolution : plan.teachers)
    {
        decisions.teachers.push_back(
            ScheduleImportReviewTeacherResolution{
                resolution.teacherKey.toStdString(),
                decisionAction(resolution.action),
                resolution.selectedRoom.trimmed().toStdString()
            }
            );
    }

    decisions.classes.reserve(
        static_cast<std::size_t>(plan.classes.size())
        );
    for (const ScheduleImportClassResolution& resolution : plan.classes)
    {
        decisions.classes.push_back(
            ScheduleImportReviewClassResolution{
                resolution.candidateIndex,
                decisionAction(resolution.action),
                decisionTargetId(resolution.targetClassId)
            }
            );
    }

    const auto decisionResult =
        ClassMngr::Next::Application::validateScheduleImportReviewDecisions(
            decisions
            );
    if (!decisionResult.accepted())
    {
        return std::unexpected(
            decisionFailure(decisionResult.issues.front(), decisions)
            );
    }

    ValidatedScheduleImportPlan validated;
    for (const ScheduleImportTeacherResolution& resolution : plan.teachers)
    {
        validated.teacherResolutions.insert(
            resolution.teacherKey,
            resolution
            );
    }
    for (const ScheduleImportClassResolution& resolution : plan.classes)
    {
        validated.classResolutions.insert(
            resolution.candidateIndex,
            resolution
            );
    }

    for (const ScheduleImportClassCandidate& candidate : plan.candidates)
    {
        if (
            !validCourse(candidate.classGrade, candidate.classLevel)
            || candidate.teacherKey.isEmpty()
            || TeacherImportNameUtils::hangulOnly(candidate.teacherKr)
                != candidate.teacherKey
            || candidate.times.isEmpty()
            )
        {
            return std::unexpected(
                QObject::tr("The import contains an invalid class.")
                );
        }
    }

    for (int index = 0; index < plan.candidates.size(); ++index)
    {
        const ScheduleImportClassResolution classResolution =
            validated.classResolutions.value(index);
        const QString meetingPatternError =
            scheduleImportMeetingPatternError(plan.candidates[index]);
        if (
            classResolution.action != ScheduleImportClassAction::Skip
            && !meetingPatternError.isEmpty()
            )
        {
            return std::unexpected(
                QObject::tr(
                    "The meeting pattern for %1 %2 is invalid: %3"
                    )
                    .arg(
                        plan.candidates[index].classGrade,
                        plan.candidates[index].classLevel,
                        meetingPatternError
                        )
                );
        }
        if (
            classResolution.action != ScheduleImportClassAction::Skip
            && (
                normalizedHexColor(classResolution.classColor).isEmpty()
                || normalizedHexColor(classResolution.fontColor).isEmpty()
                )
            )
        {
            return std::unexpected(
                QObject::tr(
                    "Choose a valid class color for every imported class."
                    )
                );
        }
    }

    return validated;
}
