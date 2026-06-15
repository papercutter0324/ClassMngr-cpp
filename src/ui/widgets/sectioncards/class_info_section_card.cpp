#include "class_info_section_card.h"

#include "ui/constants/gui_constants.h"
#include "ui/styles/roles.h"

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

    auto* titleLabel = new QLabel(title);

    titleLabel->setObjectName("sectionTitle");

    m_layout->addWidget(
        titleLabel,
        0,
        Qt::AlignLeft | Qt::AlignTop
        );
}

QVBoxLayout* SectionCard::contentLayout() const
{
    return m_layout;
}
