#include "role_style_registry.h"

#include "core/fontmanager.h"
#include "ui/shared/styles/roles.h"

#include <QFont>
#include <QStyle>
#include <QWidget>

namespace
{
bool applyRoleFont(
    QWidget* widget,
    const QString& role
    )
{
    if (role == QString::fromUtf8(UiRoles::Primary)
        || role == QString::fromUtf8(UiRoles::ButtonFooter))
    {
        widget->setFont(
            FontManager::getUiFont(
                10,
                QFont::Medium
                )
            );
        return true;
    }

    if (role == QString::fromUtf8(UiRoles::Input))
    {
        widget->setFont(
            FontManager::getUiFont(10)
            );
        return true;
    }

    return false;
}
}

void RoleStyleRegistry::apply(
    QWidget* widget,
    const QString& role
    )
{
    if (!widget)
    {
        return;
    }

    widget->setProperty(
        "role",
        role
        );

    applyRoleFont(
        widget,
        role
        );

    if (widget->style())
    {
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    }

    widget->update();
}
