#include "next/application/teacher_import_use_case.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next::Application;

namespace
{

void require(const bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

TeacherImportPersistenceFailure failure(
    const TeacherImportPersistenceOperation operation
    )
{
    return {.operation = operation, .message = u"injected failure"};
}

class FakeTeacherImportPersistence final : public TeacherImportPersistencePort
{
public:
    TeacherImportPersistenceSnapshot state;
    std::optional<TeacherImportPersistenceOperation> failAt;
    std::vector<std::string> calls;
    int beginCount = 0;
    int rollbackCount = 0;
    int commitCount = 0;
    bool failRollback = false;

    TeacherImportPersistenceResult<void> beginTransaction() override
    {
        calls.push_back("begin");
        ++beginCount;
        if (shouldFail(TeacherImportPersistenceOperation::BeginTransaction))
        {
            return std::unexpected(failure(
                TeacherImportPersistenceOperation::BeginTransaction));
        }
        beforeTransaction = state;
        active = true;
        return {};
    }

    TeacherImportPersistenceResult<TeacherImportPersistenceSnapshot>
    loadSnapshot() override
    {
        calls.push_back("load");
        if (shouldFail(TeacherImportPersistenceOperation::LoadSnapshot))
        {
            return std::unexpected(failure(
                TeacherImportPersistenceOperation::LoadSnapshot));
        }
        return state;
    }

    TeacherImportPersistenceResult<void> createKoreanTeacher(
        const TeacherImportKoreanSource& source,
        const std::u16string& canonicalTeacherKr
        ) override
    {
        calls.push_back("create-korean");
        if (shouldFail(TeacherImportPersistenceOperation::CreateKoreanTeacher))
        {
            return std::unexpected(failure(
                TeacherImportPersistenceOperation::CreateKoreanTeacher));
        }
        const std::string id = std::to_string(nextId++);
        state.koreanTeachers.push_back({
            .teacherId = *ClassMngr::Next::Domain::TeacherId::fromString(id),
            .key = ClassMngr::Next::Domain::KoreanTeacherKey::fromName(
                canonicalTeacherKr),
            .teacherKr = canonicalTeacherKr,
            .roomNumber = source.roomNumber,
            .birthday = source.birthday,
            .phoneNumber = source.phoneNumber
        });
        return {};
    }

    TeacherImportPersistenceResult<void> updateKoreanTeacher(
        const KoreanTeacherImportProfile& profile
        ) override
    {
        calls.push_back("update-korean");
        if (shouldFail(TeacherImportPersistenceOperation::UpdateKoreanTeacher))
        {
            return std::unexpected(failure(
                TeacherImportPersistenceOperation::UpdateKoreanTeacher));
        }
        for (auto& existing : state.koreanTeachers)
        {
            if (existing.teacherId == profile.teacherId)
            {
                existing = profile;
                return {};
            }
        }
        throw std::runtime_error("Korean update target was missing in fake state");
    }

    TeacherImportPersistenceResult<void> createNativeEnglishTeacher(
        const TeacherImportNativeEnglishSource& source
        ) override
    {
        calls.push_back("create-native");
        if (shouldFail(TeacherImportPersistenceOperation::CreateNativeEnglishTeacher))
        {
            return std::unexpected(failure(
                TeacherImportPersistenceOperation::CreateNativeEnglishTeacher));
        }
        NativeEnglishTeacherImportProfile profile{
            .id = nextId++,
            .name = source.name,
            .position = source.position,
            .phoneNumber = source.phoneNumber,
            .birthday = source.birthday,
            .nationality = source.nationality,
            .email = source.email
        };
        state.nativeEnglishTeachers.push_back({
            .profile = profile,
            .normalizedMatchKey = source.normalizedMatchKey
        });
        return {};
    }

    TeacherImportPersistenceResult<void> updateNativeEnglishTeacher(
        const NativeEnglishTeacherImportProfile& profile
        ) override
    {
        calls.push_back("update-native");
        if (shouldFail(TeacherImportPersistenceOperation::UpdateNativeEnglishTeacher))
        {
            return std::unexpected(failure(
                TeacherImportPersistenceOperation::UpdateNativeEnglishTeacher));
        }
        for (auto& existing : state.nativeEnglishTeachers)
        {
            if (existing.profile.id == profile.id)
            {
                existing.profile = profile;
                return {};
            }
        }
        throw std::runtime_error("Native English update target was missing in fake state");
    }

    TeacherImportPersistenceResult<void> createGsTeamMember(
        const TeacherImportGsTeamSource& source
        ) override
    {
        calls.push_back("create-gs");
        if (shouldFail(TeacherImportPersistenceOperation::CreateGsTeamMember))
        {
            return std::unexpected(failure(
                TeacherImportPersistenceOperation::CreateGsTeamMember));
        }
        GsTeamImportProfile profile{
            .id = nextId++,
            .name = source.name,
            .koreanName = source.koreanName,
            .position = source.position,
            .phoneNumber = source.phoneNumber,
            .birthday = source.birthday
        };
        state.gsTeamMembers.push_back({
            .profile = profile,
            .normalizedEnglishMatchKey = source.normalizedEnglishMatchKey,
            .normalizedKoreanMatchKey = source.normalizedKoreanMatchKey
        });
        return {};
    }

    TeacherImportPersistenceResult<void> updateGsTeamMember(
        const GsTeamImportProfile& profile
        ) override
    {
        calls.push_back("update-gs");
        if (shouldFail(TeacherImportPersistenceOperation::UpdateGsTeamMember))
        {
            return std::unexpected(failure(
                TeacherImportPersistenceOperation::UpdateGsTeamMember));
        }
        for (auto& existing : state.gsTeamMembers)
        {
            if (existing.profile.id == profile.id)
            {
                existing.profile = profile;
                return {};
            }
        }
        throw std::runtime_error("GS Team update target was missing in fake state");
    }

    TeacherImportPersistenceResult<void> saveLatestSourceDate(
        const std::string_view sourceDateIso
        ) override
    {
        calls.push_back("save-date");
        if (shouldFail(TeacherImportPersistenceOperation::SaveLatestSourceDate))
        {
            return std::unexpected(failure(
                TeacherImportPersistenceOperation::SaveLatestSourceDate));
        }
        state.latestSourceDateIso = std::string(sourceDateIso);
        return {};
    }

    TeacherImportPersistenceResult<void> commitTransaction() override
    {
        calls.push_back("commit");
        ++commitCount;
        if (shouldFail(TeacherImportPersistenceOperation::CommitTransaction))
        {
            return std::unexpected(failure(
                TeacherImportPersistenceOperation::CommitTransaction));
        }
        active = false;
        return {};
    }

    TeacherImportPersistenceResult<void> rollbackTransaction() override
    {
        calls.push_back("rollback");
        ++rollbackCount;
        if (failRollback
            || shouldFail(TeacherImportPersistenceOperation::RollbackTransaction))
        {
            return std::unexpected(failure(
                TeacherImportPersistenceOperation::RollbackTransaction));
        }
        if (active)
        {
            state = beforeTransaction;
            active = false;
        }
        return {};
    }

private:
    [[nodiscard]] bool shouldFail(
        const TeacherImportPersistenceOperation operation
        ) const
    {
        return failAt && *failAt == operation;
    }

    TeacherImportPersistenceSnapshot beforeTransaction;
    int nextId = 100;
    bool active = false;
};

TeacherImportUseCaseRequest emptyValidRequest()
{
    TeacherImportUseCaseRequest request;
    request.sourceDateIso = "2026-09-27";
    request.validation.sourceDateValid = true;
    return request;
}

TeacherImportKoreanSource koreanSource(
    const std::string& validationKey,
    std::u16string teacherKr
    )
{
    return {
        .validationKey = validationKey,
        .teacherKr = std::move(teacherKr)
    };
}

TeacherImportNativeEnglishSource nativeSource(
    const std::string& validationKey,
    std::u16string key,
    std::u16string name
    )
{
    return {
        .validationKey = validationKey,
        .normalizedMatchKey = std::move(key),
        .diagnosticName = name,
        .name = std::move(name)
    };
}

TeacherImportGsTeamSource gsSource(
    const std::string& englishValidationKey,
    const std::string& koreanValidationKey,
    std::u16string englishMatchKey,
    std::u16string koreanMatchKey,
    std::u16string name,
    std::u16string koreanName = {}
    )
{
    return {
        .englishValidationKey = englishValidationKey,
        .koreanValidationKey = koreanValidationKey,
        .normalizedEnglishMatchKey = std::move(englishMatchKey),
        .normalizedKoreanMatchKey = std::move(koreanMatchKey),
        .diagnosticEnglishName = name,
        .diagnosticKoreanName = koreanName,
        .name = std::move(name),
        .koreanName = std::move(koreanName)
    };
}

void appendKoreanValidation(
    TeacherImportUseCaseRequest& request,
    const std::string& key
    )
{
    request.validation.koreanTeacherKeys.push_back(key);
}

void appendNativeValidation(
    TeacherImportUseCaseRequest& request,
    const std::string& key
    )
{
    request.validation.nativeEnglishTeacherKeys.push_back(key);
}

void appendGsValidation(
    TeacherImportUseCaseRequest& request,
    const std::string& english,
    const std::string& korean
    )
{
    request.validation.gsTeamMemberKeys.push_back({english, korean});
}

KoreanTeacherImportProfile storedKorean(
    const std::string& id,
    const std::u16string& name,
    std::u16string room = {},
    std::u16string birthday = {},
    std::u16string phone = {}
    )
{
    return {
        .teacherId = *ClassMngr::Next::Domain::TeacherId::fromString(id),
        .key = ClassMngr::Next::Domain::KoreanTeacherKey::fromName(name),
        .teacherKr = name,
        .roomNumber = std::move(room),
        .birthday = std::move(birthday),
        .phoneNumber = std::move(phone)
    };
}

void successfulOrchestrationAcrossNamespaces()
{
    TeacherImportUseCaseRequest request = emptyValidRequest();
    auto koreanUpdate = koreanSource("selected-korean", u"김하늘");
    koreanUpdate.roomNumber = u"M3";
    auto koreanCreate = koreanSource("new-korean", u"이서연");
    request.koreanTeachers = {koreanUpdate, koreanCreate};
    request.validation.koreanTeacherKeys = {"selected-korean", "new-korean"};
    request.review = TeacherImportReview{
        .candidateGroups = {
            {"M1", {"not-selected", "selected-korean"}},
            {"M2", {"new-korean"}}
        },
        .decisions = {
            {"M1", TeacherImportGroupMode::Selected, {1}},
            {"M2", TeacherImportGroupMode::All, {}}
        }
    };

    auto nativeUpdate = nativeSource("alex", u"alex", u"Alex");
    nativeUpdate.position = u"Team Leader";
    auto nativeCreate = nativeSource("jamie", u"jamie", u"Jamie");
    auto nativeUnchanged = nativeSource("robin", u"robin", u"Robin");
    request.nativeEnglishTeachers = {
        nativeUpdate, nativeCreate, nativeUnchanged};
    request.validation.nativeEnglishTeacherKeys = {"alex", "jamie", "robin"};

    auto gsUpdate = gsSource("taylor", "kim", u"taylor", u"김하늘",
                             u"Taylor Updated", u"김하늘");
    gsUpdate.position = u"M3";
    auto gsCreate = gsSource("jordan", "", u"jordan", {}, u"Jordan");
    auto gsUnchanged = gsSource("casey", "", u"casey", {}, u"Casey");
    request.gsTeamMembers = {gsUpdate, gsCreate, gsUnchanged};
    request.validation.gsTeamMemberKeys = {
        {"taylor", "kim"}, {"jordan", ""}, {"casey", ""}};

    FakeTeacherImportPersistence persistence;
    persistence.state.koreanTeachers = {
        storedKorean("11", u"김하늘 A", u"M1", u"03-07", u"010-1111") ,
        storedKorean("12", u"박지민", u"M2")
    };
    persistence.state.nativeEnglishTeachers = {
        {
            .profile = {.id = 21, .name = u"Alex", .position = u"NET",
                        .phoneNumber = u"010-2222", .birthday = u"02-01",
                        .nationality = u"Canadian", .email = u"alex@example.com"},
            .normalizedMatchKey = u"alex"
        },
        {
            .profile = {.id = 22, .name = u"Robin", .position = u"NET"},
            .normalizedMatchKey = u"robin"
        }
    };
    persistence.state.gsTeamMembers = {
        {
            .profile = {.id = 31, .name = u"Taylor", .koreanName = u"김하늘",
                        .position = u"M2", .phoneNumber = u"010-3333",
                        .birthday = u"05-09"},
            .normalizedEnglishMatchKey = u"taylor",
            .normalizedKoreanMatchKey = u"김하늘"
        },
        {
            .profile = {.id = 32, .name = u"Casey"},
            .normalizedEnglishMatchKey = u"casey",
            .normalizedKoreanMatchKey = {}
        }
    };
    persistence.state.latestSourceDateIso = "2026-08-01";

    require(persistence.state.koreanTeachers[0].key
                == ClassMngr::Next::Domain::KoreanTeacherKey::fromName(
                    koreanUpdate.teacherKr),
            "first stored Korean test key does not match its source");
    require(persistence.state.koreanTeachers[1].key
                != ClassMngr::Next::Domain::KoreanTeacherKey::fromName(
                    koreanUpdate.teacherKr),
            "second stored Korean test key unexpectedly matches first source");
    require(ClassMngr::Next::Domain::KoreanTeacherKey::fromName(
                koreanCreate.teacherKr)
                != persistence.state.koreanTeachers[0].key
                && ClassMngr::Next::Domain::KoreanTeacherKey::fromName(
                    koreanCreate.teacherKr)
                    != persistence.state.koreanTeachers[1].key,
            "new Korean test key unexpectedly matches stored teachers");

    const auto result = TeacherImportUseCase::execute(request, persistence);
    require(result.has_value(), "valid cross-namespace plan did not apply");
    require(result->koreanTeachers == TeacherImportCounts{1, 1, 0},
            "Korean create/update counts are incorrect");
    require(result->nativeEnglishTeachers == TeacherImportCounts{1, 1, 1},
            "Native English create/update/unchanged counts are incorrect");
    require(result->gsTeamMembers == TeacherImportCounts{1, 1, 1},
            "GS Team create/update/unchanged counts are incorrect");
    require(persistence.state.koreanTeachers.size() == 3,
            "Korean create was not persisted");
    require(persistence.state.koreanTeachers[0].teacherId.value() == "11"
                && persistence.state.koreanTeachers[0].teacherKr == u"김하늘"
                && persistence.state.koreanTeachers[0].roomNumber == u"M3"
                && persistence.state.koreanTeachers[0].birthday == u"03-07"
                && persistence.state.koreanTeachers[0].phoneNumber == u"010-1111",
            "Korean merge did not retain identity or blank optional values");
    require(persistence.state.nativeEnglishTeachers[0].profile.id == 21
                && persistence.state.nativeEnglishTeachers[0].profile.position
                    == u"Team Leader"
                && persistence.state.nativeEnglishTeachers[0].profile.phoneNumber
                    == u"010-2222",
            "Native English merge did not retain identity and blank values");
    require(persistence.state.gsTeamMembers[0].profile.id == 31
                && persistence.state.gsTeamMembers[0].profile.name
                    == u"Taylor Updated"
                && persistence.state.gsTeamMembers[0].profile.koreanName == u"김하늘"
                && persistence.state.gsTeamMembers[0].profile.phoneNumber == u"010-3333",
            "GS Team merge did not use Korean identity preference or preserve blanks");
    require(persistence.state.latestSourceDateIso == "2026-09-27",
            "latest source date was not advanced");
    require(persistence.commitCount == 1 && persistence.rollbackCount == 0,
            "successful apply did not commit exactly once");
    require(persistence.calls.front() == "begin"
                && persistence.calls[1] == "load"
                && persistence.calls.back() == "commit",
            "reads, writes, and date update did not share one transaction");
}

void noMatchCreatesAcrossAllNamespaces()
{
    TeacherImportUseCaseRequest request = emptyValidRequest();
    request.koreanTeachers = {koreanSource("kim", u"김하늘")};
    request.validation.koreanTeacherKeys = {"kim"};
    request.nativeEnglishTeachers = {nativeSource("alex", u"alex", u"Alex")};
    request.validation.nativeEnglishTeacherKeys = {"alex"};
    request.gsTeamMembers = {gsSource("taylor", "", u"taylor", {}, u"Taylor")};
    request.validation.gsTeamMemberKeys = {{"taylor", ""}};

    FakeTeacherImportPersistence persistence;
    const auto result = TeacherImportUseCase::execute(request, persistence);
    require(result.has_value(), "no-match plan did not create records");
    require(result->koreanTeachers == TeacherImportCounts{1, 0, 0}
                && result->nativeEnglishTeachers == TeacherImportCounts{1, 0, 0}
                && result->gsTeamMembers == TeacherImportCounts{1, 0, 0},
            "no-match decisions did not increment create counts");
}

void reviewAndPlanRejectionPrecedePersistence()
{
    {
        TeacherImportUseCaseRequest request = emptyValidRequest();
        request.koreanTeachers = {koreanSource("selected", u"김하늘")};
        request.validation.koreanTeacherKeys = {"selected"};
        request.review = TeacherImportReview{
            .candidateGroups = {{"M1", {"different"}}},
            .decisions = {{"M1", TeacherImportGroupMode::All, {}}}
        };
        FakeTeacherImportPersistence persistence;
        const auto result = TeacherImportUseCase::execute(request, persistence);
        require(!result && result.error().kind == TeacherImportUseCaseErrorKind::InvalidPlan
                    && result.error().validationIssue
                        == TeacherImportPlanValidationIssue::ReviewSelectionMismatch,
                "mismatched reviewed plan was not rejected");
        require(persistence.beginCount == 0,
                "mismatched reviewed plan opened a transaction");
    }

    {
        TeacherImportUseCaseRequest request = emptyValidRequest();
        request.koreanTeachers = {koreanSource("selected", u"김하늘")};
        request.validation.koreanTeacherKeys = {"selected"};
        request.review = TeacherImportReview{
            .candidateGroups = {{"M1", {"selected"}}},
            .decisions = {{"M1", TeacherImportGroupMode::Selected, {4}}}
        };
        FakeTeacherImportPersistence persistence;
        const auto result = TeacherImportUseCase::execute(request, persistence);
        require(!result && result.error().kind == TeacherImportUseCaseErrorKind::InvalidReview,
                "invalid review index was not rejected");
        require(persistence.beginCount == 0,
                "invalid review opened a transaction");
    }

    {
        TeacherImportUseCaseRequest request = emptyValidRequest();
        request.nativeEnglishTeachers = {
            nativeSource("same", u"same", u"Same"),
            nativeSource("same", u"same", u" same ")};
        request.validation.nativeEnglishTeacherKeys = {"same", "same"};
        FakeTeacherImportPersistence persistence;
        const auto result = TeacherImportUseCase::execute(request, persistence);
        require(!result && result.error().kind == TeacherImportUseCaseErrorKind::InvalidPlan
                    && result.error().validationIssue
                        == TeacherImportPlanValidationIssue::DuplicateNativeEnglishTeacherName,
                "duplicate normalized plan was not rejected");
        require(persistence.beginCount == 0,
                "invalid normalized plan opened a transaction");
    }
}

void ambiguousMatchRollsBackEarlierWrites()
{
    TeacherImportUseCaseRequest request = emptyValidRequest();
    request.koreanTeachers = {koreanSource("new", u"이서연")};
    request.validation.koreanTeacherKeys = {"new"};
    request.nativeEnglishTeachers = {nativeSource("jamie", u"jamie", u"Jamie")};
    request.validation.nativeEnglishTeacherKeys = {"jamie"};

    FakeTeacherImportPersistence persistence;
    persistence.state.nativeEnglishTeachers = {
        {.profile = {.id = 1, .name = u"Jamie"}, .normalizedMatchKey = u"jamie"},
        {.profile = {.id = 2, .name = u" jamie "}, .normalizedMatchKey = u"jamie"}
    };
    const auto result = TeacherImportUseCase::execute(request, persistence);
    require(!result
                && result.error().kind == TeacherImportUseCaseErrorKind::AmbiguousStoredMatch
                && result.error().recordNamespace
                    == TeacherImportRecordNamespace::NativeEnglishTeacher,
            "multiple stored matches were not rejected by the use case");
    require(persistence.state.koreanTeachers.empty()
                && persistence.state.nativeEnglishTeachers.size() == 2
                && !persistence.state.latestSourceDateIso,
            "ambiguous match did not roll back an earlier Korean create");
    require(persistence.rollbackCount == 1 && persistence.commitCount == 0,
            "ambiguous match did not roll back exactly once");
}

void queryWriteAndCommitFailuresRollBack()
{
    TeacherImportUseCaseRequest request = emptyValidRequest();
    request.koreanTeachers = {koreanSource("kim", u"김하늘")};
    request.validation.koreanTeacherKeys = {"kim"};

    {
        FakeTeacherImportPersistence persistence;
        persistence.failAt = TeacherImportPersistenceOperation::LoadSnapshot;
        const auto result = TeacherImportUseCase::execute(request, persistence);
        require(!result
                    && result.error().kind
                        == TeacherImportUseCaseErrorKind::PersistenceFailure
                    && result.error().persistenceFailure.operation
                        == TeacherImportPersistenceOperation::LoadSnapshot,
                "query failure was hidden from the caller");
        require(persistence.state.koreanTeachers.empty()
                    && persistence.state.nativeEnglishTeachers.empty()
                    && persistence.state.gsTeamMembers.empty()
                    && !persistence.state.latestSourceDateIso
                    && persistence.rollbackCount == 1,
                "query failure did not roll back");
    }

    {
        FakeTeacherImportPersistence persistence;
        persistence.failAt = TeacherImportPersistenceOperation::LoadSnapshot;
        persistence.failRollback = true;
        const auto result = TeacherImportUseCase::execute(request, persistence);
        require(!result && result.error().rollbackFailure.has_value()
                    && result.error().rollbackFailure->operation
                        == TeacherImportPersistenceOperation::RollbackTransaction,
                "rollback failure was hidden from the caller");
        require(persistence.rollbackCount == 1,
                "rollback was not attempted after the query failure");
    }

    {
        FakeTeacherImportPersistence persistence;
        persistence.failAt = TeacherImportPersistenceOperation::CreateKoreanTeacher;
        const auto result = TeacherImportUseCase::execute(request, persistence);
        require(!result
                    && result.error().persistenceFailure.operation
                        == TeacherImportPersistenceOperation::CreateKoreanTeacher,
                "write failure was hidden from the caller");
        require(persistence.state.koreanTeachers.empty()
                    && persistence.rollbackCount == 1,
                "write failure did not roll back");
    }

    {
        FakeTeacherImportPersistence persistence;
        persistence.failAt = TeacherImportPersistenceOperation::CommitTransaction;
        persistence.state.latestSourceDateIso = "2026-01-01";
        const auto result = TeacherImportUseCase::execute(request, persistence);
        require(!result
                    && result.error().persistenceFailure.operation
                        == TeacherImportPersistenceOperation::CommitTransaction,
                "commit failure was hidden from the caller");
        require(persistence.state.koreanTeachers.empty()
                    && persistence.state.latestSourceDateIso == "2026-01-01"
                    && persistence.rollbackCount == 1,
                "commit failure did not restore writes and source date");
    }
}

} // namespace

int main()
{
    try
    {
        successfulOrchestrationAcrossNamespaces();
        noMatchCreatesAcrossAllNamespaces();
        reviewAndPlanRejectionPrecedePersistence();
        ambiguousMatchRollsBackEarlierWrites();
        queryWriteAndCommitFailuresRollBack();
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
