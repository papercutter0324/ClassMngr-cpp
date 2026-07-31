#pragma once

#include "features/schedule/ui/schedule_view_model.h"

#include <QMap>
#include <QSet>
#include <QWidget>

class ApplicationServices;
class QButtonGroup;
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
    bool showAllHours = false;
    bool showWeekends = false;
    bool testingAffectsM1 = false;
    ScheduleDisplayMode displayMode =
        ScheduleDisplayMode::Regular;
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
    void clearDatabaseState();
    void retranslateUi();
    [[nodiscard]] ScheduleDisplayState displayState() const;
    [[nodiscard]] ScheduleViewModel scheduleModel() const;
    [[nodiscard]] QSet<int> visibleClassIds() const;
    void setMaximumVisibleRows(
        int maximumVisibleRows
        );
    void setCompactPreview(
        bool compactPreview
        );
    void setPreviewModel(
        const ScheduleViewModel& model
        );
    void clearPreviewModel();

signals:
    void classInfoSaved(
        int classId
        );
    void scheduleImportRequested();
    void testingClassesRequested(
        int classId,
        const QString& day,
        const QString& startTime
        );

private slots:
    void setDisplayMode(int modeId);
    void openSettings();
    void exportSchedule();
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
    void reloadTestingBlocks();
    void editTestingAssignment(
        const QString& day,
        const QString& timeLabel,
        const TestingAssignment* existingAssignment
        );
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
    bool m_showAllHours = false;
    bool m_showWeekends = false;
    bool m_testingAffectsM1 = false;
    ScheduleDisplayMode m_displayMode =
        ScheduleDisplayMode::Regular;
    bool m_regularWeekdaySlotTogglingEnabled = false;

    QMap<QString, QString> m_intensiveSlotStates;
    QMap<QString, TestingAssignmentView> m_testingAssignments;
    ScheduleViewModel m_scheduleModel;
    ScheduleViewModel m_previewModel;
    bool m_hasPreviewModel = false;
    int m_maximumVisibleRows = 0;
    bool m_compactPreview = false;

    QTableWidget* m_table = nullptr;
    QWidget* m_controlsWidget = nullptr;
    QButtonGroup* m_modeButtonGroup = nullptr;
    QPushButton* m_regularModeButton = nullptr;
    QPushButton* m_intensiveModeButton = nullptr;
    QPushButton* m_testingModeButton = nullptr;
    QPushButton* m_settingsButton = nullptr;
    QPushButton* m_testingClassesButton = nullptr;
    QPushButton* m_exportButton = nullptr;
    QPushButton* m_importButton = nullptr;
    QLabel* m_testingBanner = nullptr;
};
