#include "next/application/sub_prep_class_information_state.h"

#include <QtTest/QtTest>

#include <algorithm>
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

ClassId classId(std::string value)
{
    return *ClassId::fromString(value);
}

TeacherId teacherId(std::string value)
{
    return *TeacherId::fromString(value);
}

ClassSummary classSummary(
    std::string id,
    std::optional<TeacherId> teacher = std::nullopt
    )
{
    const std::string displayId = id;
    return ClassSummary{
        classId(std::move(id)),
        std::move(teacher),
        "Grade 4",
        "Level B",
        "Class " + displayId,
        "Mon 09:00",
        18,
        0
    };
}

TeacherSummary teacherSummary(const TeacherId& id)
{
    const std::string displayId = id.value();
    return TeacherSummary{
        id,
        "Teacher " + displayId,
        "Room 4",
        "Notes for " + displayId
    };
}

SubPrepScheduleSummaryQueryResult scope(
    std::vector<ClassSummary> classes
    )
{
    std::vector<TeacherSummary> teachers;
    for (const auto& summary : classes)
    {
        if (!summary.teacherId.has_value())
        {
            continue;
        }

        const auto knownTeacher = std::find_if(
            teachers.cbegin(),
            teachers.cend(),
            [&summary](const TeacherSummary& candidate)
            {
                return candidate.id == *summary.teacherId;
            }
            );
        if (knownTeacher == teachers.cend())
        {
            teachers.push_back(teacherSummary(*summary.teacherId));
        }
    }

    return ClassSummaryProjection::create(ClassSummaryProjectionInput{
        std::move(teachers),
        std::move(classes),
        std::nullopt
    });
}

SubPrepClassDetails details(
    std::string classIdentifier,
    std::optional<TeacherId> teacher = std::nullopt,
    std::string classNotes = "Class notes"
    )
{
    return SubPrepClassDetails{
        classId(std::move(classIdentifier)),
        std::move(teacher),
        std::move(classNotes),
        "Teacher One",
        "Room 1",
        "Teacher notes"
    };
}

void verifyError(
    const Result<SubPrepClassInformationState>& result,
    const ErrorCode expectedCode
    )
{
    QVERIFY(!result);
    QVERIFY(!result.hasValue());
    QCOMPARE(result.error().code, expectedCode);
}

} // namespace

class ClassMngrNextApplicationSubPrepClassInformationStateTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void transitionsUseCopyableValueState();
    void successfulRefreshPreservesVisibleSelectionAndClearsDetails();
    void removedAndEmptyScopesClearSelectionAndDetails();
    void failedRefreshPropagatesErrorAndPreservesState();
    void selectingNewClassClearsDetailsAndSameClassPreservesThem();
    void selectingNonvisibleClassFailsWithoutChangingState();
    void acceptsDetailsMatchingSelectedClassAndTeacher();
    void rejectsDetailsWithoutSelectionOrWithMismatchedClass();
    void rejectsDetailsWithMismatchedTeacherIdentity();
    void rejectsMalformedDetailsWithoutChangingState();
    void changingSelectedTeacherIdentityClearsOldDetails();
    void clearRemovesSelectionAndDetails();
};

void ClassMngrNextApplicationSubPrepClassInformationStateTests::
transitionsUseCopyableValueState()
{
    static_assert(
        std::is_copy_constructible_v<SubPrepClassInformationState>
        );
    static_assert(std::is_copy_assignable_v<SubPrepClassInformationState>);
    static_assert(std::is_same_v<
        decltype(std::declval<const SubPrepClassInformationState&>().
            refreshScope(std::declval<const SubPrepScheduleSummaryQueryResult&>()
                )),
        Result<SubPrepClassInformationState>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<const SubPrepClassInformationState&>().
            selectVisibleClass(
                std::declval<const ClassId&>(),
                std::declval<const ClassSummaryProjection&>()
                )),
        Result<SubPrepClassInformationState>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<const SubPrepClassInformationState&>().
            applyDetails(std::declval<SubPrepClassDetails>())),
        Result<SubPrepClassInformationState>
        >);

    QVERIFY(true);
}

void ClassMngrNextApplicationSubPrepClassInformationStateTests::
successfulRefreshPreservesVisibleSelectionAndClearsDetails()
{
    const auto originalScope = scope({
        classSummary("class-1", teacherId("teacher-1")),
        classSummary("class-2")
    });
    QVERIFY(originalScope);

    const SubPrepClassInformationState emptyState;
    const auto refreshed = emptyState.refreshScope(originalScope);
    QVERIFY(refreshed);

    const auto selected = refreshed.value().selectVisibleClass(
        classId("class-1"),
        originalScope.value()
        );
    QVERIFY(selected);

    const auto loaded = selected.value().applyDetails(
        details("class-1", teacherId("teacher-1"), "Old notes")
        );
    QVERIFY(loaded);
    QVERIFY(loaded.value().details().has_value());

    const auto nextScope = scope({
        classSummary("class-2"),
        classSummary("class-1", teacherId("teacher-1"))
    });
    QVERIFY(nextScope);
    const auto next = loaded.value().refreshScope(nextScope);

    QVERIFY(next);
    QVERIFY(next.value().selectedClassId().has_value());
    QCOMPARE(next.value().selectedClassId()->value(), std::string("class-1"));
    QVERIFY(next.value().selectedTeacherId().has_value());
    QCOMPARE(
        next.value().selectedTeacherId()->value(),
        std::string("teacher-1")
        );
    QVERIFY(!next.value().details().has_value());

    // The transition returns a new snapshot and does not mutate its source.
    QVERIFY(loaded.value().details().has_value());
    QCOMPARE(loaded.value().details()->classNotes, std::string("Old notes"));
}

void ClassMngrNextApplicationSubPrepClassInformationStateTests::
removedAndEmptyScopesClearSelectionAndDetails()
{
    const auto initialScope = scope({
        classSummary("class-1", teacherId("teacher-1")),
        classSummary("class-2")
    });
    QVERIFY(initialScope);
    const auto selected = SubPrepClassInformationState{}
        .refreshScope(initialScope)
        .value()
        .selectVisibleClass(classId("class-1"), initialScope.value());
    QVERIFY(selected);
    const auto loaded = selected.value().applyDetails(
        details("class-1", teacherId("teacher-1"))
        );
    QVERIFY(loaded);

    const auto withoutSelection = scope({classSummary("class-2")});
    QVERIFY(withoutSelection);
    const auto removed = loaded.value().refreshScope(withoutSelection);
    QVERIFY(removed);
    QVERIFY(!removed.value().selectedClassId().has_value());
    QVERIFY(!removed.value().selectedTeacherId().has_value());
    QVERIFY(!removed.value().details().has_value());

    const auto reselected = removed.value().selectVisibleClass(
        classId("class-2"),
        withoutSelection.value()
        );
    QVERIFY(reselected);
    const auto emptying = reselected.value().applyDetails(
        details("class-2")
        );
    QVERIFY(emptying);

    const auto emptyScope = scope({});
    QVERIFY(emptyScope);
    const auto emptied = emptying.value().refreshScope(emptyScope);
    QVERIFY(emptied);
    QVERIFY(!emptied.value().selectedClassId().has_value());
    QVERIFY(!emptied.value().selectedTeacherId().has_value());
    QVERIFY(!emptied.value().details().has_value());
}

void ClassMngrNextApplicationSubPrepClassInformationStateTests::
failedRefreshPropagatesErrorAndPreservesState()
{
    const auto currentScope = scope({
        classSummary("class-1", teacherId("teacher-1"))
    });
    QVERIFY(currentScope);
    const auto selected = SubPrepClassInformationState{}
        .refreshScope(currentScope)
        .value()
        .selectVisibleClass(classId("class-1"), currentScope.value());
    QVERIFY(selected);
    const auto loaded = selected.value().applyDetails(
        details("class-1", teacherId("teacher-1"), "Preserved notes")
        );
    QVERIFY(loaded);
    const SubPrepClassInformationState before = loaded.value();

    const OperationError expectedError{
        .code = ErrorCode::Technical,
        .message = "schedule scope read failed",
        .recoverable = true
    };
    const SubPrepScheduleSummaryQueryResult failedScope =
        SubPrepScheduleSummaryQueryResult::failure(expectedError);
    const auto result = before.refreshScope(failedScope);

    QVERIFY(!result);
    QVERIFY(!result.hasValue());
    QVERIFY(result.error() == expectedError);
    QVERIFY(before == loaded.value());
    QVERIFY(before.selectedClassId().has_value());
    QVERIFY(before.selectedTeacherId().has_value());
    QVERIFY(before.details().has_value());
    QCOMPARE(before.details()->classNotes, std::string("Preserved notes"));
}

void ClassMngrNextApplicationSubPrepClassInformationStateTests::
selectingNewClassClearsDetailsAndSameClassPreservesThem()
{
    const auto currentScope = scope({
        classSummary("class-1", teacherId("teacher-1")),
        classSummary("class-2")
    });
    QVERIFY(currentScope);
    const auto initialSelection = SubPrepClassInformationState{}
        .refreshScope(currentScope)
        .value()
        .selectVisibleClass(classId("class-1"), currentScope.value());
    QVERIFY(initialSelection);
    const auto loadedFirst = initialSelection.value().applyDetails(
        details("class-1", teacherId("teacher-1"), "First class notes")
        );
    QVERIFY(loadedFirst);

    const auto nextSelection = loadedFirst.value().selectVisibleClass(
        classId("class-2"),
        currentScope.value()
        );
    QVERIFY(nextSelection);
    QVERIFY(nextSelection.value().selectedClassId().has_value());
    QCOMPARE(nextSelection.value().selectedClassId()->value(), std::string("class-2"));
    QVERIFY(!nextSelection.value().selectedTeacherId().has_value());
    QVERIFY(!nextSelection.value().details().has_value());

    const auto loadedSecond = nextSelection.value().applyDetails(
        details("class-2", std::nullopt, "Second class notes")
        );
    QVERIFY(loadedSecond);
    const auto sameSelection = loadedSecond.value().selectVisibleClass(
        classId("class-2"),
        currentScope.value()
        );
    QVERIFY(sameSelection);
    QVERIFY(sameSelection.value() == loadedSecond.value());
    QCOMPARE(
        sameSelection.value().details()->classNotes,
        std::string("Second class notes")
        );
}

void ClassMngrNextApplicationSubPrepClassInformationStateTests::
selectingNonvisibleClassFailsWithoutChangingState()
{
    const auto currentScope = scope({
        classSummary("class-1", teacherId("teacher-1"))
    });
    QVERIFY(currentScope);
    const auto selected = SubPrepClassInformationState{}
        .refreshScope(currentScope)
        .value()
        .selectVisibleClass(classId("class-1"), currentScope.value());
    QVERIFY(selected);
    const auto loaded = selected.value().applyDetails(
        details("class-1", teacherId("teacher-1"))
        );
    QVERIFY(loaded);
    const SubPrepClassInformationState before = loaded.value();

    const auto result = before.selectVisibleClass(
        classId("not-visible"),
        currentScope.value()
        );

    verifyError(result, ErrorCode::NotFound);
    QVERIFY(before == loaded.value());
    QCOMPARE(before.selectedClassId()->value(), std::string("class-1"));
    QVERIFY(before.details().has_value());
}

void ClassMngrNextApplicationSubPrepClassInformationStateTests::
acceptsDetailsMatchingSelectedClassAndTeacher()
{
    const auto teacherScope = scope({
        classSummary("class-1", teacherId("teacher-1"))
    });
    QVERIFY(teacherScope);
    const auto teacherSelected = SubPrepClassInformationState{}
        .refreshScope(teacherScope)
        .value()
        .selectVisibleClass(classId("class-1"), teacherScope.value());
    QVERIFY(teacherSelected);

    const auto matchingTeacherDetails = details(
        "class-1",
        teacherId("teacher-1"),
        "Matching teacher notes"
        );
    const auto teacherResult = teacherSelected.value().applyDetails(
        matchingTeacherDetails
        );
    QVERIFY(teacherResult);
    QVERIFY(teacherResult.value().selectedTeacherId().has_value());
    QVERIFY(teacherResult.value().details().has_value());
    QVERIFY(teacherResult.value().details().value() == matchingTeacherDetails);

    const auto noTeacherScope = scope({classSummary("class-no-teacher")});
    QVERIFY(noTeacherScope);
    const auto noTeacherSelected = SubPrepClassInformationState{}
        .refreshScope(noTeacherScope)
        .value()
        .selectVisibleClass(
            classId("class-no-teacher"),
            noTeacherScope.value()
            );
    QVERIFY(noTeacherSelected);
    QVERIFY(!noTeacherSelected.value().selectedTeacherId().has_value());

    const auto matchingNoTeacherDetails = details(
        "class-no-teacher",
        std::nullopt,
        "Missing teacher fallback notes"
        );
    const auto noTeacherResult = noTeacherSelected.value().applyDetails(
        matchingNoTeacherDetails
        );
    QVERIFY(noTeacherResult);
    QVERIFY(noTeacherResult.value().details().has_value());
    QVERIFY(noTeacherResult.value().details().value() == matchingNoTeacherDetails);
}

void ClassMngrNextApplicationSubPrepClassInformationStateTests::
rejectsDetailsWithoutSelectionOrWithMismatchedClass()
{
    const SubPrepClassInformationState emptyState;
    const auto missingSelection = emptyState.applyDetails(
        details("class-1", teacherId("teacher-1"))
        );
    verifyError(missingSelection, ErrorCode::InvalidInput);
    QVERIFY(emptyState == SubPrepClassInformationState{});

    const auto currentScope = scope({
        classSummary("class-1", teacherId("teacher-1"))
    });
    QVERIFY(currentScope);
    const auto selected = SubPrepClassInformationState{}
        .refreshScope(currentScope)
        .value()
        .selectVisibleClass(classId("class-1"), currentScope.value());
    QVERIFY(selected);
    const auto loaded = selected.value().applyDetails(
        details("class-1", teacherId("teacher-1"), "Old notes")
        );
    QVERIFY(loaded);
    const SubPrepClassInformationState before = loaded.value();

    const auto mismatch = before.applyDetails(
        details("class-other", teacherId("teacher-1"), "Wrong class notes")
        );
    verifyError(mismatch, ErrorCode::Validation);
    QVERIFY(before == loaded.value());
    QCOMPARE(before.details()->classNotes, std::string("Old notes"));
}

void ClassMngrNextApplicationSubPrepClassInformationStateTests::
rejectsDetailsWithMismatchedTeacherIdentity()
{
    const auto teacherScope = scope({
        classSummary("class-1", teacherId("teacher-1"))
    });
    QVERIFY(teacherScope);
    const auto selectedTeacher = SubPrepClassInformationState{}
        .refreshScope(teacherScope)
        .value()
        .selectVisibleClass(classId("class-1"), teacherScope.value());
    QVERIFY(selectedTeacher);
    const SubPrepClassInformationState withTeacher = selectedTeacher.value();
    const SubPrepClassInformationState unchangedTeacherState = withTeacher;

    const auto absentDetailsTeacher = withTeacher.applyDetails(
        details("class-1", std::nullopt)
        );
    verifyError(absentDetailsTeacher, ErrorCode::Validation);
    QVERIFY(withTeacher == unchangedTeacherState);

    const auto differentDetailsTeacher = withTeacher.applyDetails(
        details("class-1", teacherId("teacher-2"))
        );
    verifyError(differentDetailsTeacher, ErrorCode::Validation);
    QVERIFY(withTeacher == unchangedTeacherState);

    const auto noTeacherScope = scope({classSummary("class-no-teacher")});
    QVERIFY(noTeacherScope);
    const auto selectedWithoutTeacher = SubPrepClassInformationState{}
        .refreshScope(noTeacherScope)
        .value()
        .selectVisibleClass(
            classId("class-no-teacher"),
            noTeacherScope.value()
            );
    QVERIFY(selectedWithoutTeacher);
    const SubPrepClassInformationState withoutTeacher =
        selectedWithoutTeacher.value();
    const SubPrepClassInformationState unchangedNoTeacherState = withoutTeacher;

    const auto presentDetailsTeacher = withoutTeacher.applyDetails(
        details("class-no-teacher", teacherId("teacher-1"))
        );
    verifyError(presentDetailsTeacher, ErrorCode::Validation);
    QVERIFY(withoutTeacher == unchangedNoTeacherState);
}

void ClassMngrNextApplicationSubPrepClassInformationStateTests::
rejectsMalformedDetailsWithoutChangingState()
{
    const auto currentScope = scope({
        classSummary("class-1", teacherId("teacher-1"))
    });
    QVERIFY(currentScope);
    const auto selected = SubPrepClassInformationState{}
        .refreshScope(currentScope)
        .value()
        .selectVisibleClass(classId("class-1"), currentScope.value());
    QVERIFY(selected);
    const auto loaded = selected.value().applyDetails(
        details("class-1", teacherId("teacher-1"), "Old notes")
        );
    QVERIFY(loaded);
    const SubPrepClassInformationState before = loaded.value();

    const auto malformed = before.applyDetails(details(
        "class-1",
        teacherId("teacher-1"),
        std::string(kSelectedClassDetailsMaxClassNotesLength + 1, 'n')
        ));

    verifyError(malformed, ErrorCode::InvalidInput);
    QVERIFY(before == loaded.value());
    QCOMPARE(before.details()->classNotes, std::string("Old notes"));
}

void ClassMngrNextApplicationSubPrepClassInformationStateTests::
changingSelectedTeacherIdentityClearsOldDetails()
{
    const auto firstScope = scope({
        classSummary("class-1", teacherId("teacher-1"))
    });
    QVERIFY(firstScope);
    const auto selected = SubPrepClassInformationState{}
        .refreshScope(firstScope)
        .value()
        .selectVisibleClass(classId("class-1"), firstScope.value());
    QVERIFY(selected);
    const auto loaded = selected.value().applyDetails(
        details("class-1", teacherId("teacher-1"), "Old teacher details")
        );
    QVERIFY(loaded);

    const auto changedScope = scope({
        classSummary("class-1", teacherId("teacher-2"))
    });
    QVERIFY(changedScope);
    const auto reselection = loaded.value().selectVisibleClass(
        classId("class-1"),
        changedScope.value()
        );

    QVERIFY(reselection);
    QVERIFY(reselection.value().selectedTeacherId().has_value());
    QCOMPARE(
        reselection.value().selectedTeacherId()->value(),
        std::string("teacher-2")
        );
    QVERIFY(!reselection.value().details().has_value());
    QVERIFY(loaded.value().details().has_value());
    QCOMPARE(loaded.value().details()->teacherId->value(), std::string("teacher-1"));
}

void ClassMngrNextApplicationSubPrepClassInformationStateTests::
clearRemovesSelectionAndDetails()
{
    const auto currentScope = scope({
        classSummary("class-1", teacherId("teacher-1"))
    });
    QVERIFY(currentScope);
    const auto selected = SubPrepClassInformationState{}
        .refreshScope(currentScope)
        .value()
        .selectVisibleClass(classId("class-1"), currentScope.value());
    QVERIFY(selected);
    const auto loaded = selected.value().applyDetails(
        details("class-1", teacherId("teacher-1"))
        );
    QVERIFY(loaded);
    const SubPrepClassInformationState before = loaded.value();

    const auto cleared = before.clear();

    QVERIFY(!cleared.selectedClassId().has_value());
    QVERIFY(!cleared.selectedTeacherId().has_value());
    QVERIFY(!cleared.details().has_value());
    QVERIFY(before == loaded.value());
    QVERIFY(before.details().has_value());
}

QTEST_APPLESS_MAIN(ClassMngrNextApplicationSubPrepClassInformationStateTests)

#include "next_application_sub_prep_class_information_state_tests.moc"
