#pragma once

#include "features/schedule/ui/schedule_view_model.h"
#include "ui/shared/pages/basepage.h"

#include <QString>

class ApplicationServices;
class QLabel;
class QScrollArea;
class QShowEvent;
class ScheduleWidget;
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
    void clearDatabaseState() override;
    void retranslateUi() override;
    [[nodiscard]] ScheduleDisplayMode displayMode() const;
    [[nodiscard]] PageOutputCapabilities
        outputCapabilities() const override;
    void printCurrentPage() override;
    void saveCurrentPageAs() override;

signals:
    void displayModeChanged(
        ScheduleDisplayMode mode
        );
    void classInfoSaved(
        int classId
        );
    void scheduleImportRequested();
    void testingClassesRequested(
        int classId,
        const QString& day,
        const QString& startTime
        );

protected:
    void showEvent(
        QShowEvent* event
        ) override;

private:
    void buildUi();

    ApplicationServices* m_services = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_scrollContent = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    ScheduleWidget* m_scheduleWidget = nullptr;
};
