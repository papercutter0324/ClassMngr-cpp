#include "campus_dashboard_page.h"

#include <QLabel>
#include <QVBoxLayout>

CampusDashboardPage::CampusDashboardPage(
    QWidget *parent
    )
    : BasePage(parent)
{
    auto *layout =
        new QVBoxLayout(this);

    layout->addWidget(
        new QLabel("Campus Dashboard")
        );
}