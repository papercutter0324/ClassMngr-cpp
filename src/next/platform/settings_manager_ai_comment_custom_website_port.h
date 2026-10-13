#pragma once

#include "core/settingsmanager.h"
#include "next/application/ai_comment_custom_website_port.h"
#include "ui/shared/state/option_state_keys.h"

#include <QByteArray>
#include <QString>

#include <cstddef>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the custom AI website. The legacy
// SettingsManager singleton, option key, and QVariant conversion stay here;
// callers consume only the typed UTF-8 read/write/clear contract.
class SettingsManagerAiCommentCustomWebsitePort final
    : public Application::AiCommentCustomWebsitePort
{
public:
    explicit SettingsManagerAiCommentCustomWebsitePort(
        SettingsManager::AiCommentCustomWebsitePreferenceSync synchronize = {}
        )
        : m_synchronize(std::move(synchronize))
    {
    }

    [[nodiscard]] std::string read() const override
    {
        const QString storedWebsiteUrl =
            SettingsManager::instance().get(key()).toString();

        return toUtf8(storedWebsiteUrl);
    }

    [[nodiscard]] Domain::Result<void> save(
        const std::string& websiteUrl
        ) const override
    {
        const QSettings::Status status =
            SettingsManager::instance()
                .writeAiCommentCustomWebsitePreferences(
                    fromUtf8(websiteUrl),
                    m_synchronize
                    );
        if (status != QSettings::NoError)
        {
            const char* statusName = status == QSettings::AccessError
                ? "AccessError (settings could not be written)"
                : "FormatError (settings contain invalid data)";
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = std::string(
                    "Unable to save AI Comment Custom Website "
                    "preferences: QSettings "
                    ) + statusName + ".",
                .recoverable = true
            });
        }

        return Domain::Result<void>::success();
    }

    void clear() const override
    {
        SettingsManager::instance().remove(key());
    }

private:
    SettingsManager::AiCommentCustomWebsitePreferenceSync m_synchronize;

    [[nodiscard]] static QString key()
    {
        return QString::fromUtf8(
            OptionKeys::AiCommentCustomWebsiteUrl
            );
    }

    [[nodiscard]] static std::string toUtf8(
        const QString& value
        )
    {
        const QByteArray encoded = value.toUtf8();
        return std::string(
            encoded.constData(),
            static_cast<std::size_t>(encoded.size())
            );
    }

    [[nodiscard]] static QString fromUtf8(
        const std::string& value
        )
    {
        return QString::fromUtf8(
            value.data(),
            static_cast<qsizetype>(value.size())
            );
    }
};

} // namespace ClassMngr::Next::Platform
