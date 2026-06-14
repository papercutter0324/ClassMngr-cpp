#pragma once

#include "ui/pages/schedule/schedule_builder.h"

#include <QMap>
#include <QWidget>

class ApplicationServices;
class QLabel;
class QPushButton;
class QTableWidget;

class ScheduleSectionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ScheduleSectionWidget(
        ApplicationServices* services,
        QWidget* parent = nullptr
        );

    void refreshSchedule();

signals:
    void classInfoSaved(
        int classId
        );

private slots:
    void toggleTimeFormat();
    void toggleScheduleMode();
    void toggleWeekends();
    void onCellClicked(
        int row,
        int column
        );

private:
    void buildUi();
    void loadSettings();
    void loadSchedule();
    void updateButtons();
    void configureColumns(
        const QStringList& days
        );
    void clearTableWidgets();
    QStringList visibleDays() const;
    void reloadSlotStates();
    QString slotKey(
        const QString& day,
        const QString& timeLabel
        ) const;
    QString slotState(
        const QString& day,
        const QString& timeLabel
        ) const;
    QString formatDisplayTime(
        const QString& timeLabel
        ) const;
    QString buildTimeRangeLabel(
        const QString& startLabel,
        bool uses55Endings
        ) const;
    QWidget* createScheduleLabel(
        const ScheduleEntry& entry
        );
    QWidget* createMultiScheduleLabel(
        const QList<ScheduleEntry>& entries
        );
    QWidget* createSlotLabel(
        const QString& day,
        const QString& timeLabel
        );

private:
    ApplicationServices* m_services = nullptr;

    bool m_use24h = false;
    bool m_showIntensive = false;
    bool m_showWeekends = false;

    QMap<QString, QString> m_intensiveSlotStates;

    QTableWidget* m_table = nullptr;
    QPushButton* m_timeFormatButton = nullptr;
    QPushButton* m_weekendButton = nullptr;
    QPushButton* m_scheduleModeButton = nullptr;
};
