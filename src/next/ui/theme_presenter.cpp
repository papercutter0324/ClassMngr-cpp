#include "next/ui/theme_presenter.h"

#include "next/resources/resource_id.h"

#include <QString>

namespace ClassMngr::Next::Ui
{
namespace
{

[[nodiscard]] Domain::Result<void> invalidThemePreference()
{
    return Domain::Result<void>::failure({
        .code = Domain::ErrorCode::InvalidInput,
        .message = "Theme preference must be SystemDefault, Light, or Dark.",
        .recoverable = false
    });
}

}  // namespace

ThemePresenter::ThemePresenter(
    QApplication& application,
    Resources::ResourceLoader& resourceLoader,
    Resources::ResourceScope startupScope,
    Application::ThemePreferencesPort& preferences,
    ColorSchemeProvider colorSchemeProvider
    )
    : m_application(application)
    , m_resourceLoader(resourceLoader)
    , m_startupScope(std::move(startupScope))
    , m_preferences(preferences)
    , m_colorSchemeProvider(std::move(colorSchemeProvider))
{
    if (!m_colorSchemeProvider)
    {
        m_colorSchemeProvider = [&application] {
            return application.styleHints()->colorScheme();
        };
    }
}

ThemePresenter::~ThemePresenter()
{
    restore();
}

Domain::Result<void> ThemePresenter::applyStartupPreference(
    const Application::Theme preference
    )
{
    Application::ThemePreference mappedPreference;
    switch (preference)
    {
    case Application::Theme::SystemDefault:
        mappedPreference = Application::ThemePreference::SystemDefault;
        break;
    case Application::Theme::Light:
        mappedPreference = Application::ThemePreference::Light;
        break;
    case Application::Theme::Dark:
        mappedPreference = Application::ThemePreference::Dark;
        break;
    default:
        return invalidThemePreference();
    }

    return apply(mappedPreference, false);
}

Domain::Result<void> ThemePresenter::changePreference(
    const Application::ThemePreference preference
    )
{
    return apply(preference, true);
}

Domain::Result<void> ThemePresenter::apply(
    const Application::ThemePreference preference,
    const bool persist
    )
{
    switch (preference)
    {
    case Application::ThemePreference::SystemDefault:
    case Application::ThemePreference::Light:
    case Application::ThemePreference::Dark:
        break;
    default:
        return invalidThemePreference();
    }

    if (m_activePreference == preference)
    {
        return Domain::Result<void>::success();
    }

    const Appearance candidateAppearance = resolveAppearance(preference);
    auto candidateStylesheet = loadStylesheet(candidateAppearance);
    if (!candidateStylesheet)
    {
        return Domain::Result<void>::failure(candidateStylesheet.error());
    }

    if (persist)
    {
        Application::Theme persistedPreference;
        switch (preference)
        {
        case Application::ThemePreference::SystemDefault:
            persistedPreference = Application::Theme::SystemDefault;
            break;
        case Application::ThemePreference::Light:
            persistedPreference = Application::Theme::Light;
            break;
        case Application::ThemePreference::Dark:
            persistedPreference = Application::Theme::Dark;
            break;
        default:
            return invalidThemePreference();
        }

        const Domain::Result<void> persisted =
            m_preferences.write(persistedPreference);
        if (!persisted)
        {
            return Domain::Result<void>::failure(persisted.error());
        }
    }

    if (!m_applicationStylesheetInstalled)
    {
        m_previousApplicationStylesheet = m_application.styleSheet();
    }

    m_application.setStyleSheet(candidateStylesheet.value().get());
    m_activeStylesheet.emplace(std::move(candidateStylesheet.value()));
    m_activePreference = preference;
    m_activeAppearance = candidateAppearance;
    m_applicationStylesheetInstalled = true;
    return Domain::Result<void>::success();
}

ThemePresenter::Appearance ThemePresenter::resolveAppearance(
    const Application::ThemePreference preference
    ) const
{
    switch (preference)
    {
    case Application::ThemePreference::Dark:
        return Appearance::Dark;
    case Application::ThemePreference::Light:
        return Appearance::Light;
    case Application::ThemePreference::SystemDefault:
        return m_colorSchemeProvider() == Qt::ColorScheme::Dark
            ? Appearance::Dark
            : Appearance::Light;
    }

    return Appearance::Light;
}

Domain::Result<Resources::ScopedResource<QString>>
ThemePresenter::loadStylesheet(const Appearance appearance) const
{
    QString themeName;
    switch (appearance)
    {
    case Appearance::Light:
        themeName = QStringLiteral("light");
        break;
    case Appearance::Dark:
        themeName = QStringLiteral("dark");
        break;
    }

    const QString logicalId = QStringLiteral("resource:assets/styles/%1.qss")
        .arg(themeName);
    const auto id = Resources::ResourceId::fromString(logicalId);
    if (!id)
    {
        return Domain::Result<Resources::ScopedResource<QString>>::failure({
            .code = Domain::ErrorCode::InvalidInput,
            .message = "Selected stylesheet resource ID is invalid.",
            .recoverable = false
        });
    }

    const auto request = Resources::ResourceRequest::create(
        *id,
        m_startupScope
        );
    if (!request)
    {
        return Domain::Result<Resources::ScopedResource<QString>>::failure({
            .code = Domain::ErrorCode::InvalidInput,
            .message = request.error().message.toStdString(),
            .recoverable = false
        });
    }

    auto stylesheet = m_resourceLoader.loadStylesheet(*request);
    if (!stylesheet)
    {
        return Domain::Result<Resources::ScopedResource<QString>>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = stylesheet.error().message.toStdString(),
            .recoverable = true
        });
    }

    return Domain::Result<Resources::ScopedResource<QString>>::success(
        std::move(*stylesheet)
        );
}

void ThemePresenter::restore() noexcept
{
    if (m_applicationStylesheetInstalled)
    {
        m_application.setStyleSheet(m_previousApplicationStylesheet);
        m_applicationStylesheetInstalled = false;
    }

    // Keep the installed QSS alive until Qt has restored the prior stylesheet.
    m_activeStylesheet.reset();
    m_activePreference.reset();
    m_activeAppearance.reset();
    m_previousApplicationStylesheet.clear();
}

bool ThemePresenter::ownsActiveStylesheet() const noexcept
{
    return m_activeStylesheet.has_value();
}

}  // namespace ClassMngr::Next::Ui
