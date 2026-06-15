#pragma once

#include <QString>

class QWidget;

class RoleStyleRegistry
{
public:
    static void apply(
        QWidget* widget,
        const QString& role
        );
};
