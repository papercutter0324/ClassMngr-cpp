#pragma once

#include "domain/rules/schedule_key_rules.h"
#include "next/application/class_details_save_use_case.h"
#include "next/application/class_details_schedule_conflict_query.h"
#include "next/application/class_details_validation_context_query.h"
#include "next/application/class_details_validation_policy.h"
#include "next/application/positive_decimal_id_validation.h"
#include "next/domain/schedule_time.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace ClassMngr::Next::Application
{

struct ClassDetailsSaveWorkflowRequest final
{
    ClassDetailsValidationInput input;
    bool saveRegularTimes = true;
    bool saveIntensiveTimes = true;
};

struct ClassDetailsSaveWorkflowSuccess final
{
    ClassDetailsValidationInput normalized;
};

struct ClassDetailsSaveWorkflowConflict final
{
    ClassDetailsScheduleMode mode;
    std::vector<ClassDetailsScheduleConflict> conflicts;
};

enum class ClassDetailsSaveWorkflowStage
{
    ValidationContext,
    RegularConflicts,
    IntensiveConflicts,
    Save
};

struct ClassDetailsSaveWorkflowFailure final
{
    ClassDetailsSaveWorkflowStage stage;
    Domain::OperationError error;
};

using ClassDetailsSaveWorkflowResult = std::variant<
    ClassDetailsSaveWorkflowSuccess,
    ClassDetailsValidationOutput,
    ClassDetailsSaveWorkflowConflict,
    ClassDetailsSaveWorkflowFailure
    >;

class ClassDetailsSaveWorkflow final
{
public:
    [[nodiscard]] static ClassDetailsSaveWorkflowResult execute(
        const ClassDetailsSaveWorkflowRequest& request,
        const ClassDetailsValidationCatalog& catalog,
        const ClassDetailsValidationContextPort& contextPort,
        const ClassDetailsScheduleConflictPort& conflictPort,
        const ClassDetailsSavePort& savePort,
        const ClassDetailsCatalogCaseInsensitiveEquals& catalogEquals
        )
    {
        ClassDetailsValidationInput input = request.input;
        const Domain::ClassId& classId = input.classId;
        if (!isCanonicalPositiveDecimalId(classId.value()))
        {
            input.teacherId = Domain::TeacherId::fromString("-1").value();
            input.notes.clear();
            input.timeFillerActivities.clear();
            return ClassDetailsValidationPolicy::normalizeAndValidate(
                input, catalog, catalogEquals);
        }

        const auto context = ClassDetailsValidationContextQuery::execute(classId, contextPort);
        if (!context)
            return ClassDetailsSaveWorkflowFailure{
                ClassDetailsSaveWorkflowStage::ValidationContext, context.error()};

        const ClassDetailsValidationContextTeacherId teacherId = context.value().teacherId;
        input.teacherId = Domain::TeacherId::fromString(
            std::to_string(teacherId.value())
            ).value();
        input.notes = contextText(context.value().notes);
        input.timeFillerActivities = contextText(context.value().timeFillerActivities);

        auto validation = ClassDetailsValidationPolicy::normalizeAndValidate(
            input, catalog, catalogEquals);
        if (validation.hasErrors())
            return validation;

        for (const auto mode : {ClassDetailsScheduleMode::Regular,
                                ClassDetailsScheduleMode::Intensive})
        {
            const auto stage = mode == ClassDetailsScheduleMode::Regular
                ? ClassDetailsSaveWorkflowStage::RegularConflicts
                : ClassDetailsSaveWorkflowStage::IntensiveConflicts;
            // Conflict conversion deliberately uses the original schedule text.
            // Policy normalization accepts additional spellings that this
            // strict legacy check does not accept.
            const auto times = scheduleTimes(mode == ClassDetailsScheduleMode::Regular
                ? input.regularTimes : input.intensiveTimes);
            if (!times)
                return ClassDetailsSaveWorkflowFailure{stage, {
                    .code = Domain::ErrorCode::Validation,
                    .message = "Class schedule values could not be checked.",
                    .recoverable = true}};
            const auto conflicts = ClassDetailsScheduleConflictQuery::execute(
                {.classId = classId, .mode = mode, .candidateTimes = *times}, conflictPort);
            if (!conflicts)
                return ClassDetailsSaveWorkflowFailure{stage, conflicts.error()};
            if (!conflicts.value().empty())
                return ClassDetailsSaveWorkflowConflict{mode, conflicts.value()};
        }

        const auto& normalized = validation.normalized;
        const auto regular = scheduleTimes(normalized.regularTimes);
        const auto intensive = scheduleTimes(normalized.intensiveTimes);
        if (!regular || !intensive)
            return ClassDetailsSaveWorkflowFailure{ClassDetailsSaveWorkflowStage::Save, {
                .code = Domain::ErrorCode::Validation,
                .message = "Class schedule values could not be converted.",
                .recoverable = true}};
        const auto saved = ClassDetailsSaveUseCase::execute({
            .classId = classId,
            .classGrade = normalized.classGrade,
            .classLevel = normalized.classLevel,
            .readingBook = normalized.readingBook,
            .essayBook = normalized.essayBook,
            .classColor = normalized.classColor,
            .fontColor = normalized.fontColor,
            .regularTimes = request.saveRegularTimes ? regular : std::nullopt,
            .intensiveTimes = request.saveIntensiveTimes ? intensive : std::nullopt
        }, savePort);
        if (!saved)
            return ClassDetailsSaveWorkflowFailure{ClassDetailsSaveWorkflowStage::Save, saved.error()};
        return ClassDetailsSaveWorkflowSuccess{std::move(validation.normalized)};
    }

private:
    [[nodiscard]] static std::u16string contextText(std::u16string_view text)
    {
        // Preserve the page's former QString::fromStdU16String context decoding.
        if (text.empty())
            return {};
        const bool swap = text.front() == 0xfffe;
        if (swap || text.front() == 0xfeff)
            text.remove_prefix(1);
        std::u16string result(text);
        if (swap)
            for (auto& unit : result)
                unit = static_cast<char16_t>((unit >> 8) | (unit << 8));
        return result;
    }

    [[nodiscard]] static std::optional<int> timeMinutes(std::u16string_view text)
    {
        const auto colon = text.find(u':');
        if ((colon != 1 && colon != 2) || text.size() != colon + 6
            || (colon == 2 && text[0] != u'1')
            || text[colon + 1] < u'0' || text[colon + 1] > u'5'
            || text[colon + 2] < u'0' || text[colon + 2] > u'9'
            || text[colon + 3] != u' '
            || (text.substr(colon + 4) != u"AM" && text.substr(colon + 4) != u"PM"))
            return std::nullopt;
        int hour = 0;
        for (std::size_t index = 0; index < colon; ++index)
        {
            if (text[index] < u'0' || text[index] > u'9')
                return std::nullopt;
            hour = hour * 10 + static_cast<int>(text[index] - u'0');
        }
        if (hour < 1 || hour > 12)
            return std::nullopt;
        return (hour % 12 + (text.substr(colon + 4) == u"PM" ? 12 : 0)) * 60
            + static_cast<int>(text[colon + 1] - u'0') * 10
            + static_cast<int>(text[colon + 2] - u'0');
    }

    [[nodiscard]] static std::optional<Domain::Weekday> weekday(
        std::u16string_view text
        )
    {
        const std::u16string trimmed = trimQtWhitespace(text);
        const auto parsed = ScheduleKeyRules::parseWeekday(
            trimmed,
            [](std::u16string_view left, std::u16string_view right)
            {
                if (left.size() != right.size())
                    return false;
                const auto lowercaseAscii = [](char16_t value)
                {
                    return value >= u'A' && value <= u'Z'
                        ? static_cast<char16_t>(value + u'a' - u'A') : value;
                };
                for (std::size_t index = 0; index < left.size(); ++index)
                {
                    if (lowercaseAscii(left[index]) != lowercaseAscii(right[index]))
                        return false;
                }
                return true;
            }
            );
        if (!parsed)
            return std::nullopt;

        switch (parsed->value)
        {
        case ScheduleKeyRules::Weekday::Monday:
            return Domain::Weekday::Monday;
        case ScheduleKeyRules::Weekday::Tuesday:
            return Domain::Weekday::Tuesday;
        case ScheduleKeyRules::Weekday::Wednesday:
            return Domain::Weekday::Wednesday;
        case ScheduleKeyRules::Weekday::Thursday:
            return Domain::Weekday::Thursday;
        case ScheduleKeyRules::Weekday::Friday:
            return Domain::Weekday::Friday;
        case ScheduleKeyRules::Weekday::Saturday:
            return Domain::Weekday::Saturday;
        case ScheduleKeyRules::Weekday::Sunday:
            return Domain::Weekday::Sunday;
        }

        return std::nullopt;
    }

    [[nodiscard]] static std::optional<std::vector<Domain::ScheduleTime>> scheduleTimes(
        const std::vector<ClassDetailsScheduleValidationRow>& rows)
    {
        std::vector<Domain::ScheduleTime> result;
        result.reserve(rows.size());
        for (const auto& row : rows)
        {
            const auto day = weekday(row.day);
            const auto start = timeMinutes(row.startTime);
            const auto end = timeMinutes(row.endTime);
            if (!day || !start || !end)
                return std::nullopt;
            const auto time = Domain::ScheduleTime::fromMinutes(
                *day,
                Domain::ScheduleTime::StartMinute{*start},
                Domain::ScheduleTime::EndMinute{*end}
                );
            if (!time)
                return std::nullopt;
            result.push_back(*time);
        }
        return result;
    }
};

} // namespace ClassMngr::Next::Application
