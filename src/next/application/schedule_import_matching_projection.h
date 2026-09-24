#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

enum class ScheduleImportMatchingKind
{
    Normal,
    Intensive
};

enum class ScheduleImportMatchingConfidence
{
    None,
    Possible,
    Confident
};

enum class ScheduleImportMatchingExplanation
{
    None,
    Exact,
    PossibleWithTargetHours,
    PossibleWithOtherHours,
    PossibleWithoutHours
};

struct ScheduleImportMatchingTime final
{
    std::string day;

    friend bool operator==(
        const ScheduleImportMatchingTime&,
        const ScheduleImportMatchingTime&
        ) = default;
};

struct ScheduleImportMatchingCandidate final
{
    std::u16string teacherKey;
    std::u16string teacherName;
    std::vector<std::u16string> rooms;
    // Adapters supply QString::simplified().toCaseFolded() comparison keys;
    // raw labels stay at the platform edge for display and persistence.
    std::vector<std::u16string> roomMatchKeys;
    std::u16string gradeMatchKey;
    std::u16string levelMatchKey;
    std::vector<ScheduleImportMatchingTime> times;

    friend bool operator==(
        const ScheduleImportMatchingCandidate&,
        const ScheduleImportMatchingCandidate&
        ) = default;
};

struct ScheduleImportMatchingTeacher final
{
    std::int32_t id = -1;
    std::u16string koreanName;

    friend bool operator==(
        const ScheduleImportMatchingTeacher&,
        const ScheduleImportMatchingTeacher&
        ) = default;
};

struct ScheduleImportMatchingClass final
{
    std::int32_t id = -1;
    std::int32_t teacherId = -1;
    std::u16string roomMatchKey;
    std::u16string gradeMatchKey;
    std::u16string levelMatchKey;
    std::vector<ScheduleImportMatchingTime> regularTimes;
    std::vector<ScheduleImportMatchingTime> intensiveTimes;

    friend bool operator==(
        const ScheduleImportMatchingClass&,
        const ScheduleImportMatchingClass&
        ) = default;
};

struct ScheduleImportMatchingInput final
{
    ScheduleImportMatchingKind kind = ScheduleImportMatchingKind::Normal;
    std::vector<ScheduleImportMatchingCandidate> candidates;
    std::vector<ScheduleImportMatchingTeacher> teachers;
    std::vector<ScheduleImportMatchingClass> classes;

    friend bool operator==(
        const ScheduleImportMatchingInput&,
        const ScheduleImportMatchingInput&
        ) = default;
};

struct ScheduleImportMatchingTeacherProjection final
{
    std::u16string teacherKey;
    std::u16string teacherName;
    std::vector<std::u16string> importedRooms;
    std::vector<std::int32_t> matchingTeacherIds;
    std::size_t affectedClassCount = 0;

    friend bool operator==(
        const ScheduleImportMatchingTeacherProjection&,
        const ScheduleImportMatchingTeacherProjection&
        ) = default;
};

struct ScheduleImportMatchingClassProjection final
{
    std::size_t candidateIndex = 0;
    std::vector<std::int32_t> matchingClassIds;
    std::int32_t suggestedClassId = -1;
    bool exactMatch = false;
    ScheduleImportMatchingConfidence confidence =
        ScheduleImportMatchingConfidence::None;
    ScheduleImportMatchingExplanation explanation =
        ScheduleImportMatchingExplanation::None;

    friend bool operator==(
        const ScheduleImportMatchingClassProjection&,
        const ScheduleImportMatchingClassProjection&
        ) = default;
};

struct ScheduleImportMatchingInventory final
{
    std::size_t classCount = 0;
    bool hasRegularHours = false;
    bool hasIntensiveHours = false;

    friend bool operator==(
        const ScheduleImportMatchingInventory&,
        const ScheduleImportMatchingInventory&
        ) = default;
};

struct ScheduleImportMatchingProjection final
{
    ScheduleImportMatchingKind kind = ScheduleImportMatchingKind::Normal;
    ScheduleImportMatchingInventory inventory;
    std::vector<ScheduleImportMatchingTeacherProjection> teachers;
    std::vector<ScheduleImportMatchingClassProjection> classes;
    std::vector<std::int32_t> initiallyAbsentClassIds;

    friend bool operator==(
        const ScheduleImportMatchingProjection&,
        const ScheduleImportMatchingProjection&
        ) = default;
};

namespace ScheduleImportMatchingProjectionDetail
{

[[nodiscard]] inline std::u16string hangulOnly(
    const std::u16string_view value
    )
{
    std::u16string result;
    result.reserve(value.size());
    for (const char16_t character : value)
    {
        if (
            (character >= 0x1100 && character <= 0x11ff)
            || (character >= 0x3130 && character <= 0x318f)
            || (character >= 0xa960 && character <= 0xa97f)
            || (character >= 0xac00 && character <= 0xd7af)
            || (character >= 0xd7b0 && character <= 0xd7ff)
            )
        {
            result.push_back(character);
        }
    }
    return result;
}

[[nodiscard]] inline int dayGroup(
    const std::vector<ScheduleImportMatchingTime>& times
    )
{
    int group = 0;
    for (const ScheduleImportMatchingTime& time : times)
    {
        std::size_t first = 0;
        while (
            first < time.day.size()
            && (time.day[first] == ' ' || time.day[first] == '\t'
                || time.day[first] == '\n' || time.day[first] == '\r'
                || time.day[first] == '\f' || time.day[first] == '\v')
            )
        {
            ++first;
        }
        std::size_t last = time.day.size();
        while (
            last > first
            && (time.day[last - 1] == ' ' || time.day[last - 1] == '\t'
                || time.day[last - 1] == '\n' || time.day[last - 1] == '\r'
                || time.day[last - 1] == '\f' || time.day[last - 1] == '\v')
            )
        {
            --last;
        }
        std::string day = time.day.substr(first, last - first);
        std::transform(
            day.begin(),
            day.end(),
            day.begin(),
            [](const char character)
            {
                const unsigned char byte =
                    static_cast<unsigned char>(character);
                return byte >= 'A' && byte <= 'Z'
                    ? static_cast<char>(byte - 'A' + 'a')
                    : character;
            }
            );
        const int timeGroup =
            day == "monday" || day == "wednesday" || day == "friday"
            ? 1
            : day == "tuesday" || day == "thursday"
                ? 2
                : 0;
        if (timeGroup == 0 || (group != 0 && group != timeGroup))
        {
            return 0;
        }
        group = timeGroup;
    }
    return group;
}

[[nodiscard]] inline std::vector<std::string> meetingDays(
    const std::vector<ScheduleImportMatchingTime>& times
    )
{
    std::vector<std::string> result;
    for (const ScheduleImportMatchingTime& time : times)
    {
        const std::size_t first = time.day.find_first_not_of(" \t\n\r\f\v");
        const std::size_t last = time.day.find_last_not_of(" \t\n\r\f\v");
        std::string day = first == std::string::npos
            ? std::string{}
            : time.day.substr(first, last - first + 1);
        std::transform(
            day.begin(),
            day.end(),
            day.begin(),
            [](const char character)
            {
                const unsigned char byte =
                    static_cast<unsigned char>(character);
                return byte >= 'A' && byte <= 'Z'
                    ? static_cast<char>(byte - 'A' + 'a')
                    : character;
            }
            );
        if (
            !day.empty()
            && std::find(result.cbegin(), result.cend(), day) == result.cend()
            )
        {
            result.push_back(std::move(day));
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

[[nodiscard]] inline bool meetingDaysMatch(
    const std::vector<ScheduleImportMatchingTime>& imported,
    const std::vector<ScheduleImportMatchingTime>& existing
    )
{
    const int importedGroup = dayGroup(imported);
    return importedGroup != 0
        && importedGroup == dayGroup(existing)
        && meetingDays(imported) == meetingDays(existing);
}

[[nodiscard]] inline const std::vector<ScheduleImportMatchingTime>&
timesForKind(
    const ScheduleImportMatchingClass& value,
    const ScheduleImportMatchingKind kind
    )
{
    const auto& preferred =
        kind == ScheduleImportMatchingKind::Intensive
            ? value.intensiveTimes
            : value.regularTimes;
    if (!preferred.empty())
    {
        return preferred;
    }
    return kind == ScheduleImportMatchingKind::Intensive
        ? value.regularTimes
        : value.intensiveTimes;
}

[[nodiscard]] inline const std::vector<ScheduleImportMatchingTime>&
targetTimesForKind(
    const ScheduleImportMatchingClass& value,
    const ScheduleImportMatchingKind kind
    )
{
    return kind == ScheduleImportMatchingKind::Intensive
        ? value.intensiveTimes
        : value.regularTimes;
}

[[nodiscard]] inline bool isEligible(
    const ScheduleImportMatchingCandidate& candidate,
    const ScheduleImportMatchingClass& existing,
    const ScheduleImportMatchingKind kind
    )
{
    const auto existingTimes = timesForKind(existing, kind);
    return candidate.gradeMatchKey == existing.gradeMatchKey
        && candidate.levelMatchKey == existing.levelMatchKey
        && (
            existingTimes.empty()
            || dayGroup(candidate.times) != 0
                && dayGroup(candidate.times) == dayGroup(existingTimes)
            );
}

template <typename T>
inline void appendUnique(std::vector<T>& values, const T value)
{
    if (
        value > 0
        && std::find(values.cbegin(), values.cend(), value) == values.cend()
        )
    {
        values.push_back(value);
    }
}

} // namespace ScheduleImportMatchingProjectionDetail

[[nodiscard]] inline ScheduleImportMatchingProjection
projectScheduleImportMatching(
    const ScheduleImportMatchingInput& input
    )
{
    using namespace ScheduleImportMatchingProjectionDetail;

    ScheduleImportMatchingProjection result;
    result.kind = input.kind;
    result.inventory.classCount = input.classes.size();
    for (const ScheduleImportMatchingClass& value : input.classes)
    {
        result.inventory.hasRegularHours =
            result.inventory.hasRegularHours || !value.regularTimes.empty();
        result.inventory.hasIntensiveHours = result.inventory.hasIntensiveHours
            || !value.intensiveTimes.empty();
    }

    for (const ScheduleImportMatchingCandidate& candidate : input.candidates)
    {
        auto existingTeacher = std::find_if(
            result.teachers.begin(),
            result.teachers.end(),
            [&candidate](
                const ScheduleImportMatchingTeacherProjection& value
                )
            {
                return value.teacherKey == candidate.teacherKey;
            }
            );
        if (existingTeacher != result.teachers.end())
        {
            continue;
        }

        ScheduleImportMatchingTeacherProjection teacherProjection;
        teacherProjection.teacherKey = candidate.teacherKey;
        teacherProjection.teacherName = candidate.teacherName;
        for (const ScheduleImportMatchingCandidate& other : input.candidates)
        {
            if (other.teacherKey != candidate.teacherKey)
            {
                continue;
            }
            for (const std::u16string& room : other.rooms)
            {
                if (
                    std::find(
                        teacherProjection.importedRooms.cbegin(),
                        teacherProjection.importedRooms.cend(),
                        room
                        ) == teacherProjection.importedRooms.cend()
                    )
                {
                    teacherProjection.importedRooms.push_back(room);
                }
            }
        }
        for (const ScheduleImportMatchingTeacher& teacher : input.teachers)
        {
            if (hangulOnly(teacher.koreanName) == candidate.teacherKey)
            {
                teacherProjection.matchingTeacherIds.push_back(teacher.id);
            }
        }
        for (const ScheduleImportMatchingClass& value : input.classes)
        {
            if (
                std::find(
                    teacherProjection.matchingTeacherIds.cbegin(),
                    teacherProjection.matchingTeacherIds.cend(),
                    value.teacherId
                    ) != teacherProjection.matchingTeacherIds.cend()
                )
            {
                ++teacherProjection.affectedClassCount;
            }
        }
        result.teachers.push_back(std::move(teacherProjection));
    }

    std::vector<std::int32_t> exactTargets;
    for (
        std::size_t candidateIndex = 0;
        candidateIndex < input.candidates.size();
        ++candidateIndex
        )
    {
        const ScheduleImportMatchingCandidate& candidate =
            input.candidates[candidateIndex];
        ScheduleImportMatchingClassProjection classProjection;
        classProjection.candidateIndex = candidateIndex;

        std::vector<std::int32_t> importedTeacherIds;
        const auto matchingTeacher = std::find_if(
            result.teachers.cbegin(),
            result.teachers.cend(),
            [&candidate](
                const ScheduleImportMatchingTeacherProjection& teacher
                )
            {
                return teacher.teacherKey == candidate.teacherKey;
            }
            );
        if (matchingTeacher != result.teachers.cend())
        {
            importedTeacherIds = matchingTeacher->matchingTeacherIds;
        }

        std::vector<std::int32_t> exact;
        std::vector<std::int32_t> sameCourseTeacherRoomSameDays;
        std::vector<std::int32_t> sameCourseTeacherRoom;
        std::vector<std::int32_t> sameCourseTeacherSameDays;
        std::vector<std::int32_t> sameCourseTeacher;
        std::vector<std::int32_t> sameCourseSameDays;
        std::vector<std::int32_t> sameCourse;

        for (const ScheduleImportMatchingClass& value : input.classes)
        {
            if (!isEligible(candidate, value, input.kind))
            {
                continue;
            }

            const bool teacherMatches =
                std::find(
                    importedTeacherIds.cbegin(),
                    importedTeacherIds.cend(),
                    value.teacherId
                    ) != importedTeacherIds.cend();
            const bool roomMatches = std::any_of(
                candidate.roomMatchKeys.cbegin(),
                candidate.roomMatchKeys.cend(),
                [&value](const std::u16string& roomKey)
                {
                    return roomKey == value.roomMatchKey;
                }
                );
            const auto& targetTimes = targetTimesForKind(value, input.kind);
            const bool targetDaysMatch = !targetTimes.empty()
                && meetingDaysMatch(candidate.times, targetTimes);
            const auto& referenceTimes = timesForKind(value, input.kind);
            const bool referenceDaysMatch = !referenceTimes.empty()
                && meetingDaysMatch(candidate.times, referenceTimes);

            if (teacherMatches && roomMatches && targetDaysMatch)
            {
                appendUnique(exact, value.id);
            }
            else if (teacherMatches && roomMatches && referenceDaysMatch)
            {
                appendUnique(sameCourseTeacherRoomSameDays, value.id);
            }
            else if (teacherMatches && roomMatches)
            {
                appendUnique(sameCourseTeacherRoom, value.id);
            }
            else if (teacherMatches && referenceDaysMatch)
            {
                appendUnique(sameCourseTeacherSameDays, value.id);
            }
            else if (teacherMatches)
            {
                appendUnique(sameCourseTeacher, value.id);
            }
            else if (referenceDaysMatch)
            {
                appendUnique(sameCourseSameDays, value.id);
            }
            else
            {
                appendUnique(sameCourse, value.id);
            }
        }

        const std::vector<const std::vector<std::int32_t>*> rankedMatches{
            &exact,
            &sameCourseTeacherRoomSameDays,
            &sameCourseTeacherRoom,
            &sameCourseTeacherSameDays,
            &sameCourseTeacher,
            &sameCourseSameDays,
            &sameCourse
        };
        for (const auto* matches : rankedMatches)
        {
            for (const std::int32_t classId : *matches)
            {
                appendUnique(classProjection.matchingClassIds, classId);
            }
        }

        if (exact.size() == 1)
        {
            classProjection.suggestedClassId = exact.front();
            classProjection.exactMatch = true;
            classProjection.confidence =
                ScheduleImportMatchingConfidence::Confident;
            classProjection.explanation =
                ScheduleImportMatchingExplanation::Exact;
            appendUnique(exactTargets, exact.front());
        }
        else if (!classProjection.matchingClassIds.empty())
        {
            classProjection.suggestedClassId =
                classProjection.matchingClassIds.front();
            classProjection.confidence =
                ScheduleImportMatchingConfidence::Possible;
            bool hasTargetHours = false;
            bool hasOtherHours = false;
            for (const std::int32_t classId : classProjection.matchingClassIds)
            {
                const auto existing = std::find_if(
                    input.classes.cbegin(),
                    input.classes.cend(),
                    [classId](const ScheduleImportMatchingClass& value)
                    {
                        return value.id == classId;
                    }
                    );
                if (existing == input.classes.cend())
                {
                    continue;
                }
                hasTargetHours = hasTargetHours
                    || !targetTimesForKind(*existing, input.kind).empty();
                hasOtherHours = hasOtherHours
                    || !timesForKind(*existing, input.kind).empty();
            }
            classProjection.explanation = hasTargetHours
                ? ScheduleImportMatchingExplanation::PossibleWithTargetHours
                : hasOtherHours
                    ? ScheduleImportMatchingExplanation::PossibleWithOtherHours
                    : ScheduleImportMatchingExplanation::PossibleWithoutHours;
        }

        result.classes.push_back(std::move(classProjection));
    }

    for (const ScheduleImportMatchingClass& value : input.classes)
    {
        if (
            std::find(
                exactTargets.cbegin(),
                exactTargets.cend(),
                value.id
                ) == exactTargets.cend()
            )
        {
            result.initiallyAbsentClassIds.push_back(value.id);
        }
    }
    return result;
}

} // namespace ClassMngr::Next::Application
