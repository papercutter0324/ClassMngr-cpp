#pragma once

namespace ClassMngr::Next::Application
{

inline constexpr int kMinimumExcelImportTimeoutSeconds = 30;
inline constexpr int kMaximumExcelImportTimeoutSeconds = 300;
inline constexpr int kDefaultExcelImportTimeoutSeconds = 120;

[[nodiscard]] constexpr bool isSupportedExcelImportTimeoutSeconds(
    const int seconds
    ) noexcept
{
    switch (seconds)
    {
    case 30:
    case 60:
    case 120:
    case 300:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] constexpr int normalizeExcelImportTimeoutSeconds(
    const int seconds
    ) noexcept
{
    return isSupportedExcelImportTimeoutSeconds(seconds)
        ? seconds
        : kDefaultExcelImportTimeoutSeconds;
}

// The timeout value is kept as a small value object so the application
// boundary exposes neither Qt storage types nor the legacy settings singleton.
struct ExcelImportTimeoutPreferences final
{
    int seconds = kDefaultExcelImportTimeoutSeconds;

    [[nodiscard]] constexpr int normalizedSeconds() const noexcept
    {
        return normalizeExcelImportTimeoutSeconds(seconds);
    }

    friend bool operator==(
        const ExcelImportTimeoutPreferences&,
        const ExcelImportTimeoutPreferences&
        ) = default;
};

// Persistence is deliberately expressed as read/write operations. Concrete
// adapters own the storage mechanism and may normalize unsupported input at
// their boundary while the application contract remains singleton-free.
class ExcelImportTimeoutPreferencesPort
{
public:
    virtual ~ExcelImportTimeoutPreferencesPort() = default;

    [[nodiscard]] virtual ExcelImportTimeoutPreferences read() const = 0;

    virtual void write(
        const ExcelImportTimeoutPreferences& preferences
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
