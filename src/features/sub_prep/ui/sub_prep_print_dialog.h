#pragma once

#include "domain/models/calendar_event.h"

#include <QDate>
#include <QDialog>
#include <QList>
#include <QStringList>

class QCheckBox;

class SubPrepPrintDialog : public QDialog
{
public:
    enum class Action
    {
        Print,
        SaveAs
    };

    explicit SubPrepPrintDialog(
        const QList<CalendarEvent>& calendarEvents,
        const QDate& referenceDate = QDate::currentDate(),
        QWidget* parent = nullptr
        );

    [[nodiscard]] Action selectedAction() const;
    [[nodiscard]] QString selectedSavePath() const;
    [[nodiscard]] QStringList selectedDays() const;

    [[nodiscard]] static QStringList defaultSelectedDays(
        const QList<CalendarEvent>& calendarEvents,
        const QDate& referenceDate
        );

private:
    void acceptPrint();
    void chooseSavePath();

    Action m_selectedAction = Action::Print;
    QString m_selectedSavePath;
    QList<QCheckBox*> m_dayChecks;
};
