#include "teacher_section_card.h"

#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"

#include <QLabel>
#include <QVBoxLayout>

TeacherSectionCard::TeacherSectionCard(const QString& title, QWidget* parent)
    : QFrame(parent)
{
    setProperty("role", UiRoles::Card);
    setObjectName("TeacherSectionCard");

    m_layout = new QVBoxLayout(this);
    m_layout->setAlignment(Qt::AlignTop);

    m_layout->setContentsMargins(
        UiConstants::Cards::Margin,
        UiConstants::Cards::Margin,
        UiConstants::Cards::Margin,
        UiConstants::Cards::Margin);

    m_layout->setSpacing(
        UiConstants::Cards::Spacing);

    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setObjectName("sectionTitle");

    m_layout->addWidget(
        m_titleLabel,
        0,
        Qt::AlignLeft | Qt::AlignTop
        );
}

void TeacherSectionCard::setTitle(
    const QString& title
    )
{
    if (!m_titleLabel)
    {
        return;
    }

    m_titleLabel->setText(title);
}

QVBoxLayout* TeacherSectionCard::contentLayout() const
{
    return m_layout;
}
