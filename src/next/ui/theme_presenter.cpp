#include "next/ui/theme_presenter.h"

#include "next/resources/resource_id.h"

#include <QString>
#include <QScopedValueRollback>

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
    Application::PreferencesModel& preferences,
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

Domain::Result<void> ThemePresenter::applyStartupPreference()
{
    return apply(m_preferences.snapshot().themePreference(), false);
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
    if (m_applying)
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::Conflict,
            .message = "A theme preference change is already in progress.",
            .recoverable = true
        });
    }
    const QScopedValueRollback<bool> applying(m_applying, true);

    switch (preference)
    {
    case Application::ThemePreference::SystemDefault:
    case Application::ThemePreference::Light:
    case Application::ThemePreference::Dark:
        break;
    default:
        return invalidThemePreference();
    }

    if (m_activePreference == preference
        && (!persist || (!m_preferences.isDirty()
            && m_preferences.snapshot().themePreference() == preference)))
    {
        return Domain::Result<void>::success();
    }

    const Appearance candidateAppearance = resolveAppearance(preference);
    auto candidateStylesheet = loadStylesheet(candidateAppearance);
    if (!candidateStylesheet)
    {
        return Domain::Result<void>::failure(candidateStylesheet.error());
    }

    const QString previousStylesheet = m_application.styleSheet();
    // The old scoped handle stays alive while Qt stages the candidate and
    // while synchronous model observers inspect the newly committed state.
    m_application.setStyleSheet(candidateStylesheet.value().get());
    const auto retainStylesheet = [&] {
        if (!m_applicationStylesheetInstalled)
        {
            m_previousApplicationStylesheet = previousStylesheet;
        }
        m_activeStylesheet.emplace(std::move(candidateStylesheet.value()));
        m_activePreference = preference;
        m_activeAppearance = candidateAppearance;
        m_applicationStylesheetInstalled = true;
    };
    if (persist)
    {
        auto candidate = m_preferences.state();
        const auto validated = candidate.setThemePreference(preference);
        if (!validated)
        {
            m_application.setStyleSheet(previousStylesheet);
            return validated;
        }
        try
        {
            const auto persisted = m_preferences.applyAndPersist(candidate);
            if (!persisted)
            {
                m_application.setStyleSheet(previousStylesheet);
                return Domain::Result<void>::failure(persisted.error());
            }
        }
        catch (...)
        {
            // Model observer exceptions are rethrown after committing and
            // fanout. Retain the committed QSS even when a subscriber throws.
            if (!m_preferences.isDirty() && m_preferences.state() == candidate)
            {
                retainStylesheet();
            }
            else
            {
                m_application.setStyleSheet(previousStylesheet);
            }
            throw;
        }
    }
    retainStylesheet();
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
