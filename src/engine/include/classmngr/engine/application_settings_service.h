#pragma once

#include "classmngr/engine/result.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace classmngr::engine
{

class SqliteDatabase;

using SettingValue = std::variant<
    std::monostate,
    std::int64_t,
    double,
    std::string,
    std::vector<std::byte>
    >;

using ApplicationSetting = std::pair<std::string, SettingValue>;
using ApplicationSettings = std::vector<ApplicationSetting>;

class ApplicationSettingsService final
{
public:
    explicit ApplicationSettingsService(
        SqliteDatabase& database
        );

    [[nodiscard]] Status save(
        std::string_view key,
        const SettingValue& value
        );

    [[nodiscard]] Status saveBatch(
        const ApplicationSettings& settings
        );

    [[nodiscard]] Result<SettingValue> load(
        std::string_view key
        );

private:
    SqliteDatabase& m_database;
};

} // namespace classmngr::engine
