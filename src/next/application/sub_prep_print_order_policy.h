#pragma once

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <functional>
#include <limits>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

#include "next/domain/domain_types.h"

namespace ClassMngr::Next::Application
{

inline constexpr int kSubPrepPrintUnknownOrder =
    std::numeric_limits<int>::max();

class SubPrepPrintWeekdayOrder final
{
public:
    constexpr SubPrepPrintWeekdayOrder() noexcept = default;
    explicit constexpr SubPrepPrintWeekdayOrder(const int value) noexcept
        : value_(value)
    {
    }

    [[nodiscard]] constexpr int value() const noexcept
    {
        return value_;
    }

    friend constexpr bool operator==(
        const SubPrepPrintWeekdayOrder&,
        const SubPrepPrintWeekdayOrder&
        ) = default;

private:
    int value_ = 0;
};

class SubPrepPrintStartMinute final
{
public:
    constexpr SubPrepPrintStartMinute() noexcept = default;
    explicit constexpr SubPrepPrintStartMinute(const int value) noexcept
        : value_(value)
    {
    }

    [[nodiscard]] constexpr int value() const noexcept
    {
        return value_;
    }

    friend constexpr bool operator==(
        const SubPrepPrintStartMinute&,
        const SubPrepPrintStartMinute&
        ) = default;

private:
    int value_ = 0;
};

enum class SubPrepPrintTeacherNameField
{
    English,
    Korean
};

// These facts contain ordering keys only. The caller keeps teacher display
// text and supplies its locale-aware comparison through the callback.
struct SubPrepPrintTeacherOrderFact final
{
    Domain::TeacherId teacherId = *Domain::TeacherId::fromString("0");
    bool hasEnglishName = false;
};

struct SubPrepPrintClassOrderFact final
{
    Domain::TeacherId teacherId = *Domain::TeacherId::fromString("0");
    int gradeOrder = kSubPrepPrintUnknownOrder;
    int levelOrder = kSubPrepPrintUnknownOrder;
    SubPrepPrintWeekdayOrder weekdayOrder{kSubPrepPrintUnknownOrder};
    SubPrepPrintStartMinute startMinute{kSubPrepPrintUnknownOrder};
    Domain::ClassId classId = *Domain::ClassId::fromString("0");
};

namespace SubPrepPrintOrderPolicyDetail
{

template <typename TypedId>
[[nodiscard]] inline bool tryParseNumericId(
    const TypedId& id,
    int& numericId
    ) noexcept
{
    const std::string& value = id.value();
    const char* const begin = value.data();
    const char* const end = begin + value.size();
    const auto parsed = std::from_chars(begin, end, numericId);
    return parsed.ec == std::errc{} && parsed.ptr == end;
}

template <typename TypedId>
[[nodiscard]] inline bool numericIdLess(
    const TypedId& left,
    const TypedId& right
    ) noexcept
{
    int leftNumericId = 0;
    int rightNumericId = 0;
    const bool leftIsNumeric = tryParseNumericId(left, leftNumericId);
    const bool rightIsNumeric = tryParseNumericId(right, rightNumericId);

    if (leftIsNumeric && rightIsNumeric)
    {
        if (leftNumericId != rightNumericId)
        {
            return leftNumericId < rightNumericId;
        }
        return left.value() < right.value();
    }
    if (leftIsNumeric != rightIsNumeric)
    {
        return leftIsNumeric;
    }
    return left.value() < right.value();
}

} // namespace SubPrepPrintOrderPolicyDetail

using SubPrepPrintTeacherNameComparator = std::function<int(
    std::size_t,
    std::size_t,
    SubPrepPrintTeacherNameField
    )>;

struct SubPrepPrintGroupOrder final
{
    std::size_t teacherIndex = 0;
    std::vector<std::size_t> classIndexes;

    friend bool operator==(
        const SubPrepPrintGroupOrder&,
        const SubPrepPrintGroupOrder&
        ) = default;
};

// compareTeacherNames must compare the caller's prepared, trimmed names and
// return a negative, zero, or positive result. Returns teacher fact indexes
// and, for each teacher, ordered class fact indexes. Teacher names remain
// caller-owned; indexes let the injected comparator inspect those names
// without copying display text into this policy.
[[nodiscard]] inline std::vector<SubPrepPrintGroupOrder>
orderSubPrepPrintGroups(
    const std::vector<SubPrepPrintTeacherOrderFact>& teachers,
    const std::vector<SubPrepPrintClassOrderFact>& classes,
    const SubPrepPrintTeacherNameComparator& compareTeacherNames
    )
{
    std::unordered_map<std::string, std::size_t> teacherIndexesById;
    teacherIndexesById.reserve(teachers.size());
    for (std::size_t index = 0; index < teachers.size(); ++index)
    {
        teacherIndexesById.emplace(teachers[index].teacherId.value(), index);
    }

    std::vector<SubPrepPrintGroupOrder> groups;
    groups.reserve(teachers.size());
    for (std::size_t index = 0; index < teachers.size(); ++index)
    {
        groups.push_back({index, {}});
    }

    for (std::size_t classIndex = 0; classIndex < classes.size(); ++classIndex)
    {
        const auto teacher = teacherIndexesById.find(
            classes[classIndex].teacherId.value()
            );
        if (teacher == teacherIndexesById.end())
        {
            continue;
        }
        groups[teacher->second].classIndexes.push_back(classIndex);
    }

    std::sort(
        groups.begin(),
        groups.end(),
        [&teachers, &compareTeacherNames](
            const SubPrepPrintGroupOrder& leftGroup,
            const SubPrepPrintGroupOrder& rightGroup
        )
        {
            const SubPrepPrintTeacherOrderFact& left =
                teachers[leftGroup.teacherIndex];
            const SubPrepPrintTeacherOrderFact& right =
                teachers[rightGroup.teacherIndex];

            if (left.hasEnglishName != right.hasEnglishName)
            {
                return left.hasEnglishName;
            }

            const int englishComparison = compareTeacherNames(
                leftGroup.teacherIndex,
                rightGroup.teacherIndex,
                SubPrepPrintTeacherNameField::English
                );
            if (englishComparison != 0)
            {
                return englishComparison < 0;
            }

            const int koreanComparison = compareTeacherNames(
                leftGroup.teacherIndex,
                rightGroup.teacherIndex,
                SubPrepPrintTeacherNameField::Korean
                );
            if (koreanComparison != 0)
            {
                return koreanComparison < 0;
            }

            return SubPrepPrintOrderPolicyDetail::numericIdLess(
                left.teacherId,
                right.teacherId
                );
        }
        );

    for (SubPrepPrintGroupOrder& group : groups)
    {
        std::sort(
            group.classIndexes.begin(),
            group.classIndexes.end(),
            [&classes](const std::size_t leftIndex, const std::size_t rightIndex)
            {
                const SubPrepPrintClassOrderFact& left = classes[leftIndex];
                const SubPrepPrintClassOrderFact& right = classes[rightIndex];
                if (left.gradeOrder != right.gradeOrder)
                {
                    return left.gradeOrder < right.gradeOrder;
                }
                if (left.levelOrder != right.levelOrder)
                {
                    return left.levelOrder < right.levelOrder;
                }
                if (
                    left.weekdayOrder.value()
                    != right.weekdayOrder.value()
                    )
                {
                    return left.weekdayOrder.value()
                        < right.weekdayOrder.value();
                }
                if (
                    left.startMinute.value()
                    != right.startMinute.value()
                    )
                {
                    return left.startMinute.value()
                        < right.startMinute.value();
                }
                return SubPrepPrintOrderPolicyDetail::numericIdLess(
                    left.classId,
                    right.classId
                    );
            }
            );
    }

    return groups;
}

} // namespace ClassMngr::Next::Application
