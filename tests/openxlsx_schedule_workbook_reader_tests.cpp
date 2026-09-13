#include "schedule_workbook_openxlsx_reader.h"

#include <OpenXLSX.hpp>

#include "classmngr/engine/schedule_workbook_reader.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
std::string pathToUtf8(const std::filesystem::path& path)
{
    const auto value = path.u8string();
    return std::string(
        reinterpret_cast<const char*>(value.data()),
        value.size()
        );
}

std::filesystem::path unicodeFixturePath(
    const std::filesystem::path& directory
    )
{
    return directory / std::filesystem::path(std::u8string{u8"일정.xlsx"});
}

void writeScheduleFixture(const std::filesystem::path& path)
{
    OpenXLSX::XLDocument document;
    document.create(pathToUtf8(path), OpenXLSX::XLForceOverwrite);

    auto workbook = document.workbook();
    auto regular = workbook.worksheet("Sheet1");
    regular.setName("Regular");

    workbook.addWorksheet("Hidden");
    workbook.addWorksheet("VeryHidden");
    workbook.worksheet("Hidden").setVisibility(OpenXLSX::XLSheetState::Hidden);
    workbook.worksheet("VeryHidden").setVisibility(OpenXLSX::XLSheetState::VeryHidden);

    auto& styles = document.styles();
    auto& fills = styles.fills();
    const OpenXLSX::XLStyleIndex fillIndex = fills.create();
    fills[fillIndex].setPatternType(OpenXLSX::XLPatternSolid);
    fills[fillIndex].setColor(OpenXLSX::XLColor("FF6D9EEB"));

    auto& fonts = styles.fonts();
    const OpenXLSX::XLStyleIndex fontIndex = fonts.create();
    fonts[fontIndex].setBold();
    fonts[fontIndex].setFontColor(OpenXLSX::XLColor("FF000000"));

    auto& formats = styles.cellFormats();
    const OpenXLSX::XLStyleIndex formatIndex = formats.create();
    formats[formatIndex].setFontIndex(fontIndex);
    formats[formatIndex].setFillIndex(fillIndex);
    formats[formatIndex].setApplyFont();
    formats[formatIndex].setApplyFill();

    regular.cell("A1").value() = "Alice";
    regular.cell("B1").value() = "MON";
    regular.cell("C1").value() = "TUE";
    regular.cell("D1").value() = "WED";
    regular.cell("E1").value() = "THU";
    regular.cell("F1").value() = "FRI";
    regular.cell("A2").value() = "4:00~4:55";
    regular.cell("A3").value() = "5:00~5:55";
    regular.cell("A4").value() = "12:00~12:50";
    regular.cell("A5").value() = "1:00~1:50";
    regular.cell("B2").value() = "\xED\x99\x8D\xEC\x8A\xB9\xED\x98\x84 (413)\nE5-Zeus";
    regular.cell("B2").setCellFormat(formatIndex);
    regular.mergeCells("B2:B3");
    regular.cell("B4").value() = "Lunch";
    regular.cell("B5").value() = "Essay";

    document.save();
}

void writeMalformedFixture(const std::filesystem::path& path)
{
    OpenXLSX::XLDocument document;
    document.create(pathToUtf8(path), OpenXLSX::XLForceOverwrite);
    document.workbook().worksheet("Sheet1").cell("A1").value() = "Not a schedule";
    document.save();
}

const classmngr::engine::ScheduleImportSheet& regularSheet(
    const classmngr::engine::ScheduleImportWorkbook& workbook
    )
{
    assert(workbook.sheets.size() == 3);
    assert(workbook.sheets[0].name == "Regular");
    return workbook.sheets[0];
}
}

int main()
{
    using classmngr::engine::ErrorCode;
    using classmngr::engine::ScheduleImportKind;

    const auto uniqueSuffix = std::chrono::steady_clock::now()
        .time_since_epoch().count();
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path()
        / ("ClassMngr-openxlsx-reader-" + std::to_string(uniqueSuffix));
    std::filesystem::create_directories(directory);

    const std::filesystem::path fixture = unicodeFixturePath(directory);
    writeScheduleFixture(fixture);
    const auto originalSize = std::filesystem::file_size(fixture);
    const auto originalWriteTime = std::filesystem::last_write_time(fixture);

    classmngr::winui::ScheduleWorkbookOpenXLSXReader reader;
    const auto normal = reader.read(fixture, ScheduleImportKind::Normal);
    assert(normal.has_value());
    const auto& normalRegular = regularSheet(*normal);
    assert(normalRegular.visible);
    assert(normalRegular.users.size() == 1);
    assert(normalRegular.users.front().name == "Alice");
    assert(normalRegular.users.front().headerCell == "A1");
    assert(normalRegular.users.front().classes.size() == 1);
    const auto& normalClass = normalRegular.users.front().classes.front();
    assert(normalClass.teacherKr == "\xED\x99\x8D\xEC\x8A\xB9\xED\x98\x84");
    assert(normalClass.rooms.size() == 1 && normalClass.rooms.front() == "413");
    assert(normalClass.importedColors.size() == 1
           && normalClass.importedColors.front() == "#6D9EEB");
    assert(normalClass.times.size() == 1);
    assert(normalClass.times.front().endTime == "5:55 PM");
    assert(!normal->sheets[1].visible);
    assert(!normal->sheets[2].visible);

    const auto intensive = reader.read(fixture, ScheduleImportKind::Intensive);
    assert(intensive.has_value());
    const auto& intensiveUser = regularSheet(*intensive).users.front();
    assert(intensiveUser.intensiveSlotStates.size() == 75);
    bool foundLunch = false;
    bool foundClassSlot = false;
    for (const auto& state : intensiveUser.intensiveSlotStates)
    {
        foundLunch = foundLunch
            || (state.startTime == "12:00" && state.state == "lunch");
        foundClassSlot = foundClassSlot
            || (state.startTime == "13:00" && state.state == "essay");
    }
    assert(foundLunch);
    assert(foundClassSlot);

    assert(std::filesystem::file_size(fixture) == originalSize);
    assert(std::filesystem::last_write_time(fixture) == originalWriteTime);

    const auto cancelled = reader.read(
        fixture,
        ScheduleImportKind::Normal,
        [] { return true; }
        );
    assert(!cancelled.has_value());
    assert(cancelled.error().code == ErrorCode::Cancelled);

    const std::filesystem::path malformed = directory / "malformed.xlsx";
    writeMalformedFixture(malformed);
    const auto malformedResult = reader.read(malformed, ScheduleImportKind::Normal);
    assert(!malformedResult.has_value());
    assert(malformedResult.error().code == ErrorCode::NotFound);

    const std::filesystem::path nonXlsx = directory / "schedule.txt";
    {
        std::ofstream output(nonXlsx, std::ios::binary);
        output << "not an XLSX workbook";
    }
    const auto unsupported = reader.read(nonXlsx, ScheduleImportKind::Normal);
    assert(!unsupported.has_value());
    assert(unsupported.error().code == ErrorCode::Unsupported);

    const std::filesystem::path corrupt = directory / "corrupt.xlsx";
    {
        std::ofstream output(corrupt, std::ios::binary);
        output << "not a ZIP package";
    }
    const auto corruptResult = reader.read(corrupt, ScheduleImportKind::Normal);
    assert(!corruptResult.has_value());
    assert(corruptResult.error().code == ErrorCode::InvalidFormat);

    const std::filesystem::path oversized = directory / "oversized.xlsx";
    {
        std::ofstream output(oversized, std::ios::binary);
    }
    std::error_code resizeError;
    std::filesystem::resize_file(
        oversized,
        64u * 1024u * 1024u + 1u,
        resizeError
        );
    assert(!resizeError);
    const auto oversizedResult = reader.read(
        oversized,
        ScheduleImportKind::Normal
        );
    assert(!oversizedResult.has_value());
    assert(oversizedResult.error().code == ErrorCode::InvalidFormat);
    assert(oversizedResult.error().message.find("size limit")
        != std::string::npos);

    std::error_code cleanupError;
    std::filesystem::remove_all(directory, cleanupError);
    assert(!cleanupError);
    return 0;
}
