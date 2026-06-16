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

    auto* titleLabel = new QLabel(title);
    titleLabel->setObjectName("sectionTitle");

    m_layout->addWidget(
        titleLabel,
        0,
        Qt::AlignLeft | Qt::AlignTop
        );
}

QVBoxLayout* TeacherSectionCard::contentLayout() const
{
    return m_layout;
}
