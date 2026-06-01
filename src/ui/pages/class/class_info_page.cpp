#include "class_info_page.h"

#include <QLabel>
#include <QVBoxLayout>

ClassInfoPage::ClassInfoPage(QWidget *parent)
    : BasePage(parent)
{
    contentLayout()->addWidget(
        new QLabel(tr("Class Information"))
        );
}