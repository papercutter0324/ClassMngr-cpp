#pragma once

#include "dialog_shell.h"
#include <QString>

class AboutDialog : public DialogShell
{
    Q_OBJECT

public:
    explicit AboutDialog(
        QWidget* parent = nullptr,
        DialogGeometryPreferencesCallbacks geometryPreferences = {}
        );

private slots:
    void showInterLicense();
    void showJustAnotherHandLicense();
    void showPretendardLicense();
    void showTypedSignatureFontLicenses();

private:
    void buildUi();
    DialogGeometryPreferencesCallbacks m_geometryPreferences;
    void showLicense(
        const QString& title,
        const QString& relativePath
        );
};
