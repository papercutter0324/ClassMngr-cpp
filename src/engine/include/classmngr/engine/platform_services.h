#pragma once

#include "classmngr/engine/result.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace classmngr::engine
{

// These contracts deliberately use UTF-8 strings and byte strings.  Adapters
// may translate them to native paths, QVariant, QNetworkReply, or WinRT types
// at the boundary, but no platform type crosses into the portable engine.

using ByteBuffer = std::vector<std::byte>;
using Header = std::pair<std::string, std::string>;
using PlatformSettingValue = std::variant<
    std::monostate,
    bool,
    std::int64_t,
    double,
    std::string,
    ByteBuffer
    >;

class SettingsStore
{
public:
    virtual ~SettingsStore() = default;

    [[nodiscard]] virtual Result<std::optional<PlatformSettingValue>> read(
        std::string_view key
        ) const = 0;

    [[nodiscard]] virtual Status write(
        std::string_view key,
        const PlatformSettingValue& value
        ) = 0;

    [[nodiscard]] virtual Status remove(std::string_view key) = 0;

    template<class T>
    [[nodiscard]] Result<std::optional<T>> readAs(std::string_view key) const
    {
        const auto value = read(key);
        if (!value)
        {
            return std::unexpected(value.error());
        }
        if (!value->has_value())
        {
            return std::optional<T>{};
        }
        const auto* typed = std::get_if<T>(&value->value());
        if (typed == nullptr)
        {
            return std::unexpected(Error{
                ErrorCode::InvalidFormat,
                "The setting has a different value type.",
                std::nullopt
            });
        }
        return std::optional<T>{*typed};
    }
};

class CancellationToken
{
public:
    virtual ~CancellationToken() = default;
    [[nodiscard]] virtual bool isCancellationRequested() const noexcept = 0;
};

class AtomicCancellationToken final : public CancellationToken
{
public:
    AtomicCancellationToken();

    [[nodiscard]] bool isCancellationRequested() const noexcept override;
    void requestCancellation() noexcept;
    void reset() noexcept;

private:
    explicit AtomicCancellationToken(std::shared_ptr<std::atomic_bool> state);
    friend class CancellationSource;
    std::shared_ptr<std::atomic_bool> m_state;
};

class CancellationSource final
{
public:
    CancellationSource();

    [[nodiscard]] const CancellationToken& token() const noexcept;
    [[nodiscard]] AtomicCancellationToken& mutableToken() noexcept;
    [[nodiscard]] bool isCancellationRequested() const noexcept;
    void requestCancellation() noexcept;
    void reset() noexcept;

private:
    AtomicCancellationToken m_token;
};

class CallbackCancellationToken final : public CancellationToken
{
public:
    explicit CallbackCancellationToken(std::function<bool()> callback);
    [[nodiscard]] bool isCancellationRequested() const noexcept override;

private:
    std::function<bool()> m_callback;
};

struct NetworkRequest
{
    std::string url;
    std::string method = "GET";
    std::vector<Header> headers;
    ByteBuffer body;
    std::chrono::milliseconds timeout{30'000};
    const CancellationToken* cancellation = nullptr;
};

struct NetworkResponse
{
    int statusCode = 0;
    std::vector<Header> headers;
    ByteBuffer body;
};

class NetworkClient
{
public:
    virtual ~NetworkClient() = default;
    [[nodiscard]] virtual Result<NetworkResponse> request(
        const NetworkRequest& request
        ) = 0;
};

struct DetachedSignature
{
    ByteBuffer payload;
    ByteBuffer signature;
    ByteBuffer publicKey;
};

class SignatureVerifier
{
public:
    virtual ~SignatureVerifier() = default;
    [[nodiscard]] virtual Status verify(
        const DetachedSignature& detached
        ) const = 0;
};

struct ProcessLaunchRequest
{
    std::string executable;
    std::vector<std::string> arguments;
    std::string workingDirectory;
    std::chrono::milliseconds timeout{30'000};
    const CancellationToken* cancellation = nullptr;
};

struct ProcessResult
{
    int exitCode = 0;
    ByteBuffer standardOutput;
    ByteBuffer standardError;
};

class ProcessLauncher
{
public:
    virtual ~ProcessLauncher() = default;
    [[nodiscard]] virtual Result<ProcessResult> launch(
        const ProcessLaunchRequest& request
        ) = 0;
    [[nodiscard]] virtual Status launchDetached(
        const ProcessLaunchRequest& request
        ) = 0;
};

using WallClockTimePoint = std::chrono::system_clock::time_point;
using MonotonicTimePoint = std::chrono::steady_clock::time_point;

class Clock
{
public:
    virtual ~Clock() = default;
    [[nodiscard]] virtual WallClockTimePoint nowUtc() const noexcept = 0;
    [[nodiscard]] virtual MonotonicTimePoint monotonicNow() const noexcept = 0;
};

class SystemClock final : public Clock
{
public:
    [[nodiscard]] WallClockTimePoint nowUtc() const noexcept override;
    [[nodiscard]] MonotonicTimePoint monotonicNow() const noexcept override;
};

enum class LogSeverity
{
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

struct LogRecord
{
    LogSeverity severity = LogSeverity::Info;
    std::string message;
    std::map<std::string, std::string, std::less<>> context;
};

class Logger
{
public:
    virtual ~Logger() = default;
    virtual void log(const LogRecord& record) noexcept = 0;
};

class NullLogger final : public Logger
{
public:
    void log(const LogRecord& record) noexcept override;
};

struct ResourceMetadata
{
    std::uintmax_t sizeBytes = 0;
    bool isRegularFile = true;
};

class ResourceProvider
{
public:
    virtual ~ResourceProvider() = default;
    [[nodiscard]] virtual Result<bool> exists(
        std::string_view logicalPath
        ) const = 0;
    [[nodiscard]] virtual Result<ByteBuffer> readBytes(
        std::string_view logicalPath
        ) const = 0;
    [[nodiscard]] virtual Result<ResourceMetadata> metadata(
        std::string_view logicalPath
        ) const = 0;
};

} // namespace classmngr::engine
