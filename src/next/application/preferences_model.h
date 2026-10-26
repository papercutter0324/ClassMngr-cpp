#pragma once

#include "next/application/user_preferences_state.h"

#include <cstdint>
#include <exception>
#include <functional>
#include <map>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

using PreferencesStateReadResult = Domain::Result<UserPreferencesState>;
using PreferencesPersistResult = Domain::Result<void>;

// Reads the complete, validated application preference state.
class PreferencesStateReadPort
{
public:
    virtual ~PreferencesStateReadPort() = default;

    [[nodiscard]] virtual PreferencesStateReadResult read() const = 0;
};

// Persists one complete application preference snapshot. Implementations may
// write sequentially: failure can leave partial storage changes, so callers
// must permit an idempotent full-snapshot retry. No rollback is guaranteed.
class PreferencesSnapshotPersistPort
{
public:
    virtual ~PreferencesSnapshotPersistPort() = default;

    [[nodiscard]] virtual PreferencesPersistResult persist(
        const UserPreferencesSnapshot& snapshot
        ) const = 0;
};

// Owns the current in-memory preference state and coordinates explicit read,
// apply, persist, and observer notification operations. The injected ports
// must outlive this model. Observer callbacks run synchronously in subscription
// order. Changed apply operations and all load/persist operations are rejected
// while an observer delivery is in progress, so every observer sees the same
// current committed value. Unsubscribing during delivery skips an observer
// that has not yet run for the current snapshot, while new subscriptions begin
// with the next snapshot delivery. If an observer throws, delivery continues
// to the remaining observers and the first exception is rethrown after delivery
// finishes; the committed state remains in place.
class PreferencesModel final
{
public:
    using ObserverId = std::uint64_t;
    using Observer = std::function<void(UserPreferencesSnapshot)>;

    explicit PreferencesModel(
        const PreferencesStateReadPort& readPort,
        const PreferencesSnapshotPersistPort& persistPort
        ) noexcept
        : m_readPort(readPort),
          m_persistPort(persistPort)
    {
    }

    PreferencesModel(const PreferencesModel&) = delete;
    PreferencesModel& operator=(const PreferencesModel&) = delete;
    PreferencesModel(PreferencesModel&&) = delete;
    PreferencesModel& operator=(PreferencesModel&&) = delete;

    [[nodiscard]] UserPreferencesState state() const
    {
        return m_state;
    }

    [[nodiscard]] UserPreferencesSnapshot snapshot() const
    {
        return m_state.snapshot();
    }

    [[nodiscard]] bool isDirty() const noexcept
    {
        return m_dirty;
    }

    // A successful load replaces the current state and marks it clean. A
    // failed read leaves both state and dirty status untouched. Loads invoked
    // during observer delivery fail with Conflict without calling the port.
    [[nodiscard]] Domain::Result<void> load()
    {
        if (m_isNotifying)
        {
            return Domain::Result<void>::failure(notificationConflict());
        }

        PreferencesStateReadResult result = m_readPort.read();
        if (!result)
        {
            return Domain::Result<void>::failure(result.error());
        }

        UserPreferencesState candidate = std::move(result.value());
        const bool changed = candidate != m_state;
        m_state = std::move(candidate);
        m_dirty = false;

        if (changed)
        {
            notifyObservers(m_state.snapshot());
        }

        return Domain::Result<void>::success();
    }

    // The candidate is prepared and validated through UserPreferencesState's
    // setters by the caller. Applying it never writes through to persistence.
    // Returns true only when the current value changed. A changed candidate
    // passed during observer delivery fails with Conflict and is not committed.
    [[nodiscard]] Domain::Result<bool> apply(UserPreferencesState candidate)
    {
        if (candidate == m_state)
        {
            return Domain::Result<bool>::success(false);
        }

        if (m_isNotifying)
        {
            return Domain::Result<bool>::failure(notificationConflict());
        }

        m_state = std::move(candidate);
        m_dirty = true;
        notifyObservers(m_state.snapshot());
        return Domain::Result<bool>::success(true);
    }

    // A clean model has nothing to write. A failed write keeps the current
    // state dirty for an idempotent whole-snapshot retry. Persists invoked
    // during observer delivery fail with Conflict without calling the port.
    [[nodiscard]] Domain::Result<void> persist()
    {
        if (m_isNotifying)
        {
            return Domain::Result<void>::failure(notificationConflict());
        }

        if (!m_dirty)
        {
            return Domain::Result<void>::success();
        }

        const UserPreferencesSnapshot candidate = m_state.snapshot();
        const PreferencesPersistResult result =
            m_persistPort.persist(candidate);
        if (!result)
        {
            return Domain::Result<void>::failure(result.error());
        }

        // A port may synchronously call back into the model. Clear dirty only
        // when the state that was written is still the current state.
        if (m_state.snapshot() == candidate)
        {
            m_dirty = false;
        }

        return Domain::Result<void>::success();
    }

    // Save before committing or notifying. The writer may have partially
    // persisted a failed candidate: keep the old state, mark it dirty, and
    // allow a full snapshot retry to repair storage. No rollback is promised.
    [[nodiscard]] Domain::Result<bool> applyAndPersist(UserPreferencesState candidate)
    {
        if (m_isNotifying)
        {
            return Domain::Result<bool>::failure(notificationConflict());
        }
        const bool changed = candidate != m_state;
        if (!changed && !m_dirty)
        {
            return Domain::Result<bool>::success(false);
        }
        const PreferencesPersistResult result = m_persistPort.persist(candidate.snapshot());
        if (!result)
        {
            m_dirty = true;
            return Domain::Result<bool>::failure(result.error());
        }
        m_state = std::move(candidate);
        m_dirty = false;
        if (changed)
        {
            notifyObservers(m_state.snapshot());
        }
        return Domain::Result<bool>::success(changed);
    }

    [[nodiscard]] Domain::Result<ObserverId> subscribe(Observer observer)
    {
        if (!observer)
        {
            return Domain::Result<ObserverId>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Preferences observer callback must not be empty.",
                .recoverable = false
            });
        }

        if (m_nextObserverId == 0)
        {
            return Domain::Result<ObserverId>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "Preferences observer subscription IDs are exhausted.",
                .recoverable = false
            });
        }

        const ObserverId id = m_nextObserverId;
        ++m_nextObserverId;
        m_observers.emplace(id, std::move(observer));
        return Domain::Result<ObserverId>::success(id);
    }

    [[nodiscard]] bool unsubscribe(const ObserverId id) noexcept
    {
        return m_observers.erase(id) != 0;
    }

private:
    [[nodiscard]] static Domain::OperationError notificationConflict()
    {
        return {
            .code = Domain::ErrorCode::Conflict,
            .message = "Preferences operations cannot run during observer notification.",
            .recoverable = true
        };
    }

    void notifyObservers(UserPreferencesSnapshot snapshot)
    {
        m_isNotifying = true;
        std::exception_ptr firstObserverException;
        try
        {
            std::vector<ObserverId> observerIds;
            observerIds.reserve(m_observers.size());
            for (const auto& [id, observer] : m_observers)
            {
                static_cast<void>(observer);
                observerIds.push_back(id);
            }

            for (const ObserverId id : observerIds)
            {
                const auto found = m_observers.find(id);
                if (found == m_observers.end())
                {
                    continue;
                }

                // Keep the callable alive even if it unsubscribes itself.
                const Observer observer = found->second;
                try
                {
                    observer(snapshot);
                }
                catch (...)
                {
                    if (!firstObserverException)
                    {
                        firstObserverException = std::current_exception();
                    }
                }
            }
        }
        catch (...)
        {
            m_isNotifying = false;
            throw;
        }
        m_isNotifying = false;

        // Finish the observer delivery before surfacing a callback failure so
        // one observer cannot prevent others from receiving committed values.
        if (firstObserverException)
        {
            std::rethrow_exception(firstObserverException);
        }
    }

    const PreferencesStateReadPort& m_readPort;
    const PreferencesSnapshotPersistPort& m_persistPort;
    UserPreferencesState m_state;
    bool m_dirty = false;
    std::map<ObserverId, Observer> m_observers;
    ObserverId m_nextObserverId = 1;
    bool m_isNotifying = false;
};

} // namespace ClassMngr::Next::Application
