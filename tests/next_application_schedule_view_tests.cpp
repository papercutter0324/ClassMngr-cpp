#include "next/application/schedule_view_projection.h"

#include <QtTest/QtTest>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

namespace
{

ClassId classId(
    std::string value
    )
{
    return *ClassId::fromString(value);
}

TeacherId teacherId(
    std::string value
    )
{
    return *TeacherId::fromString(value);
}

ScheduleViewCell scheduleCell(
    const std::int32_t order,
    const std::int32_t slot,
    std::string start = "08:00",
    std::string end = "09:00",
    std::string meeting = "Class One",
    std::string room = "Room 1",
    std::optional<ClassId> classReference = classId("class-1"),
    std::optional<TeacherId> teacherReference = teacherId("teacher-1"),
    const ScheduleViewMode mode = ScheduleViewMode::Regular
    )
{
    ScheduleViewCell cell;
    cell.order = order;
    cell.slot = slot;
    cell.classId = std::move(classReference);
    cell.teacherId = std::move(teacherReference);
    cell.startText = std::move(start);
    cell.endText = std::move(end);
    cell.meetingText = std::move(meeting);
    cell.roomText = std::move(room);
    cell.mode = mode;
    return cell;
}

ScheduleViewRow scheduleRow(
    const std::int32_t order,
    const std::int32_t day,
    std::string dayLabel = "Monday",
    std::string rowLabel = "Morning",
    std::vector<ScheduleViewCell> cells = {
        scheduleCell(0, 0)
    }
    )
{
    ScheduleViewRow row;
    row.order = order;
    row.day = day;
    row.dayLabel = std::move(dayLabel);
    row.rowLabel = std::move(rowLabel);
    row.cells = std::move(cells);
    return row;
}

ScheduleViewProjectionInput validInput()
{
    ScheduleViewProjectionInput input;
    input.rows = {
        scheduleRow(
            10,
            1,
            "Monday",
            "Morning",
            {
                scheduleCell(
                    4,
                    2,
                    "08:00",
                    "09:00",
                    "Class One",
                    "Room 1"
                    ),
                scheduleCell(
                    5,
                    3,
                    "09:00",
                    "10:00",
                    "",
                    "",
                    std::nullopt,
                    std::nullopt
                    )
            }
            ),
        scheduleRow(
            20,
            2,
            "Tuesday",
            "Afternoon",
            {
                scheduleCell(
                    1,
                    7,
                    "14:00",
                    "15:00",
                    "Intensive Class",
                    "Room 2",
                    classId("class-2"),
                    std::nullopt,
                    ScheduleViewMode::Intensive
                    )
            }
            )
    };
    return input;
}

void verifyInvalid(
    const Domain::Result<ScheduleViewProjection>& result
    )
{
    QVERIFY(!result);
    QVERIFY(!result.hasValue());
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QVERIFY(!result.error().message.empty());
}

}

class NextApplicationScheduleViewTests final : public QObject
{
    Q_OBJECT

private slots:
    void validVisibleAnd96ScaleRowsRetainCompactData();
    void typedReferencesAndModesRemainDistinctAndOptional();
    void orderingAndSafeValueLookupsAreDeterministic();
    void emptyProjectionRowsAndCellsFollowExplicitPolicy();
    void duplicateNegativeAndInvalidValuesAreRejected();
    void blankAndOversizedFieldsAreRejected();
    void rowAndCellCapsAreRejected();
    void inclusiveCapsAndFieldBoundsConstructSuccessfully();
    void projectionsAreCopyableEqualAndIndependentlyReleasable();
    void contractHasNoMutablePointerOrRichRecordSurface();
};

void NextApplicationScheduleViewTests::validVisibleAnd96ScaleRowsRetainCompactData()
{
    ScheduleViewProjectionInput input;
    input.rows.reserve(96);
    for (std::size_t index = 0; index < 96; ++index)
    {
        std::vector<ScheduleViewCell> cells;
        cells.reserve(8);
        for (std::int32_t slot = 0; slot < 8; ++slot)
        {
            const bool emptyCell = slot == 7;
            cells.push_back(
                scheduleCell(
                    slot,
                    slot,
                    std::to_string(8 + slot) + ":00",
                    std::to_string(9 + slot) + ":00",
                    emptyCell
                        ? std::string{}
                        : "Class " + std::to_string(index + 1),
                    emptyCell ? std::string{} : "Room " + std::to_string(slot + 1),
                    emptyCell
                        ? std::optional<ClassId>{}
                        : std::optional<ClassId>{
                              classId(
                                  "class-" + std::to_string(index + 1)
                                  )
                          },
                    emptyCell
                        ? std::optional<TeacherId>{}
                        : std::optional<TeacherId>{
                              teacherId(
                                  "teacher-" + std::to_string((index % 3) + 1)
                                  )
                          },
                    index % 2 == 0
                        ? ScheduleViewMode::Regular
                        : ScheduleViewMode::Intensive
                    )
                );
        }

        input.rows.push_back(
            scheduleRow(
                static_cast<std::int32_t>(index * 2),
                static_cast<std::int32_t>(index % 5),
                "Day " + std::to_string((index % 5) + 1),
                "Row " + std::to_string(index + 1),
                std::move(cells)
                )
            );
    }

    const auto result = ScheduleViewProjection::create(std::move(input));

    QVERIFY(result);
    const auto& projection = result.value();
    QCOMPARE(projection.rowCount(), std::size_t(96));
    QCOMPARE(projection.cellCount(), std::size_t(768));
    QCOMPARE(projection.rows().front().order, std::int32_t(0));
    QCOMPARE(projection.rows().back().order, std::int32_t(190));
    QCOMPARE(projection.rows().at(41).day, std::int32_t(1));
    QCOMPARE(projection.rows().at(41).cells.size(), std::size_t(8));
    QCOMPARE(
        projection.rows().at(41).cells.at(0).meetingText,
        std::string("Class 42")
        );
    QVERIFY(projection.rows().at(41).cells.at(7).isEmpty());
    QCOMPARE(
        projection.rows().at(41).cells.at(1).mode,
        ScheduleViewMode::Intensive
        );
}

void NextApplicationScheduleViewTests::typedReferencesAndModesRemainDistinctAndOptional()
{
    static_assert(!std::is_same_v<ClassId, TeacherId>);
    static_assert(!std::is_convertible_v<ClassId, TeacherId>);
    static_assert(!std::is_convertible_v<TeacherId, ClassId>);
    static_assert(std::is_same_v<
        decltype(std::declval<ScheduleViewCell>().classId),
        std::optional<ClassId>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<ScheduleViewCell>().teacherId),
        std::optional<TeacherId>
        >);

    const auto result = ScheduleViewProjection::create(validInput());
    QVERIFY(result);
    const auto& cells = result.value().rows().front().cells;
    QVERIFY(cells.at(0).hasClass());
    QVERIFY(cells.at(0).hasTeacher());
    QCOMPARE(cells.at(0).classId->value(), std::string("class-1"));
    QCOMPARE(cells.at(0).teacherId->value(), std::string("teacher-1"));
    QVERIFY(!cells.at(1).classId.has_value());
    QVERIFY(!cells.at(1).teacherId.has_value());
    QCOMPARE(
        result.value().rows().at(1).cells.front().mode,
        ScheduleViewMode::Intensive
        );
}

void NextApplicationScheduleViewTests::orderingAndSafeValueLookupsAreDeterministic()
{
    auto input = validInput();
    input.rows.front().cells = {
        scheduleCell(42, 8),
        scheduleCell(7, 3)
    };

    const auto result = ScheduleViewProjection::create(std::move(input));
    QVERIFY(result);
    const auto& projection = result.value();
    QCOMPARE(projection.rows().front().order, std::int32_t(10));
    QCOMPARE(projection.rows().front().cells.front().order, std::int32_t(42));
    QCOMPARE(projection.rows().front().cells.at(1).order, std::int32_t(7));

    auto rowCopy = projection.findRow(20);
    QVERIFY(rowCopy.has_value());
    rowCopy->rowLabel = "Changed outside projection";
    QCOMPARE(projection.findRow(20)->rowLabel, std::string("Afternoon"));

    auto cellCopy = projection.findCell(10, 7);
    QVERIFY(cellCopy.has_value());
    cellCopy->meetingText = "Changed outside projection";
    QCOMPARE(projection.findCell(10, 7)->meetingText, std::string("Class One"));
    QVERIFY(projection.findCellBySlot(10, 3).has_value());
    QVERIFY(!projection.lookupRow(999).has_value());
    QVERIFY(!projection.lookupCell(999, 7).has_value());
    QVERIFY(!projection.lookupCellBySlot(10, 999).has_value());
}

void NextApplicationScheduleViewTests::emptyProjectionRowsAndCellsFollowExplicitPolicy()
{
    const auto emptyResult = ScheduleViewProjection::create({});
    QVERIFY(emptyResult);
    QVERIFY(emptyResult.value().empty());
    QCOMPARE(emptyResult.value().rowCount(), std::size_t(0));
    QCOMPARE(emptyResult.value().cellCount(), std::size_t(0));

    ScheduleViewProjectionInput input;
    input.rows.push_back(scheduleRow(1, 0, "Monday", "No entries", {}));
    input.rows.push_back(
        scheduleRow(
            2,
            0,
            "Monday",
            "Empty slot",
            {
                scheduleCell(
                    0,
                    0,
                    "08:00",
                    "09:00",
                    "",
                    "",
                    std::nullopt,
                    std::nullopt
                    )
            }
            )
        );

    const auto result = ScheduleViewProjection::create(std::move(input));
    QVERIFY(result);
    QVERIFY(result.value().rows().front().empty());
    QVERIFY(result.value().rows().at(1).cells.front().isEmpty());
    QVERIFY(ScheduleViewProjection::validate(validInput()));
}

void NextApplicationScheduleViewTests::duplicateNegativeAndInvalidValuesAreRejected()
{
    {
        auto input = validInput();
        input.rows.at(1).order = input.rows.front().order;
        verifyInvalid(ScheduleViewProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.rows.front().cells.at(1).order =
            input.rows.front().cells.front().order;
        verifyInvalid(ScheduleViewProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.rows.front().cells.at(1).slot =
            input.rows.front().cells.front().slot;
        verifyInvalid(ScheduleViewProjection::create(std::move(input)));
    }

    for (const auto mutate : {
             0,
             1,
             2,
             3
         })
    {
        auto input = validInput();
        if (mutate == 0)
        {
            input.rows.front().order = -1;
        }
        else if (mutate == 1)
        {
            input.rows.front().day = -1;
        }
        else if (mutate == 2)
        {
            input.rows.front().cells.front().order = -1;
        }
        else
        {
            input.rows.front().cells.front().slot = -1;
        }
        verifyInvalid(ScheduleViewProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.rows.front().cells.front().classId =
            classId(std::string(kScheduleViewMaxIdentifierLength + 1, 'c'));
        verifyInvalid(ScheduleViewProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.rows.front().cells.front().teacherId =
            teacherId(std::string(kScheduleViewMaxIdentifierLength + 1, 't'));
        verifyInvalid(ScheduleViewProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.rows.front().cells.front().classId = classId(" \t");
        verifyInvalid(ScheduleViewProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.rows.front().cells.front().teacherId = teacherId(" \t");
        verifyInvalid(ScheduleViewProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.rows.front().cells.front().mode =
            static_cast<ScheduleViewMode>(99);
        verifyInvalid(ScheduleViewProjection::create(std::move(input)));
    }
}

void NextApplicationScheduleViewTests::blankAndOversizedFieldsAreRejected()
{
    {
        auto input = validInput();
        input.rows.front().dayLabel = " \t";
        verifyInvalid(ScheduleViewProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.rows.front().rowLabel = "";
        verifyInvalid(ScheduleViewProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.rows.front().cells.front().startText = "\n";
        verifyInvalid(ScheduleViewProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.rows.front().cells.front().endText = "";
        verifyInvalid(ScheduleViewProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.rows.front().cells.front().meetingText = " \t";
        verifyInvalid(ScheduleViewProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.rows.front().dayLabel =
            std::string(kScheduleViewMaxDayLabelLength + 1, 'd');
        verifyInvalid(ScheduleViewProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.rows.front().rowLabel =
            std::string(kScheduleViewMaxRowLabelLength + 1, 'r');
        verifyInvalid(ScheduleViewProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.rows.front().cells.front().startText =
            std::string(kScheduleViewMaxStartTextLength + 1, 's');
        verifyInvalid(ScheduleViewProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.rows.front().cells.front().endText =
            std::string(kScheduleViewMaxEndTextLength + 1, 'e');
        verifyInvalid(ScheduleViewProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.rows.front().cells.front().meetingText =
            std::string(kScheduleViewMaxMeetingTextLength + 1, 'm');
        verifyInvalid(ScheduleViewProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.rows.front().cells.front().roomText =
            std::string(kScheduleViewMaxRoomTextLength + 1, 'r');
        verifyInvalid(ScheduleViewProjection::create(std::move(input)));
    }
}

void NextApplicationScheduleViewTests::rowAndCellCapsAreRejected()
{
    {
        ScheduleViewProjectionInput input;
        input.rows.reserve(kScheduleViewMaxRows + 1);
        for (std::size_t index = 0; index <= kScheduleViewMaxRows; ++index)
        {
            input.rows.push_back(
                scheduleRow(
                    static_cast<std::int32_t>(index),
                    static_cast<std::int32_t>(index % 5),
                    "Day",
                    "Row",
                    {}
                    )
                );
        }
        verifyInvalid(ScheduleViewProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.rows.front().cells.clear();
        input.rows.front().cells.reserve(kScheduleViewMaxCellsPerRow + 1);
        for (std::size_t index = 0;
             index <= kScheduleViewMaxCellsPerRow;
             ++index)
        {
            input.rows.front().cells.push_back(
                scheduleCell(
                    static_cast<std::int32_t>(index),
                    static_cast<std::int32_t>(index),
                    "08:00",
                    "09:00",
                    "",
                    "",
                    std::nullopt,
                    std::nullopt
                    )
                );
        }
        verifyInvalid(ScheduleViewProjection::create(std::move(input)));
    }

    {
        ScheduleViewProjectionInput input;
        const std::size_t rowsNeeded =
            kScheduleViewMaxCells / kScheduleViewMaxCellsPerRow + 1;
        input.rows.reserve(rowsNeeded);
        for (std::size_t rowIndex = 0; rowIndex < rowsNeeded; ++rowIndex)
        {
            std::vector<ScheduleViewCell> cells;
            cells.reserve(kScheduleViewMaxCellsPerRow);
            for (std::size_t cellIndex = 0;
                 cellIndex < kScheduleViewMaxCellsPerRow;
                 ++cellIndex)
            {
                cells.push_back(
                    scheduleCell(
                        static_cast<std::int32_t>(cellIndex),
                        static_cast<std::int32_t>(cellIndex),
                        "08:00",
                        "09:00",
                        "",
                        "",
                        std::nullopt,
                        std::nullopt
                        )
                    );
            }
            input.rows.push_back(
                scheduleRow(
                    static_cast<std::int32_t>(rowIndex),
                    static_cast<std::int32_t>(rowIndex % 5),
                    "Day",
                    "Row",
                    std::move(cells)
                    )
                );
        }
        verifyInvalid(ScheduleViewProjection::create(std::move(input)));
    }
}

void NextApplicationScheduleViewTests::inclusiveCapsAndFieldBoundsConstructSuccessfully()
{
    {
        ScheduleViewProjectionInput input;
        input.rows.reserve(kScheduleViewMaxRows);
        for (std::size_t index = 0; index < kScheduleViewMaxRows; ++index)
        {
            input.rows.push_back(
                scheduleRow(
                    static_cast<std::int32_t>(index),
                    static_cast<std::int32_t>(index % 5),
                    "Day",
                    "Row",
                    {}
                    )
                );
        }

        const auto result = ScheduleViewProjection::create(std::move(input));

        QVERIFY(result);
        QCOMPARE(result.value().rowCount(), kScheduleViewMaxRows);
        QCOMPARE(result.value().cellCount(), std::size_t(0));
    }

    {
        ScheduleViewProjectionInput input;
        std::vector<ScheduleViewCell> cells;
        cells.reserve(kScheduleViewMaxCellsPerRow);
        for (std::size_t index = 0;
             index < kScheduleViewMaxCellsPerRow;
             ++index)
        {
            cells.push_back(
                scheduleCell(
                    static_cast<std::int32_t>(index),
                    static_cast<std::int32_t>(index),
                    "08:00",
                    "09:00",
                    "",
                    "",
                    std::nullopt,
                    std::nullopt
                    )
                );
        }
        input.rows.push_back(
            scheduleRow(
                0,
                0,
                "Day",
                "Row",
                std::move(cells)
                )
            );

        const auto result = ScheduleViewProjection::create(std::move(input));

        QVERIFY(result);
        QCOMPARE(result.value().rowCount(), std::size_t(1));
        QCOMPARE(result.value().rows().front().cells.size(),
            kScheduleViewMaxCellsPerRow);
        QCOMPARE(result.value().cellCount(), kScheduleViewMaxCellsPerRow);
    }

    {
        ScheduleViewProjectionInput input;
        const std::size_t fullRows =
            kScheduleViewMaxCells / kScheduleViewMaxCellsPerRow;
        const std::size_t remainder =
            kScheduleViewMaxCells % kScheduleViewMaxCellsPerRow;
        input.rows.reserve(fullRows + (remainder == 0 ? 0 : 1));

        const auto appendRow =
            [&input](
                const std::size_t rowIndex,
                const std::size_t cellCount
                )
            {
                std::vector<ScheduleViewCell> cells;
                cells.reserve(cellCount);
                for (std::size_t cellIndex = 0;
                     cellIndex < cellCount;
                     ++cellIndex)
                {
                    cells.push_back(
                        scheduleCell(
                            static_cast<std::int32_t>(cellIndex),
                            static_cast<std::int32_t>(cellIndex),
                            "08:00",
                            "09:00",
                            "",
                            "",
                            std::nullopt,
                            std::nullopt
                            )
                        );
                }
                input.rows.push_back(
                    scheduleRow(
                        static_cast<std::int32_t>(rowIndex),
                        static_cast<std::int32_t>(rowIndex % 5),
                        "Day",
                        "Row",
                        std::move(cells)
                        )
                    );
            };

        for (std::size_t rowIndex = 0; rowIndex < fullRows; ++rowIndex)
        {
            appendRow(rowIndex, kScheduleViewMaxCellsPerRow);
        }
        if (remainder != 0)
        {
            appendRow(fullRows, remainder);
        }

        const auto result = ScheduleViewProjection::create(std::move(input));

        QVERIFY(result);
        QCOMPARE(
            result.value().rowCount(),
            fullRows + (remainder == 0 ? 0 : 1)
            );
        QCOMPARE(result.value().cellCount(), kScheduleViewMaxCells);
    }

    {
        auto input = validInput();
        auto& row = input.rows.front();
        row.dayLabel = std::string(kScheduleViewMaxDayLabelLength, 'd');
        row.rowLabel = std::string(kScheduleViewMaxRowLabelLength, 'r');
        auto& cell = row.cells.front();
        cell.classId =
            classId(std::string(kScheduleViewMaxIdentifierLength, 'c'));
        cell.teacherId =
            teacherId(std::string(kScheduleViewMaxIdentifierLength, 't'));
        cell.startText =
            std::string(kScheduleViewMaxStartTextLength, 's');
        cell.endText = std::string(kScheduleViewMaxEndTextLength, 'e');
        cell.meetingText =
            std::string(kScheduleViewMaxMeetingTextLength, 'm');
        cell.roomText = std::string(kScheduleViewMaxRoomTextLength, 'r');

        const auto result = ScheduleViewProjection::create(std::move(input));

        QVERIFY(result);
        QCOMPARE(
            result.value().rows().front().dayLabel.size(),
            kScheduleViewMaxDayLabelLength
            );
        QCOMPARE(
            result.value().rows().front().rowLabel.size(),
            kScheduleViewMaxRowLabelLength
            );
        QCOMPARE(
            result.value().rows().front().cells.front().classId->value().size(),
            kScheduleViewMaxIdentifierLength
            );
        QCOMPARE(
            result.value().rows().front().cells.front().teacherId->value().size(),
            kScheduleViewMaxIdentifierLength
            );
        QCOMPARE(
            result.value().rows().front().cells.front().startText.size(),
            kScheduleViewMaxStartTextLength
            );
        QCOMPARE(
            result.value().rows().front().cells.front().endText.size(),
            kScheduleViewMaxEndTextLength
            );
        QCOMPARE(
            result.value().rows().front().cells.front().meetingText.size(),
            kScheduleViewMaxMeetingTextLength
            );
        QCOMPARE(
            result.value().rows().front().cells.front().roomText.size(),
            kScheduleViewMaxRoomTextLength
            );
    }
}

void NextApplicationScheduleViewTests::projectionsAreCopyableEqualAndIndependentlyReleasable()
{
    static_assert(std::is_copy_constructible_v<ScheduleViewCell>);
    static_assert(std::is_copy_assignable_v<ScheduleViewCell>);
    static_assert(std::is_copy_constructible_v<ScheduleViewRow>);
    static_assert(std::is_copy_assignable_v<ScheduleViewRow>);
    static_assert(std::is_copy_constructible_v<ScheduleViewProjection>);
    static_assert(std::is_copy_assignable_v<ScheduleViewProjection>);

    const auto result = ScheduleViewProjection::create(validInput());
    QVERIFY(result);
    ScheduleViewProjection original = result.value();
    const ScheduleViewProjection copy = original;
    QVERIFY(copy == original);

    ScheduleViewProjection released = std::move(original);
    QVERIFY(released == copy);

    original = ScheduleViewProjection{};
    QVERIFY(original.empty());
    QVERIFY(released == copy);
}

void NextApplicationScheduleViewTests::contractHasNoMutablePointerOrRichRecordSurface()
{
    static_assert(std::is_same_v<
        decltype(std::declval<const ScheduleViewProjection>().rows()),
        const std::vector<ScheduleViewRow>&
        >);
    static_assert(!std::is_pointer_v<
        decltype(std::declval<const ScheduleViewProjection>().rows())
        >);
    static_assert(!std::is_pointer_v<
        decltype(std::declval<const ScheduleViewProjection>().findRow(0))
        >);
    static_assert(!std::is_pointer_v<
        decltype(std::declval<const ScheduleViewProjection>().findCell(0, 0))
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<const ScheduleViewProjection>().findRow(0)),
        std::optional<ScheduleViewRow>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<const ScheduleViewProjection>().findCell(0, 0)),
        std::optional<ScheduleViewCell>
        >);

    QVERIFY(true);
}

QTEST_APPLESS_MAIN(NextApplicationScheduleViewTests)

#include "next_application_schedule_view_tests.moc"
