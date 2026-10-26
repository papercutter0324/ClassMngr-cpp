#pragma once

#include "next/application/theme_preferences_port.h"
#include "next/application/user_preferences_state.h"
#include "next/resources/resource_loader.h"

#include <QApplication>
#include <QStyleHints>

#include <functional>
#include <optional>

namespace ClassMngr::Next::Ui
{

class ThemePresenter final
{
public:
    using ColorSchemeProvider = std::function<Qt::ColorScheme()>;

    ThemePresenter(
        QApplication& application,
        Resources::ResourceLoader& resourceLoader,
        Resources::ResourceScope startupScope,
        Application::ThemePreferencesPort& preferences,
        ColorSchemeProvider colorSchemeProvider = {}
        );
    ~ThemePresenter();

    ThemePresenter(const ThemePresenter&) = delete;
    ThemePresenter& operator=(const ThemePresenter&) = delete;

    // Startup uses the value already read at the settings stage and never
    // writes it back. Application::Theme is the persisted port vocabulary.
    [[nodiscard]] Domain::Result<void> applyStartupPreference(
        Application::Theme preference
        );

    // This value-only seam is ready for the future preference action consumer.
    // It persists before changing the installed stylesheet or active value.
    [[nodiscard]] Domain::Result<void> changePreference(
        Application::ThemePreference preference
        );

    // Restore the app stylesheet before releasing the scoped stylesheet value.
    void restore() noexcept;

    [[nodiscard]] bool ownsActiveStylesheet() const noexcept;

private:
    enum class Appearance
    {
        Light,
        Dark
    };

    [[nodiscard]] Domain::Result<void> apply(
        Application::ThemePreference preference,
        bool persist
        );
    [[nodiscard]] Appearance resolveAppearance(
        Application::ThemePreference preference
        ) const;
    [[nodiscard]] Domain::Result<Resources::ScopedResource<QString>>
    loadStylesheet(Appearance appearance) const;

    QApplication& m_application;
    Resources::ResourceLoader& m_resourceLoader;
    Resources::ResourceScope m_startupScope;
    Application::ThemePreferencesPort& m_preferences;
    ColorSchemeProvider m_colorSchemeProvider;
    std::optional<Resources::ScopedResource<QString>> m_activeStylesheet;
    std::optional<Application::ThemePreference> m_activePreference;
    std::optional<Appearance> m_activeAppearance;
    QString m_previousApplicationStylesheet;
    bool m_applicationStylesheetInstalled = false;
};

}  // namespace ClassMngr::Next::Ui
