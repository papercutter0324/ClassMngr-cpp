#pragma once

#include "next/application/class_summary_projection.h"
#include "next/application/schedule_view_projection.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

// Application-owned weekday values keep the Sub Prep request independent of
// Qt and of the localized day labels used by the current page.
enum class SubPrepWeekday : std::uint8_t
{
    Monday = 1,
    Tuesday,
    Wednesday,
    Thursday,
    Friday,
    Saturday,
    Sunday
};

inline constexpr std::size_t kSubPrepScheduleScopeMaxVisibleClasses =
    kClassSummaryMaxEntries;

struct SubPrepScheduleScopeRequest final
{
    std::vector<Domain::ClassId> visibleClassIds;
    std::vector<SubPrepWeekday> selectedDays;
    ScheduleViewMode mode = ScheduleViewMode::Regular;

    friend bool operator==(
        const SubPrepScheduleScopeRequest&,
        const SubPrepScheduleScopeRequest&
        ) = default;
};

using SubPrepScheduleSummaryReadResult =
    Domain::Result<ClassSummaryProjectionInput>;

// The read owner filters by the full schedule scope before loading class
// records and returns only copied summary values. It must not retain request
// references after this call returns.
class SubPrepScheduleSummaryReadPort
{
public:
    virtual ~SubPrepScheduleSummaryReadPort() = default;

    [[nodiscard]] virtual SubPrepScheduleSummaryReadResult loadSummaries(
        const SubPrepScheduleScopeRequest& request
        ) = 0;
};

using SubPrepScheduleSummaryQueryResult =
    Domain::Result<ClassSummaryProjection>;

// This operation owns no cache or UI state. A successful result is a complete
// bounded projection for the request; errors never contain partial rows.
class SubPrepScheduleSummaryQuery final
{
public:
    explicit SubPrepScheduleSummaryQuery(
        SubPrepScheduleSummaryReadPort& readPort
        ) noexcept
        : m_readPort(readPort)
    {
    }

    [[nodiscard]] SubPrepScheduleSummaryQueryResult execute(
        const SubPrepScheduleScopeRequest& request
        ) const
    {
        const auto requestValidation = validateRequest(request);
        if (!requestValidation)
        {
            return SubPrepScheduleSummaryQueryResult::failure(
                requestValidation.error()
                );
        }

        // Empty visibility or an empty day selection is a valid empty view and
        // must not force an adapter/database read.
        if (request.visibleClassIds.empty() || request.selectedDays.empty())
        {
            return ClassSummaryProjection::create({});
        }

        auto source = m_readPort.loadSummaries(request);
        if (!source)
        {
            return SubPrepScheduleSummaryQueryResult::failure(source.error());
        }

        auto input = std::move(source.value());
        if (input.selectedDetails.has_value())
        {
            return SubPrepScheduleSummaryQueryResult::failure(
                validationError(
                    "A schedule summary read must not return selected details."
                )
            );
        }

        const auto projectionValidation =
            ClassSummaryProjection::validate(input);
        if (!projectionValidation)
        {
            return SubPrepScheduleSummaryQueryResult::failure(
                projectionValidation.error()
                );
        }

        const std::set<Domain::ClassId> visibleIds(
            request.visibleClassIds.cbegin(),
            request.visibleClassIds.cend()
            );
        std::set<Domain::TeacherId> referencedTeacherIds;
        for (const auto& classSummary : input.classes)
        {
            if (!visibleIds.contains(classSummary.id))
            {
                return SubPrepScheduleSummaryQueryResult::failure(
                    validationError(
                        "A schedule summary read returned a class outside the requested scope."
                        )
                    );
            }

            if (classSummary.teacherId.has_value())
            {
                referencedTeacherIds.insert(*classSummary.teacherId);
            }
        }

        for (const auto& teacher : input.teachers)
        {
            if (!referencedTeacherIds.contains(teacher.id))
            {
                return SubPrepScheduleSummaryQueryResult::failure(
                    validationError(
                        "A schedule summary read returned an unused teacher summary."
                        )
                    );
            }
        }

        // Canonical tie-breakers make output stable even when the read source
        // provides equal display-order values or returns rows in a different
        // order between calls.
        std::sort(
            input.classes.begin(),
            input.classes.end(),
            [](const ClassSummary& left, const ClassSummary& right)
            {
                if (left.order != right.order)
                {
                    return left.order < right.order;
                }

                return left.id < right.id;
            }
            );
        std::sort(
            input.teachers.begin(),
            input.teachers.end(),
            [](const TeacherSummary& left, const TeacherSummary& right)
            {
                return left.id < right.id;
            }
            );

        return ClassSummaryProjection::create(std::move(input));
    }

private:
    [[nodiscard]] static Domain::OperationError invalidInput(
        const char* message
        )
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::InvalidInput,
            .message = message,
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::OperationError validationError(
        const char* message
        )
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Validation,
            .message = message,
            .recoverable = false
        };
    }

    [[nodiscard]] static bool isValidIdentifier(
        const std::string_view value
        ) noexcept
    {
        if (value.empty() || value.size() > kSummaryMaxIdentifierLength)
        {
            return false;
        }

        return !std::all_of(
            value.cbegin(),
            value.cend(),
            [](const char character)
            {
                return std::isspace(
                    static_cast<unsigned char>(character)
                    ) != 0;
            }
            );
    }

    [[nodiscard]] static Domain::Result<void> validateRequest(
        const SubPrepScheduleScopeRequest& request
        )
    {
        if (request.visibleClassIds.size()
            > kSubPrepScheduleScopeMaxVisibleClasses)
        {
            return Domain::Result<void>::failure(
                invalidInput(
                    "Visible Sub Prep class scope exceeds its bounded limit."
                    )
                );
        }

        std::set<Domain::ClassId> classIds;
        for (const auto& classId : request.visibleClassIds)
        {
            if (!isValidIdentifier(classId.value()))
            {
                return Domain::Result<void>::failure(
                    invalidInput(
                        "Visible class identifiers must be non-blank and bounded."
                        )
                    );
            }

            if (!classIds.insert(classId).second)
            {
                return Domain::Result<void>::failure(
                    invalidInput(
                        "Visible class identifiers must be unique."
                        )
                    );
            }
        }

        if (request.selectedDays.size() > 7)
        {
            return Domain::Result<void>::failure(
                invalidInput("Selected Sub Prep days exceed the weekly limit.")
                );
        }

        std::set<SubPrepWeekday> selectedDays;
        for (const auto day : request.selectedDays)
        {
            const auto dayValue = static_cast<std::uint8_t>(day);
            if (dayValue < static_cast<std::uint8_t>(SubPrepWeekday::Monday)
                || dayValue > static_cast<std::uint8_t>(SubPrepWeekday::Sunday))
            {
                return Domain::Result<void>::failure(
                    invalidInput("A selected Sub Prep weekday is invalid.")
                    );
            }

            if (!selectedDays.insert(day).second)
            {
                return Domain::Result<void>::failure(
                    invalidInput("Selected Sub Prep weekdays must be unique.")
                    );
            }
        }

        if (request.mode != ScheduleViewMode::Regular
            && request.mode != ScheduleViewMode::Intensive)
        {
            return Domain::Result<void>::failure(
                invalidInput("The Sub Prep schedule mode is invalid.")
                );
        }

        return Domain::Result<void>::success();
    }

    SubPrepScheduleSummaryReadPort& m_readPort;
};

} // namespace ClassMngr::Next::Application
