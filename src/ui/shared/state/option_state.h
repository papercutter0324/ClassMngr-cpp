#pragma once

#include <QObject>
#include <QAction>
#include <QActionGroup>
#include <QHash>

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

        m_currentValue = value;
        m_hasValue = true;

        m_actions[value]->setChecked(true);

        if (onPersist)
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
