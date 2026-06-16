#include "bottom_bar_builder.h"

#include "ui/shared/styles/role_style_registry.h"
#include "ui/shared/styles/roles.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QSizePolicy>
#include <QWidget>

BottomBarBuilder::BottomBarBuilder(
    QHBoxLayout* layout,
    QObject* parent
    )
    : QObject(parent),
    m_layout(layout)
{
}

void BottomBarBuilder::clear()
{
    while (m_layout->count())
    {
        auto* item = m_layout->takeAt(0);

        if (auto* widget = item->widget())
            widget->deleteLater();

        delete item;
    }

    m_buttons.clear();
}

void BottomBarBuilder::setupBase(
    int height,
    int spacing,
    int left,
    int top,
    int right,
    int bottom
    )
{
    if (auto* parent = m_layout->parentWidget())
        parent->setFixedHeight(height);

    m_layout->setSpacing(spacing);
    m_layout->setContentsMargins(
        left,
        top,
        right,
        bottom
        );
}

void BottomBarBuilder::addStretch()
{
    m_layout->addStretch();
}

void BottomBarBuilder::addSpacing(int amount)
{
    m_layout->addSpacing(amount);
}

QPushButton* BottomBarBuilder::addButton(
    const QString& key,
    const QString& text,
    const QObject* receiver,
    const char* slot,
    bool expand
    )
{
    auto* button = new QPushButton(text);

    RoleStyleRegistry::apply(
        button,
        UiRoles::ButtonFooter
        );

    if (expand)
    {
        button->setSizePolicy(
            QSizePolicy::Expanding,
            QSizePolicy::Preferred
            );
    }
    else
    {
        button->setSizePolicy(
            QSizePolicy::Fixed,
            QSizePolicy::Preferred
            );
    }

    button->setFixedHeight(32);

    connect(
        button,
        SIGNAL(clicked()),
        receiver,
        slot
        );

    m_layout->addWidget(button);

    m_buttons.insert(key, button);

    return button;
}

QPushButton* BottomBarBuilder::get(
    const QString& key
    ) const
{
    return m_buttons.value(key, nullptr);
}
