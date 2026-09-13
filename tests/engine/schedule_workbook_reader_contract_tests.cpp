#include "classmngr/engine/schedule_workbook_layout.h"
#include "classmngr/engine/schedule_workbook_reader.h"

#include <cassert>
#include <filesystem>

namespace
{
class FakeScheduleWorkbookReader final
    : public classmngr::engine::ScheduleWorkbookReader
{
public:
    [[nodiscard]] classmngr::engine::Result<
        classmngr::engine::ScheduleImportWorkbook> read(
        const std::filesystem::path& file,
        classmngr::engine::ScheduleImportKind kind,
        const classmngr::engine::ScheduleWorkbookCancellation& isCancelled
        ) const override
    {
        if (isCancelled && isCancelled())
        {
            return std::unexpected(classmngr::engine::Error{
                classmngr::engine::ErrorCode::Cancelled,
                "cancelled",
                std::nullopt
            });
        }

        lastFile = file;
        lastKind = kind;
        classmngr::engine::ScheduleImportWorkbook workbook;
        workbook.sheets.push_back({
            "Current",
            true,
            {{
                "Alice",
                "A1",
                {{
                    "Hong",
                    "Hong",
                    {"413"},
                    {"#6D9EEB"},
                    "E5",
                    "Zeus",
                    {{"Monday", "4:00 PM", "4:55 PM"}},
                    {"B2"},
                    {}
                }},
                {},
                {}
            }},
            {}
        });
        return workbook;
    }

    mutable std::filesystem::path lastFile;
    mutable classmngr::engine::ScheduleImportKind lastKind =
        classmngr::engine::ScheduleImportKind::Normal;
};
}

int main()
{
    using namespace classmngr::engine;

    ScheduleWorkbookLayout layout;
    layout.styles.push_back({"#6D9EEB", "#000000", true, false});
    layout.sheets.push_back({
        "Current",
        true,
        {{1, 1, 0, "Alice", ""}},
        {{1, 1, 2, 5}}
    });
    assert(layout.sheets.front().mergedRanges.front().contains(2, 3));
    assert(!layout.sheets.front().mergedRanges.front().contains(3, 3));

    FakeScheduleWorkbookReader reader;
    const std::filesystem::path path = "schedule.xlsx";
    const auto result = reader.read(path, ScheduleImportKind::Intensive);
    assert(result.has_value());
    assert(reader.lastFile == path);
    assert(reader.lastKind == ScheduleImportKind::Intensive);
    assert(result->sheets.size() == 1);
    assert(result->sheets.front().users.size() == 1);

    const auto cancelled = reader.read(
        path,
        ScheduleImportKind::Normal,
        [] { return true; }
        );
    assert(!cancelled.has_value());
    assert(cancelled.error().code == ErrorCode::Cancelled);
    return 0;
}
