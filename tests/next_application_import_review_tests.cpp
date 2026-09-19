#include "next/application/import_review_session.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

namespace
{

TeacherId teacherId(
    const char* value
    )
{
    return *TeacherId::fromString(value);
}

ClassId classId(
    const char* value
    )
{
    return *ClassId::fromString(value);
}

ImportReviewSessionInput resolvedInput()
{
    const auto teacherDecision = TeacherImportDecision::create(
        "teacher-source",
        ImportDecisionAction::UpdateOrReplace,
        teacherId("teacher-1")
        );
    const auto classDecision = ClassImportDecision::create(
        "class-source",
        ImportDecisionAction::Create
        );

    Q_ASSERT(teacherDecision);
    Q_ASSERT(classDecision);

    ImportReviewSessionInput input;
    input.teacherMatchIndexes.push_back(
        TeacherMatchIndex{
            .sourceKey = "teacher-source",
            .candidates = {teacherId("teacher-1"), teacherId("teacher-2")}
        }
        );
    input.classMatchIndexes.push_back(
        ClassMatchIndex{
            .sourceKey = "class-source",
            .candidates = {classId("class-1")}
        }
        );
    input.teacherDecisions.push_back(teacherDecision.value());
    input.classDecisions.push_back(classDecision.value());
    return input;
}

}

class NextApplicationImportReviewTests final : public QObject
{
    Q_OBJECT

private slots:
    void typedMatchIndexesKeepTeacherAndClassIdsSeparate();
    void decisionsExposeExplicitActionsAndTypedTargets();
    void diagnosticsRemainSeparateAndCategorySpecific();
    void readyToApplyRequiresResolvedDecisionsAndNoConflicts();
    void invalidDecisionsReturnStructuredInvalidInput();
    void invalidSessionTextReturnsStructuredInvalidInput();
    void topLevelCollectionLimitReturnsStructuredInvalidInput();
    void candidateListLimitReturnsStructuredInvalidInput();
    void typedIdentifierLimitsReturnStructuredInvalidInput();
    void sessionsAreCopyableEqualValueSnapshots();
};

void NextApplicationImportReviewTests::typedMatchIndexesKeepTeacherAndClassIdsSeparate()
{
    static_assert(!std::is_same_v<TeacherId, ClassId>);
    static_assert(!std::is_convertible_v<TeacherId, ClassId>);
    static_assert(!std::is_convertible_v<ClassId, TeacherId>);
    static_assert(
        !std::is_constructible_v<
            TeacherMatchIndex,
            ImportSourceKey,
            std::vector<ClassId>
            >
        );

    const auto result = ImportReviewSession::create(resolvedInput());

    QVERIFY(result);
    const auto& session = result.value();
    QCOMPARE(session.teacherMatchIndexes().size(), std::size_t(1));
    QCOMPARE(session.classMatchIndexes().size(), std::size_t(1));
    QVERIFY(
        session.teacherMatchIndexes().front().candidates.front().value()
        == "teacher-1"
        );
    QVERIFY(
        session.classMatchIndexes().front().candidates.front().value()
        == "class-1"
        );
    QVERIFY(session.teacherMatches() == session.teacherMatchIndexes());
    QVERIFY(session.classMatches() == session.classMatchIndexes());
}

void NextApplicationImportReviewTests::decisionsExposeExplicitActionsAndTypedTargets()
{
    const auto teacherUpdate = TeacherImportDecision::create(
        "teacher-source",
        ImportDecisionAction::UpdateOrReplace,
        teacherId("teacher-1")
        );
    const auto teacherCreate = TeacherImportDecision::create(
        "teacher-new",
        ImportDecisionAction::Create
        );
    const auto classSkip = ClassImportDecision::create(
        "class-source",
        ImportDecisionAction::Skip
        );

    QVERIFY(teacherUpdate);
    QVERIFY(teacherCreate);
    QVERIFY(classSkip);

    QCOMPARE(
        teacherUpdate.value().action(),
        ImportDecisionAction::UpdateOrReplace
        );
    QVERIFY(teacherUpdate.value().targetId().has_value());
    QVERIFY(teacherUpdate.value().targetId()->value() == "teacher-1");
    QVERIFY(teacherUpdate.value().isResolved());
    QCOMPARE(teacherCreate.value().action(), ImportDecisionAction::Create);
    QVERIFY(!teacherCreate.value().targetId().has_value());
    QCOMPARE(classSkip.value().action(), ImportDecisionAction::Skip);
    QVERIFY(!classSkip.value().targetId().has_value());

    static_assert(
        !std::is_same_v<
            decltype(teacherUpdate.value().targetId()),
            const std::optional<ClassId>&
            >
        );
}

void NextApplicationImportReviewTests::diagnosticsRemainSeparateAndCategorySpecific()
{
    auto input = resolvedInput();
    input.warnings.push_back(
        ImportWarning{
            .category = ImportReviewCategory::Teacher,
            .kind = ImportWarningKind::NormalizedValue,
            .sourceKey = "teacher-source",
            .field = "name",
            .message = "Name was normalized."
        }
        );
    input.unmatchedValues.push_back(
        ImportUnmatchedValue{
            .category = ImportReviewCategory::Class,
            .kind = ImportUnmatchedValueKind::Unknown,
            .sourceKey = "class-source",
            .field = "teacher",
            .message = "Teacher value was not matched."
        }
        );
    input.conflicts.push_back(
        ImportConflict{
            .category = ImportReviewCategory::Class,
            .kind = ImportConflictKind::ExistingRecord,
            .sourceKey = "class-source",
            .field = "name",
            .message = "Existing class requires review."
        }
        );

    const auto result = ImportReviewSession::create(std::move(input));

    QVERIFY(result);
    const auto& session = result.value();
    QCOMPARE(session.warnings().size(), std::size_t(1));
    QCOMPARE(session.unmatchedValues().size(), std::size_t(1));
    QCOMPARE(session.conflicts().size(), std::size_t(1));
    QCOMPARE(
        session.warnings().front().category,
        ImportReviewCategory::Teacher
        );
    QCOMPARE(
        session.unmatchedValues().front().category,
        ImportReviewCategory::Class
        );
    QCOMPARE(
        session.conflicts().front().kind,
        ImportConflictKind::ExistingRecord
        );
}

void NextApplicationImportReviewTests::readyToApplyRequiresResolvedDecisionsAndNoConflicts()
{
    const auto readyResult = ImportReviewSession::create(resolvedInput());
    QVERIFY(readyResult);
    QVERIFY(!readyResult.value().hasConflicts());
    QVERIFY(!readyResult.value().hasUnresolvedDecisions());
    QVERIFY(readyResult.value().readyToApply());

    auto unresolvedInput = resolvedInput();
    const auto unresolved = TeacherImportDecision::create(
        "teacher-unresolved",
        ImportDecisionAction::Unresolved
        );
    QVERIFY(unresolved);
    unresolvedInput.teacherDecisions.push_back(unresolved.value());

    const auto unresolvedResult = ImportReviewSession::create(
        std::move(unresolvedInput)
        );
    QVERIFY(unresolvedResult);
    QVERIFY(unresolvedResult.value().hasUnresolvedDecisions());
    QVERIFY(!unresolvedResult.value().readyToApply());

    auto conflictInput = resolvedInput();
    conflictInput.conflicts.push_back(
        ImportConflict{
            .category = ImportReviewCategory::Teacher,
            .kind = ImportConflictKind::AmbiguousMatch,
            .sourceKey = "teacher-source",
            .field = "name",
            .message = "More than one teacher matched."
        }
        );

    const auto conflictResult = ImportReviewSession::create(
        std::move(conflictInput)
        );
    QVERIFY(conflictResult);
    QVERIFY(conflictResult.value().hasConflicts());
    QVERIFY(!conflictResult.value().readyToApply());
}

void NextApplicationImportReviewTests::invalidDecisionsReturnStructuredInvalidInput()
{
    const auto emptyKey = TeacherImportDecision::create(
        "",
        ImportDecisionAction::Create
        );
    QVERIFY(!emptyKey);
    QCOMPARE(emptyKey.error().code, ErrorCode::InvalidInput);

    const auto createWithTarget = TeacherImportDecision::create(
        "teacher-source",
        ImportDecisionAction::Create,
        teacherId("teacher-1")
        );
    QVERIFY(!createWithTarget);
    QCOMPARE(createWithTarget.error().code, ErrorCode::InvalidInput);

    const auto updateWithoutTarget = ClassImportDecision::create(
        "class-source",
        ImportDecisionAction::UpdateOrReplace
        );
    QVERIFY(!updateWithoutTarget);
    QCOMPARE(updateWithoutTarget.error().code, ErrorCode::InvalidInput);

    const auto unresolvedWithTarget = ClassImportDecision::create(
        "class-source",
        ImportDecisionAction::Unresolved,
        classId("class-1")
        );
    QVERIFY(!unresolvedWithTarget);
    QCOMPARE(unresolvedWithTarget.error().code, ErrorCode::InvalidInput);
}

void NextApplicationImportReviewTests::invalidSessionTextReturnsStructuredInvalidInput()
{
    auto input = resolvedInput();
    input.classMatchIndexes.front().sourceKey.clear();

    const auto result = ImportReviewSession::create(std::move(input));

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);

    auto longDiagnosticInput = resolvedInput();
    longDiagnosticInput.warnings.push_back(
        ImportWarning{
            .category = ImportReviewCategory::Teacher,
            .kind = ImportWarningKind::General,
            .sourceKey = "teacher-source",
            .field = "name",
            .message = std::string(
                kImportReviewMaxMessageLength + 1,
                'x'
                )
        }
        );

    const auto longDiagnosticResult = ImportReviewSession::create(
        std::move(longDiagnosticInput)
        );
    QVERIFY(!longDiagnosticResult);
    QCOMPARE(longDiagnosticResult.error().code, ErrorCode::InvalidInput);
}

void NextApplicationImportReviewTests::topLevelCollectionLimitReturnsStructuredInvalidInput()
{
    auto input = resolvedInput();
    input.warnings.resize(kImportReviewMaxDiagnosticEntries + 1);

    const auto result = ImportReviewSession::create(std::move(input));

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
}

void NextApplicationImportReviewTests::candidateListLimitReturnsStructuredInvalidInput()
{
    auto input = resolvedInput();
    input.teacherMatchIndexes.front().candidates.assign(
        kImportReviewMaxCandidatesPerIndex + 1,
        teacherId("teacher-candidate")
        );

    const auto result = ImportReviewSession::create(std::move(input));

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
}

void NextApplicationImportReviewTests::typedIdentifierLimitsReturnStructuredInvalidInput()
{
    const auto longIdentifier = *TeacherId::fromString(
        std::string(kImportReviewMaxIdentifierLength + 1, 't')
        );

    const auto longTarget = TeacherImportDecision::create(
        "teacher-long-target",
        ImportDecisionAction::UpdateOrReplace,
        longIdentifier
        );
    QVERIFY(!longTarget);
    QCOMPARE(longTarget.error().code, ErrorCode::InvalidInput);

    auto input = resolvedInput();
    input.teacherMatchIndexes.front().candidates.push_back(longIdentifier);

    const auto longCandidateResult = ImportReviewSession::create(
        std::move(input)
        );
    QVERIFY(!longCandidateResult);
    QCOMPARE(longCandidateResult.error().code, ErrorCode::InvalidInput);
}

void NextApplicationImportReviewTests::sessionsAreCopyableEqualValueSnapshots()
{
    static_assert(std::is_copy_constructible_v<ImportReviewSession>);
    static_assert(std::is_copy_assignable_v<ImportReviewSession>);
    static_assert(std::is_copy_constructible_v<TeacherImportDecision>);
    static_assert(std::is_copy_assignable_v<TeacherImportDecision>);
    static_assert(std::is_copy_constructible_v<ClassImportDecision>);
    static_assert(std::is_copy_assignable_v<ClassImportDecision>);

    auto input = resolvedInput();
    const auto result = ImportReviewSession::create(input);
    QVERIFY(result);

    const ImportReviewSession original = result.value();
    const ImportReviewSession copy = original;
    QVERIFY(copy == original);

    input.teacherMatchIndexes.clear();
    input.teacherDecisions.clear();
    QVERIFY(original == copy);
    QCOMPARE(original.teacherMatchIndexes().size(), std::size_t(1));
    QCOMPARE(original.teacherDecisions().size(), std::size_t(1));

    ImportReviewSession assigned;
    assigned = original;
    QVERIFY(assigned == original);
}

QTEST_APPLESS_MAIN(NextApplicationImportReviewTests)

#include "next_application_import_review_tests.moc"
