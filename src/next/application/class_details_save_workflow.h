#pragma once

#include "next/application/class_details_save_use_case.h"
#include "next/application/class_details_schedule_conflict_query.h"
#include "next/application/class_details_validation_context_query.h"
#include "next/application/class_details_validation_policy.h"

#include <array>
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
        const ClassDetailsSavePort& savePort
        )
    {
        ClassDetailsValidationInput input = request.input;
        const auto classId = *Domain::ClassId::fromString(std::to_string(input.classId));
        const auto context = ClassDetailsValidationContextQuery::execute(classId, contextPort);
        // The form historically continued with default hidden fields if the
        // fresh context was unavailable or belonged to another class.
        input.teacherId = context ? context.value().teacherId : -1;
        input.notes = context ? contextText(context.value().notes) : std::u16string{};
        input.timeFillerActivities = context
            ? contextText(context.value().timeFillerActivities) : std::u16string{};

        auto validation = ClassDetailsValidationPolicy::normalizeAndValidate(input, catalog);
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

    [[nodiscard]] static std::optional<int> weekday(std::u16string_view text)
    {
        constexpr std::array names{u"Monday", u"Tuesday", u"Wednesday", u"Thursday",
            u"Friday", u"Saturday", u"Sunday"};
        auto trimmed = trimQtWhitespace(text);
        for (auto& unit : trimmed)
            if (unit >= u'A' && unit <= u'Z')
                unit = static_cast<char16_t>(unit + u'a' - u'A');
        for (std::size_t index = 0; index < names.size(); ++index)
        {
            std::u16string name(names[index]);
            name[0] = static_cast<char16_t>(name[0] + u'a' - u'A');
            if (trimmed == name)
                return static_cast<int>(index);
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
            const auto time = Domain::ScheduleTime::fromMinutes(*day, *start, *end);
            if (!time)
                return std::nullopt;
            result.push_back(*time);
        }
        return result;
    }
};

} // namespace ClassMngr::Next::Application
