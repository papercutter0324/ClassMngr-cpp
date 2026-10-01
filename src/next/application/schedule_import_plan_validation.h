#pragma once

#include "next/application/schedule_import_review_decisions.h"
#include "next/domain/course.h"
#include "next/domain/korean_teacher_key.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

enum class ScheduleImportPlanIntensiveMode
{
    UpdateExisting,
    ReplaceWithNew,
    Invalid
};

struct ScheduleImportPlanEligibilityCandidate final
{
    std::string grade;
    std::string level;
    std::u16string teacherKey;
    std::u16string teacherName;
    // The legacy feature edge preserves the exact source spelling here.
    // Only English weekday names with matching case are recognized.
    std::vector<std::string> weekdays;
};

struct ScheduleImportPlanEligibilityClassColors final
{
    int candidateIndex = -1;
    // Values are trimmed by the feature edge before crossing into Application.
    std::string classColor;
    std::string fontColor;
};

struct ScheduleImportPlanEligibilityRequest final
{
    bool intensiveSchedule = false;
    ScheduleImportPlanIntensiveMode intensiveMode =
        ScheduleImportPlanIntensiveMode::UpdateExisting;
    bool hasDiagnostics = false;
    bool diagnosticsAcknowledged = false;
    std::vector<ScheduleImportPlanEligibilityCandidate> candidates;
    ScheduleImportReviewDecisionRequest reviewDecisions;
    std::vector<ScheduleImportPlanEligibilityClassColors> classColors;
};

enum class ScheduleImportPlanEligibilityIssueCode
{
    InvalidIntensiveMode,
    UnacknowledgedDiagnostics,
    InvalidReviewDecision,
    InvalidCourse,
    InvalidTeacherKey,
    MissingMeetingTimes,
    InvalidMeetingWeekday,
    InvalidMeetingPattern,
    InvalidClassColor
};

struct ScheduleImportPlanEligibilityIssue final
{
    ScheduleImportPlanEligibilityIssueCode code;
    std::size_t candidateIndex = 0;
    std::optional<ScheduleImportReviewDecisionIssue> reviewDecisionIssue;
    std::optional<Domain::Course::WeeklyMeetingDayRuleKind>
        meetingPatternRuleKind;
    std::vector<Domain::Weekday> meetingWeekdays;
};

[[nodiscard]] inline std::optional<ScheduleImportPlanEligibilityIssue>
validateScheduleImportPlanEligibility(
    const ScheduleImportPlanEligibilityRequest& request
    )
{
    using IssueCode = ScheduleImportPlanEligibilityIssueCode;

    if (request.intensiveSchedule
        && request.intensiveMode != ScheduleImportPlanIntensiveMode::UpdateExisting
        && request.intensiveMode != ScheduleImportPlanIntensiveMode::ReplaceWithNew)
    {
        return ScheduleImportPlanEligibilityIssue{
            IssueCode::InvalidIntensiveMode
        };
    }

    if (request.hasDiagnostics && !request.diagnosticsAcknowledged)
    {
        return ScheduleImportPlanEligibilityIssue{
            IssueCode::UnacknowledgedDiagnostics
        };
    }

    const ScheduleImportReviewDecisionResult decisionResult =
        validateScheduleImportReviewDecisions(request.reviewDecisions);
    if (!decisionResult.accepted())
    {
        ScheduleImportPlanEligibilityIssue issue{
            IssueCode::InvalidReviewDecision
        };
        issue.reviewDecisionIssue = decisionResult.issues.front();
        return issue;
    }

    for (std::size_t candidateIndex = 0;
         candidateIndex < request.candidates.size();
         ++candidateIndex)
    {
        const ScheduleImportPlanEligibilityCandidate& candidate =
            request.candidates[candidateIndex];
        if (!Domain::Course::fromNames(candidate.grade, candidate.level))
        {
            return ScheduleImportPlanEligibilityIssue{
                IssueCode::InvalidCourse,
                candidateIndex
            };
        }

        const Domain::KoreanTeacherKey key =
            Domain::KoreanTeacherKey::fromName(candidate.teacherKey);
        const Domain::KoreanTeacherKey keyFromName =
            Domain::KoreanTeacherKey::fromName(candidate.teacherName);
        if (candidate.teacherKey.empty()
            || key.value() != candidate.teacherKey
            || key != keyFromName)
        {
            return ScheduleImportPlanEligibilityIssue{
                IssueCode::InvalidTeacherKey,
                candidateIndex
            };
        }

        if (candidate.weekdays.empty())
        {
            return ScheduleImportPlanEligibilityIssue{
                IssueCode::MissingMeetingTimes,
                candidateIndex
            };
        }
    }

    std::vector<ScheduleImportReviewClassAction> classActions(
        request.candidates.size(),
        ScheduleImportReviewClassAction::Unselected
        );
    for (const ScheduleImportReviewClassResolution& resolution :
         request.reviewDecisions.classes)
    {
        classActions[static_cast<std::size_t>(resolution.candidateIndex)] =
            resolution.action;
    }

    std::vector<const ScheduleImportPlanEligibilityClassColors*> colorsByCandidate(
        request.candidates.size(),
        nullptr
        );
    for (const ScheduleImportPlanEligibilityClassColors& colors :
         request.classColors)
    {
        if (colors.candidateIndex >= 0
            && static_cast<std::size_t>(colors.candidateIndex)
                < colorsByCandidate.size())
        {
            colorsByCandidate[static_cast<std::size_t>(colors.candidateIndex)] =
                &colors;
        }
    }

    const auto weekdayFromName = [](const std::string& name)
        -> std::optional<Domain::Weekday>
    {
        if (name == "Monday")
        {
            return Domain::Weekday::Monday;
        }
        if (name == "Tuesday")
        {
            return Domain::Weekday::Tuesday;
        }
        if (name == "Wednesday")
        {
            return Domain::Weekday::Wednesday;
        }
        if (name == "Thursday")
        {
            return Domain::Weekday::Thursday;
        }
        if (name == "Friday")
        {
            return Domain::Weekday::Friday;
        }
        return std::nullopt;
    };

    const auto validHexColor = [](const std::string& color)
    {
        if (color.size() != 7 || color.front() != '#')
        {
            return false;
        }
        for (std::size_t index = 1; index < color.size(); ++index)
        {
            const char digit = color[index];
            if (!((digit >= '0' && digit <= '9')
                  || (digit >= 'A' && digit <= 'F')
                  || (digit >= 'a' && digit <= 'f')))
            {
                return false;
            }
        }
        return true;
    };

    for (std::size_t candidateIndex = 0;
         candidateIndex < request.candidates.size();
         ++candidateIndex)
    {
        if (classActions[candidateIndex]
            != ScheduleImportReviewClassAction::Skip)
        {
            const ScheduleImportPlanEligibilityCandidate& candidate =
                request.candidates[candidateIndex];
            Domain::Course::WeeklyMeetingDayPattern weekdays;
            weekdays.reserve(candidate.weekdays.size());
            for (const std::string& dayName : candidate.weekdays)
            {
                const std::optional<Domain::Weekday> weekday =
                    weekdayFromName(dayName);
                if (!weekday.has_value()
                    || std::find(
                           weekdays.begin(),
                           weekdays.end(),
                           *weekday
                           ) != weekdays.end())
                {
                    return ScheduleImportPlanEligibilityIssue{
                        IssueCode::InvalidMeetingWeekday,
                        candidateIndex
                    };
                }
                weekdays.push_back(*weekday);
            }

            const Domain::Course course =
                *Domain::Course::fromNames(candidate.grade, candidate.level);
            const std::optional<Domain::Course::WeeklyMeetingDayRule> rule =
                course.weeklyMeetingDayRule();
            if (rule.has_value() && !rule->allows(weekdays))
            {
                ScheduleImportPlanEligibilityIssue issue{
                    IssueCode::InvalidMeetingPattern,
                    candidateIndex
                };
                issue.meetingPatternRuleKind = rule->kind();
                issue.meetingWeekdays = std::move(weekdays);
                return issue;
            }

            const ScheduleImportPlanEligibilityClassColors* colors =
                colorsByCandidate[candidateIndex];
            if (colors == nullptr
                || !validHexColor(colors->classColor)
                || !validHexColor(colors->fontColor))
            {
                return ScheduleImportPlanEligibilityIssue{
                    IssueCode::InvalidClassColor,
                    candidateIndex
                };
            }
        }
    }

    return std::nullopt;
}

} // namespace ClassMngr::Next::Application
