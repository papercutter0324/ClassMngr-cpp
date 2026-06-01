#include "speaking_eval_page.h"

#include <QLabel>
#include <QVBoxLayout>

SpeakingEvalPage::SpeakingEvalPage(QWidget *parent)
    : BasePage(parent)
{
    contentLayout()->addWidget(
        new QLabel(tr("Speaking Evaluations"))
        );
}