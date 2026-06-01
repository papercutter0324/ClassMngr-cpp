#include "roster_page.h"

#include <QLabel>
#include <QVBoxLayout>

RosterPage::RosterPage(QWidget *parent)
    : BasePage(parent)
{
    contentLayout()->addWidget(
        new QLabel(tr("Class Roster"))
        );
}