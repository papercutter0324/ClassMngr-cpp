#pragma once

#include "next/domain/operation_result.h"

#include <optional>

namespace ClassMngr::Next::Application
{

inline constexpr int kMinimumExcelImportTimeoutSeconds = 30;
inline constexpr int kMaximumExcelImportTimeoutSeconds = 300;
inline constexpr int kDefaultExcelImportTimeoutSeconds = 120;

enum class ExcelImportTimeoutChoice
{
    Seconds30,
    Seconds60,
    Seconds120,
    Seconds300
};

inline constexpr ExcelImportTimeoutChoice kDefaultExcelImportTimeoutChoice =
    ExcelImportTimeoutChoice::Seconds120;

[[nodiscard]] constexpr bool isValidExcelImportTimeoutChoice(
    const ExcelImportTimeoutChoice choice
    ) noexcept
{
    switch (choice)
    {
    case ExcelImportTimeoutChoice::Seconds30:
    case ExcelImportTimeoutChoice::Seconds60:
    case ExcelImportTimeoutChoice::Seconds120:
    case ExcelImportTimeoutChoice::Seconds300:
        return true;
    }

    return false;
}

[[nodiscard]] constexpr int excelImportTimeoutChoiceToSeconds(
    const ExcelImportTimeoutChoice choice
    ) noexcept
{
    switch (choice)
    {
    case ExcelImportTimeoutChoice::Seconds30:
        return 30;
    case ExcelImportTimeoutChoice::Seconds60:
        return 60;
    case ExcelImportTimeoutChoice::Seconds120:
        return 120;
    case ExcelImportTimeoutChoice::Seconds300:
        return 300;
    }

    return kDefaultExcelImportTimeoutSeconds;
}

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

[[nodiscard]] constexpr std::optional<ExcelImportTimeoutChoice>
tryExcelImportTimeoutSecondsToChoice(const int seconds) noexcept
{
    switch (seconds)
    {
    case 30:
        return ExcelImportTimeoutChoice::Seconds30;
    case 60:
        return ExcelImportTimeoutChoice::Seconds60;
    case 120:
        return ExcelImportTimeoutChoice::Seconds120;
    case 300:
        return ExcelImportTimeoutChoice::Seconds300;
    default:
        return std::nullopt;
    }
}

[[nodiscard]] constexpr ExcelImportTimeoutChoice
excelImportTimeoutSecondsToChoice(const int seconds) noexcept
{
    return tryExcelImportTimeoutSecondsToChoice(seconds).value_or(
        kDefaultExcelImportTimeoutChoice
        );
}

// The timeout value is kept as a small value object so the application
// boundary exposes neither Qt storage types nor the legacy settings singleton.
struct ExcelImportTimeoutPreferences final
{
    ExcelImportTimeoutChoice choice = kDefaultExcelImportTimeoutChoice;

    [[nodiscard]] constexpr int normalizedSeconds() const noexcept
    {
        return excelImportTimeoutChoiceToSeconds(choice);
    }

    friend bool operator==(
        const ExcelImportTimeoutPreferences&,
        const ExcelImportTimeoutPreferences&
        ) = default;
};

// Persistence is deliberately expressed as read/write operations. Concrete
// adapters own the storage mechanism and reject unsupported writes while the
// application contract remains singleton-free.
class ExcelImportTimeoutPreferencesPort
{
public:
    virtual ~ExcelImportTimeoutPreferencesPort() = default;

    [[nodiscard]] virtual ExcelImportTimeoutPreferences read() const = 0;

    [[nodiscard]] virtual Domain::Result<void> write(
        const ExcelImportTimeoutPreferences& preferences
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
