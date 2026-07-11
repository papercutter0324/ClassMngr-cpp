#pragma once

#include "ui/shared/pages/basepage.h"

#include "features/schedule/ui/schedule_view_model.h"

#include <QMap>

class ApplicationServices;
class QCheckBox;
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
    void setUse24HourTime(bool use24h);

    void setShowAllHours(bool showAllHours);

    void setHideEmptyRows(bool hideEmptyRows);

    void setShowIntensiveSchedule(bool showIntensive);

    void setShowWeekends(bool showWeekends);

    void exportSchedule();

    void printSchedule();

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

    void reloadSlotStates();

    ScheduleViewRequest buildScheduleViewRequest() const;

    ScheduleViewModel buildScheduleModel();

    QWidget* createScheduleLabel(
        const ScheduleEntry& entry
        );

    QWidget* createMultiScheduleLabel(
        const QList<ScheduleEntry>& entries
        );

    QWidget* createSlotLabel(
        const ScheduleCellView& cell
        );

private:
    ApplicationServices* m_services = nullptr;

    bool m_use24h = false;
    bool m_showAllHours = false;
    bool m_hideEmptyBlocks = false;
    bool m_showIntensive = false;
    bool m_showWeekends = false;
    bool m_regularWeekdaySlotTogglingEnabled = false;

    QMap<QString, QString> m_intensiveSlotStates;
    ScheduleViewModel m_scheduleModel;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QTableWidget* m_table = nullptr;
    QCheckBox* m_use24HourTimeCheckBox = nullptr;
    QCheckBox* m_showWeekendsCheckBox = nullptr;
    QCheckBox* m_showAllHoursCheckBox = nullptr;
    QCheckBox* m_hideEmptyRowsCheckBox = nullptr;
    QCheckBox* m_showIntensiveScheduleCheckBox = nullptr;
    QPushButton* m_exportButton = nullptr;
    QPushButton* m_printButton = nullptr;
};
