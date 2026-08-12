#include "personal_details_page.h"

#include "core/fontmanager.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/pages/page_header.h"
#include "ui/shared/pages/scrollable_page_body.h"

#include <QLabel>
#include <QVBoxLayout>

void PersonalDetailsPage::buildUi()
{
    contentLayout()->setContentsMargins(0, 0, 0, 0);

    m_pageBody = new ScrollablePageBody(this);
    m_scrollContent = m_pageBody->contentWidget();
    m_scrollContentLayout = m_pageBody->contentLayout();

    m_pageHeader = new PageHeader(
        tr("My Details"),
        tr("Manage your personal information and signature."),
        m_scrollContent
        );
    m_scrollContentLayout->addWidget(m_pageHeader);
    m_scrollContentLayout->addSpacing(
        UiConstants::Pages::HeaderContentSpacing
        );

    buildMyInformationSection();
    buildSignatureSection();
    m_scrollContentLayout->addStretch();

    contentLayout()->addWidget(m_pageBody);
}

QLabel* PersonalDetailsPage::createTopLevelHeading(
    const QString& text,
    QWidget* parent
    ) const
{
    auto* label = new QLabel(text, parent);
    label->setObjectName("sectionTitle");
    label->setAlignment(Qt::AlignCenter);
    label->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::SectionTitleFontSize,
            QFont::DemiBold
            )
        );
    return label;
}

QLabel* PersonalDetailsPage::createFieldLabel(
    const QString& text,
    QWidget* parent
    ) const
{
    auto* label = new QLabel(text, parent);
    label->setContentsMargins(
        UiConstants::ClassInfo::Form::LabelIndent,
        0,
        0,
        0
        );
    return label;
}
