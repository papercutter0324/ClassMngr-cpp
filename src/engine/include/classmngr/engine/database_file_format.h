#pragma once

#include <string>
#include <string_view>

namespace classmngr::engine::DatabaseFileFormat
{

[[nodiscard]] constexpr std::string_view nativeExtension() noexcept
{
    return ".tps";
}

[[nodiscard]] constexpr std::string_view legacyExtension() noexcept
{
    return ".db";
}

[[nodiscard]] bool isNativePath(
    std::string_view filePath
    ) noexcept;

[[nodiscard]] bool isLegacyPath(
    std::string_view filePath
    ) noexcept;

[[nodiscard]] bool isSupportedInputPath(
    std::string_view filePath
    ) noexcept;

[[nodiscard]] std::string nativeOutputPath(
    std::string_view filePath
    );

[[nodiscard]] std::string supportedInputPath(
    std::string_view filePath
    );

} // namespace classmngr::engine::DatabaseFileFormat
