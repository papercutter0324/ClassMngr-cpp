#pragma once

#include "domain/models/calendar_event.h"
#include "ui/shared/dialogs/dialog_shell.h"

class QDateEdit;
class QDialogButtonBox;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QTimeEdit;
class OnScreenKeyboard;

enum class CalendarEventRepeatFrequency
{
    Daily,
    Weekly,
    Monthly
};

enum class CalendarEventSeriesEditScope
{
    ThisEventOnly,
    ThisAndFollowingEvents
};

class CalendarEventDialog : public DialogShell
{
    Q_OBJECT

public:
    explicit CalendarEventDialog(
        const CalendarEvent& event,
        bool existingEvent,
        bool use24h,
        QWidget* parent = nullptr
        );

    CalendarEvent eventData() const;
    bool deleteRequested() const;
    bool repeatEnabled() const;
    CalendarEventRepeatFrequency repeatFrequency() const;
    QDate repeatUntilDate() const;
    CalendarEventSeriesEditScope seriesEditScope() const;

private slots:
    void accept() override;
    void requestDelete();

private:
    void buildUi();
    void loadEvent();
    void updateTimeFieldAvailability();
    void updateRepeatFieldAvailability();

private:
    CalendarEvent m_event;
    bool m_existingEvent = false;
    bool m_use24h = false;
    bool m_deleteRequested = false;

    QLineEdit* m_titleEdit = nullptr;
    QDateEdit* m_startDateEdit = nullptr;
    QTimeEdit* m_startTimeEdit = nullptr;
    QDateEdit* m_endDateEdit = nullptr;
    QTimeEdit* m_endTimeEdit = nullptr;
    QCheckBox* m_allDayCheck = nullptr;
    QCheckBox* m_unconfirmedTimeCheck = nullptr;
    QCheckBox* m_repeatCheck = nullptr;
    QComboBox* m_repeatFrequencyCombo = nullptr;
    QDateEdit* m_repeatUntilDateEdit = nullptr;
    QButtonGroup* m_seriesScopeGroup = nullptr;
    QButtonGroup* m_eventTypeGroup = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
    QPushButton* m_deleteButton = nullptr;
    QPushButton* m_koreanKeyboardButton = nullptr;
    OnScreenKeyboard* m_onScreenKeyboard = nullptr;
};
