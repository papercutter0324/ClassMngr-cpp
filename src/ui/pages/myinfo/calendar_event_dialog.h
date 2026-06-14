#pragma once

#include "models/calendar_event.h"

#include <QDialog>

class QDateEdit;
class QDialogButtonBox;
class QLineEdit;
class QPushButton;
class QTimeEdit;

class CalendarEventDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CalendarEventDialog(
        const CalendarEvent& event,
        bool existingEvent,
        QWidget* parent = nullptr
        );

    CalendarEvent eventData() const;
    bool deleteRequested() const;

private slots:
    void accept() override;
    void requestDelete();

private:
    void buildUi();
    void loadEvent();

private:
    CalendarEvent m_event;
    bool m_existingEvent = false;
    bool m_deleteRequested = false;

    QLineEdit* m_titleEdit = nullptr;
    QDateEdit* m_startDateEdit = nullptr;
    QTimeEdit* m_startTimeEdit = nullptr;
    QDateEdit* m_endDateEdit = nullptr;
    QTimeEdit* m_endTimeEdit = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
    QPushButton* m_deleteButton = nullptr;
};
