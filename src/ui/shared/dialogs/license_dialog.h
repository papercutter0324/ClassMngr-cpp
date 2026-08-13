#pragma once

#include "dialog_shell.h"

#include <QString>

class QWidget;

class LicenseDialog final : public DialogShell
{
    Q_OBJECT

public:
    LicenseDialog(
        const QString& title,
        const QString& licenseText,
        QWidget* parent = nullptr
        );
};
