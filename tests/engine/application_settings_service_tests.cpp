#include "classmngr/engine/application_settings_service.h"
#include "classmngr/engine/open_database.h"
#include "classmngr/engine/sqlite_database.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace
{
using classmngr::engine::ApplicationSettings;
using classmngr::engine::ApplicationSettingsService;
using classmngr::engine::ErrorCode;
using classmngr::engine::OpenDatabase;
using classmngr::engine::SettingValue;
using classmngr::engine::SqliteDatabase;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineApplicationSettingsServiceTests: "
              << message
              << '\n';
    return false;
}

const SettingValue* loadedValue(
    const classmngr::engine::Result<SettingValue>& result
    )
{
    return result ? &*result : nullptr;
}
} // namespace

int main()
{
    const auto opened = OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        std::cerr << "ClassMngrEngineApplicationSettingsServiceTests: "
                  << "OpenDatabase failed\n";
        return 1;
    }

    SqliteDatabase& database = **opened;
    ApplicationSettingsService service(database);
    bool passed = true;

    const std::string utf8Key = "설정/名前";
    const std::string utf8Value = "값-значение-値";
    passed &= expect(
        service.save(utf8Key, SettingValue{utf8Value}).has_value(),
        "UTF-8 setting save failed"
        );
    const auto loadedUtf8 = service.load(utf8Key);
    const auto* loadedUtf8Value = loadedValue(loadedUtf8);
    passed &= expect(
        loadedUtf8Value != nullptr
            && std::get_if<std::string>(loadedUtf8Value) != nullptr
            && *std::get_if<std::string>(loadedUtf8Value) == utf8Value,
        "UTF-8 setting round trip failed"
        );

    passed &= expect(
        service.save("bool", SettingValue{std::int64_t{1}}).has_value(),
        "boolean setting save failed"
        );
    // The retained schema declares app_settings.value as TEXT. SQLite keeps
    // NULL and BLOB storage classes, but applies TEXT affinity to numerics.
    passed &= expect(
        service.save("integer", SettingValue{std::int64_t{-42}}).has_value(),
        "integer setting save failed"
        );
    passed &= expect(
        service.save("double", SettingValue{3.125}).has_value(),
        "double setting save failed"
        );

    const std::vector<std::byte> blob{
        std::byte{0x00},
        std::byte{0x7f},
        std::byte{0xff}
    };
    passed &= expect(
        service.save("blob", SettingValue{blob}).has_value(),
        "blob setting save failed"
        );
    passed &= expect(
        service.save("null", SettingValue{std::monostate{}}).has_value(),
        "null setting save failed"
        );

    const auto loadedInteger = service.load("integer");
    passed &= expect(
        loadedInteger
            && std::get_if<std::string>(&*loadedInteger) != nullptr
            && *std::get_if<std::string>(&*loadedInteger) == "-42",
        "integer setting did not preserve TEXT-column semantics"
        );
    const auto loadedDouble = service.load("double");
    passed &= expect(
        loadedDouble
            && std::get_if<std::string>(&*loadedDouble) != nullptr
            && *std::get_if<std::string>(&*loadedDouble) == "3.125",
        "double setting did not preserve TEXT-column semantics"
        );
    const auto loadedBlob = service.load("blob");
    passed &= expect(
        loadedBlob
            && std::get_if<std::vector<std::byte>>(&*loadedBlob) != nullptr
            && *std::get_if<std::vector<std::byte>>(&*loadedBlob) == blob,
        "blob setting round trip failed"
        );
    const auto loadedNull = service.load("null");
    passed &= expect(
        loadedNull
            && std::holds_alternative<std::monostate>(*loadedNull),
        "NULL setting was not represented as an empty value"
        );
    const auto missing = service.load("missing");
    passed &= expect(
        missing && std::holds_alternative<std::monostate>(*missing),
        "missing setting did not return an empty value"
        );

    passed &= expect(
        service.save(utf8Key, SettingValue{std::string("updated")})
            .has_value(),
        "setting update failed"
        );
    const auto updated = service.load(utf8Key);
    passed &= expect(
        updated
            && std::get_if<std::string>(&*updated) != nullptr
            && *std::get_if<std::string>(&*updated) == "updated",
        "setting update did not replace the previous value"
        );

    const ApplicationSettings batch{
        {"batch/one", SettingValue{std::string("one")}},
        {"batch/two", SettingValue{std::int64_t{2}}},
        {"batch/null", SettingValue{std::monostate{}}}
    };
    passed &= expect(
        service.saveBatch(batch).has_value(),
        "batch setting save failed"
        );
    passed &= expect(
        service.saveBatch(ApplicationSettings{}).has_value(),
        "empty setting batch was not a no-op"
        );

    passed &= expect(
        database.execute(
            "CREATE TRIGGER reject_application_setting "
            "BEFORE INSERT ON app_settings "
            "WHEN NEW.key = 'rollback/reject' "
            "BEGIN "
            "SELECT RAISE(ABORT, 'injected setting failure'); "
            "END"
            ).has_value(),
        "rollback trigger creation failed"
        );
    const auto rolledBack = service.saveBatch({
        {"rollback/first", SettingValue{std::string("must roll back")}},
        {"rollback/reject", SettingValue{std::string("reject")}}
    });
    passed &= expect(
        !rolledBack && rolledBack.error().code == ErrorCode::Database,
        "batch failure did not return a database error"
        );
    const auto rolledBackFirst = service.load("rollback/first");
    passed &= expect(
        rolledBackFirst
            && std::holds_alternative<std::monostate>(*rolledBackFirst),
        "failed setting batch was not rolled back"
        );
    passed &= expect(
        !rolledBack
            && rolledBack.error().message.find("rollback/reject")
                != std::string::npos,
        "batch failure did not retain the setting key context"
        );

    passed &= expect(
        database.execute("DROP TABLE app_settings").has_value()
            && database.execute(
                "CREATE TABLE app_settings (key TEXT, value TEXT)"
                ).has_value()
            && database.execute(
                "INSERT INTO app_settings (key, value) VALUES "
                "('malformed/key', 'first'), "
                "('malformed/key', 'second')"
                ).has_value(),
        "malformed app_settings fixture creation failed"
        );
    const auto malformed = service.load("malformed/key");
    passed &= expect(
        !malformed && malformed.error().code == ErrorCode::Schema,
        "duplicate app_settings rows did not return a schema error"
        );

    return passed ? 0 : 1;
}
