#include "next/application/workspace_state.h"

#include <QtTest/QtTest>

#include <type_traits>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

namespace
{

WorkspaceSession testSession(
    const char* id,
    const char* location
    )
{
    return WorkspaceSession(
        *WorkspaceId::fromString(id),
        WorkspaceLocation(location)
        );
}

}

class NextApplicationStateTests final : public QObject
{
    Q_OBJECT

private slots:
    void initialStateIsClosedAndClean();
    void openCopiesAndReplacesCleanSession();
    void dirtyAndSavedTransitionsAreDeterministic();
    void markSavedAsReplacesLocationAndCleans();
    void invalidMarkSavedAsPreservesState();
    void closeResetsState();
    void invalidTransitionsReturnStructuredErrors();
    void dirtyReplacementAndCloseReturnConflict();
    void snapshotsAreCopyableValuesWithEquality();
};

void NextApplicationStateTests::initialStateIsClosedAndClean()
{
    WorkspaceState state;

    const WorkspaceStateSnapshot snapshot = state.snapshot();

    QCOMPARE(snapshot.lifecycle(), WorkspaceLifecycleState::Closed);
    QCOMPARE(snapshot.unsavedState(), WorkspaceUnsavedState::Clean);
    QVERIFY(!snapshot.session().has_value());
}

void NextApplicationStateTests::openCopiesAndReplacesCleanSession()
{
    WorkspaceState state;
    const WorkspaceSession first = testSession(
        "workspace-1",
        "C:/workspaces/one.tps"
        );
    const WorkspaceSession second = testSession(
        "workspace-2",
        "C:/workspaces/two.tps"
        );

    QVERIFY(state.open(first));
    const WorkspaceStateSnapshot firstSnapshot = state.snapshot();
    QVERIFY(firstSnapshot.lifecycle() == WorkspaceLifecycleState::Open);
    QVERIFY(firstSnapshot.session().has_value());
    QVERIFY(*firstSnapshot.session() == first);
    QCOMPARE(firstSnapshot.unsavedState(), WorkspaceUnsavedState::Clean);

    QVERIFY(state.open(second));
    const WorkspaceStateSnapshot replacementSnapshot = state.snapshot();
    QVERIFY(replacementSnapshot.session().has_value());
    QVERIFY(*replacementSnapshot.session() == second);
    QCOMPARE(replacementSnapshot.unsavedState(), WorkspaceUnsavedState::Clean);
}

void NextApplicationStateTests::dirtyAndSavedTransitionsAreDeterministic()
{
    WorkspaceState state;
    QVERIFY(state.open(testSession("workspace-1", "C:/workspaces/one.tps")));

    QVERIFY(state.markDirty());
    QCOMPARE(state.snapshot().unsavedState(), WorkspaceUnsavedState::Dirty);

    QVERIFY(state.markDirty());
    QCOMPARE(state.snapshot().unsavedState(), WorkspaceUnsavedState::Dirty);

    QVERIFY(state.markSaved());
    QCOMPARE(state.snapshot().unsavedState(), WorkspaceUnsavedState::Clean);

    QVERIFY(state.markSaved());
    QCOMPARE(state.snapshot().unsavedState(), WorkspaceUnsavedState::Clean);
}

void NextApplicationStateTests::markSavedAsReplacesLocationAndCleans()
{
    WorkspaceState state;
    const WorkspaceSession current = testSession(
        "workspace-1",
        "C:/workspaces/one.tps"
        );
    const WorkspaceLocation destination("C:/workspaces/one-copy.tps");
    QVERIFY(state.open(current));
    QVERIFY(state.markDirty());

    const auto result = state.markSavedAs(destination);

    QVERIFY(result);
    const WorkspaceSession expected(current.workspaceId(), destination);
    QVERIFY(state.snapshot().session().has_value());
    QVERIFY(*state.snapshot().session() == expected);
    QCOMPARE(state.snapshot().unsavedState(), WorkspaceUnsavedState::Clean);
}

void NextApplicationStateTests::invalidMarkSavedAsPreservesState()
{
    WorkspaceState state;
    QVERIFY(
        state.open(
            testSession("workspace-1", "C:/workspaces/one.tps")
            )
        );
    QVERIFY(state.markDirty());
    const WorkspaceStateSnapshot before = state.snapshot();

    const auto result = state.markSavedAs(WorkspaceLocation(" \t\r\n"));

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QVERIFY(state.snapshot() == before);
}

void NextApplicationStateTests::closeResetsState()
{
    WorkspaceState state;
    QVERIFY(state.open(testSession("workspace-1", "C:/workspaces/one.tps")));
    QVERIFY(state.markDirty());
    QVERIFY(state.markSaved());

    QVERIFY(state.close());
    const WorkspaceStateSnapshot snapshot = state.snapshot();
    QCOMPARE(snapshot.lifecycle(), WorkspaceLifecycleState::Closed);
    QCOMPARE(snapshot.unsavedState(), WorkspaceUnsavedState::Clean);
    QVERIFY(!snapshot.session().has_value());
}

void NextApplicationStateTests::invalidTransitionsReturnStructuredErrors()
{
    WorkspaceState state;

    const auto dirtyResult = state.markDirty();
    QVERIFY(!dirtyResult);
    QCOMPARE(dirtyResult.error().code, ErrorCode::NotFound);

    const auto savedResult = state.markSaved();
    QVERIFY(!savedResult);
    QCOMPARE(savedResult.error().code, ErrorCode::NotFound);

    const auto closeResult = state.close();
    QVERIFY(!closeResult);
    QCOMPARE(closeResult.error().code, ErrorCode::NotFound);

    const WorkspaceSession invalidSession(
        *WorkspaceId::fromString("workspace-1"),
        WorkspaceLocation(" \t\r\n")
        );
    const auto openResult = state.open(invalidSession);
    QVERIFY(!openResult);
    QCOMPARE(openResult.error().code, ErrorCode::InvalidInput);

    const WorkspaceStateSnapshot snapshot = state.snapshot();
    QCOMPARE(snapshot.lifecycle(), WorkspaceLifecycleState::Closed);
    QCOMPARE(snapshot.unsavedState(), WorkspaceUnsavedState::Clean);
}

void NextApplicationStateTests::dirtyReplacementAndCloseReturnConflict()
{
    WorkspaceState state;
    const WorkspaceSession first = testSession(
        "workspace-1",
        "C:/workspaces/one.tps"
        );
    const WorkspaceSession second = testSession(
        "workspace-2",
        "C:/workspaces/two.tps"
        );

    QVERIFY(state.open(first));
    QVERIFY(state.markDirty());

    const auto replaceResult = state.open(second);
    QVERIFY(!replaceResult);
    QCOMPARE(replaceResult.error().code, ErrorCode::Conflict);
    QVERIFY(replaceResult.error().recoverable);
    QVERIFY(
        replaceResult.error().message
        == "Cannot replace a workspace with unsaved changes."
        );

    const auto closeResult = state.close();
    QVERIFY(!closeResult);
    QCOMPARE(closeResult.error().code, ErrorCode::Conflict);
    QVERIFY(closeResult.error().recoverable);
    QVERIFY(
        closeResult.error().message
        == "Cannot close a workspace with unsaved changes."
        );

    const WorkspaceStateSnapshot snapshot = state.snapshot();
    QVERIFY(snapshot.session().has_value());
    QVERIFY(*snapshot.session() == first);
    QCOMPARE(snapshot.unsavedState(), WorkspaceUnsavedState::Dirty);
}

void NextApplicationStateTests::snapshotsAreCopyableValuesWithEquality()
{
    static_assert(std::is_copy_constructible_v<WorkspaceStateSnapshot>);
    static_assert(std::is_copy_assignable_v<WorkspaceStateSnapshot>);

    WorkspaceState state;
    QVERIFY(state.open(testSession("workspace-1", "C:/workspaces/one.tps")));
    QVERIFY(state.markDirty());

    const WorkspaceStateSnapshot original = state.snapshot();
    const WorkspaceStateSnapshot copy = original;
    QVERIFY(copy == original);

    QVERIFY(state.markSaved());
    QCOMPARE(original.unsavedState(), WorkspaceUnsavedState::Dirty);
    QCOMPARE(state.snapshot().unsavedState(), WorkspaceUnsavedState::Clean);
}

QTEST_APPLESS_MAIN(NextApplicationStateTests)

#include "next_application_state_tests.moc"
