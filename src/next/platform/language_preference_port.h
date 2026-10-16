#pragma once

#include "core/language_service.h"
#include "next/application/user_preferences_state.h"

namespace ClassMngr::Next::Platform
{

// Explicit Qt-facing adapter for the application language preference. The
// application state remains typed and Qt-free; only this boundary knows the
// legacy Language representation and applies it to LanguageService.
class LanguagePreferencePort final
{
public:
    explicit LanguagePreferencePort(
        LanguageService& languageService
        ) noexcept
        : m_languageService(languageService)
    {
    }

    LanguagePreferencePort(
        const LanguagePreferencePort&
        ) = delete;
    LanguagePreferencePort& operator=(
        const LanguagePreferencePort&
        ) = delete;
    LanguagePreferencePort(
        LanguagePreferencePort&&
        ) = delete;
    LanguagePreferencePort& operator=(
        LanguagePreferencePort&&
        ) = delete;

    [[nodiscard]] Domain::Result<void> apply(
        const Application::LanguagePreference preference
        ) const
    {
        auto prepared = prepare(preference);
        if (!prepared)
        {
            return Domain::Result<void>::failure(prepared.error());
        }
        return applyPrepared(std::move(prepared.value()));
    }

    using PreparedLanguage = std::unique_ptr<LanguageService::PreparedLanguage>;

    [[nodiscard]] Domain::Result<PreparedLanguage> prepare(
        const Application::LanguagePreference preference
        ) const
    {
        Language language;
        switch (preference)
        {
        case Application::LanguagePreference::SystemDefault:
            language = Language::SystemDefault;
            break;

        case Application::LanguagePreference::English:
            language = Language::English;
            break;

        case Application::LanguagePreference::Korean:
            language = Language::Korean;
            break;

        default:
            return Domain::Result<PreparedLanguage>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message =
                    "Language preference must be SystemDefault, English, or Korean.",
                .recoverable = false
            });
        }

        auto prepared = m_languageService.prepareLanguage(language);
        if (!prepared)
        {
            return Domain::Result<PreparedLanguage>::failure({
                .code = Domain::ErrorCode::Technical,
                .message =
                    "Language service could not load the requested locale.",
                .recoverable = false
            });
        }

        return Domain::Result<PreparedLanguage>::success(std::move(prepared));
    }

    [[nodiscard]] Domain::Result<void> applyPrepared(PreparedLanguage prepared) const
    {
        if (!m_languageService.applyPreparedLanguage(std::move(prepared)))
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "Language service could not install the requested locale.",
                .recoverable = false
            });
        }
        return Domain::Result<void>::success();
    }

    [[nodiscard]] QString loadedLocaleName() const
    {
        return m_languageService.loadedLocaleName();
    }

    [[nodiscard]] Language currentLanguage() const
    {
        return m_languageService.currentLanguage();
    }

private:
    LanguageService& m_languageService;
};

} // namespace ClassMngr::Next::Platform
