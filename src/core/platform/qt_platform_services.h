#pragma once

#include "classmngr/engine/platform_services.h"

#include <optional>
#include <string_view>

class QNetworkAccessManager;
class QSettings;

namespace classmngr::qt
{

class QtSettingsStore final : public engine::SettingsStore
{
public:
    explicit QtSettingsStore(QSettings& settings);

    [[nodiscard]] engine::Result<std::optional<engine::PlatformSettingValue>> read(
        std::string_view key
        ) const override;
    [[nodiscard]] engine::Status write(
        std::string_view key,
        const engine::PlatformSettingValue& value
        ) override;
    [[nodiscard]] engine::Status remove(std::string_view key) override;

private:
    QSettings& m_settings;
};

class QtNetworkClient final : public engine::NetworkClient
{
public:
    explicit QtNetworkClient(QNetworkAccessManager& manager);

    [[nodiscard]] engine::Result<engine::NetworkResponse> request(
        const engine::NetworkRequest& request
        ) override;

private:
    QNetworkAccessManager& m_manager;
};

class QtSignatureVerifier final : public engine::SignatureVerifier
{
public:
    [[nodiscard]] engine::Status verify(
        const engine::DetachedSignature& detached
        ) const override;
};

class QtProcessLauncher final : public engine::ProcessLauncher
{
public:
    [[nodiscard]] engine::Result<engine::ProcessResult> launch(
        const engine::ProcessLaunchRequest& request
        ) override;
    [[nodiscard]] engine::Status launchDetached(
        const engine::ProcessLaunchRequest& request
        ) override;
};

class QtClock final : public engine::Clock
{
public:
    [[nodiscard]] engine::WallClockTimePoint nowUtc() const noexcept override;
    [[nodiscard]] engine::MonotonicTimePoint monotonicNow() const noexcept override;
};

class QtLogger final : public engine::Logger
{
public:
    void log(const engine::LogRecord& record) noexcept override;
};

class QtResourceProvider final : public engine::ResourceProvider
{
public:
    [[nodiscard]] engine::Result<bool> exists(
        std::string_view logicalPath
        ) const override;
    [[nodiscard]] engine::Result<engine::ByteBuffer> readBytes(
        std::string_view logicalPath
        ) const override;
    [[nodiscard]] engine::Result<engine::ResourceMetadata> metadata(
        std::string_view logicalPath
        ) const override;
};

using QtCancellationToken = engine::CallbackCancellationToken;

} // namespace classmngr::qt
