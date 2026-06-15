#include "theme_service.h"

#include "core/resources_paths.h"

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
        palette.setColor(QPalette::Window, QColor("#f6f8fb"));
        palette.setColor(QPalette::WindowText, QColor("#111827"));
        palette.setColor(QPalette::Base, QColor("#ffffff"));
        palette.setColor(QPalette::AlternateBase, QColor("#eef2f7"));
        palette.setColor(QPalette::Text, QColor("#111827"));
        palette.setColor(QPalette::Button, QColor("#ffffff"));
        palette.setColor(QPalette::ButtonText, QColor("#111827"));
        palette.setColor(QPalette::BrightText, QColor("#ffffff"));
        palette.setColor(QPalette::Light, QColor("#ffffff"));
        palette.setColor(QPalette::Midlight, QColor("#eef2f7"));
        palette.setColor(QPalette::Mid, QColor("#d0d7e2"));
        palette.setColor(QPalette::Dark, QColor("#b8c6da"));
        palette.setColor(QPalette::Highlight, QColor("#dbeafe"));
        palette.setColor(QPalette::HighlightedText, QColor("#0f172a"));
        palette.setColor(QPalette::Link, QColor("#2563eb"));
        palette.setColor(QPalette::ToolTipBase, QColor("#ffffff"));
        palette.setColor(QPalette::ToolTipText, QColor("#111827"));
        palette.setColor(QPalette::PlaceholderText, QColor("#667085"));

        palette.setColor(
            QPalette::Disabled,
            QPalette::Text,
            QColor("#98a2b3")
            );
        palette.setColor(
            QPalette::Disabled,
            QPalette::ButtonText,
            QColor("#98a2b3")
            );
        palette.setColor(
            QPalette::Disabled,
            QPalette::WindowText,
            QColor("#98a2b3")
            );

        return palette;
    }

    palette.setColor(QPalette::Window, QColor("#2b2b2b"));
    palette.setColor(QPalette::WindowText, QColor("#f0f0f0"));
    palette.setColor(QPalette::Base, QColor("#1e1e1e"));
    palette.setColor(QPalette::AlternateBase, QColor("#2a2a2a"));
    palette.setColor(QPalette::Text, QColor("#f0f0f0"));
    palette.setColor(QPalette::Button, QColor("#3a3a3a"));
    palette.setColor(QPalette::ButtonText, QColor("#f0f0f0"));
    palette.setColor(QPalette::BrightText, QColor("#ffffff"));
    palette.setColor(QPalette::Light, QColor("#454545"));
    palette.setColor(QPalette::Midlight, QColor("#3a3a3a"));
    palette.setColor(QPalette::Mid, QColor("#4b5563"));
    palette.setColor(QPalette::Dark, QColor("#374151"));
    palette.setColor(QPalette::Highlight, QColor("#0a84ff"));
    palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    palette.setColor(QPalette::Link, QColor("#0a84ff"));
    palette.setColor(QPalette::ToolTipBase, QColor("#2f2f2f"));
    palette.setColor(QPalette::ToolTipText, QColor("#f0f0f0"));
    palette.setColor(QPalette::PlaceholderText, QColor("#a0a0a0"));

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
