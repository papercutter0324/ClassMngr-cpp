#pragma once

#include "ui/shared/pages/basepage.h"

class ApplicationServices;
class QLabel;
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
    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    ScheduleWidget* m_scheduleWidget = nullptr;
};
