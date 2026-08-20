#include "class_info_section_card.h"

#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"

#include <QLabel>

SectionCard::SectionCard(
    const QString& title,
    QWidget* parent
    )
    : QFrame(parent)
{
    setProperty("role", UiRoles::Card);
    setObjectName("sectionCard");

    m_layout = new QVBoxLayout(this);

    m_layout->setAlignment(Qt::AlignTop);
    m_layout->setContentsMargins(
        UiConstants::ClassInfo::SectionCard::Margin,
        UiConstants::ClassInfo::SectionCard::Margin,
        UiConstants::ClassInfo::SectionCard::Margin,
        UiConstants::ClassInfo::SectionCard::Margin
        );
    m_layout->setSpacing(
        UiConstants::ClassInfo::SectionCard::Spacing
        );

    m_titleLabel = new QLabel(title, this);

    m_titleLabel->setObjectName("sectionTitle");

    m_titleLabel->setVisible(
        !title.isEmpty()
        );

    m_layout->addWidget(
        m_titleLabel,
        0,
        Qt::AlignLeft | Qt::AlignTop
        );
}

void SectionCard::setTitle(
    const QString& title
    )
{
    if (!m_titleLabel)
    {
        return;
    }

    m_titleLabel->setText(title);
    m_titleLabel->setVisible(
        !title.isEmpty()
        );
}

void SectionCard::setTitleFont(
    const QFont& font
    )
{
    if (!m_titleLabel)
    {
        return;
    }

    m_titleLabel->setFont(font);
}

void SectionCard::setTitleAlignment(
    Qt::Alignment alignment
    )
{
    if (!m_titleLabel || !m_layout)
    {
        return;
    }

    m_titleLabel->setAlignment(alignment);
    m_layout->setAlignment(
        m_titleLabel,
        alignment
        );
}

QVBoxLayout* SectionCard::contentLayout() const
{
    return m_layout;
}
