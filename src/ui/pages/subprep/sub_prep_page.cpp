#include "sub_prep_page.h"

#include "core/fontmanager.h"
#include "ui/constants/gui_constants.h"

#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

SubPrepPage::SubPrepPage(
    QWidget* parent
    )
    : BasePage(parent)
{
    buildUi();
}

void SubPrepPage::buildUi()
{
    contentLayout()->setContentsMargins(
        UiConstants::Pages::Margin,
        18,
        UiConstants::Pages::Margin,
        0
        );

    contentLayout()->setSpacing(12);

    auto* headerLayout =
        new QVBoxLayout;

    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(2);

    m_titleLabel =
        new QLabel(
            tr("Sub Prep"),
            this
            );

    m_titleLabel->setObjectName("pageTitle");
    m_titleLabel->setFont(
        FontManager::getUiFont(
            20,
            QFont::DemiBold
            )
        );

    m_subtitleLabel =
        new QLabel(
            tr("Sub prep tools will appear here."),
            this
            );

    m_subtitleLabel->setObjectName("pageSubtitle");
    m_subtitleLabel->setFont(
        FontManager::getUiFont(11)
        );

    headerLayout->addWidget(m_titleLabel);
    headerLayout->addWidget(m_subtitleLabel);

    contentLayout()->addLayout(headerLayout);
    contentLayout()->addStretch();
}
