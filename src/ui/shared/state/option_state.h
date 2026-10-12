#pragma once

#include <QObject>
#include <QAction>
#include <QActionGroup>
#include <QHash>

#include "next/domain/operation_result.h"

#include <functional>

template <typename T>
class OptionState : public QObject
{
public:
    explicit OptionState(QObject* parent = nullptr)
        : QObject(parent)
    {
        m_group = new QActionGroup(this);
        m_group->setExclusive(true);
    }

    std::function<void(T)> onChanged;
    std::function<void(T)> onPersist;
    std::function<ClassMngr::Next::Domain::Result<void>(T)> onBeforeCommit;
    std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )> onCommitFailed;

    QAction* addOption(
        T value,
        QAction* action,
        bool selectOnTrigger = true
        )
    {
        action->setCheckable(true);

        m_group->addAction(action);
        m_actions[value] = action;

        if (selectOnTrigger)
        {
            connect(
                action,
                &QAction::triggered,
                this,
                [this, value]()
                {
                    set(value);
                });
        }

        return action;
    }

    QAction* action(T value) const
    {
        return m_actions.value(value, nullptr);
    }

    void set(T value)
    {
        if (!m_actions.contains(value))
            return;

        if (m_hasValue && m_currentValue == value)
        {
            if (!m_actions[value]->isChecked())
            {
                m_actions[value]->setChecked(true);
            }
            return;
        }

        const bool hasPreCommitGate = static_cast<bool>(onBeforeCommit);
        if (hasPreCommitGate)
        {
            const auto result = onBeforeCommit(value);
            if (!result)
            {
                // QAction may already have checked the candidate before its
                // triggered signal reaches this state. Restore the old visual
                // selection synchronously while current state is still old.
                if (m_hasValue)
                {
                    if (QAction* previousAction =
                            m_actions.value(m_currentValue, nullptr))
                    {
                        previousAction->setChecked(true);
                    }
                }

                if (QAction* attemptedAction = m_actions.value(value, nullptr))
                {
                    attemptedAction->setChecked(false);
                }

                if (onCommitFailed)
                {
                    onCommitFailed(result.error());
                }
                return;
            }
        }

        m_currentValue = value;
        m_hasValue = true;

        m_actions[value]->setChecked(true);

        // A pre-commit gate owns persistence for this option. Keep the legacy
        // post-commit callback unchanged for all ungated OptionState users.
        if (!hasPreCommitGate && onPersist)
        {
            onPersist(value);
        }

        if (onChanged)
            onChanged(value);
    }

    T current() const
    {
        return m_currentValue;
    }

private:
    QActionGroup* m_group = nullptr;
    QHash<T, QAction*> m_actions;
    T m_currentValue{};
    bool m_hasValue = false;
};
