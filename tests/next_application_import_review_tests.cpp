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

ImportSourceKey importSourceKey(
    std::string value
    )
{
    return ImportSourceKey(std::move(value));
}

ImportReviewSessionInput resolvedInput()
{
    const auto teacherDecision = TeacherImportDecision::create(
        importSourceKey("teacher-source"),
        ImportDecisionAction::UpdateOrReplace,
        teacherId("teacher-1")
        );
    const auto classDecision = ClassImportDecision::create(
        importSourceKey("class-source"),
        ImportDecisionAction::Create
        );

    Q_ASSERT(teacherDecision);
    Q_ASSERT(classDecision);

    ImportReviewSessionInput input;
    input.teacherMatchIndexes.push_back(
        TeacherMatchIndex{
            .sourceKey = importSourceKey("teacher-source"),
            .candidates = {teacherId("teacher-1"), teacherId("teacher-2")}
        }
        );
    input.classMatchIndexes.push_back(
        ClassMatchIndex{
            .sourceKey = importSourceKey("class-source"),
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
    void sourceKeysAreExplicitExactValueTypes();
    void sourceKeyValidationPreservesWhitespaceAndByteLimits();
    void sourceKeyCollectionsRetainInputOrder();
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
    static_assert(!std::is_convertible_v<std::string, ImportSourceKey>);
    static_assert(!std::is_convertible_v<ImportSourceKey, std::string>);

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

void NextApplicationImportReviewTests::sourceKeysAreExplicitExactValueTypes()
{
    static_assert(std::is_default_constructible_v<ImportSourceKey>);
    static_assert(std::is_copy_constructible_v<ImportSourceKey>);
    static_assert(!std::is_convertible_v<std::string, ImportSourceKey>);
    static_assert(!std::is_convertible_v<ImportSourceKey, std::string>);
    static_assert(!std::is_same_v<ImportSourceKey, TeacherId>);
    static_assert(!std::is_same_v<ImportSourceKey, ClassId>);

    const ImportSourceKey emptyKey;
    QVERIFY(emptyKey.empty());
    QVERIFY(emptyKey.value().empty());

    const std::string exactBytes(" \t\0\xC3\xA9\xFF! ", 8);
    const auto key = importSourceKey(exactBytes);
    const auto equalKey = importSourceKey(exactBytes);
    const auto differentKey = importSourceKey(exactBytes + "x");

    QVERIFY(key.value() == exactBytes);
    QVERIFY(key == equalKey);
    QVERIFY(key != differentKey);
    QVERIFY(key < differentKey);
    QVERIFY(!(differentKey < key));

    const auto teacherDecision = TeacherImportDecision::create(
        key,
        ImportDecisionAction::Create
        );
    const auto classDecision = ClassImportDecision::create(
        key,
        ImportDecisionAction::Skip
        );

    QVERIFY(teacherDecision);
    QVERIFY(classDecision);
    QVERIFY(teacherDecision.value().sourceKey() == key);
    QVERIFY(classDecision.value().sourceKey() == key);

    auto input = resolvedInput();
    input.teacherMatchIndexes.front().sourceKey = key;
    input.classMatchIndexes.front().sourceKey = key;
    input.teacherDecisions = {teacherDecision.value()};
    input.classDecisions = {classDecision.value()};
    input.warnings.push_back(ImportWarning{
        .category = ImportReviewCategory::Teacher,
        .kind = ImportWarningKind::General,
        .sourceKey = key,
        .field = "name",
        .message = "Exact source bytes."
    });
    input.unmatchedValues.push_back(ImportUnmatchedValue{
        .category = ImportReviewCategory::Class,
        .kind = ImportUnmatchedValueKind::Unknown,
        .sourceKey = key,
        .field = "teacher",
        .message = "Exact source bytes."
    });
    input.conflicts.push_back(ImportConflict{
        .category = ImportReviewCategory::Class,
        .kind = ImportConflictKind::ExistingRecord,
        .sourceKey = key,
        .field = "name",
        .message = "Exact source bytes."
    });

    const auto result = ImportReviewSession::create(std::move(input));
    QVERIFY(result);
    const auto& session = result.value();
    QVERIFY(session.teacherMatchIndexes().front().sourceKey.value() == exactBytes);
    QVERIFY(session.classMatchIndexes().front().sourceKey.value() == exactBytes);
    QVERIFY(session.teacherDecisions().front().sourceKey().value() == exactBytes);
    QVERIFY(session.classDecisions().front().sourceKey().value() == exactBytes);
    QVERIFY(session.warnings().front().sourceKey.value() == exactBytes);
    QVERIFY(session.unmatchedValues().front().sourceKey.value() == exactBytes);
    QVERIFY(session.conflicts().front().sourceKey.value() == exactBytes);
}

void NextApplicationImportReviewTests::sourceKeyValidationPreservesWhitespaceAndByteLimits()
{
    const std::string whitespace(" \t\n\r\v\f", 6);
    const std::string maximumLength(
        kImportReviewMaxSourceKeyLength,
        'k'
        );
    const std::string overMaximumLength(
        kImportReviewMaxSourceKeyLength + 1,
        'k'
        );

    const auto blankTeacherDecision = TeacherImportDecision::create(
        importSourceKey(whitespace),
        ImportDecisionAction::Create
        );
    const auto blankClassDecision = ClassImportDecision::create(
        importSourceKey(whitespace),
        ImportDecisionAction::Create
        );
    QVERIFY(!blankTeacherDecision);
    QVERIFY(!blankClassDecision);
    QCOMPARE(blankTeacherDecision.error().code, ErrorCode::InvalidInput);
    QCOMPARE(blankClassDecision.error().code, ErrorCode::InvalidInput);

    const auto maximumTeacherDecision = TeacherImportDecision::create(
        importSourceKey(maximumLength),
        ImportDecisionAction::Create
        );
    const auto maximumClassDecision = ClassImportDecision::create(
        importSourceKey(maximumLength),
        ImportDecisionAction::Create
        );
    QVERIFY(maximumTeacherDecision);
    QVERIFY(maximumClassDecision);

    const auto overlongTeacherDecision = TeacherImportDecision::create(
        importSourceKey(overMaximumLength),
        ImportDecisionAction::Create
        );
    const auto overlongClassDecision = ClassImportDecision::create(
        importSourceKey(overMaximumLength),
        ImportDecisionAction::Create
        );
    QVERIFY(!overlongTeacherDecision);
    QVERIFY(!overlongClassDecision);
    QCOMPARE(overlongTeacherDecision.error().code, ErrorCode::InvalidInput);
    QCOMPARE(overlongClassDecision.error().code, ErrorCode::InvalidInput);

    const auto sessionRejectsKey = [](auto assignInvalidKey)
    {
        auto input = resolvedInput();
        assignInvalidKey(input);
        const auto result = ImportReviewSession::create(std::move(input));
        return !result && result.error().code == ErrorCode::InvalidInput;
    };

    QVERIFY(sessionRejectsKey([&](ImportReviewSessionInput& input)
    {
        input.teacherMatchIndexes.front().sourceKey = importSourceKey(whitespace);
    }));
    QVERIFY(sessionRejectsKey([&](ImportReviewSessionInput& input)
    {
        input.classMatchIndexes.front().sourceKey = importSourceKey(whitespace);
    }));
    QVERIFY(sessionRejectsKey([&](ImportReviewSessionInput& input)
    {
        input.warnings.push_back(ImportWarning{
            .category = ImportReviewCategory::Teacher,
            .kind = ImportWarningKind::General,
            .sourceKey = importSourceKey(whitespace),
            .field = "name",
            .message = "Blank source key."
        });
    }));
    QVERIFY(sessionRejectsKey([&](ImportReviewSessionInput& input)
    {
        input.unmatchedValues.push_back(ImportUnmatchedValue{
            .category = ImportReviewCategory::Class,
            .kind = ImportUnmatchedValueKind::Unknown,
            .sourceKey = importSourceKey(whitespace),
            .field = "teacher",
            .message = "Blank source key."
        });
    }));
    QVERIFY(sessionRejectsKey([&](ImportReviewSessionInput& input)
    {
        input.conflicts.push_back(ImportConflict{
            .category = ImportReviewCategory::Class,
            .kind = ImportConflictKind::ExistingRecord,
            .sourceKey = importSourceKey(whitespace),
            .field = "name",
            .message = "Blank source key."
        });
    }));

    QVERIFY(sessionRejectsKey([&](ImportReviewSessionInput& input)
    {
        input.teacherMatchIndexes.front().sourceKey =
            importSourceKey(overMaximumLength);
    }));
    QVERIFY(sessionRejectsKey([&](ImportReviewSessionInput& input)
    {
        input.classMatchIndexes.front().sourceKey = importSourceKey(overMaximumLength);
    }));
    QVERIFY(sessionRejectsKey([&](ImportReviewSessionInput& input)
    {
        input.warnings.push_back(ImportWarning{
            .category = ImportReviewCategory::Teacher,
            .kind = ImportWarningKind::General,
            .sourceKey = importSourceKey(overMaximumLength),
            .field = "name",
            .message = "Overlong source key."
        });
    }));
    QVERIFY(sessionRejectsKey([&](ImportReviewSessionInput& input)
    {
        input.unmatchedValues.push_back(ImportUnmatchedValue{
            .category = ImportReviewCategory::Class,
            .kind = ImportUnmatchedValueKind::Unknown,
            .sourceKey = importSourceKey(overMaximumLength),
            .field = "teacher",
            .message = "Overlong source key."
        });
    }));
    QVERIFY(sessionRejectsKey([&](ImportReviewSessionInput& input)
    {
        input.conflicts.push_back(ImportConflict{
            .category = ImportReviewCategory::Class,
            .kind = ImportConflictKind::ExistingRecord,
            .sourceKey = importSourceKey(overMaximumLength),
            .field = "name",
            .message = "Overlong source key."
        });
    }));

    auto validMaximumInput = resolvedInput();
    validMaximumInput.teacherMatchIndexes.front().sourceKey =
        importSourceKey(maximumLength);
    validMaximumInput.classMatchIndexes.front().sourceKey =
        importSourceKey(maximumLength);
    validMaximumInput.teacherDecisions.front() = maximumTeacherDecision.value();
    validMaximumInput.classDecisions.front() = maximumClassDecision.value();
    validMaximumInput.warnings.push_back(ImportWarning{
        .category = ImportReviewCategory::Teacher,
        .kind = ImportWarningKind::General,
        .sourceKey = importSourceKey(maximumLength),
        .field = "name",
        .message = "Maximum length source key."
    });
    validMaximumInput.unmatchedValues.push_back(ImportUnmatchedValue{
        .category = ImportReviewCategory::Class,
        .kind = ImportUnmatchedValueKind::Unknown,
        .sourceKey = importSourceKey(maximumLength),
        .field = "teacher",
        .message = "Maximum length source key."
    });
    validMaximumInput.conflicts.push_back(ImportConflict{
        .category = ImportReviewCategory::Class,
        .kind = ImportConflictKind::ExistingRecord,
        .sourceKey = importSourceKey(maximumLength),
        .field = "name",
        .message = "Maximum length source key."
    });

    const auto validMaximumResult = ImportReviewSession::create(
        std::move(validMaximumInput)
        );
    QVERIFY(validMaximumResult);
}

void NextApplicationImportReviewTests::sourceKeyCollectionsRetainInputOrder()
{
    auto input = resolvedInput();
    input.teacherMatchIndexes = {
        TeacherMatchIndex{
            .sourceKey = importSourceKey("z-source"),
            .candidates = {teacherId("teacher-1")}
        },
        TeacherMatchIndex{
            .sourceKey = importSourceKey("a-source"),
            .candidates = {teacherId("teacher-2")}
        },
        TeacherMatchIndex{
            .sourceKey = importSourceKey("z-source"),
            .candidates = {teacherId("teacher-1")}
        }
    };
    input.warnings = {
        ImportWarning{
            .category = ImportReviewCategory::Teacher,
            .kind = ImportWarningKind::General,
            .sourceKey = importSourceKey("z-warning"),
            .field = "name",
            .message = "First warning."
        },
        ImportWarning{
            .category = ImportReviewCategory::Teacher,
            .kind = ImportWarningKind::General,
            .sourceKey = importSourceKey("a-warning"),
            .field = "name",
            .message = "Second warning."
        }
    };
    input.unmatchedValues = {
        ImportUnmatchedValue{
            .category = ImportReviewCategory::Class,
            .kind = ImportUnmatchedValueKind::Unknown,
            .sourceKey = importSourceKey("z-unmatched"),
            .field = "teacher",
            .message = "First unmatched value."
        },
        ImportUnmatchedValue{
            .category = ImportReviewCategory::Class,
            .kind = ImportUnmatchedValueKind::Unknown,
            .sourceKey = importSourceKey("a-unmatched"),
            .field = "teacher",
            .message = "Second unmatched value."
        }
    };
    input.conflicts = {
        ImportConflict{
            .category = ImportReviewCategory::Class,
            .kind = ImportConflictKind::ExistingRecord,
            .sourceKey = importSourceKey("z-conflict"),
            .field = "name",
            .message = "First conflict."
        },
        ImportConflict{
            .category = ImportReviewCategory::Class,
            .kind = ImportConflictKind::ExistingRecord,
            .sourceKey = importSourceKey("a-conflict"),
            .field = "name",
            .message = "Second conflict."
        }
    };

    const auto result = ImportReviewSession::create(std::move(input));
    QVERIFY(result);

    const auto& session = result.value();
    QCOMPARE(session.teacherMatchIndexes().size(), std::size_t(3));
    QVERIFY(session.teacherMatchIndexes()[0].sourceKey.value() == "z-source");
    QVERIFY(session.teacherMatchIndexes()[1].sourceKey.value() == "a-source");
    QVERIFY(session.teacherMatchIndexes()[2].sourceKey.value() == "z-source");
    QVERIFY(session.warnings()[0].sourceKey.value() == "z-warning");
    QVERIFY(session.warnings()[1].sourceKey.value() == "a-warning");
    QVERIFY(session.unmatchedValues()[0].sourceKey.value() == "z-unmatched");
    QVERIFY(session.unmatchedValues()[1].sourceKey.value() == "a-unmatched");
    QVERIFY(session.conflicts()[0].sourceKey.value() == "z-conflict");
    QVERIFY(session.conflicts()[1].sourceKey.value() == "a-conflict");
}

void NextApplicationImportReviewTests::decisionsExposeExplicitActionsAndTypedTargets()
{
    const auto teacherUpdate = TeacherImportDecision::create(
        importSourceKey("teacher-source"),
        ImportDecisionAction::UpdateOrReplace,
        teacherId("teacher-1")
        );
    const auto teacherCreate = TeacherImportDecision::create(
        importSourceKey("teacher-new"),
        ImportDecisionAction::Create
        );
    const auto classSkip = ClassImportDecision::create(
        importSourceKey("class-source"),
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
            .sourceKey = importSourceKey("teacher-source"),
            .field = "name",
            .message = "Name was normalized."
        }
        );
    input.unmatchedValues.push_back(
        ImportUnmatchedValue{
            .category = ImportReviewCategory::Class,
            .kind = ImportUnmatchedValueKind::Unknown,
            .sourceKey = importSourceKey("class-source"),
            .field = "teacher",
            .message = "Teacher value was not matched."
        }
        );
    input.conflicts.push_back(
        ImportConflict{
            .category = ImportReviewCategory::Class,
            .kind = ImportConflictKind::ExistingRecord,
            .sourceKey = importSourceKey("class-source"),
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
        importSourceKey("teacher-unresolved"),
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
            .sourceKey = importSourceKey("teacher-source"),
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
        importSourceKey(""),
        ImportDecisionAction::Create
        );
    QVERIFY(!emptyKey);
    QCOMPARE(emptyKey.error().code, ErrorCode::InvalidInput);

    const auto createWithTarget = TeacherImportDecision::create(
        importSourceKey("teacher-source"),
        ImportDecisionAction::Create,
        teacherId("teacher-1")
        );
    QVERIFY(!createWithTarget);
    QCOMPARE(createWithTarget.error().code, ErrorCode::InvalidInput);

    const auto updateWithoutTarget = ClassImportDecision::create(
        importSourceKey("class-source"),
        ImportDecisionAction::UpdateOrReplace
        );
    QVERIFY(!updateWithoutTarget);
    QCOMPARE(updateWithoutTarget.error().code, ErrorCode::InvalidInput);

    const auto unresolvedWithTarget = ClassImportDecision::create(
        importSourceKey("class-source"),
        ImportDecisionAction::Unresolved,
        classId("class-1")
        );
    QVERIFY(!unresolvedWithTarget);
    QCOMPARE(unresolvedWithTarget.error().code, ErrorCode::InvalidInput);
}

void NextApplicationImportReviewTests::invalidSessionTextReturnsStructuredInvalidInput()
{
    auto input = resolvedInput();
    input.classMatchIndexes.front().sourceKey = ImportSourceKey{};

    const auto result = ImportReviewSession::create(std::move(input));

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);

    auto longDiagnosticInput = resolvedInput();
    longDiagnosticInput.warnings.push_back(
        ImportWarning{
            .category = ImportReviewCategory::Teacher,
            .kind = ImportWarningKind::General,
            .sourceKey = importSourceKey("teacher-source"),
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
        importSourceKey("teacher-long-target"),
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
