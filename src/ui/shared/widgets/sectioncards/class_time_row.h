#pragma once

#include "core/enums/schedule_type.h"

#include <QWidget>

class QComboBox;
class QPushButton;


class ClassTimeRow : public QWidget
{
    Q_OBJECT

public:
    explicit ClassTimeRow(
        ScheduleType type,
        QWidget* parent = nullptr
        );

    // =========================================================
    // Data API (for serialization / loading)
    // =========================================================
    QString day() const;
    QString startTime() const;
    QString endTime() const;

    void setDay(const QString& day);
    void setStartTime(const QString& value);
    void setEndTime(const QString& value);

    void retranslateUi();

    // =========================================================
    // Widget access (IMPORTANT for layout ownership)
    // =========================================================
    QComboBox* dayCombo() const { return m_dayCombo; }
    QComboBox* startHourCombo() const { return m_startHourCombo; }
    QWidget* startWidget() const { return m_startWidget; }
    QComboBox* endCombo() const { return m_endCombo; }
    QPushButton* removeButton() const { return m_removeButton; }

signals:
    void dataChanged();
    void rowChanged();
    void removeRequested(ClassTimeRow* row);

private slots:
    void onRemoveClicked();
    void updateEndTimes();

private:
    static int toTotalMinutes(
        const QString& hour,
        const QString& minute,
        const QString& period
        );

    static QString fromTotalMinutes(int totalMinutes);

    int currentStartMinutes() const;
    int durationForEndTime(const QString& endTime) const;
    QStringList endTimeOptions(int startMinutes) const;

private:
    ScheduleType m_type;

    QComboBox* m_dayCombo = nullptr;

    // startWidget is a container (important for layout reuse)
    QWidget* m_startWidget = nullptr;

    QComboBox* m_startHourCombo = nullptr;
    QComboBox* m_startMinuteCombo = nullptr;
    QComboBox* m_startPeriodCombo = nullptr;

    QComboBox* m_endCombo = nullptr;
    QPushButton* m_removeButton = nullptr;

    int m_previousStartMinutes = 0;
};
