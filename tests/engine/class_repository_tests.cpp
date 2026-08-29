#include "classmngr/engine/class_repository.h"
#include "classmngr/engine/database_schema.h"
#include "classmngr/engine/open_database.h"

#include <cstdint>
#include <iostream>
#include <string_view>

namespace
{
using classmngr::engine::ClassRepository;
using classmngr::engine::DatabaseSchemaManager;
using classmngr::engine::ErrorCode;
using classmngr::engine::OpenDatabase;
using classmngr::engine::SqliteParameters;
using classmngr::engine::SqliteValue;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineClassRepositoryTests: "
              << message
              << '\n';
    return false;
}
} // namespace

int main()
{
    const auto opened = OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        std::cerr << "ClassMngrEngineClassRepositoryTests: "
                  << "OpenDatabase failed\n";
        return 1;
    }

    auto& database = **opened;
    ClassRepository repository(database);
    bool passed = true;

    const auto first = repository.create("국어");
    const auto second = repository.create("Testing");
    passed &= expect(
        first && second && *first > 0 && *second > *first,
        "class creation did not return ordered ids"
        );

    if (second)
    {
        passed &= expect(
            database.execute(
                "INSERT INTO testing_classes (class_id, room) VALUES (?, ?)",
                SqliteParameters{
                    SqliteValue{std::int64_t{*second}},
                    SqliteValue{std::string("Room 1")}
                }
                ).has_value(),
            "testing-class fixture could not be inserted"
            );
    }

    const auto classes = repository.list();
    passed &= expect(
        classes && classes->size() == 1
            && classes->front().id == (first ? *first : -1)
            && classes->front().name == "국어",
        "class listing did not return regular UTF-8 classes"
        );

    if (first)
    {
        const auto loaded = repository.get(*first);
        passed &= expect(
            loaded && loaded->name == "국어",
            "class lookup did not preserve UTF-8 text"
            );
        passed &= expect(
            repository.rename(*first, "국어 수정").has_value(),
            "class rename failed"
            );
        const auto renamed = repository.get(*first);
        passed &= expect(
            renamed && renamed->name == "국어 수정",
            "class rename was not readable"
            );
        passed &= expect(
            database.execute(
                "INSERT INTO class_info (class_id, notes) VALUES (?, ?)",
                SqliteParameters{
                    SqliteValue{std::int64_t{*first}},
                    SqliteValue{std::string("cascade")}
                }
                ).has_value(),
            "class-info cascade fixture could not be inserted"
            );
        passed &= expect(
            repository.remove(*first).has_value(),
            "class deletion failed"
            );
        const auto deleted = repository.get(*first);
        passed &= expect(
            !deleted && deleted.error().code == ErrorCode::NotFound,
            "deleted class did not return a typed not-found error"
            );
        const auto dependentRows = database.query(
            "SELECT COUNT(*) FROM class_info"
            );
        passed &= expect(
            dependentRows && dependentRows->rows.size() == 1
                && dependentRows->rows.front().values.size() == 1
                && std::get_if<std::int64_t>(
                    &dependentRows->rows.front().values.front()) != nullptr
                && *std::get_if<std::int64_t>(
                    &dependentRows->rows.front().values.front()) == 0,
            "class deletion did not cascade dependent class information"
            );
    }

    const auto invalid = repository.get(0);
    passed &= expect(
        !invalid && invalid.error().code == ErrorCode::InvalidArgument,
        "invalid class id was not rejected"
        );
    passed &= expect(
        database.schemaVersion().value_or(-1)
            == DatabaseSchemaManager::LatestSchemaVersion,
        "repository test database did not remain on the latest schema"
        );

    return passed ? 0 : 1;
}
