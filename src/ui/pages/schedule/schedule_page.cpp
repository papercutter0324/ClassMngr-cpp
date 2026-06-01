#include "schedule_page.h"

#include <QLabel>
#include <QVBoxLayout>

SchedulePage::SchedulePage(QWidget *parent)
    : BasePage(parent)
{
    contentLayout()->addWidget(
        new QLabel(tr("Weekly Schedule"))
        );
}