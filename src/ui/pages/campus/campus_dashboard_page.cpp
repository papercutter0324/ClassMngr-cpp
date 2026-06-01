#include "campus_dashboard_page.h"

#include <QLabel>
#include <QVBoxLayout>

CampusDashboardPage::CampusDashboardPage(QWidget *parent)
    : BasePage(parent)
{
    contentLayout()->addWidget(
        new QLabel(tr("Campus Dashboard"))
        );
}