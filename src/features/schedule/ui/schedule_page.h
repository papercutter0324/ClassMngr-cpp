#pragma once

#include "ui/shared/pages/basepage.h"

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

signals:
    void classInfoSaved(
        int classId
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
