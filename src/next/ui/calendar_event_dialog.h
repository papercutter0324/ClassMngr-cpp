#pragma once

#include "next/application/calendar_event_edit_draft.h"
#include "next/application/calendar_event_repeat_occurrence_plan.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QTimeEdit;

namespace ClassMngr::Next::Ui
{

enum class CalendarEventDialogOperation
{
    Save,
    Delete
};

enum class CalendarEventSeriesEditScope
{
    ThisEventOnly,
    ThisAndFollowingEvents
};

class CalendarEventDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit CalendarEventDialog(
        Application::CalendarEventEditDraft draft,
        bool existingEvent,
        QWidget* parent = nullptr,
        CalendarEventSeriesEditScope initialScope =
            CalendarEventSeriesEditScope::ThisEventOnly
        );

    [[nodiscard]] Application::CalendarEventEditDraft eventData() const;
    [[nodiscard]] CalendarEventDialogOperation operation() const noexcept;
    [[nodiscard]] bool repeatEnabled() const;
    [[nodiscard]] Application::CalendarEventRepeatFrequency repeatFrequency()
        const;
    [[nodiscard]] QString repeatUntilDate() const;
    [[nodiscard]] CalendarEventSeriesEditScope seriesEditScope() const;

private:
    void accept() override;
    void updateTimeAvailability();
    void updateRepeatAvailability();

    Application::CalendarEventEditDraft m_draft;
    bool m_existingEvent = false;
    CalendarEventDialogOperation m_operation = CalendarEventDialogOperation::Save;
    QLineEdit* m_title = nullptr;
    QDateEdit* m_startDate = nullptr;
    QDateEdit* m_endDate = nullptr;
    QTimeEdit* m_startTime = nullptr;
    QTimeEdit* m_endTime = nullptr;
    QCheckBox* m_allDay = nullptr;
    QComboBox* m_type = nullptr;
    QComboBox* m_timeStatus = nullptr;
    QCheckBox* m_repeat = nullptr;
    QComboBox* m_repeatFrequency = nullptr;
    QDateEdit* m_repeatUntil = nullptr;
    QComboBox* m_seriesScope = nullptr;
    QLabel* m_validationMessage = nullptr;
    QPushButton* m_deleteButton = nullptr;
};

} // namespace ClassMngr::Next::Ui
