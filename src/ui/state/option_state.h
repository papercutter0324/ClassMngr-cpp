#pragma once

#include <QObject>
#include <QAction>
#include <QActionGroup>
#include <QHash>
#include <QSettings>

template <typename T>
class OptionState : public QObject
{
    // Q_OBJECT

public:
    explicit OptionState(const QString& settingsKey, QObject* parent = nullptr)
        : QObject(parent),
        m_settingsKey(settingsKey)
    {
        group = new QActionGroup(this);
        group->setExclusive(true);
    }

    QAction* addOption(T value, QAction* action)
    {
        action->setCheckable(true);

        group->addAction(action);
        actions[value] = action;

        connect(action, &QAction::triggered, this, [this, value]()
                {
                    set(value);
                });

        return action;
    }

    QAction* action(T value) const
    {
        return actions.value(value, nullptr);
    }

    void set(T value)
    {
        if (!actions.contains(value))
            return;

        if (currentValue == value)
            return; // prevent redundant writes

        currentValue = value;

        actions[value]->setChecked(true);

        saveToSettings(value);
        emit changed(value);
    }

    T current() const
    {
        return currentValue;
    }

    void loadFromSettings(T fallback = T())
    {
        QSettings settings;

        T value =
            static_cast<T>(
                settings.value(m_settingsKey, static_cast<int>(fallback)).toInt()
                );

        if (!actions.contains(value))
            return;

        set(value); // reuse full logic (BEST FIX)
    }

signals:
    void changed(T value);

private:
    void saveToSettings(T value)
    {
        QSettings settings;
        settings.setValue(m_settingsKey, static_cast<int>(value));
    }

private:
    QActionGroup* group = nullptr;
    QHash<T, QAction*> actions;
    T currentValue{};
    QString m_settingsKey;
};