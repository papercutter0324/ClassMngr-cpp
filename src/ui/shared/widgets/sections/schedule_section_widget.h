#pragma once

#include "features/schedule/ui/schedule_builder.h"

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
    void retranslateUi();

signals:
    void classInfoSaved(
        int classId
        );

private slots:
    void toggleTimeFormat();
    void toggleScheduleMode();
    void toggleShowAllHours();
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
    void updateTableMinimumHeight();
    QStringList visibleDays() const;
    void reloadSlotStates();
    QString slotKey(
        const QString& day,
        const QString& timeLabel
        ) const;
    QString slotState(
        const QString& day,
        const QString& timeLabel,
        const QString& defaultState
        ) const;
    QString defaultSlotState(
        const QString& day,
        const QString& timeLabel
        ) const;
    bool regularSlotTogglingEnabled(
        const QString& day
        ) const;
    bool slotTogglingEnabled(
        const QString& day
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
    bool m_showAllHours = false;
    bool m_showWeekends = false;
    bool m_regularWeekdaySlotTogglingEnabled = false;

    QMap<QString, QString> m_intensiveSlotStates;

    QTableWidget* m_table = nullptr;
    QPushButton* m_timeFormatButton = nullptr;
    QPushButton* m_weekendButton = nullptr;
    QPushButton* m_showAllHoursButton = nullptr;
    QPushButton* m_scheduleModeButton = nullptr;
};
