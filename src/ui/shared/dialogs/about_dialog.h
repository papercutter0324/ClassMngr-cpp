#pragma once

#include "dialog_shell.h"
#include <QString>

class AboutDialog : public DialogShell
{
    Q_OBJECT

public:
    explicit AboutDialog(
        QWidget* parent = nullptr
        );

private slots:
    void showInterLicense();
    void showJustAnotherHandLicense();
    void showPretendardLicense();

private:
    void buildUi();
    void showLicense(
        const QString& title,
        const QString& relativePath
        );
};
