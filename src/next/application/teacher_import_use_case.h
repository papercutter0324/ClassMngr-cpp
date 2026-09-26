#pragma once

#include "next/application/gs_team_import_update.h"
#include "next/application/import_review_session.h"
#include "next/application/korean_teacher_import_update.h"
#include "next/application/native_english_teacher_import_update.h"
#include "next/application/teacher_import_match_cardinality.h"
#include "next/application/teacher_import_plan_validation.h"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

struct TeacherImportKoreanSource final
{
    std::string validationKey;
    std::u16string teacherKr;
    std::u16string teacherEn;
    std::u16string preferredRomanization;
    std::u16string preferredName;
    std::u16string roomNumber;
    std::u16string birthday;
    std::u16string phoneNumber;
    std::u16string wifiName;
    std::u16string wifiPassword;
    std::u16string internetType;
    std::u16string zoomId;
    std::u16string zoomPassword;
    std::u16string projectionType;
    std::u16string notes;
};

struct TeacherImportNativeEnglishSource final
{
    std::string validationKey;
    std::u16string normalizedMatchKey;
    std::u16string diagnosticName;
    std::u16string name;
    std::u16string position;
    std::u16string phoneNumber;
    std::u16string birthday;
    std::u16string nationality;
    std::u16string email;
};

struct TeacherImportGsTeamSource final
{
    std::string englishValidationKey;
    std::string koreanValidationKey;
    std::u16string normalizedEnglishMatchKey;
    std::u16string normalizedKoreanMatchKey;
    std::u16string diagnosticEnglishName;
    std::u16string diagnosticKoreanName;
    std::u16string name;
    std::u16string koreanName;
    std::u16string position;
    std::u16string phoneNumber;
    std::u16string birthday;
};

struct TeacherImportReviewGroup final
{
    std::string id;
    std::vector<std::string> candidateValidationKeys;
};

struct TeacherImportReview final
{
    std::vector<TeacherImportReviewGroup> candidateGroups;
    std::vector<TeacherImportGroupDecision> decisions;
};

struct TeacherImportUseCaseRequest final
{
    // This carries adapter-normalized identity keys and source-date validity.
    // The use case checks it against these records and owns applying the
    // validation and review-selection contracts before opening a transaction.
    TeacherImportPlanValidationInput validation;
    std::string sourceDateIso;
    std::vector<TeacherImportKoreanSource> koreanTeachers;
    std::vector<TeacherImportNativeEnglishSource> nativeEnglishTeachers;
    std::vector<TeacherImportGsTeamSource> gsTeamMembers;
    std::optional<TeacherImportReview> review;
};

struct TeacherImportNativeEnglishStoredRecord final
{
    NativeEnglishTeacherImportProfile profile;
    std::u16string normalizedMatchKey;
};

struct TeacherImportGsTeamStoredRecord final
{
    GsTeamImportProfile profile;
    std::u16string normalizedEnglishMatchKey;
    std::u16string normalizedKoreanMatchKey;
};

struct TeacherImportPersistenceSnapshot final
{
    std::vector<KoreanTeacherImportProfile> koreanTeachers;
    std::vector<TeacherImportNativeEnglishStoredRecord> nativeEnglishTeachers;
    std::vector<TeacherImportGsTeamStoredRecord> gsTeamMembers;
    // The adapter returns only a valid ISO date, or nullopt for a missing or
    // malformed setting. The use case decides whether it must be advanced.
    std::optional<std::string> latestSourceDateIso;
};

enum class TeacherImportPersistenceOperation
{
    BeginTransaction,
    LoadSnapshot,
    CreateKoreanTeacher,
    UpdateKoreanTeacher,
    CreateNativeEnglishTeacher,
    UpdateNativeEnglishTeacher,
    CreateGsTeamMember,
    UpdateGsTeamMember,
    SaveLatestSourceDate,
    CommitTransaction,
    RollbackTransaction
};

struct TeacherImportPersistenceFailure final
{
    TeacherImportPersistenceOperation operation =
        TeacherImportPersistenceOperation::LoadSnapshot;
    std::u16string message;

    friend bool operator==(
        const TeacherImportPersistenceFailure&,
        const TeacherImportPersistenceFailure&
        ) = default;
};

template <typename Value>
using TeacherImportPersistenceResult =
    std::expected<Value, TeacherImportPersistenceFailure>;

// One port represents the complete transaction boundary. Implementations must
// keep every read, write, latest-date update, and commit on the same unit of
// work; individual write methods must never commit independently.
class TeacherImportPersistencePort
{
public:
    virtual ~TeacherImportPersistencePort() = default;

    virtual TeacherImportPersistenceResult<void> beginTransaction() = 0;
    virtual TeacherImportPersistenceResult<TeacherImportPersistenceSnapshot>
    loadSnapshot() = 0;
    virtual TeacherImportPersistenceResult<void> createKoreanTeacher(
        const TeacherImportKoreanSource& source,
        const std::u16string& canonicalTeacherKr
        ) = 0;
    virtual TeacherImportPersistenceResult<void> updateKoreanTeacher(
        const KoreanTeacherImportProfile& profile
        ) = 0;
    virtual TeacherImportPersistenceResult<void> createNativeEnglishTeacher(
        const TeacherImportNativeEnglishSource& source
        ) = 0;
    virtual TeacherImportPersistenceResult<void> updateNativeEnglishTeacher(
        const NativeEnglishTeacherImportProfile& profile
        ) = 0;
    virtual TeacherImportPersistenceResult<void> createGsTeamMember(
        const TeacherImportGsTeamSource& source
        ) = 0;
    virtual TeacherImportPersistenceResult<void> updateGsTeamMember(
        const GsTeamImportProfile& profile
        ) = 0;
    virtual TeacherImportPersistenceResult<void> saveLatestSourceDate(
        std::string_view sourceDateIso
        ) = 0;
    virtual TeacherImportPersistenceResult<void> commitTransaction() = 0;
    virtual TeacherImportPersistenceResult<void> rollbackTransaction() = 0;
};

struct TeacherImportCounts final
{
    int created = 0;
    int updated = 0;
    int unchanged = 0;

    [[nodiscard]] int total() const noexcept
    {
        return created + updated + unchanged;
    }

    friend bool operator==(
        const TeacherImportCounts&,
        const TeacherImportCounts&
        ) = default;
};

struct TeacherImportUseCaseResult final
{
    TeacherImportCounts koreanTeachers;
    TeacherImportCounts nativeEnglishTeachers;
    TeacherImportCounts gsTeamMembers;

    friend bool operator==(
        const TeacherImportUseCaseResult&,
        const TeacherImportUseCaseResult&
        ) = default;
};

enum class TeacherImportUseCaseErrorKind
{
    InvalidRequest,
    InvalidReview,
    InvalidPlan,
    AmbiguousStoredMatch,
    KoreanMatchPolicyRejected,
    PersistenceFailure
};

enum class TeacherImportRecordNamespace
{
    KoreanTeacher,
    NativeEnglishTeacher,
    GsTeamMember
};

struct TeacherImportUseCaseError final
{
    TeacherImportUseCaseErrorKind kind =
        TeacherImportUseCaseErrorKind::InvalidRequest;
    TeacherImportReviewIssue reviewIssue = TeacherImportReviewIssue::None;
    TeacherImportPlanValidationIssue validationIssue =
        TeacherImportPlanValidationIssue::None;
    TeacherImportRecordNamespace recordNamespace =
        TeacherImportRecordNamespace::KoreanTeacher;
    std::u16string matchLabel;
    TeacherImportPersistenceFailure persistenceFailure;
    std::optional<TeacherImportPersistenceFailure> rollbackFailure;

    friend bool operator==(
        const TeacherImportUseCaseError&,
        const TeacherImportUseCaseError&
        ) = default;
};

namespace Detail
{

[[nodiscard]] inline bool isTeacherImportIsoDate(
    const std::string_view value
    ) noexcept
{
    if (value.size() != 10 || value[4] != '-' || value[7] != '-')
    {
        return false;
    }
    for (std::size_t index = 0; index < value.size(); ++index)
    {
        if (index != 4 && index != 7
            && (value[index] < '0' || value[index] > '9'))
        {
            return false;
        }
    }

    const auto number = [&value](const std::size_t first, const std::size_t count)
    {
        int result = 0;
        for (std::size_t index = first; index < first + count; ++index)
        {
            result = result * 10 + value[index] - '0';
        }
        return result;
    };
    const int year = number(0, 4);
    const int month = number(5, 2);
    const int day = number(8, 2);
    if (year == 0 || month < 1 || month > 12)
    {
        return false;
    }
    constexpr int daysPerMonth[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leapYear = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    const int daysInMonth = month == 2 && leapYear
        ? 29 : daysPerMonth[month - 1];
    return day >= 1 && day <= daysInMonth;
}

[[nodiscard]] inline TeacherImportUseCaseError invalidRequestError()
{
    return {.kind = TeacherImportUseCaseErrorKind::InvalidRequest};
}

[[nodiscard]] inline TeacherImportUseCaseError validationError(
    const TeacherImportPlanValidationIssue issue
    )
{
    return {
        .kind = TeacherImportUseCaseErrorKind::InvalidPlan,
        .validationIssue = issue
    };
}

[[nodiscard]] inline TeacherImportUseCaseError persistenceError(
    TeacherImportPersistenceFailure failure
    )
{
    return {
        .kind = TeacherImportUseCaseErrorKind::PersistenceFailure,
        .persistenceFailure = std::move(failure)
    };
}

[[nodiscard]] inline TeacherImportUseCaseError rollbackAndReturn(
    TeacherImportPersistencePort& port,
    TeacherImportUseCaseError error
    )
{
    auto rollback = port.rollbackTransaction();
    if (!rollback)
    {
        error.rollbackFailure = rollback.error();
    }
    return error;
}

[[nodiscard]] inline TeacherImportPlanValidationIssue validateRequestShape(
    const TeacherImportUseCaseRequest& request
    )
{
    if (request.validation.koreanTeacherKeys.size() != request.koreanTeachers.size()
        || request.validation.nativeEnglishTeacherKeys.size()
            != request.nativeEnglishTeachers.size()
        || request.validation.gsTeamMemberKeys.size() != request.gsTeamMembers.size())
    {
        return TeacherImportPlanValidationIssue::ReviewSelectionMismatch;
    }

    for (std::size_t index = 0; index < request.koreanTeachers.size(); ++index)
    {
        if (request.validation.koreanTeacherKeys[index]
            != request.koreanTeachers[index].validationKey)
        {
            return TeacherImportPlanValidationIssue::ReviewSelectionMismatch;
        }
    }
    for (std::size_t index = 0; index < request.nativeEnglishTeachers.size(); ++index)
    {
        if (request.validation.nativeEnglishTeacherKeys[index]
            != request.nativeEnglishTeachers[index].validationKey)
        {
            return TeacherImportPlanValidationIssue::ReviewSelectionMismatch;
        }
    }
    for (std::size_t index = 0; index < request.gsTeamMembers.size(); ++index)
    {
        if (request.validation.gsTeamMemberKeys[index].english
                != request.gsTeamMembers[index].englishValidationKey
            || request.validation.gsTeamMemberKeys[index].korean
                != request.gsTeamMembers[index].koreanValidationKey)
        {
            return TeacherImportPlanValidationIssue::ReviewSelectionMismatch;
        }
    }
    return TeacherImportPlanValidationIssue::None;
}

} // namespace Detail

class TeacherImportUseCase final
{
public:
    [[nodiscard]] static std::expected<
        TeacherImportUseCaseResult,
        TeacherImportUseCaseError
        > execute(
            const TeacherImportUseCaseRequest& request,
            TeacherImportPersistencePort& persistence
            )
    {
        TeacherImportPlanValidationInput validation = request.validation;
        validation.reviewedKoreanTeacherKeys.reset();
        const TeacherImportPlanValidationIssue shapeIssue =
            Detail::validateRequestShape(request);
        if (shapeIssue != TeacherImportPlanValidationIssue::None)
        {
            return std::unexpected(Detail::validationError(shapeIssue));
        }

        if (request.review)
        {
            std::vector<TeacherImportCandidateGroup> groups;
            groups.reserve(request.review->candidateGroups.size());
            for (const TeacherImportReviewGroup& group : request.review->candidateGroups)
            {
                groups.push_back({group.id, group.candidateValidationKeys.size()});
            }
            const TeacherImportReviewResolution resolution =
                resolveTeacherImportReview(groups, request.review->decisions);
            if (!resolution.accepted())
            {
                return std::unexpected(TeacherImportUseCaseError{
                    .kind = TeacherImportUseCaseErrorKind::InvalidReview,
                    .reviewIssue = resolution.issue
                });
            }

            auto& selectedKeys = validation.reviewedKoreanTeacherKeys.emplace();
            for (std::size_t groupIndex = 0;
                 groupIndex < resolution.selectedCandidateIndexes.size();
                 ++groupIndex)
            {
                const TeacherImportReviewGroup& group =
                    request.review->candidateGroups[groupIndex];
                for (const std::size_t candidateIndex :
                     resolution.selectedCandidateIndexes[groupIndex])
                {
                    selectedKeys.push_back(
                        group.candidateValidationKeys[candidateIndex]);
                }
            }
        }

        validation.sourceDateValid = validation.sourceDateValid
            && Detail::isTeacherImportIsoDate(request.sourceDateIso);
        const TeacherImportPlanValidationIssue validationIssue =
            validateTeacherImportPlan(validation);
        if (validationIssue != TeacherImportPlanValidationIssue::None)
        {
            return std::unexpected(Detail::validationError(validationIssue));
        }

        const auto started = persistence.beginTransaction();
        if (!started)
        {
            return std::unexpected(Detail::persistenceError(started.error()));
        }

        const auto loaded = persistence.loadSnapshot();
        if (!loaded)
        {
            return std::unexpected(Detail::rollbackAndReturn(
                persistence,
                Detail::persistenceError(loaded.error())
                ));
        }
        const TeacherImportPersistenceSnapshot& existing = loaded.value();
        TeacherImportUseCaseResult result;

        for (const TeacherImportKoreanSource& source : request.koreanTeachers)
        {
            const Domain::KoreanTeacherKey key =
                Domain::KoreanTeacherKey::fromName(source.teacherKr);
            std::vector<const KoreanTeacherImportProfile*> matches;
            for (const KoreanTeacherImportProfile& teacher : existing.koreanTeachers)
            {
                if (teacher.key == key)
                {
                    matches.push_back(&teacher);
                }
            }

            switch (classifyTeacherImportMatchCardinality(matches.size()))
            {
            case TeacherImportMatchCardinality::MultipleMatches:
                return std::unexpected(Detail::rollbackAndReturn(
                    persistence,
                    TeacherImportUseCaseError{
                        .kind = TeacherImportUseCaseErrorKind::AmbiguousStoredMatch,
                        .recordNamespace = TeacherImportRecordNamespace::KoreanTeacher,
                        .matchLabel = source.teacherKr
                    }
                    ));
            case TeacherImportMatchCardinality::NoMatch:
            {
                const auto written = persistence.createKoreanTeacher(
                    source, key.value());
                if (!written)
                {
                    return std::unexpected(Detail::rollbackAndReturn(
                        persistence,
                        Detail::persistenceError(written.error())
                        ));
                }
                ++result.koreanTeachers.created;
                break;
            }
            case TeacherImportMatchCardinality::UniqueMatch:
            {
                const KoreanTeacherImportProfile& matched = *matches.front();
                const auto update = mergeMatchedKoreanTeacherImport(
                    matched,
                    KoreanTeacherImportFields{
                        .key = key,
                        .teacherKr = source.teacherKr,
                        .roomNumber = source.roomNumber,
                        .birthday = source.birthday,
                        .phoneNumber = source.phoneNumber
                    }
                    );
                if (!update)
                {
                    return std::unexpected(Detail::rollbackAndReturn(
                        persistence,
                        TeacherImportUseCaseError{
                            .kind = TeacherImportUseCaseErrorKind::KoreanMatchPolicyRejected,
                            .recordNamespace = TeacherImportRecordNamespace::KoreanTeacher,
                            .matchLabel = source.teacherKr
                        }
                        ));
                }
                if (!update->changed)
                {
                    ++result.koreanTeachers.unchanged;
                    break;
                }
                const auto written = persistence.updateKoreanTeacher(update->profile);
                if (!written)
                {
                    return std::unexpected(Detail::rollbackAndReturn(
                        persistence,
                        Detail::persistenceError(written.error())
                        ));
                }
                ++result.koreanTeachers.updated;
                break;
            }
            }
        }

        for (const TeacherImportNativeEnglishSource& source : request.nativeEnglishTeachers)
        {
            std::vector<const TeacherImportNativeEnglishStoredRecord*> matches;
            for (const TeacherImportNativeEnglishStoredRecord& teacher :
                 existing.nativeEnglishTeachers)
            {
                if (teacher.normalizedMatchKey == source.normalizedMatchKey)
                {
                    matches.push_back(&teacher);
                }
            }

            switch (classifyTeacherImportMatchCardinality(matches.size()))
            {
            case TeacherImportMatchCardinality::MultipleMatches:
                return std::unexpected(Detail::rollbackAndReturn(
                    persistence,
                    TeacherImportUseCaseError{
                        .kind = TeacherImportUseCaseErrorKind::AmbiguousStoredMatch,
                        .recordNamespace = TeacherImportRecordNamespace::NativeEnglishTeacher,
                        .matchLabel = source.diagnosticName
                    }
                    ));
            case TeacherImportMatchCardinality::NoMatch:
            {
                const auto written = persistence.createNativeEnglishTeacher(source);
                if (!written)
                {
                    return std::unexpected(Detail::rollbackAndReturn(
                        persistence,
                        Detail::persistenceError(written.error())
                        ));
                }
                ++result.nativeEnglishTeachers.created;
                break;
            }
            case TeacherImportMatchCardinality::UniqueMatch:
            {
                const auto update = mergeNativeEnglishTeacherImport(
                    matches.front()->profile,
                    NativeEnglishTeacherImportFields{
                        .name = source.name,
                        .position = source.position,
                        .phoneNumber = source.phoneNumber,
                        .birthday = source.birthday,
                        .nationality = source.nationality,
                        .email = source.email
                    }
                    );
                if (!update.changed)
                {
                    ++result.nativeEnglishTeachers.unchanged;
                    break;
                }
                const auto written = persistence.updateNativeEnglishTeacher(
                    update.profile);
                if (!written)
                {
                    return std::unexpected(Detail::rollbackAndReturn(
                        persistence,
                        Detail::persistenceError(written.error())
                        ));
                }
                ++result.nativeEnglishTeachers.updated;
                break;
            }
            }
        }

        for (const TeacherImportGsTeamSource& source : request.gsTeamMembers)
        {
            const bool useKoreanName = !source.koreanName.empty();
            const std::u16string& sourceKey = useKoreanName
                ? source.normalizedKoreanMatchKey
                : source.normalizedEnglishMatchKey;
            std::vector<const TeacherImportGsTeamStoredRecord*> matches;
            for (const TeacherImportGsTeamStoredRecord& member : existing.gsTeamMembers)
            {
                const std::u16string& existingKey = useKoreanName
                    ? member.normalizedKoreanMatchKey
                    : member.normalizedEnglishMatchKey;
                if (existingKey == sourceKey)
                {
                    matches.push_back(&member);
                }
            }

            switch (classifyTeacherImportMatchCardinality(matches.size()))
            {
            case TeacherImportMatchCardinality::MultipleMatches:
                return std::unexpected(Detail::rollbackAndReturn(
                    persistence,
                    TeacherImportUseCaseError{
                        .kind = TeacherImportUseCaseErrorKind::AmbiguousStoredMatch,
                        .recordNamespace = TeacherImportRecordNamespace::GsTeamMember,
                        .matchLabel = useKoreanName
                            ? source.diagnosticKoreanName
                            : source.diagnosticEnglishName
                    }
                    ));
            case TeacherImportMatchCardinality::NoMatch:
            {
                const auto written = persistence.createGsTeamMember(source);
                if (!written)
                {
                    return std::unexpected(Detail::rollbackAndReturn(
                        persistence,
                        Detail::persistenceError(written.error())
                        ));
                }
                ++result.gsTeamMembers.created;
                break;
            }
            case TeacherImportMatchCardinality::UniqueMatch:
            {
                const auto update = mergeGsTeamImport(
                    matches.front()->profile,
                    GsTeamImportFields{
                        .name = source.name,
                        .koreanName = source.koreanName,
                        .position = source.position,
                        .phoneNumber = source.phoneNumber,
                        .birthday = source.birthday
                    }
                    );
                if (!update.changed)
                {
                    ++result.gsTeamMembers.unchanged;
                    break;
                }
                const auto written = persistence.updateGsTeamMember(update.profile);
                if (!written)
                {
                    return std::unexpected(Detail::rollbackAndReturn(
                        persistence,
                        Detail::persistenceError(written.error())
                        ));
                }
                ++result.gsTeamMembers.updated;
                break;
            }
            }
        }

        if (!existing.latestSourceDateIso
            || request.sourceDateIso > *existing.latestSourceDateIso)
        {
            const auto saved = persistence.saveLatestSourceDate(request.sourceDateIso);
            if (!saved)
            {
                return std::unexpected(Detail::rollbackAndReturn(
                    persistence,
                    Detail::persistenceError(saved.error())
                    ));
            }
        }

        const auto committed = persistence.commitTransaction();
        if (!committed)
        {
            return std::unexpected(Detail::rollbackAndReturn(
                persistence,
                Detail::persistenceError(committed.error())
                ));
        }
        return result;
    }
};

} // namespace ClassMngr::Next::Application
