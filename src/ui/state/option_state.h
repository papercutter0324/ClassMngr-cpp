#pragma once

#include <QObject>
#include <QAction>
#include <QActionGroup>
#include <QHash>
#include <QSettings>

template <typename T>
class OptionState : public QObject
{
public:
    explicit OptionState(
        const QString& settingsKey,
        QObject* parent = nullptr
        )
        : QObject(parent)
        , m_settingsKey(settingsKey)
    {
        m_group = new QActionGroup(this);
        m_group->setExclusive(true);
    }

    std::function<void(T)> onChanged;

    QAction* addOption(
        T value,
        QAction* action
        )
    {
        action->setCheckable(true);

        m_group->addAction(action);
        m_actions[value] = action;

        connect(
            action,
            &QAction::triggered,
            this,
            [this, value]()
            {
                set(value);
            });

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

        saveToSettings(value);

        if (onChanged)
            onChanged(value);
    }

    T current() const
    {
        return m_currentValue;
    }

    void loadFromSettings(T fallback = T())
    {
        QSettings settings;

        T value =
            static_cast<T>(
                settings.value(
                            m_settingsKey,
                            static_cast<int>(fallback)
                            ).toInt()
                );

        if (!m_actions.contains(value))
        {
            value = fallback;
        }

        if (!m_actions.contains(value))
        {
            return;
        }

        set(value);
    }

private:
    void saveToSettings(T value)
    {
        QSettings settings;
        settings.setValue(
            m_settingsKey,
            static_cast<int>(value)
            );
    }

private:
    QActionGroup* m_group = nullptr;
    QHash<T, QAction*> m_actions;
    T m_currentValue{};
    bool m_hasValue = false;
    QString m_settingsKey;
};
