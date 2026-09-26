#pragma once

#include "next/domain/domain_types.h"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

// Names, grade, and level are supplied in the adapter's normalized form.
// This policy compares those values as-is so the feature adapter remains the
// owner of QString::simplified().toCaseFolded() semantics.
struct ClassTransferMatchingTeacherNames final
{
    std::string english;
    std::string korean;

    friend bool operator==(
        const ClassTransferMatchingTeacherNames&,
        const ClassTransferMatchingTeacherNames&
        ) = default;
};

struct ClassTransferMatchingSourceTeacher final
{
    std::string sourceKey;
    ClassTransferMatchingTeacherNames names;
};

struct ClassTransferMatchingDestinationTeacher final
{
    Domain::TeacherId id;
    ClassTransferMatchingTeacherNames names;
};

struct ClassTransferMatchingSourceClass final
{
    std::size_t packageClassIndex = 0;
    std::string teacherSourceKey;
    std::string grade;
    std::string level;
};

struct ClassTransferMatchingDestinationClass final
{
    Domain::ClassId id;
    std::string grade;
    std::string level;
    std::optional<ClassTransferMatchingDestinationTeacher> teacher;
};

struct ClassTransferTeacherMatchList final
{
    std::string teacherSourceKey;
    std::vector<Domain::TeacherId> matchingTeacherIds;
};

struct ClassTransferClassMatchList final
{
    std::size_t packageClassIndex = 0;
    std::vector<Domain::ClassId> matchingClassIds;
};

struct ClassTransferMatchingRequest final
{
    std::vector<ClassTransferMatchingSourceTeacher> sourceTeachers;
    std::vector<ClassTransferMatchingDestinationTeacher> destinationTeachers;
    std::vector<ClassTransferMatchingSourceClass> sourceClasses;
    std::vector<ClassTransferMatchingDestinationClass> destinationClasses;
};

struct ClassTransferMatchingResult final
{
    std::vector<ClassTransferTeacherMatchList> teachers;
    std::vector<ClassTransferClassMatchList> classes;
};

[[nodiscard]] inline bool classTransferTeacherNamesMatch(
    const ClassTransferMatchingTeacherNames& source,
    const ClassTransferMatchingTeacherNames& destination
    ) noexcept
{
    if (!source.english.empty() && !source.korean.empty())
    {
        return source.english == destination.english
            && source.korean == destination.korean;
    }

    if (!source.english.empty())
    {
        return source.english == destination.english;
    }

    if (!source.korean.empty())
    {
        return source.korean == destination.korean;
    }

    return false;
}

// The request contains ordered snapshots. Results retain source order, and
// each match list retains destination order for deterministic review choices.
[[nodiscard]] inline ClassTransferMatchingResult matchClassTransferCandidates(
    const ClassTransferMatchingRequest& request
    )
{
    std::unordered_map<
        std::string,
        const ClassTransferMatchingSourceTeacher*> sourceTeachersByKey;
    sourceTeachersByKey.reserve(request.sourceTeachers.size());
    for (const auto& teacher : request.sourceTeachers)
    {
        sourceTeachersByKey.try_emplace(teacher.sourceKey, &teacher);
    }

    ClassTransferMatchingResult result;
    result.teachers.reserve(request.sourceTeachers.size());
    for (const auto& source : request.sourceTeachers)
    {
        ClassTransferTeacherMatchList matches;
        matches.teacherSourceKey = source.sourceKey;
        for (const auto& destination : request.destinationTeachers)
        {
            if (classTransferTeacherNamesMatch(source.names, destination.names))
            {
                matches.matchingTeacherIds.push_back(destination.id);
            }
        }
        result.teachers.push_back(std::move(matches));
    }

    result.classes.reserve(request.sourceClasses.size());
    for (const auto& source : request.sourceClasses)
    {
        ClassTransferClassMatchList matches;
        matches.packageClassIndex = source.packageClassIndex;

        if (source.grade.empty() || source.level.empty())
        {
            result.classes.push_back(std::move(matches));
            continue;
        }

        const auto sourceTeacher = sourceTeachersByKey.find(
            source.teacherSourceKey
            );
        for (const auto& destination : request.destinationClasses)
        {
            if (source.grade != destination.grade
                || source.level != destination.level)
            {
                continue;
            }

            bool sameTeacher = false;
            if (sourceTeacher == sourceTeachersByKey.cend())
            {
                sameTeacher = !destination.teacher.has_value();
            }
            else if (destination.teacher.has_value())
            {
                sameTeacher = classTransferTeacherNamesMatch(
                    sourceTeacher->second->names,
                    destination.teacher->names
                    );
            }

            if (sameTeacher)
            {
                matches.matchingClassIds.push_back(destination.id);
            }
        }

        result.classes.push_back(std::move(matches));
    }

    return result;
}

}
