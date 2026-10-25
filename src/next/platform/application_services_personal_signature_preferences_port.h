#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/settings_repository.h"
#include "next/application/personal_signature_preferences.h"

#include <QByteArray>
#include <QDebug>
#include <QString>
#include <QVariant>

#include <cstddef>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the read-only personal signature preferences. Exact
// keys, QVariant conversion, and persisted-mode normalization remain here;
// the caller receives only typed values and opaque UTF-8 text. Reads use only
// the active workspace repository and never materialize missing defaults.
class ApplicationServicesPersonalSignaturePreferencesPort final
    : public Application::PersonalSignaturePreferencesPort
{
public:
    explicit ApplicationServicesPersonalSignaturePreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_store(services.workspaceStore())
    {
    }

    explicit ApplicationServicesPersonalSignaturePreferencesPort(
        ApplicationServices* services
        ) noexcept
        : m_store(services ? services->workspaceStore() : nullptr)
    {
    }

    ApplicationServicesPersonalSignaturePreferencesPort(
        const ApplicationServicesPersonalSignaturePreferencesPort&
        ) = delete;
    ApplicationServicesPersonalSignaturePreferencesPort& operator=(
        const ApplicationServicesPersonalSignaturePreferencesPort&
        ) = delete;
    ApplicationServicesPersonalSignaturePreferencesPort(
        ApplicationServicesPersonalSignaturePreferencesPort&&
        ) = delete;
    ApplicationServicesPersonalSignaturePreferencesPort& operator=(
        ApplicationServicesPersonalSignaturePreferencesPort&&
        ) = delete;

    [[nodiscard]] Application::PersonalSignaturePreferencesResult load()
        const override
    {
        if (!m_store || !m_store->isOpen())
        {
            return Application::
                PersonalSignaturePreferencesResult::failure(
                    unavailableError()
                    );
        }

        SettingsRepository* const repository =
            m_store->settingsRepository();
        if (!repository)
        {
            return Application::
                PersonalSignaturePreferencesResult::failure(
                    unavailableError()
                    );
        }

        const QString modeKey = signatureModeKey();
        const auto storedMode = repository->loadSetting(modeKey);
        if (!storedMode)
        {
            return Application::PersonalSignaturePreferencesResult::failure(
                settingLoadError(
                    modeKey,
                    toLegacyDiagnostic(storedMode.error()))
                );
        }

        const QString fontKey = typedSignatureFontKey();
        const auto storedFont = repository->loadSetting(fontKey);
        if (!storedFont)
        {
            return Application::PersonalSignaturePreferencesResult::failure(
                settingLoadError(
                    fontKey,
                    toLegacyDiagnostic(storedFont.error()))
                );
        }

        const QString textKey = typedSignatureTextKey();
        const auto storedText = repository->loadSetting(textKey);
        if (!storedText)
        {
            return Application::PersonalSignaturePreferencesResult::failure(
                settingLoadError(
                    textKey,
                    toLegacyDiagnostic(storedText.error()))
                );
        }

        const QVariant modeValue = storedMode->isValid()
            ? *storedMode
            : QVariant(0);
        const QVariant fontValue = storedFont->isValid()
            ? *storedFont
            : QVariant(0);
        const QVariant textValue = storedText->isValid()
            ? *storedText
            : QVariant(QString());
        const int mode = modeValue.toInt();
        const QByteArray text = textValue.toString().toUtf8();

        return Application::PersonalSignaturePreferencesResult::success({
            .mode = mode == 1
                ? Application::PersonalSignatureMode::Type
                : Application::PersonalSignatureMode::Image,
            .typedSignatureText = std::string(
                text.constData(),
                static_cast<std::size_t>(text.size())
                ),
            .typedSignatureFont = signatureFontChoice(fontValue)
        });
    }

private:
    [[nodiscard]] static Application::PersonalSignatureFontChoice
    signatureFontChoice(const QVariant& value)
    {
        bool integerConversionSucceeded = false;
        const int font = value.toInt(&integerConversionSucceeded);
        bool numericConversionSucceeded = false;
        const double numericValue = value.toDouble(&numericConversionSucceeded);
        if (
            !integerConversionSucceeded
            || !numericConversionSucceeded
            || numericValue != static_cast<double>(font)
            )
        {
            return Application::PersonalSignatureFontChoice::JustAnotherHand;
        }

        switch (font)
        {
        case 0:
            return Application::PersonalSignatureFontChoice::JustAnotherHand;
        case 1:
            return Application::PersonalSignatureFontChoice::DancingScript;
        case 2:
            return Application::PersonalSignatureFontChoice::GreatVibes;
        case 3:
            return Application::PersonalSignatureFontChoice::Caveat;
        default:
            return Application::PersonalSignatureFontChoice::JustAnotherHand;
        }
    }

    [[nodiscard]] static Domain::OperationError settingLoadError(
        const QString& key,
        const QString& diagnostic
        )
    {
        qWarning()
            << "Failed to load setting"
            << key
            << ':'
            << diagnostic;

        std::string message = "Personal signature preference '";
        message.append(key.toStdString());
        message.append("' could not be loaded");
        if (!diagnostic.isEmpty())
        {
            message.append(": ");
            const QByteArray diagnosticBytes = diagnostic.toUtf8();
            message.append(
                diagnosticBytes.constData(),
                static_cast<std::size_t>(diagnosticBytes.size())
                );
        }
        message.push_back('.');

        return {
            .code = Domain::ErrorCode::Technical,
            .message = std::move(message),
            .recoverable = false
        };
    }

    [[nodiscard]] static QString signatureModeKey()
    {
        return QStringLiteral("myInfo/signatureMode");
    }

    [[nodiscard]] static QString typedSignatureTextKey()
    {
        return QStringLiteral("myInfo/typedSignatureText");
    }

    [[nodiscard]] static QString typedSignatureFontKey()
    {
        return QStringLiteral("myInfo/typedSignatureFont");
    }

    [[nodiscard]] static Domain::OperationError unavailableError()
    {
        return {
            .code = Domain::ErrorCode::Technical,
            .message = "Personal signature preferences service is unavailable.",
            .recoverable = false
        };
    }

    WorkspaceStore* m_store = nullptr;
};

} // namespace ClassMngr::Next::Platform
