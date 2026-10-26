#include "next/application/preferences_model.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

void expect(const bool condition, const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

void expectApplyResult(
    const Domain::Result<bool>& result,
    const bool expectedChanged,
    const std::string& message
    )
{
    expect(static_cast<bool>(result), message);
    expect(result.value() == expectedChanged, message);
}

Domain::OperationError error(
    const Domain::ErrorCode code,
    std::string message
    )
{
    return {
        .code = code,
        .message = std::move(message),
        .recoverable = false
    };
}

UserPreferencesState stateWith(
    const ThemePreference theme,
    const LanguagePreference language = LanguagePreference::SystemDefault
    )
{
    UserPreferencesState state;
    expect(
        static_cast<bool>(state.setThemePreference(theme)),
        "Test state theme should be valid."
        );
    expect(
        static_cast<bool>(state.setLanguagePreference(language)),
        "Test state language should be valid."
        );
    return state;
}

class FakeReadPort final : public PreferencesStateReadPort
{
public:
    mutable int calls = 0;
    PreferencesStateReadResult result =
        PreferencesStateReadResult::failure(error(
            Domain::ErrorCode::Technical,
            "Unconfigured preference reader."
            ));

    [[nodiscard]] PreferencesStateReadResult read() const override
    {
        ++calls;
        if (!result)
        {
            return PreferencesStateReadResult::failure(result.error());
        }
        return PreferencesStateReadResult::success(result.value());
    }
};

class FakePersistPort final : public PreferencesSnapshotPersistPort
{
public:
    mutable int calls = 0;
    mutable std::vector<UserPreferencesSnapshot> writes;
    PreferencesPersistResult result = PreferencesPersistResult::success();

    [[nodiscard]] PreferencesPersistResult persist(
        const UserPreferencesSnapshot& snapshot
        ) const override
    {
        ++calls;
        writes.push_back(snapshot);
        if (!result)
        {
            return PreferencesPersistResult::failure(result.error());
        }
        return PreferencesPersistResult::success();
    }
};

void readSuccessFailureAndAtomicity()
{
    FakeReadPort reader;
    FakePersistPort persister;
    PreferencesModel model(reader, persister);

    const UserPreferencesState stored = stateWith(
        ThemePreference::Dark,
        LanguagePreference::Korean
        );
    reader.result = PreferencesStateReadResult::success(stored);

    std::vector<UserPreferencesSnapshot> notifications;
    const auto subscription = model.subscribe(
        [&notifications](UserPreferencesSnapshot snapshot) {
            notifications.push_back(std::move(snapshot));
        }
        );
    expect(static_cast<bool>(subscription), "Observer subscription should succeed.");

    const auto loaded = model.load();
    expect(static_cast<bool>(loaded), "Valid state should load successfully.");
    expect(reader.calls == 1, "Load should call the reader once.");
    expect(model.state() == stored, "Load should replace the current state.");
    expect(!model.isDirty(), "A loaded state should be clean.");
    expect(notifications.size() == 1, "A changed load should notify once.");
    expect(
        notifications.front() == stored.snapshot(),
        "Load observers should receive the committed full snapshot."
        );

    UserPreferencesState changed = model.state();
    expect(
        static_cast<bool>(changed.setThemePreference(ThemePreference::Light)),
        "Candidate theme should be valid."
        );
    expectApplyResult(model.apply(changed), true, "A changed candidate should apply.");
    const UserPreferencesState beforeFailure = model.state();
    const bool wasDirty = model.isDirty();
    const std::size_t notificationCount = notifications.size();
    const Domain::OperationError readError = error(
        Domain::ErrorCode::Technical,
        "Preference storage is unavailable."
        );
    reader.result = PreferencesStateReadResult::failure(readError);

    const auto failedLoad = model.load();
    expect(!failedLoad, "A reader failure should be returned.");
    expect(failedLoad.error() == readError, "Load should preserve the port error.");
    expect(model.state() == beforeFailure, "A failed read must not change state.");
    expect(model.isDirty() == wasDirty, "A failed read must preserve dirty status.");
    expect(
        notifications.size() == notificationCount,
        "A failed read must not notify observers."
        );

    reader.result = PreferencesStateReadResult::success(beforeFailure);
    const auto identicalLoad = model.load();
    expect(static_cast<bool>(identicalLoad), "An identical state should load.");
    expect(!model.isDirty(), "A successful load should mark identical state clean.");
    expect(
        notifications.size() == notificationCount,
        "An identical load must not notify observers."
        );
    expect(persister.calls == 0, "Read operations must not persist preferences.");
}

void applyValidationChangeNoOpAndNoWriteThrough()
{
    FakeReadPort reader;
    FakePersistPort persister;
    PreferencesModel model(reader, persister);

    int notifications = 0;
    UserPreferencesSnapshot observed;
    const auto subscription = model.subscribe(
        [&notifications, &observed](UserPreferencesSnapshot snapshot) {
            ++notifications;
            observed = std::move(snapshot);
        }
        );
    expect(static_cast<bool>(subscription), "Observer subscription should succeed.");

    UserPreferencesState invalidCandidate = model.state();
    const UserPreferencesSnapshot beforeInvalid = invalidCandidate.snapshot();
    const auto invalidPreference = invalidCandidate.setThemePreference(
        static_cast<ThemePreference>(99)
        );
    expect(!invalidPreference, "The state setter should reject invalid values.");
    expect(
        invalidCandidate.snapshot() == beforeInvalid,
        "A rejected setter must leave the candidate unchanged."
        );
    expectApplyResult(
        model.apply(std::move(invalidCandidate)),
        false,
        "An unchanged candidate should be a no-op."
        );
    expect(model.state().snapshot() == beforeInvalid, "Invalid input must not commit.");
    expect(!model.isDirty(), "A no-op apply should leave a clean model clean.");
    expect(notifications == 0, "A no-op apply must not notify.");

    UserPreferencesState changed = model.state();
    expect(
        static_cast<bool>(changed.setThemePreference(ThemePreference::Dark)),
        "The changed theme should be valid."
        );
    expect(
        static_cast<bool>(changed.setExcelImportTimeoutSeconds(300)),
        "The changed timeout should be valid."
        );
    const UserPreferencesSnapshot committed = changed.snapshot();
    expectApplyResult(
        model.apply(std::move(changed)),
        true,
        "A changed candidate should be applied."
        );
    expect(model.snapshot() == committed, "Apply should commit the whole candidate.");
    expect(model.isDirty(), "A changed apply should mark the model dirty.");
    expect(notifications == 1, "A changed apply should notify exactly once.");
    expect(observed == committed, "The observer should see the committed snapshot.");
    expect(persister.calls == 0, "Apply must not write through to persistence.");

    expectApplyResult(
        model.apply(model.state()),
        false,
        "Applying identical state should be a no-op."
        );
    expect(model.isDirty(), "An identical apply should preserve existing dirty state.");
    expect(notifications == 1, "An identical apply must not notify again.");
    expect(persister.calls == 0, "An identical apply must not write.");
}

void persistSuccessFailureAndRetry()
{
    FakeReadPort reader;
    FakePersistPort persister;
    PreferencesModel model(reader, persister);

    expect(static_cast<bool>(model.persist()), "Persisting a clean model is a no-op.");
    expect(persister.calls == 0, "A clean model should not call the writer.");

    UserPreferencesState changed = stateWith(
        ThemePreference::Light,
        LanguagePreference::English
        );
    const UserPreferencesSnapshot expected = changed.snapshot();
    expectApplyResult(
        model.apply(std::move(changed)),
        true,
        "The changed candidate should apply."
        );
    expect(model.isDirty(), "The changed candidate should be dirty.");

    const Domain::OperationError writeError = error(
        Domain::ErrorCode::Technical,
        "Preference storage rejected the write."
        );
    persister.result = PreferencesPersistResult::failure(writeError);
    const auto failedPersist = model.persist();
    expect(!failedPersist, "A write failure should be returned.");
    expect(
        failedPersist.error() == writeError,
        "Persist should preserve the port error."
        );
    expect(model.snapshot() == expected, "A failed write must preserve current state.");
    expect(model.isDirty(), "A failed write must keep the model dirty.");
    expect(persister.calls == 1, "The failed persistence attempt should be recorded.");
    expect(
        persister.writes.front() == expected,
        "Persistence should receive the full current snapshot."
        );

    persister.result = PreferencesPersistResult::success();
    const auto retriedPersist = model.persist();
    expect(static_cast<bool>(retriedPersist), "Retry should persist successfully.");
    expect(!model.isDirty(), "Successful persistence should clear dirty status.");
    expect(persister.calls == 2, "Retry should make one additional write.");
    expect(
        persister.writes.back() == expected,
        "Retry should write the same idempotent full snapshot."
        );

    expect(static_cast<bool>(model.persist()), "Persisting after success should succeed.");
    expect(persister.calls == 2, "A clean model should not write a second time.");
}

void observerOrderingCommittedValuesAndUnsubscribe()
{
    FakeReadPort reader;
    FakePersistPort persister;
    PreferencesModel model(reader, persister);

    std::vector<UserPreferencesSnapshot> firstObserverValues;
    std::vector<UserPreferencesSnapshot> secondObserverValues;
    std::vector<bool> firstObserverSawCurrentValue;
    std::vector<bool> secondObserverSawCurrentValue;
    std::vector<int> observerOrder;
    const UserPreferencesState nestedCandidate = stateWith(
        ThemePreference::Dark,
        LanguagePreference::Korean
        );
    Domain::Result<bool> reentrantApply = Domain::Result<bool>::success(false);
    Domain::Result<void> reentrantLoad = Domain::Result<void>::success();
    Domain::Result<void> reentrantPersist = Domain::Result<void>::success();
    const auto firstObserver = model.subscribe(
        [&model,
         &firstObserverValues,
         &firstObserverSawCurrentValue,
         &observerOrder,
         &nestedCandidate,
         &reentrantApply,
         &reentrantLoad,
         &reentrantPersist](UserPreferencesSnapshot snapshot) {
            observerOrder.push_back(1);
            firstObserverValues.push_back(snapshot);
            firstObserverSawCurrentValue.push_back(
                model.snapshot() == snapshot
                );
            if (snapshot.themePreference() == ThemePreference::Dark
                && snapshot.languagePreference()
                    == LanguagePreference::SystemDefault)
            {
                reentrantApply = model.apply(nestedCandidate);
                reentrantLoad = model.load();
                reentrantPersist = model.persist();
                firstObserverSawCurrentValue.back() =
                    model.snapshot() == snapshot;
            }
        }
        );
    const auto secondObserver = model.subscribe(
        [&model,
         &secondObserverValues,
         &secondObserverSawCurrentValue,
         &observerOrder](
            UserPreferencesSnapshot snapshot
            ) {
            observerOrder.push_back(2);
            secondObserverValues.push_back(std::move(snapshot));
            secondObserverSawCurrentValue.push_back(
                model.snapshot() == secondObserverValues.back()
                );
        }
        );
    expect(static_cast<bool>(firstObserver), "First observer should subscribe.");
    expect(static_cast<bool>(secondObserver), "Second observer should subscribe.");

    UserPreferencesState initialChange = model.state();
    expect(
        static_cast<bool>(initialChange.setThemePreference(ThemePreference::Dark)),
        "Initial changed theme should be valid."
        );
    const UserPreferencesSnapshot firstCommit = initialChange.snapshot();
    expectApplyResult(
        model.apply(std::move(initialChange)),
        true,
        "Initial state should apply."
        );

    expect(
        !reentrantApply && reentrantApply.error().code == Domain::ErrorCode::Conflict,
        "Reentrant apply should fail with Conflict."
        );
    expect(
        reentrantApply.error().recoverable,
        "Reentrant apply can be retried after notification."
        );
    expect(
        !reentrantLoad && reentrantLoad.error().code == Domain::ErrorCode::Conflict,
        "Reentrant load should fail with Conflict."
        );
    expect(
        !reentrantPersist
            && reentrantPersist.error().code == Domain::ErrorCode::Conflict,
        "Reentrant persist should fail with Conflict."
        );
    expect(
        model.snapshot() == firstCommit && model.isDirty(),
        "Rejected reentrant operations must preserve the committed state."
        );
    expect(reader.calls == 0, "Reentrant load must not call the reader port.");
    expect(persister.calls == 0, "Reentrant persist must not call the writer port.");
    expect(
        firstObserverValues.size() == 1 && secondObserverValues.size() == 1,
        "Rejected reentrant apply must not create another delivery."
        );
    expect(
        firstObserverValues.front() == firstCommit
            && secondObserverValues.front() == firstCommit,
        "Both observers should receive the committed snapshot."
        );
    expect(
        observerOrder == std::vector<int>{1, 2},
        "Observers should run in subscription order."
        );
    expect(
        firstObserverSawCurrentValue == std::vector<bool>{true}
            && secondObserverSawCurrentValue == std::vector<bool>{true},
        "Each observer should see model.snapshot() equal its delivery argument."
        );

    expectApplyResult(
        model.apply(nestedCandidate),
        true,
        "The rejected candidate should apply after notification completes."
        );
    const UserPreferencesSnapshot nestedCommit = nestedCandidate.snapshot();
    expect(
        firstObserverValues.size() == 2 && secondObserverValues.size() == 2,
        "The later ordinary apply should notify each observer once."
        );
    expect(
        firstObserverValues.back() == nestedCommit
            && secondObserverValues.back() == nestedCommit,
        "Both observers should receive the later committed snapshot."
        );
    expect(
        observerOrder == std::vector<int>{1, 2, 1, 2},
        "Each delivery should preserve subscription order."
        );
    expect(
        firstObserverSawCurrentValue.back()
            && secondObserverSawCurrentValue.back(),
        "Observers should still see the current committed value."
        );

    expect(
        model.unsubscribe(secondObserver.value()),
        "Unsubscribe should remove an existing observer."
        );
    expect(
        !model.unsubscribe(secondObserver.value()),
        "Unsubscribing an absent observer should return false."
        );

    UserPreferencesState finalChange = model.state();
    expect(
        static_cast<bool>(finalChange.setThemePreference(ThemePreference::Light)),
        "Final changed theme should be valid."
        );
    const UserPreferencesSnapshot finalCommit = finalChange.snapshot();
    expectApplyResult(
        model.apply(std::move(finalChange)),
        true,
        "Final state should apply."
        );
    expect(
        firstObserverValues.size() == 3
            && firstObserverValues.back() == finalCommit,
        "The remaining observer should see the next committed value once."
        );
    expect(
        observerOrder == std::vector<int>{1, 2, 1, 2, 1},
        "An unsubscribed observer should be absent from later ordered delivery."
        );
    expect(
        secondObserverValues.size() == 2,
        "An unsubscribed observer should receive no later snapshots."
        );
}

void observerExceptionsAreRethrownAfterFanout()
{
    FakeReadPort reader;
    FakePersistPort persister;
    PreferencesModel model(reader, persister);

    const auto throwingObserver = model.subscribe(
        [](UserPreferencesSnapshot) {
            throw std::runtime_error("observer callback failed");
        }
        );
    expect(static_cast<bool>(throwingObserver), "Throwing observer should subscribe.");

    std::vector<UserPreferencesSnapshot> received;
    bool sawCurrentValue = false;
    const auto receivingObserver = model.subscribe(
        [&model, &received, &sawCurrentValue](UserPreferencesSnapshot snapshot) {
            received.push_back(std::move(snapshot));
            sawCurrentValue = model.snapshot() == received.back();
        }
        );
    expect(static_cast<bool>(receivingObserver), "Receiving observer should subscribe.");

    UserPreferencesState changed = stateWith(
        ThemePreference::Dark,
        LanguagePreference::Korean
        );
    const UserPreferencesSnapshot committed = changed.snapshot();
    bool caughtExpectedException = false;
    try
    {
        static_cast<void>(model.apply(std::move(changed)));
    }
    catch (const std::runtime_error& exception)
    {
        caughtExpectedException =
            std::string(exception.what()) == "observer callback failed";
    }

    expect(
        caughtExpectedException,
        "The first observer exception should be rethrown after delivery."
        );
    expect(
        received.size() == 1 && received.front() == committed,
        "A throwing observer must not prevent remaining observers from receiving the snapshot."
        );
    expect(sawCurrentValue, "The remaining observer should see committed model state.");
    expect(
        model.snapshot() == committed && model.isDirty(),
        "Observer failure should leave the committed state in place and dirty."
        );
    expect(
        static_cast<bool>(model.persist()),
        "The model should accept operations again after the exception is rethrown."
        );
    expect(
        !model.isDirty() && persister.calls == 1,
        "A post-notification persist should complete normally."
        );
}

void observerSubscriptionsCanChangeDuringDelivery()
{
    FakeReadPort reader;
    FakePersistPort persister;
    PreferencesModel model(reader, persister);

    PreferencesModel::ObserverId firstId = 0;
    PreferencesModel::ObserverId laterId = 0;
    PreferencesModel::ObserverId newId = 0;
    int firstCalls = 0;
    int laterCalls = 0;
    std::vector<UserPreferencesSnapshot> newObserverValues;

    const auto first = model.subscribe(
        [&model,
         &firstId,
         &laterId,
         &newId,
         &firstCalls,
         &newObserverValues](UserPreferencesSnapshot) {
            ++firstCalls;
            if (newId == 0)
            {
                expect(model.unsubscribe(firstId), "Observer can unsubscribe itself.");
                expect(
                    model.unsubscribe(laterId),
                    "Observer can remove a later subscriber during delivery."
                    );
                const auto newObserver = model.subscribe(
                    [&model, &newObserverValues](UserPreferencesSnapshot snapshot) {
                        newObserverValues.push_back(std::move(snapshot));
                        expect(
                            model.snapshot() == newObserverValues.back(),
                            "A new observer should see the committed value."
                            );
                    }
                    );
                expect(
                    static_cast<bool>(newObserver),
                    "A subscriber can be added during delivery."
                    );
                newId = newObserver.value();
            }
        }
        );
    expect(static_cast<bool>(first), "First observer should subscribe.");
    firstId = first.value();

    const auto later = model.subscribe(
        [&laterCalls](UserPreferencesSnapshot) { ++laterCalls; }
        );
    expect(static_cast<bool>(later), "Later observer should subscribe.");
    laterId = later.value();

    UserPreferencesState firstChange = stateWith(ThemePreference::Dark);
    expectApplyResult(
        model.apply(std::move(firstChange)),
        true,
        "First changed state should apply."
        );
    expect(firstCalls == 1, "The first observer should run once.");
    expect(laterCalls == 0, "A removed observer should be skipped in this delivery.");
    expect(
        newObserverValues.empty(),
        "A new observer should not join the delivery already in progress."
        );

    UserPreferencesState secondChange = model.state();
    expect(
        static_cast<bool>(secondChange.setLanguagePreference(LanguagePreference::Korean)),
        "Second changed language should be valid."
        );
    const UserPreferencesSnapshot secondCommit = secondChange.snapshot();
    expectApplyResult(
        model.apply(std::move(secondChange)),
        true,
        "Second changed state should apply."
        );
    expect(firstCalls == 1, "The self-unsubscribed observer should not run again.");
    expect(laterCalls == 0, "The removed observer should not run again.");
    expect(
        newObserverValues.size() == 1
            && newObserverValues.front() == secondCommit,
        "The new observer should start with the next committed snapshot."
        );
}

void saveBeforeCommitFailureAndRecovery()
{
    FakeReadPort reader;
    FakePersistPort writer;
    PreferencesModel model(reader, writer);
    int notifications = 0;
    auto subscribed = model.subscribe([&](UserPreferencesSnapshot snapshot) {
        ++notifications;
        expect(!model.isDirty(), "Saved values must be clean during notification.");
        expect(snapshot == writer.writes.back(), "Observers see persisted snapshot.");
        const auto reentrant = model.applyAndPersist(model.state());
        expect(!reentrant && reentrant.error().code == Domain::ErrorCode::Conflict,
               "Even same-state save must reject notification reentrancy.");
    });
    expect(static_cast<bool>(subscribed), "Subscribe should succeed.");
    expectApplyResult(model.applyAndPersist(model.state()), false, "Clean same state is a no-op.");
    expect(writer.calls == 0, "Clean same state skips writer.");
    const auto oldState = model.state();
    auto candidate = stateWith(ThemePreference::Dark, LanguagePreference::Korean);
    expect(static_cast<bool>(candidate.setFontSize(FontSize::ExtraLarge)), "Font is valid.");
    const auto failure = error(Domain::ErrorCode::Technical, "Later field failed.");
    writer.result = PreferencesPersistResult::failure(failure);
    const auto failed = model.applyAndPersist(candidate);
    expect(!failed && failed.error() == failure, "First write failure is preserved.");
    expect(model.state() == oldState && model.isDirty(), "Failure preserves current state and marks dirty.");
    expect(notifications == 0, "Failure must not notify.");
    writer.result = PreferencesPersistResult::success();
    expectApplyResult(model.applyAndPersist(model.state()), false, "Retry current snapshot repairs storage.");
    expect(!model.isDirty() && writer.writes.back() == oldState.snapshot(), "Retry writes old state cleanly.");
    expect(notifications == 0, "Recovery without change must not notify.");
    expectApplyResult(model.applyAndPersist(candidate), true, "Changed candidate persists and commits.");
    expect(model.state() == candidate && notifications == 1, "Changed candidate notifies once.");
    expectApplyResult(model.applyAndPersist(candidate), false, "Repeated candidate is a no-op.");
    expect(writer.calls == 3 && notifications == 1, "Repeated candidate neither writes nor notifies.");
}

}

int main()
{
    try
    {
        saveBeforeCommitFailureAndRecovery();
        readSuccessFailureAndAtomicity();
        applyValidationChangeNoOpAndNoWriteThrough();
        persistSuccessFailureAndRetry();
        observerOrderingCommittedValuesAndUnsubscribe();
        observerExceptionsAreRethrownAfterFanout();
        observerSubscriptionsCanChangeDuringDelivery();
    }
    catch (const std::exception& exception)
    {
        std::cerr << "PreferencesModel test failed: " << exception.what() << '\n';
        return 1;
    }

    std::cout << "PreferencesModel tests passed.\n";
    return 0;
}
