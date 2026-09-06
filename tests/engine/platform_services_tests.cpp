#include "classmngr/engine/platform_services.h"

#include <chrono>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <utility>

using namespace classmngr::engine;

namespace
{
bool expect(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "ClassMngrEnginePlatformServicesTests: " << message << '\n';
    }
    return condition;
}

ByteBuffer bytes(std::string_view text)
{
    ByteBuffer result;
    result.reserve(text.size());
    for (const auto character : text)
    {
        result.push_back(static_cast<std::byte>(character));
    }
    return result;
}

class FakeSettings final : public SettingsStore
{
public:
    Result<std::optional<PlatformSettingValue>> read(std::string_view key) const override
    {
        const auto found = values.find(std::string(key));
        if (found == values.end()) return std::optional<PlatformSettingValue>{};
        return found->second;
    }

    Status write(std::string_view key, const PlatformSettingValue& value) override
    {
        values[std::string(key)] = value;
        return {};
    }

    Status remove(std::string_view key) override
    {
        values.erase(std::string(key));
        return {};
    }

    std::map<std::string, PlatformSettingValue> values;
};

class FakeNetwork final : public NetworkClient
{
public:
    Result<NetworkResponse> request(const NetworkRequest& request) override
    {
        if (request.cancellation != nullptr
            && request.cancellation->isCancellationRequested())
        {
            return std::unexpected(Error{
                ErrorCode::Cancelled, "request cancelled", std::nullopt
            });
        }
        if (failure)
        {
            return std::unexpected(*failure);
        }
        seenUrl = request.url;
        seenTimeout = request.timeout;
        return NetworkResponse{200, {}, bytes("ok")};
    }

    std::string seenUrl;
    std::chrono::milliseconds seenTimeout{};
    std::optional<Error> failure;
};

class FakeSignatureVerifier final : public SignatureVerifier
{
public:
    Status verify(const DetachedSignature& detached) const override
    {
        if (failure)
        {
            return std::unexpected(*failure);
        }
        if (detached.payload == bytes("payload")
            && detached.signature == bytes("signature")
            && detached.publicKey == bytes("public-key")) return {};
        return std::unexpected(Error{
            ErrorCode::InvalidArgument, "signature mismatch", std::nullopt
        });
    }

    std::optional<Error> failure;
};

class FakeLauncher final : public ProcessLauncher
{
public:
    Result<ProcessResult> launch(const ProcessLaunchRequest& request) override
    {
        if (request.cancellation != nullptr
            && request.cancellation->isCancellationRequested())
        {
            return std::unexpected(Error{
                ErrorCode::Cancelled, "launch cancelled", std::nullopt
            });
        }
        if (failure)
        {
            return std::unexpected(*failure);
        }
        seenExecutable = request.executable;
        seenWorkingDirectory = request.workingDirectory;
        return ProcessResult{0, bytes("stdout"), bytes("stderr")};
    }

    Status launchDetached(const ProcessLaunchRequest& request) override
    {
        if (detachedFailure)
        {
            return std::unexpected(*detachedFailure);
        }
        seenExecutable = request.executable;
        return {};
    }

    std::string seenExecutable;
    std::string seenWorkingDirectory;
    std::optional<Error> failure;
    std::optional<Error> detachedFailure;
};

class FakeResources final : public ResourceProvider
{
public:
    Result<bool> exists(std::string_view path) const override
    {
        return path == "fixture.txt";
    }

    Result<ByteBuffer> readBytes(std::string_view path) const override
    {
        if (path != "fixture.txt")
        {
            return std::unexpected(Error{
                ErrorCode::NotFound, "resource missing", std::nullopt
            });
        }
        return bytes("fixture bytes");
    }

    Result<ResourceMetadata> metadata(std::string_view path) const override
    {
        if (path != "fixture.txt")
        {
            return std::unexpected(Error{
                ErrorCode::NotFound, "resource missing", std::nullopt
            });
        }
        return ResourceMetadata{13, true};
    }
};

class FixedClock final : public Clock
{
public:
    WallClockTimePoint nowUtc() const noexcept override { return wall; }
    MonotonicTimePoint monotonicNow() const noexcept override { return monotonic; }

    WallClockTimePoint wall{};
    MonotonicTimePoint monotonic{};
};

class CaptureLogger final : public Logger
{
public:
    void log(const LogRecord& record) noexcept override
    {
        last = record;
        ++count;
    }

    LogRecord last;
    int count = 0;
};
} // namespace

int main()
{
    bool passed = true;

    FakeSettings settings;
    passed &= expect(settings.write("enabled", true).has_value(), "settings write failed");
    passed &= expect(settings.readAs<bool>("enabled").value().value(), "typed setting read failed");
    passed &= expect(settings.readAs<std::string>("enabled").error().code == ErrorCode::InvalidFormat,
                     "setting type mismatch was not reported");
    passed &= expect(settings.remove("enabled").has_value() && !settings.read("enabled").value().has_value(),
                     "setting removal failed");

    CancellationSource source;
    FakeNetwork network;
    NetworkRequest request{"https://example.test", "POST", {}, bytes("body"), std::chrono::seconds(4), &source.token()};
    const auto response = network.request(request);
    passed &= expect(response && response->statusCode == 200 && response->body == bytes("ok"),
                     "fake network response failed");
    passed &= expect(network.seenTimeout == std::chrono::seconds(4), "network timeout was not preserved");
    source.requestCancellation();
    const auto cancelledRequest = network.request(request);
    passed &= expect(!cancelledRequest && cancelledRequest.error().code == ErrorCode::Cancelled,
                     "network cancellation result failed");
    source.reset();
    network.failure = Error{ErrorCode::Io, "network unavailable", 12007};
    const auto failedRequest = network.request(request);
    passed &= expect(
        !failedRequest
            && failedRequest.error().code == ErrorCode::Io
            && failedRequest.error().nativeCode == 12007,
        "network failure was not propagated"
        );
    network.failure.reset();

    FakeSignatureVerifier verifier;
    passed &= expect(verifier.verify({bytes("payload"), bytes("signature"), bytes("public-key")}).has_value(),
                     "signature verification success failed");
    passed &= expect(verifier.verify({bytes("bad"), bytes("signature"), bytes("public-key")}).error().code == ErrorCode::InvalidArgument,
                     "signature verification error failed");
    verifier.failure = Error{ErrorCode::InvalidFormat, "signature invalid", std::nullopt};
    const auto failedVerification = verifier.verify(
        {bytes("payload"), bytes("signature"), bytes("public-key")}
        );
    passed &= expect(
        !failedVerification
            && failedVerification.error().code == ErrorCode::InvalidFormat,
        "signature failure was not propagated"
        );

    source.reset();
    FakeLauncher launcher;
    ProcessLaunchRequest launch{"tool.exe", {"--check"}, "C:/work", std::chrono::seconds(2), &source.token()};
    const auto process = launcher.launch(launch);
    passed &= expect(process && process->standardOutput == bytes("stdout") && process->standardError == bytes("stderr"),
                     "captured process result failed");
    passed &= expect(launcher.launchDetached(launch).has_value() && launcher.seenExecutable == "tool.exe",
                     "detached process launch failed");
    source.requestCancellation();
    const auto cancelledLaunch = launcher.launch(launch);
    passed &= expect(!cancelledLaunch && cancelledLaunch.error().code == ErrorCode::Cancelled,
                     "process cancellation result failed");
    source.reset();
    launcher.failure = Error{ErrorCode::Io, "process unavailable", 2};
    const auto failedLaunch = launcher.launch(launch);
    passed &= expect(
        !failedLaunch
            && failedLaunch.error().code == ErrorCode::Io
            && failedLaunch.error().nativeCode == 2,
        "process failure was not propagated"
        );
    launcher.failure.reset();
    launcher.detachedFailure = Error{ErrorCode::Io, "detached process unavailable", 2};
    const auto failedDetachedLaunch = launcher.launchDetached(launch);
    passed &= expect(
        !failedDetachedLaunch
            && failedDetachedLaunch.error().code == ErrorCode::Io,
        "detached process failure was not propagated"
        );

    FakeResources resources;
    passed &= expect(resources.exists("fixture.txt").value() && resources.readBytes("fixture.txt").value() == bytes("fixture bytes"),
                     "resource fixture read failed");
    passed &= expect(resources.metadata("fixture.txt")->sizeBytes == 13, "resource metadata failed");
    passed &= expect(resources.readBytes("missing").error().code == ErrorCode::NotFound,
                     "resource missing error failed");

    FixedClock clock;
    clock.wall = WallClockTimePoint(std::chrono::seconds(123));
    clock.monotonic = MonotonicTimePoint(std::chrono::milliseconds(456));
    passed &= expect(clock.nowUtc() == clock.wall && clock.monotonicNow() == clock.monotonic,
                     "deterministic clock failed");
    SystemClock systemClock;
    passed &= expect(systemClock.nowUtc() != WallClockTimePoint{} && systemClock.monotonicNow() != MonotonicTimePoint{},
                     "system clock failed");

    CaptureLogger logger;
    logger.log({LogSeverity::Warning, "warning", {{"source", "test"}}});
    passed &= expect(logger.count == 1 && logger.last.context.at("source") == "test",
                     "logger capture failed");
    NullLogger nullLogger;
    nullLogger.log({LogSeverity::Info, "ignored", {}});

    source.reset();
    passed &= expect(!source.mutableToken().isCancellationRequested(), "cancellation reset failed");
    source.requestCancellation();
    passed &= expect(source.isCancellationRequested() && source.token().isCancellationRequested(),
                     "cancellation source/token failed");
    CallbackCancellationToken callback([] { return true; });
    passed &= expect(callback.isCancellationRequested(), "callback cancellation adapter failed");

    return passed ? 0 : 1;
}
