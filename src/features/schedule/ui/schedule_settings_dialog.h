#pragma once

#include <QDialog>

class DataService;
class QCheckBox;

struct ScheduleSettingsValues
{
    bool use24HourTime = false;
    bool showEnglishNames = false;
    bool showWeekends = false;
    bool showAllIntensiveHours = false;
    bool testingAffectsM1 = false;
};

class ScheduleSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    ScheduleSettingsDialog(
        DataService* dataService,
        const ScheduleSettingsValues& values,
        QWidget* parent = nullptr
        );

    [[nodiscard]] ScheduleSettingsValues values() const;

signals:
    void testingBlocksCleared();

private:
    void buildUi();
    QWidget* buildDisplayTab();
    QWidget* buildIntensiveTab();
    QWidget* buildTestingTab();
    void clearTestingLayout();

    DataService* m_dataService = nullptr;
    ScheduleSettingsValues m_initialValues;
    QCheckBox* m_use24HourCheck = nullptr;
    QCheckBox* m_showEnglishNamesCheck = nullptr;
    QCheckBox* m_showWeekendsCheck = nullptr;
    QCheckBox* m_showAllIntensiveHoursCheck = nullptr;
    QCheckBox* m_testingAffectsM1Check = nullptr;
};
