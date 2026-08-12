#include "autosave_coordinator.h"

#include <QPushButton>
#include <QTimer>

AutosaveCoordinator::AutosaveCoordinator(QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    m_timer->setInterval(DefaultDebounceIntervalMs);

    connect(m_timer, &QTimer::timeout, this, [this]() {
        requestSave(false);
    });
}

bool AutosaveCoordinator::isDirty() const
{
    return m_dirty;
}

bool AutosaveCoordinator::isLoading() const
{
    return m_loading;
}

bool AutosaveCoordinator::isValid() const
{
    return m_valid;
}

bool AutosaveCoordinator::isSaveAvailable() const
{
    return m_saveAvailable;
}

SaveMode AutosaveCoordinator::saveMode() const
{
    return m_saveMode;
}

void AutosaveCoordinator::setDirty(
    bool dirty,
    bool scheduleAutosave
    )
{
    if (m_loading && dirty)
    {
        return;
    }

    const bool changed = m_dirty != dirty;
    m_dirty = dirty;

    if (!m_dirty)
    {
        m_timer->stop();
    }
    else if (scheduleAutosave)
    {
        scheduleIfNeeded();
    }

    updateSaveButton();

    if (changed)
    {
        emit dirtyChanged(m_dirty);
    }

    emit stateChanged();
}

void AutosaveCoordinator::markDirty(bool scheduleAutosave)
{
    setDirty(true, scheduleAutosave);
}

void AutosaveCoordinator::markClean()
{
    setDirty(false, false);
}

void AutosaveCoordinator::setLoading(bool loading)
{
    if (m_loading == loading)
    {
        return;
    }

    m_loading = loading;
    if (m_loading)
    {
        m_timer->stop();
    }
    else
    {
        scheduleIfNeeded();
    }

    updateSaveButton();
    emit loadingChanged(m_loading);
    emit stateChanged();
}

void AutosaveCoordinator::setValid(bool valid)
{
    if (m_valid == valid)
    {
        return;
    }

    m_valid = valid;
    if (!m_valid)
    {
        m_timer->stop();
    }
    else
    {
        scheduleIfNeeded();
    }

    updateSaveButton();
    emit validityChanged(m_valid);
    emit stateChanged();
}

void AutosaveCoordinator::setSaveAvailable(bool available)
{
    if (m_saveAvailable == available)
    {
        return;
    }

    m_saveAvailable = available;
    if (!m_saveAvailable)
    {
        m_timer->stop();
    }
    else
    {
        scheduleIfNeeded();
    }

    updateSaveButton();
    emit stateChanged();
}

void AutosaveCoordinator::setManualSaveRequired(bool required)
{
    if (m_manualSaveRequired == required)
    {
        return;
    }

    m_manualSaveRequired = required;
    scheduleIfNeeded();
    updateSaveButton();
    emit stateChanged();
}

void AutosaveCoordinator::setSaveMode(SaveMode mode)
{
    if (m_saveMode == mode)
    {
        updateSaveButton();
        scheduleIfNeeded();
        return;
    }

    m_saveMode = mode;
    if (m_saveMode == SaveMode::Automatic)
    {
        scheduleIfNeeded();
    }
    else
    {
        m_timer->stop();
    }

    updateSaveButton();
    emit saveModeChanged(m_saveMode);
    emit stateChanged();
}

void AutosaveCoordinator::setDebounceInterval(int milliseconds)
{
    m_timer->setInterval(qMax(0, milliseconds));
}

void AutosaveCoordinator::bindSaveButton(QPushButton* button)
{
    if (m_saveButton)
    {
        disconnect(m_saveButton, nullptr, this, nullptr);
    }

    m_saveButton = button;
    if (m_saveButton)
    {
        connect(
            m_saveButton,
            &QPushButton::clicked,
            this,
            [this]() { requestSave(true); }
            );
    }

    updateSaveButton();
}

void AutosaveCoordinator::cancelPendingSave()
{
    m_timer->stop();
}

void AutosaveCoordinator::requestSave(bool interactive)
{
    m_timer->stop();

    if (
        !m_dirty
        || m_loading
        || !m_valid
        || !m_saveAvailable
        || (m_manualSaveRequired && !interactive)
        )
    {
        return;
    }

    emit saveRequested(interactive);
}

void AutosaveCoordinator::scheduleIfNeeded()
{
    if (
        m_dirty
        && !m_loading
        && m_valid
        && m_saveAvailable
        && !m_manualSaveRequired
        && m_saveMode == SaveMode::Automatic
        )
    {
        m_timer->start();
    }
    else
    {
        m_timer->stop();
    }
}

void AutosaveCoordinator::updateSaveButton()
{
    if (!m_saveButton)
    {
        return;
    }

    const bool manual =
        m_saveMode != SaveMode::Automatic
        || m_manualSaveRequired;
    m_saveButton->setVisible(manual);
    m_saveButton->setEnabled(
        manual
        && m_dirty
        && !m_loading
        && m_valid
        && m_saveAvailable
        );
    m_saveButton->setText(
        m_dirty
            ? tr("Save Changes *")
            : tr("Save Changes")
        );
}
