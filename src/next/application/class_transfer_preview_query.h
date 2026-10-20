#pragma once

#include "next/application/class_transfer_apply_request.h"
#include "next/application/class_transfer_matching_policy.h"
#include "next/domain/operation_result.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

struct ClassTransferPreviewRequest final
{
    // Source records have already been normalized by the Platform adapter.
    std::vector<ClassTransferMatchingSourceTeacher> teachers;
    std::vector<ClassTransferMatchingSourceClass> classes;
};

struct ClassTransferPreviewReadSnapshot final
{
    std::vector<ClassTransferMatchingDestinationTeacher> teachers;
    std::vector<ClassTransferMatchingDestinationClass> classes;
};

struct ClassTransferPreviewResult final
{
    std::vector<ClassTransferReviewClassCandidate> classes;
    std::vector<ClassTransferReviewTeacherCandidate> teachers;
};

using ClassTransferPreviewReadResult =
    Domain::Result<ClassTransferPreviewReadSnapshot>;
using ClassTransferPreviewQueryResult =
    Domain::Result<ClassTransferPreviewResult>;

class ClassTransferPreviewReadPort
{
public:
    virtual ~ClassTransferPreviewReadPort() = default;

    [[nodiscard]] virtual ClassTransferPreviewReadResult readSnapshot(
        const ClassTransferPreviewRequest& request
        ) const = 0;
};

class ClassTransferPreviewQuery final
{
public:
    explicit ClassTransferPreviewQuery(
        const ClassTransferPreviewReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] ClassTransferPreviewQueryResult execute(
        const ClassTransferPreviewRequest& request
        ) const
    {
        ClassTransferMatchingRequest matchingRequest;
        matchingRequest.sourceTeachers = request.teachers;
        matchingRequest.sourceClasses = request.classes;

        const bool needsTeacherMatches = std::any_of(
            request.teachers.cbegin(),
            request.teachers.cend(),
            [](const ClassTransferMatchingSourceTeacher& teacher)
            {
                return !teacher.names.english.empty()
                    || !teacher.names.korean.empty();
            }
            );
        const bool needsClassMatches = std::any_of(
            request.classes.cbegin(),
            request.classes.cend(),
            [](const ClassTransferMatchingSourceClass& sourceClass)
            {
                return !sourceClass.grade.empty()
                    && !sourceClass.level.empty();
            }
            );

        if (needsTeacherMatches || needsClassMatches)
        {
            ClassTransferPreviewReadResult snapshot =
                m_port.readSnapshot(request);
            if (!snapshot)
            {
                return ClassTransferPreviewQueryResult::failure(
                    snapshot.error());
            }

            matchingRequest.destinationTeachers =
                std::move(snapshot.value().teachers);
            matchingRequest.destinationClasses =
                std::move(snapshot.value().classes);
        }

        const ClassTransferMatchingResult matched =
            matchClassTransferCandidates(matchingRequest);
        ClassTransferPreviewResult result;
        result.teachers.reserve(matched.teachers.size());
        for (const ClassTransferTeacherMatchList& source : matched.teachers)
        {
            ClassTransferReviewTeacherCandidate candidate;
            candidate.teacherKey = source.teacherSourceKey;
            candidate.matchingTeacherIds = source.matchingTeacherIds;
            result.teachers.push_back(std::move(candidate));
        }

        result.classes.reserve(matched.classes.size());
        for (const ClassTransferClassMatchList& source : matched.classes)
        {
            ClassTransferReviewClassCandidate candidate;
            candidate.packageClassIndex = ClassTransferPackageClassIndex{
                static_cast<int>(source.packageClassIndex)
            };
            candidate.matchingClassIds = source.matchingClassIds;
            candidate.sourceKey = source.sourceKey;
            result.classes.push_back(std::move(candidate));
        }

        return ClassTransferPreviewQueryResult::success(std::move(result));
    }

private:
    const ClassTransferPreviewReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
