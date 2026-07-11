#include "theme_service.h"

#include "core/resource_paths.h"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QIODevice>
#include <QPalette>
#include <QStyle>
#include <QWidget>
#include <QDebug>

ThemeService::ThemeService(
    QObject* parent
    )
    : QObject(parent)
{
}

Theme ThemeService::currentTheme() const
{
    return m_currentTheme;
}

void ThemeService::setTheme(
    Theme theme
    )
{
    auto* app =
        qobject_cast<QApplication*>(
            QApplication::instance()
            );

    if (!app)
    {
        m_currentTheme = theme;
        m_hasAppliedTheme = false;
        return;
    }

    if (m_hasAppliedTheme && m_currentTheme == theme)
    {
        refreshThemeProperties(
            themeKey(theme)
            );
        return;
    }

    m_currentTheme = theme;
    m_hasAppliedTheme = true;

    app->setPalette(
        buildPalette(theme)
        );

    app->setStyleSheet(
        loadStylesheet(theme)
        );

    refreshThemeProperties(
        themeKey(theme)
        );

    emit themeChanged(theme);
    emit themeChanged();
}

void ThemeService::notifyThemeChanged()
{
    emit themeChanged(m_currentTheme);
    emit themeChanged();
}

QString ThemeService::stylesheetPath(
    Theme theme
    ) const
{
    switch (theme)
    {
    case Theme::Light:
        return ResourcePaths::Styles::light();

    case Theme::Dark:
        return ResourcePaths::Styles::dark();
    }

    return ResourcePaths::Styles::dark();
}

QString ThemeService::themeKey(
    Theme theme
    ) const
{
    switch (theme)
    {
    case Theme::Light:
        return QStringLiteral("light");

    case Theme::Dark:
        return QStringLiteral("dark");
    }

    return QStringLiteral("dark");
}

QString ThemeService::loadStylesheet(
    Theme theme
    ) const
{
    const QString path =
        stylesheetPath(theme);

    QFile file(path);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning()
            << "[ThemeService] Failed to load stylesheet:"
            << path;
        return {};
    }

    return QString::fromUtf8(
        file.readAll()
        );
}

QPalette ThemeService::buildPalette(
    Theme theme
    ) const
{
    QPalette palette;

    if (theme == Theme::Light)
    {
        palette.setColor(QPalette::Window, QColor("#eff0f1"));
        palette.setColor(QPalette::WindowText, QColor("#232629"));
        palette.setColor(QPalette::Base, QColor("#ffffff"));
        palette.setColor(QPalette::AlternateBase, QColor("#f7f7f7"));
        palette.setColor(QPalette::Text, QColor("#232629"));
        palette.setColor(QPalette::Button, QColor("#fcfcfc"));
        palette.setColor(QPalette::ButtonText, QColor("#232629"));
        palette.setColor(QPalette::BrightText, QColor("#ffffff"));
        palette.setColor(QPalette::Light, QColor("#ffffff"));
        palette.setColor(QPalette::Midlight, QColor("#e3e5e6"));
        palette.setColor(QPalette::Mid, QColor("#b0b5bb"));
        palette.setColor(QPalette::Dark, QColor("#7f8c8d"));
        palette.setColor(QPalette::Highlight, QColor("#3daee9"));
        palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
        palette.setColor(QPalette::Link, QColor("#2980b9"));
        palette.setColor(QPalette::ToolTipBase, QColor("#f7f7f7"));
        palette.setColor(QPalette::ToolTipText, QColor("#232629"));
        palette.setColor(QPalette::PlaceholderText, QColor("#707d8a"));

        palette.setColor(
            QPalette::Disabled,
            QPalette::Text,
            QColor("#707d8a")
            );
        palette.setColor(
            QPalette::Disabled,
            QPalette::ButtonText,
            QColor("#707d8a")
            );
        palette.setColor(
            QPalette::Disabled,
            QPalette::WindowText,
            QColor("#707d8a")
            );

        return palette;
    }

    palette.setColor(QPalette::Window, QColor("#202326"));
    palette.setColor(QPalette::WindowText, QColor("#fcfcfc"));
    palette.setColor(QPalette::Base, QColor("#141618"));
    palette.setColor(QPalette::AlternateBase, QColor("#1d1f22"));
    palette.setColor(QPalette::Text, QColor("#fcfcfc"));
    palette.setColor(QPalette::Button, QColor("#292c30"));
    palette.setColor(QPalette::ButtonText, QColor("#fcfcfc"));
    palette.setColor(QPalette::BrightText, QColor("#ffffff"));
    palette.setColor(QPalette::Light, QColor("#31363b"));
    palette.setColor(QPalette::Midlight, QColor("#292c30"));
    palette.setColor(QPalette::Mid, QColor("#4b5056"));
    palette.setColor(QPalette::Dark, QColor("#202326"));
    palette.setColor(QPalette::Highlight, QColor("#3daee9"));
    palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    palette.setColor(QPalette::Link, QColor("#1d99f3"));
    palette.setColor(QPalette::ToolTipBase, QColor("#292c30"));
    palette.setColor(QPalette::ToolTipText, QColor("#fcfcfc"));
    palette.setColor(QPalette::PlaceholderText, QColor("#a1a9b1"));

    palette.setColor(
        QPalette::Disabled,
        QPalette::Text,
        QColor("#777777")
        );
    palette.setColor(
        QPalette::Disabled,
        QPalette::ButtonText,
        QColor("#777777")
        );
    palette.setColor(
        QPalette::Disabled,
        QPalette::WindowText,
        QColor("#777777")
        );

    return palette;
}

void ThemeService::refreshThemeProperties(
    const QString& themeName
    ) const
{
    const auto widgets =
        QApplication::allWidgets();

    for (QWidget* widget : widgets)
    {
        if (!widget)
        {
            continue;
        }

        widget->setProperty(
            "theme",
            themeName
            );

        if (widget->style())
        {
            widget->style()->unpolish(widget);
            widget->style()->polish(widget);
        }

        widget->update();
    }
}
