#pragma once

#include "ui/shared/constants/options.h"

#include <QObject>

class QPushButton;
class QTimer;

class AutosaveCoordinator final : public QObject
{
    Q_OBJECT

public:
    static constexpr int DefaultDebounceIntervalMs = 750;

    explicit AutosaveCoordinator(
        QObject* parent = nullptr
        );

    [[nodiscard]] bool isDirty() const;
    [[nodiscard]] bool isLoading() const;
    [[nodiscard]] bool isValid() const;
    [[nodiscard]] bool isSaveAvailable() const;
    [[nodiscard]] SaveMode saveMode() const;

    void setDirty(
        bool dirty,
        bool scheduleAutosave = true
        );
    void markDirty(
        bool scheduleAutosave = true
        );
    void markClean();
    void setLoading(bool loading);
    void setValid(bool valid);
    void setSaveAvailable(bool available);
    void setManualSaveRequired(bool required);
    void setSaveMode(SaveMode mode);
    void setDebounceInterval(int milliseconds);
    void bindSaveButton(QPushButton* button);
    void cancelPendingSave();
    void requestSave(bool interactive = true);

signals:
    void saveRequested(bool interactive);
    void dirtyChanged(bool dirty);
    void loadingChanged(bool loading);
    void validityChanged(bool valid);
    void saveModeChanged(SaveMode mode);
    void stateChanged();

private:
    void scheduleIfNeeded();
    void updateSaveButton();

    QTimer* m_timer = nullptr;
    QPushButton* m_saveButton = nullptr;
    bool m_dirty = false;
    bool m_loading = false;
    bool m_valid = true;
    bool m_saveAvailable = true;
    bool m_manualSaveRequired = false;
    SaveMode m_saveMode = SaveMode::Automatic;
};
