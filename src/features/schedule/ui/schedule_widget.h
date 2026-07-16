#pragma once

#include "features/schedule/ui/schedule_view_model.h"

#include <QMap>
#include <QSet>
#include <QWidget>

class ApplicationServices;
class QCheckBox;
class QLabel;
class QPushButton;
class QTableWidget;

enum class ScheduleMode
{
    Interactive,
    ReadOnly
};

struct ScheduleDisplayState
{
    bool use24HourTime = false;
    bool showKoreanTeacherEnglishNames = false;
    bool showIntensive = false;
    bool showAllHours = false;
    bool hideEmptyRows = true;
    bool showWeekends = false;
};

class ScheduleWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ScheduleWidget(
        ApplicationServices* services,
        QWidget* parent = nullptr,
        ScheduleMode mode = ScheduleMode::Interactive
        );

    void refreshSchedule();
    void retranslateUi();
    [[nodiscard]] ScheduleDisplayState displayState() const;
    [[nodiscard]] ScheduleViewModel scheduleModel() const;
    [[nodiscard]] QSet<int> visibleClassIds() const;

signals:
    void classInfoSaved(
        int classId
        );

private slots:
    void setUse24HourTime(bool use24h);
    void setShowKoreanTeacherEnglishNames(bool showEnglishNames);
    void setShowIntensiveSchedule(bool showIntensive);
    void setShowAllHours(bool showAllHours);
    void setHideEmptyRows(bool hideEmptyRows);
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
    void updateTableMinimumHeight();
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
    ScheduleMode m_mode = ScheduleMode::Interactive;

    bool m_use24h = false;
    bool m_showKoreanTeacherEnglishNames = false;
    bool m_showIntensive = false;
    bool m_showAllHours = false;
    bool m_hideEmptyRows = true;
    bool m_showWeekends = false;
    bool m_regularWeekdaySlotTogglingEnabled = false;

    QMap<QString, QString> m_intensiveSlotStates;
    ScheduleViewModel m_scheduleModel;

    QTableWidget* m_table = nullptr;
    QWidget* m_controlsWidget = nullptr;
    QCheckBox* m_use24HourTimeCheckBox = nullptr;
    QCheckBox* m_showKoreanTeacherEnglishNamesCheckBox = nullptr;
    QCheckBox* m_showWeekendsCheckBox = nullptr;
    QCheckBox* m_showAllHoursCheckBox = nullptr;
    QCheckBox* m_hideEmptyRowsCheckBox = nullptr;
    QCheckBox* m_showIntensiveScheduleCheckBox = nullptr;
    QPushButton* m_exportButton = nullptr;
    QPushButton* m_printButton = nullptr;
};
