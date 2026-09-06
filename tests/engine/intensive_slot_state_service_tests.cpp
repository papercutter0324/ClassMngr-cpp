#include "classmngr/engine/intensive_slot_state_service.h"
#include "classmngr/engine/open_database.h"

#include <iostream>
#include <string>
#include <string_view>

namespace
{
using classmngr::engine::ErrorCode;
using classmngr::engine::IntensiveSlotStateService;
using classmngr::engine::OpenDatabase;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineIntensiveSlotStateServiceTests: "
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
        std::cerr << "ClassMngrEngineIntensiveSlotStateServiceTests: "
                  << "OpenDatabase failed\n";
        return 1;
    }

    auto& database = **opened;
    IntensiveSlotStateService service(database);
    bool passed = true;

    passed &= expect(
        service.save("Tuesday", "16:00", "lunch").has_value()
            && service.save("Monday", "10:00", "essay").has_value()
            && service.save("Monday", "09:00", "custom").has_value()
            && service.save("Monday", "09:00", "custom-updated").has_value(),
        "initial, default, and upsert saves failed"
        );

    const auto listed = service.list();
    passed &= expect(
        listed
            && listed->size() == 2
            && listed->at(0).day == "Monday"
            && listed->at(0).startTime == "09:00"
            && listed->at(0).state == "custom-updated"
            && listed->at(1).day == "Tuesday"
            && listed->at(1).startTime == "16:00"
            && listed->at(1).state == "lunch",
        "listing did not preserve ordering or upserted values"
        );

    const auto afterDefaultDelete = service.save(
        "Tuesday",
        "16:00",
        "essay"
        );
    const auto statesAfterDefaultDelete = service.list();
    passed &= expect(
        afterDefaultDelete
            && statesAfterDefaultDelete
            && statesAfterDefaultDelete->size() == 1,
        "the default state did not delete the stored row"
        );

    const auto customDefaultInsert = service.save(
        "Monday",
        "09:00",
        "essay",
        "empty"
        );
    const auto statesAfterCustomDefaultInsert = service.list();
    const auto customDefaultDelete = service.save(
        "Monday",
        "09:00",
        "empty",
        "empty"
        );
    const auto statesAfterCustomDefaultDelete = service.list();
    passed &= expect(
        customDefaultInsert
            && statesAfterCustomDefaultInsert
            && statesAfterCustomDefaultInsert->size() == 1
            && customDefaultDelete
            && statesAfterCustomDefaultDelete
            && statesAfterCustomDefaultDelete->empty(),
        "custom default-state semantics were not preserved"
        );

    passed &= expect(
        service.save("토요일", "午後4時", "점심 🍜").has_value(),
        "UTF-8 state save failed"
        );
    const auto utf8States = service.list();
    passed &= expect(
        utf8States
            && utf8States->size() == 1
            && utf8States->front().day == "토요일"
            && utf8States->front().startTime == "午後4時"
            && utf8States->front().state == "점심 🍜",
        "UTF-8 state round-trip changed the stored values"
        );

    passed &= expect(
        database.execute("DROP TABLE intensive_slot_states").has_value(),
        "read-failure fixture could not drop the state table"
        );
    const auto readFailure = service.list();
    passed &= expect(
        !readFailure && readFailure.error().code == ErrorCode::Database,
        "missing state table did not return a typed database error"
        );

    const auto writeOpened = OpenDatabase::execute(":memory:");
    passed &= expect(
        writeOpened && *writeOpened != nullptr,
        "OpenDatabase failed for write-failure fixture"
        );
    if (writeOpened && *writeOpened != nullptr)
    {
        auto& writeDatabase = **writeOpened;
        IntensiveSlotStateService writeService(writeDatabase);
        passed &= expect(
            writeDatabase.execute(
                "CREATE TRIGGER reject_slot_insert "
                "BEFORE INSERT ON intensive_slot_states "
                "BEGIN "
                "SELECT RAISE(ABORT, 'injected insert failure'); "
                "END"
                ).has_value(),
            "insert-failure trigger could not be created"
            );

        const auto insertFailure = writeService.save(
            "Monday",
            "09:00",
            "lunch"
            );
        passed &= expect(
            !insertFailure
                && insertFailure.error().code == ErrorCode::Database,
            "injected insert failure did not return a typed database error"
            );

        passed &= expect(
            writeDatabase.execute(
                "DROP TRIGGER reject_slot_insert"
                ).has_value()
                && writeService.save("Monday", "09:00", "lunch").has_value()
                && writeDatabase.execute(
                    "CREATE TRIGGER reject_slot_delete "
                    "BEFORE DELETE ON intensive_slot_states "
                    "BEGIN "
                    "SELECT RAISE(ABORT, 'injected delete failure'); "
                    "END"
                    ).has_value(),
            "delete-failure fixture could not be prepared"
            );

        const auto deleteFailure = writeService.save(
            "Monday",
            "09:00",
            "essay"
            );
        passed &= expect(
            !deleteFailure
                && deleteFailure.error().code == ErrorCode::Database,
            "injected delete failure did not return a typed database error"
            );

        const auto retained = writeService.list();
        passed &= expect(
            retained
                && retained->size() == 1
                && retained->front().state == "lunch",
            "failed default-state deletion changed the stored row"
            );
    }

    return passed ? 0 : 1;
}
