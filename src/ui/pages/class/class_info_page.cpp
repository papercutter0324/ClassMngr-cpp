#include "class_info_page.h"

#include <QLabel>
#include <QVBoxLayout>

ClassInfoPage::ClassInfoPage(
    QWidget *parent
    )
    : BasePage(parent)
{
    auto *layout =
        new QVBoxLayout(this);

    layout->addWidget(
        new QLabel("Class Information")
        );
}