#include "speaking_eval_page.h"

#include <QLabel>
#include <QVBoxLayout>

SpeakingEvalPage::SpeakingEvalPage(
    QWidget *parent
    )
    : BasePage(parent)
{
    auto *layout =
        new QVBoxLayout(this);

    layout->addWidget(
        new QLabel("Speaking Evaluations")
        );
}