#include "page_header.h"

#include "core/fontmanager.h"
#include "ui/shared/constants/gui_constants.h"

#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

PageHeader::PageHeader(
    const QString& title,
    const QString& subtitle,
    QWidget* parent
    )
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin
        );
    layout->setSpacing(UiConstants::Pages::HeaderSpacing);

    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setObjectName(QStringLiteral("pageTitle"));

    m_subtitleLabel = new QLabel(subtitle, this);
    m_subtitleLabel->setObjectName(QStringLiteral("pageSubtitle"));
    m_subtitleLabel->setWordWrap(true);

    m_titleRowLayout = new QHBoxLayout;
    m_titleRowLayout->setContentsMargins(0, 0, 0, 0);
    m_titleRowLayout->setSpacing(UiConstants::Pages::HeaderSpacing);
    m_titleRowLayout->addWidget(m_titleLabel);
    m_titleRowLayout->addStretch();

    layout->addLayout(m_titleRowLayout);
    layout->addWidget(m_subtitleLabel);
    refreshFonts();
}

QLabel* PageHeader::titleLabel() const
{
    return m_titleLabel;
}

QLabel* PageHeader::subtitleLabel() const
{
    return m_subtitleLabel;
}

QString PageHeader::title() const
{
    return m_titleLabel->text();
}

QString PageHeader::subtitle() const
{
    return m_subtitleLabel->text();
}

void PageHeader::setTitle(const QString& title)
{
    m_titleLabel->setText(title);
}

void PageHeader::setSubtitle(const QString& subtitle)
{
    m_subtitleLabel->setText(subtitle);
}

void PageHeader::setTrailingWidget(QWidget* widget)
{
    if (widget == m_trailingWidget || !m_titleRowLayout)
    {
        return;
    }

    if (m_trailingWidget)
    {
        m_titleRowLayout->removeWidget(m_trailingWidget);
        m_trailingWidget->hide();
    }

    m_trailingWidget = widget;

    if (m_trailingWidget)
    {
        if (m_trailingWidget->parentWidget() != this)
        {
            m_trailingWidget->setParent(this);
        }
        m_titleRowLayout->addWidget(m_trailingWidget);
        m_trailingWidget->show();
    }
}

void PageHeader::refreshFonts()
{
    m_titleLabel->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::TitleFontSize,
            QFont::Bold
            )
        );
    m_subtitleLabel->setFont(
        FontManager::getUiFont(UiConstants::Pages::SubtitleFontSize)
        );
}

void PageHeader::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);

    if (event && event->type() == QEvent::LanguageChange)
    {
        refreshFonts();
        emit retranslationRequested();
    }
}
