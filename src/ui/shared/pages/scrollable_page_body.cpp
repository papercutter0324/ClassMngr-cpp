#include "scrollable_page_body.h"

#include "ui/shared/constants/gui_constants.h"

#include <QFrame>
#include <QVBoxLayout>

ScrollablePageBody::ScrollablePageBody(
    QWidget* parent,
    const QMargins& margins,
    int spacing
    )
    : QScrollArea(parent)
{
    setObjectName(QStringLiteral("pageScrollArea"));
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    m_contentWidget = new QWidget(this);
    m_contentWidget->setObjectName(QStringLiteral("pageScrollContent"));
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(
        margins.left() < 0
            ? QMargins(
                UiConstants::Pages::Margin,
                UiConstants::Pages::Margin,
                UiConstants::Pages::Margin,
                UiConstants::Pages::Margin
                )
            : margins
        );
    m_contentLayout->setSpacing(
        spacing < 0 ? UiConstants::Pages::Spacing : spacing
        );
    m_contentLayout->setAlignment(Qt::AlignTop);
    setWidget(m_contentWidget);
}

QWidget* ScrollablePageBody::contentWidget() const
{
    return m_contentWidget;
}

QVBoxLayout* ScrollablePageBody::contentLayout() const
{
    return m_contentLayout;
}

void ScrollablePageBody::setContentMargins(const QMargins& margins)
{
    m_contentLayout->setContentsMargins(margins);
}
