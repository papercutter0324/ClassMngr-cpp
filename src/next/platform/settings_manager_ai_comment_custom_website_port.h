#pragma once

#include "core/settingsmanager.h"
#include "next/application/ai_comment_custom_website_port.h"
#include "ui/shared/state/option_state_keys.h"

#include <QByteArray>
#include <QString>

#include <cstddef>
#include <string>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the custom AI website. The legacy
// SettingsManager singleton, option key, and QVariant conversion stay here;
// callers consume only the typed UTF-8 read/write/clear contract.
class SettingsManagerAiCommentCustomWebsitePort final
    : public Application::AiCommentCustomWebsitePort
{
public:
    SettingsManagerAiCommentCustomWebsitePort() = default;

    [[nodiscard]] std::string read() const override
    {
        const QString storedWebsiteUrl =
            SettingsManager::instance().get(key()).toString();

        return toUtf8(storedWebsiteUrl);
    }

    void write(
        const std::string& websiteUrl
        ) const override
    {
        SettingsManager::instance().set(
            key(),
            fromUtf8(websiteUrl)
            );
    }

    void clear() const override
    {
        SettingsManager::instance().remove(key());
    }

private:
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
