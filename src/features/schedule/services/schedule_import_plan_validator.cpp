#include "schedule_import_plan_validator.h"

#include "domain/rules/schedule_import_rules.h"
#include "next/application/schedule_import_plan_validation.h"
#include "next/application/schedule_import_review_decisions.h"
#include "next/domain/course.h"
#include "next/domain/domain_types.h"

#include <QObject>

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
using ClassMngr::Next::Application::ScheduleImportPlanEligibilityIssue;
using ClassMngr::Next::Application::ScheduleImportPlanEligibilityIssueCode;
using ClassMngr::Next::Domain::Course;
using ClassMngr::Next::Domain::Weekday;

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

QString meetingPatternExpectation(
    const Course::WeeklyMeetingDayRuleKind ruleKind
    )
{
    switch (ruleKind)
    {
    case Course::WeeklyMeetingDayRuleKind::PairedWeekdays:
        return QObject::tr(
            "Expected Monday/Wednesday, Monday/Friday, Wednesday/Friday, or Tuesday/Thursday."
            );
    case Course::WeeklyMeetingDayRuleKind::ThreeDayOrTuesdayThursday:
        return QObject::tr(
            "Expected Monday/Wednesday/Friday or Tuesday/Thursday."
            );
    case Course::WeeklyMeetingDayRuleKind::SingleWeekday:
        return QObject::tr("Expected one weekday meeting.");
    }
    return {};
}

QString weekdayName(const Weekday weekday)
{
    switch (weekday)
    {
    case Weekday::Monday:
        return QStringLiteral("Monday");
    case Weekday::Tuesday:
        return QStringLiteral("Tuesday");
    case Weekday::Wednesday:
        return QStringLiteral("Wednesday");
    case Weekday::Thursday:
        return QStringLiteral("Thursday");
    case Weekday::Friday:
        return QStringLiteral("Friday");
    case Weekday::Saturday:
        return QStringLiteral("Saturday");
    case Weekday::Sunday:
        return QStringLiteral("Sunday");
    }
    return {};
}

QString meetingPatternFailure(
    const ScheduleImportPlanEligibilityIssue& issue
    )
{
    if (
        issue.code
        == ScheduleImportPlanEligibilityIssueCode::InvalidMeetingWeekday
        )
    {
        return QObject::tr(
            "Each imported class must have exactly one meeting per scheduled weekday."
            );
    }

    QStringList displayDays;
    for (const Weekday weekday : issue.meetingWeekdays)
    {
        displayDays.append(
            scheduleImportWeekdayDisplayName(weekdayName(weekday))
            );
    }

    const QString expectation = issue.meetingPatternRuleKind.has_value()
        ? meetingPatternExpectation(*issue.meetingPatternRuleKind)
        : QString();
    return QObject::tr("%1 Detected: %2.")
        .arg(
            expectation,
            displayDays.isEmpty()
                ? QObject::tr("no meetings")
                : displayDays.join(QStringLiteral(", "))
            );
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
    ClassMngr::Next::Application::ScheduleImportPlanEligibilityRequest request;
    request.intensiveSchedule = plan.kind == ScheduleImportKind::Intensive;
    switch (plan.intensiveMode)
    {
    case ScheduleImportIntensiveMode::UpdateExisting:
        request.intensiveMode =
            ClassMngr::Next::Application::ScheduleImportPlanIntensiveMode::
                UpdateExisting;
        break;
    case ScheduleImportIntensiveMode::ReplaceWithNew:
        request.intensiveMode =
            ClassMngr::Next::Application::ScheduleImportPlanIntensiveMode::
                ReplaceWithNew;
        break;
    default:
        request.intensiveMode =
            ClassMngr::Next::Application::ScheduleImportPlanIntensiveMode::
                Invalid;
        break;
    }
    request.hasDiagnostics = !plan.diagnostics.isEmpty();
    request.diagnosticsAcknowledged = plan.unknownCellsAcknowledged;

    request.candidates.reserve(
        static_cast<std::size_t>(plan.candidates.size())
        );
    for (const ScheduleImportClassCandidate& candidate : plan.candidates)
    {
        ClassMngr::Next::Application::
            ScheduleImportPlanEligibilityCandidate eligibilityCandidate;
        eligibilityCandidate.grade = candidate.classGrade.toStdString();
        eligibilityCandidate.level = candidate.classLevel.toStdString();
        eligibilityCandidate.teacherKey = candidate.teacherKey.toStdU16String();
        eligibilityCandidate.teacherName = candidate.teacherKr.toStdU16String();
        eligibilityCandidate.weekdays.reserve(
            static_cast<std::size_t>(candidate.times.size())
            );
        for (const ClassTime& time : candidate.times)
        {
            eligibilityCandidate.weekdays.push_back(time.day.toStdString());
        }
        request.candidates.push_back(std::move(eligibilityCandidate));

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
        request.reviewDecisions.candidates.push_back(
            std::move(decisionCandidate)
            );
    }

    request.reviewDecisions.teachers.reserve(
        static_cast<std::size_t>(plan.teachers.size())
        );
    for (const ScheduleImportTeacherResolution& resolution : plan.teachers)
    {
        request.reviewDecisions.teachers.push_back(
            ScheduleImportReviewTeacherResolution{
                resolution.teacherKey.toStdString(),
                decisionAction(resolution.action),
                resolution.selectedRoom.trimmed().toStdString()
            }
            );
    }

    request.reviewDecisions.classes.reserve(
        static_cast<std::size_t>(plan.classes.size())
        );
    request.classColors.reserve(
        static_cast<std::size_t>(plan.classes.size())
        );
    for (const ScheduleImportClassResolution& resolution : plan.classes)
    {
        request.reviewDecisions.classes.push_back(
            ScheduleImportReviewClassResolution{
                resolution.candidateIndex,
                decisionAction(resolution.action),
                decisionTargetId(resolution.targetClassId)
            }
            );
        request.classColors.push_back(
            ClassMngr::Next::Application::
                ScheduleImportPlanEligibilityClassColors{
                    resolution.candidateIndex,
                    resolution.classColor.trimmed().toStdString(),
                    resolution.fontColor.trimmed().toStdString()
                }
            );
    }

    const std::optional<ScheduleImportPlanEligibilityIssue> issue =
        ClassMngr::Next::Application::validateScheduleImportPlanEligibility(
            request
            );
    if (issue.has_value())
    {
        using IssueCode = ScheduleImportPlanEligibilityIssueCode;
        QString error;
        switch (issue->code)
        {
        case IssueCode::InvalidIntensiveMode:
            error = QObject::tr(
                "Choose how the existing intensive schedule should be handled."
                );
            break;
        case IssueCode::UnacknowledgedDiagnostics:
            error = QObject::tr(
                "Unrecognized timetable cells must be acknowledged before importing."
                );
            break;
        case IssueCode::InvalidReviewDecision:
            error = issue->reviewDecisionIssue.has_value()
                ? decisionFailure(
                    *issue->reviewDecisionIssue,
                    request.reviewDecisions
                    )
                : QObject::tr(
                    "The import plan contains an invalid resolution."
                    );
            break;
        case IssueCode::InvalidCourse:
        case IssueCode::InvalidTeacherKey:
        case IssueCode::MissingMeetingTimes:
            error = QObject::tr("The import contains an invalid class.");
            break;
        case IssueCode::InvalidMeetingWeekday:
        case IssueCode::InvalidMeetingPattern:
            error = QObject::tr(
                "The meeting pattern for %1 %2 is invalid: %3"
                )
                .arg(
                    plan.candidates[static_cast<qsizetype>(issue->candidateIndex)]
                        .classGrade,
                    plan.candidates[static_cast<qsizetype>(issue->candidateIndex)]
                        .classLevel,
                    meetingPatternFailure(*issue)
                    );
            break;
        case IssueCode::InvalidClassColor:
            error = QObject::tr(
                "Choose a valid class color for every imported class."
                );
            break;
        }
        return std::unexpected(
            error
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

    return validated;
}
