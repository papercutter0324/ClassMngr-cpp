#pragma once

#include "ui/shared/pages/basepage.h"

#include "domain/models/intensive_slot_state.h"
#include "features/schedule/ui/schedule_builder.h"

#include <QMap>

class ApplicationServices;
class QLabel;
class QPushButton;
class QShowEvent;
class QTableWidget;
class QWidget;

class SchedulePage : public BasePage
{
    Q_OBJECT

public:
    explicit SchedulePage(
        ApplicationServices* services,
        QWidget* parent = nullptr
        );

    void refresh() override;
    void retranslateUi() override;

signals:
    void classInfoSaved(
        int classId
        );

protected:
    void showEvent(
        QShowEvent* event
        ) override;

private slots:
    void toggleTimeFormat();

    void toggleHideEmpty();

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
    bool m_hideEmptyBlocks = false;
    bool m_showIntensive = false;
    bool m_showWeekends = false;

    QMap<QString, QString> m_intensiveSlotStates;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QTableWidget* m_table = nullptr;
    QPushButton* m_timeFormatButton = nullptr;
    QPushButton* m_weekendButton = nullptr;
    QPushButton* m_hideEmptyButton = nullptr;
    QPushButton* m_scheduleModeButton = nullptr;
};
