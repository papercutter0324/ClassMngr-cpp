#include "teacher_info_page.h"

#include <QLabel>
#include <QVBoxLayout>

TeacherInfoPage::TeacherInfoPage(
    QWidget *parent
    )
    : BasePage(parent)
{
    auto *layout =
        new QVBoxLayout(this);

    layout->addWidget(
        new QLabel("Co-Teacher Information")
        );
}