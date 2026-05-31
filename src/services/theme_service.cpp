#include "theme_service.h"

ThemeService::ThemeService(
    QObject* parent
    )
    : QObject(parent)
{
}

void ThemeService::notifyThemeChanged()
{
    emit themeChanged();
}