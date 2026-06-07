#include "class_info_section_card.h"

#include <QLabel>

SectionCard::SectionCard(
    const QString& title,
    QWidget* parent
    )
    : QFrame(parent)
{
    setProperty("role", "card");
    setObjectName("sectionCard");

    m_layout = new QVBoxLayout(this);

    m_layout->setAlignment(Qt::AlignTop);
    m_layout->setContentsMargins(20, 20, 20, 20);
    m_layout->setSpacing(16);

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