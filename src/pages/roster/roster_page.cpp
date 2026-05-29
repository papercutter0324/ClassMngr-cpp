#include "roster_page.h"

#include <QLabel>
#include <QVBoxLayout>

RosterPage::RosterPage(
    QWidget *parent
    )
    : BasePage(parent)
{
    auto *layout =
        new QVBoxLayout(this);

    layout->addWidget(
        new QLabel("Class Roster")
        );
}