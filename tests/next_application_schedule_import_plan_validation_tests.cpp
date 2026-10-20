#include "next/application/schedule_import_plan_validation.h"

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next::Application;

namespace
{
using IssueCode = ScheduleImportPlanEligibilityIssueCode;
using ReviewClassAction = ScheduleImportReviewClassAction;
using ReviewIssueCode = ScheduleImportReviewDecisionIssueCode;
using ReviewTeacherAction = ScheduleImportReviewTeacherAction;
using ReviewTeacherKey = ScheduleImportReviewTeacherKey;
using Request = ScheduleImportPlanEligibilityRequest;
using EligibilityCandidateIndex =
    ScheduleImportPlanEligibilityCandidateIndex;

static_assert(std::is_same_v<
    decltype(ScheduleImportPlanEligibilityCandidate{}.teacherKey),
    std::u16string
    >);
static_assert(std::is_same_v<
    decltype(ScheduleImportPlanEligibilityCandidate{}.grade),
    std::string
    >);
static_assert(std::is_same_v<
    decltype(ScheduleImportPlanEligibilityCandidate{}.level),
    std::string
    >);
static_assert(std::is_same_v<
    decltype(ScheduleImportPlanEligibilityClassColors{}.candidateIndex),
    ScheduleImportCandidateIndex
    >);
static_assert(std::is_same_v<
    decltype(ScheduleImportPlanEligibilityIssue{}.candidateIndex),
    EligibilityCandidateIndex
    >);
static_assert(std::is_constructible_v<EligibilityCandidateIndex, std::size_t>);
static_assert(!std::is_convertible_v<std::size_t, EligibilityCandidateIndex>);
static_assert(!std::is_convertible_v<EligibilityCandidateIndex, std::size_t>);
static_assert(!std::is_convertible_v<
    ScheduleImportCandidateIndex,
    EligibilityCandidateIndex
    >);
static_assert(!std::is_convertible_v<
    EligibilityCandidateIndex,
    ScheduleImportCandidateIndex
    >);
static_assert(std::is_same_v<
    decltype(EligibilityCandidateIndex{}.rawValue()),
    std::size_t
    >);

void require(bool condition, const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

Request validRequest(
    std::string grade = "E4",
    std::string level = "Theseus",
    std::vector<std::string> weekdays = {"Monday", "Wednesday"}
    )
{
    Request request;
    request.candidates.push_back(
        ScheduleImportPlanEligibilityCandidate{
            std::move(grade),
            std::move(level),
            u"\uAE40\uC9C0\uC6D0",
            u"\uAE40\uC9C0\uC6D0",
            std::move(weekdays)
        }
        );
    request.reviewDecisions.candidates.push_back(
        {ReviewTeacherKey{"teacher-1"}, {
            ScheduleImportReviewRoomValue{"Room 101"}
        }}
        );
    request.reviewDecisions.teachers.push_back(
        {ReviewTeacherKey{"teacher-1"}, ReviewTeacherAction::Reuse, {}}
        );
    request.reviewDecisions.classes.push_back(
        {ScheduleImportCandidateIndex{0}, ReviewClassAction::CreateNew, std::nullopt}
        );
    request.classColors.push_back(
        {ScheduleImportCandidateIndex{0}, "#aabbcc", "#000000"}
        );
    return request;
}

void expectIssue(
    const std::string& name,
    const Request& request,
    IssueCode expected
    )
{
    const std::optional<ScheduleImportPlanEligibilityIssue> result =
        validateScheduleImportPlanEligibility(request);
    require(result.has_value(), name + " unexpectedly accepted");
    require(result->code == expected, name + " returned the wrong issue");
    require(
        result->candidateIndex == EligibilityCandidateIndex{},
        name + " did not retain the zero candidate position"
        );
}

void expectAccepted(const std::string& name, const Request& request)
{
    require(
        !validateScheduleImportPlanEligibility(request).has_value(),
        name + " unexpectedly rejected"
        );
}

void appendSecondCandidate(
    Request& request,
    std::vector<std::string> weekdays
    )
{
    request.candidates.push_back(
        ScheduleImportPlanEligibilityCandidate{
            "E4",
            "Theseus",
            u"\uAE40\uC9C0\uC6D0",
            u"\uAE40\uC9C0\uC6D0",
            std::move(weekdays)
        }
        );
    request.reviewDecisions.candidates.push_back(
        {ReviewTeacherKey{"teacher-1"}, {
            ScheduleImportReviewRoomValue{"Room 101"}
        }}
        );
    request.reviewDecisions.classes.push_back(
        {ScheduleImportCandidateIndex{1}, ReviewClassAction::CreateNew, std::nullopt}
        );
    request.classColors.push_back(
        {ScheduleImportCandidateIndex{1}, "#112233", "#445566"}
        );
}

void verifyValidMeetingRules()
{
    expectAccepted("paired weekdays", validRequest());
    expectAccepted(
        "paired Tuesday/Thursday",
        validRequest("E4", "Theseus", {"Tuesday", "Thursday"})
        );
    expectAccepted(
        "three-day rule",
        validRequest("E5", "Athena", {"Monday", "Wednesday", "Friday"})
        );
    expectAccepted(
        "three-day alternate rule",
        validRequest("E5", "Athena", {"Tuesday", "Thursday"})
        );
    expectAccepted(
        "single-weekday rule",
        validRequest("E6", "Helios", {"Friday"})
        );
    expectAccepted(
        "M3 Song's paired-weekday rule",
        validRequest("M3", "Song's", {"Monday", "Friday"})
        );
}

void verifyRejectionsAndSkipRules()
{
    expectIssue(
        "invalid course",
        validRequest("E4", "Not a catalog level"),
        IssueCode::InvalidCourse
        );
    expectIssue(
        "course grade case is exact",
        validRequest("e4", "Theseus"),
        IssueCode::InvalidCourse
        );
    expectIssue(
        "course level whitespace is preserved",
        validRequest("E4", "Theseus "),
        IssueCode::InvalidCourse
        );

    Request emptyTeacherKey = validRequest();
    emptyTeacherKey.candidates.front().teacherKey.clear();
    expectIssue("empty teacher key", emptyTeacherKey, IssueCode::InvalidTeacherKey);

    Request mismatchedTeacherKey = validRequest();
    mismatchedTeacherKey.candidates.front().teacherKey = u"\uAE40";
    expectIssue(
        "mismatched teacher key",
        mismatchedTeacherKey,
        IssueCode::InvalidTeacherKey
        );

    Request teacherKeyContainsNonHangul = validRequest();
    teacherKeyContainsNonHangul.candidates.front().teacherKey =
        u"\uAE40\uC9C0\uC6D0A";
    expectIssue(
        "teacher key with non-Hangul characters",
        teacherKeyContainsNonHangul,
        IssueCode::InvalidTeacherKey
        );

    expectIssue(
        "missing times",
        validRequest("E4", "Theseus", {}),
        IssueCode::MissingMeetingTimes
        );
    const auto unsupportedPattern = validateScheduleImportPlanEligibility(
        validRequest("E4", "Theseus", {"Monday", "Tuesday"})
        );
    require(
        unsupportedPattern.has_value()
            && unsupportedPattern->code == IssueCode::InvalidMeetingPattern,
        "unsupported pattern did not report a pattern issue"
        );
    require(
        unsupportedPattern->meetingPatternRuleKind
                == ClassMngr::Next::Domain::Course::
                    WeeklyMeetingDayRuleKind::PairedWeekdays
            && unsupportedPattern->meetingWeekdays
                == std::vector<ClassMngr::Next::Domain::Weekday>{
                    ClassMngr::Next::Domain::Weekday::Monday,
                    ClassMngr::Next::Domain::Weekday::Tuesday
                },
        "pattern issue did not preserve its rule and detected weekdays"
        );
    expectIssue(
        "weekday case is exact",
        validRequest("E4", "Theseus", {"monday", "Wednesday"}),
        IssueCode::InvalidMeetingWeekday
        );
    expectIssue(
        "weekday whitespace is not trimmed",
        validRequest("E4", "Theseus", {"Monday ", "Wednesday"}),
        IssueCode::InvalidMeetingWeekday
        );
    expectIssue(
        "weekend weekday is rejected",
        validRequest("E4", "Theseus", {"Saturday"}),
        IssueCode::InvalidMeetingWeekday
        );
    expectIssue(
        "duplicate weekday is rejected",
        validRequest("E4", "Theseus", {"Monday", "Monday"}),
        IssueCode::InvalidMeetingWeekday
        );

    Request invalidClassColor = validRequest();
    invalidClassColor.classColors.front().classColor = "#12345G";
    expectIssue("invalid class color", invalidClassColor, IssueCode::InvalidClassColor);

    Request invalidFontColor = validRequest();
    invalidFontColor.classColors.front().fontColor = "#ABCDEF00";
    expectIssue("invalid font color", invalidFontColor, IssueCode::InvalidClassColor);

    Request negativeClassColorIndex = validRequest();
    negativeClassColorIndex.classColors.front().candidateIndex =
        ScheduleImportCandidateIndex{-1};
    expectIssue(
        "negative class color candidate index is ignored",
        negativeClassColorIndex,
        IssueCode::InvalidClassColor
        );

    Request outOfRangeClassColorIndex = validRequest();
    outOfRangeClassColorIndex.classColors.front().candidateIndex =
        ScheduleImportCandidateIndex{1};
    expectIssue(
        "out-of-range class color candidate index is ignored",
        outOfRangeClassColorIndex,
        IssueCode::InvalidClassColor
        );

    Request laterInvalidClassColor = validRequest();
    laterInvalidClassColor.classColors.push_back(
        {ScheduleImportCandidateIndex{0}, "invalid", "#000000"}
        );
    expectIssue(
        "later class color record wins",
        laterInvalidClassColor,
        IssueCode::InvalidClassColor
        );

    Request laterValidClassColor = validRequest();
    laterValidClassColor.classColors.front().classColor = "invalid";
    laterValidClassColor.classColors.push_back(
        {ScheduleImportCandidateIndex{0}, "#123456", "#000000"}
        );
    expectAccepted("later valid class color record wins", laterValidClassColor);

    Request skipped = validRequest("E4", "Theseus", {"Monday", "Tuesday"});
    skipped.reviewDecisions.classes.front().action = ReviewClassAction::Skip;
    skipped.classColors.front().classColor = "invalid";
    skipped.classColors.front().fontColor.clear();
    expectAccepted("skip exempts only pattern and colors", skipped);

    skipped.candidates.front().weekdays.clear();
    expectIssue("skip still requires times", skipped, IssueCode::MissingMeetingTimes);
}

void verifyFirstErrorOrder()
{
    Request invalidMode = validRequest("invalid", "invalid", {});
    invalidMode.intensiveSchedule = true;
    invalidMode.intensiveMode = ScheduleImportPlanIntensiveMode::Invalid;
    invalidMode.hasDiagnostics = true;
    invalidMode.diagnosticsAcknowledged = false;
    invalidMode.reviewDecisions.teachers.front().action =
        ReviewTeacherAction::Invalid;
    expectIssue("intensive mode is first", invalidMode, IssueCode::InvalidIntensiveMode);

    Request diagnostics = validRequest("invalid", "invalid", {});
    diagnostics.hasDiagnostics = true;
    diagnostics.reviewDecisions.teachers.front().action =
        ReviewTeacherAction::Invalid;
    expectIssue(
        "diagnostics precede review and candidates",
        diagnostics,
        IssueCode::UnacknowledgedDiagnostics
        );

    Request decisions = validRequest("invalid", "invalid", {});
    decisions.reviewDecisions.teachers.front().action =
        ReviewTeacherAction::Invalid;
    const auto decisionIssue = validateScheduleImportPlanEligibility(decisions);
    require(decisionIssue.has_value(), "invalid review decision was accepted");
    require(
        decisionIssue->code == IssueCode::InvalidReviewDecision,
        "review decision did not precede candidate checks"
        );
    require(
        decisionIssue->candidateIndex == EligibilityCandidateIndex{},
        "global review issue did not retain candidate position zero"
        );
    require(
        decisionIssue->reviewDecisionIssue.has_value()
            && decisionIssue->reviewDecisionIssue->code
                == ReviewIssueCode::InvalidTeacherAction,
        "review decision detail was not preserved"
        );

    Request allCandidateBasicsBeforePatterns =
        validRequest("E4", "Theseus", {"Monday", "Tuesday"});
    allCandidateBasicsBeforePatterns.classColors.front().classColor = "invalid";
    appendSecondCandidate(allCandidateBasicsBeforePatterns, {"Monday"});
    allCandidateBasicsBeforePatterns.candidates[1].level = "invalid";
    const auto laterBasicIssue =
        validateScheduleImportPlanEligibility(allCandidateBasicsBeforePatterns);
    require(
        laterBasicIssue.has_value()
            && laterBasicIssue->code == IssueCode::InvalidCourse
            && laterBasicIssue->candidateIndex == EligibilityCandidateIndex{1},
        "a later candidate's basic check did not precede earlier pattern/color checks"
        );

    Request laterPatternIssue = validRequest();
    appendSecondCandidate(laterPatternIssue, {"Monday", "Tuesday"});
    const auto laterMeetingIssue =
        validateScheduleImportPlanEligibility(laterPatternIssue);
    require(
        laterMeetingIssue.has_value()
            && laterMeetingIssue->code == IssueCode::InvalidMeetingPattern
            && laterMeetingIssue->candidateIndex == EligibilityCandidateIndex{1},
        "a later candidate's meeting failure did not retain its position"
        );

    Request candidateBeforePattern = validRequest("invalid", "invalid", {"Monday"});
    candidateBeforePattern.classColors.front().classColor = "invalid";
    expectIssue(
        "candidate basics precede meeting pattern and color",
        candidateBeforePattern,
        IssueCode::InvalidCourse
        );

    Request patternBeforeColor =
        validRequest("E4", "Theseus", {"Monday", "Tuesday"});
    patternBeforeColor.classColors.front().classColor = "invalid";
    expectIssue(
        "meeting pattern precedes color",
        patternBeforeColor,
        IssueCode::InvalidMeetingPattern
        );

    Request candidateOrder = validRequest();
    candidateOrder.classColors.front().classColor = "invalid";
    appendSecondCandidate(candidateOrder, {"Monday"});
    const auto firstCandidateIssue =
        validateScheduleImportPlanEligibility(candidateOrder);
    require(firstCandidateIssue.has_value(), "multiple errors were accepted");
    require(
        firstCandidateIssue->code == IssueCode::InvalidClassColor
            && firstCandidateIssue->candidateIndex == EligibilityCandidateIndex{0},
        "candidate order did not determine the first post-basic failure"
        );
}

void verifyEligibilityCandidateIndexValueType()
{
    const EligibilityCandidateIndex defaultIndex;
    require(defaultIndex.rawValue() == 0, "candidate index did not default to zero");
    require(
        EligibilityCandidateIndex{17}.rawValue() == 17,
        "candidate index did not preserve its explicit value"
        );
    require(
        EligibilityCandidateIndex{17} == EligibilityCandidateIndex{17},
        "equal candidate indices did not compare equal"
        );
    require(
        EligibilityCandidateIndex{17} != EligibilityCandidateIndex{18},
        "different candidate indices compared equal"
        );
}

void verifyIntensiveAndColorNormalization()
{
    Request intensive = validRequest();
    intensive.intensiveSchedule = true;
    intensive.intensiveMode = ScheduleImportPlanIntensiveMode::UpdateExisting;
    expectAccepted("update intensive mode", intensive);

    intensive.intensiveMode = ScheduleImportPlanIntensiveMode::ReplaceWithNew;
    expectAccepted("replace intensive mode", intensive);

    Request regular = validRequest();
    regular.intensiveMode = ScheduleImportPlanIntensiveMode::Invalid;
    expectAccepted("regular import ignores intensive mode", regular);

    Request lowerCaseColors = validRequest();
    lowerCaseColors.classColors.front().classColor = "#abcdef";
    lowerCaseColors.classColors.front().fontColor = "#ABCDEF";
    expectAccepted("hex colors accept mixed case", lowerCaseColors);
}
}

int main()
{
    try
    {
        verifyEligibilityCandidateIndexValueType();
        verifyValidMeetingRules();
        verifyRejectionsAndSkipRules();
        verifyFirstErrorOrder();
        verifyIntensiveAndColorNormalization();
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
