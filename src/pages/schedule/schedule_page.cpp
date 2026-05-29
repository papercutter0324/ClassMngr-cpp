#include "schedule_page.h"

#include <QLabel>
#include <QVBoxLayout>

SchedulePage::SchedulePage(
    QWidget *parent
    )
    : BasePage(parent)
{
    auto *layout =
        new QVBoxLayout(this);

    layout->addWidget(
        new QLabel("Weekly Schedule")
        );
}