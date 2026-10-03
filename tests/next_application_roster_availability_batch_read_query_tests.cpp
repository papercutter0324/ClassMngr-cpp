#include "next/application/roster_availability_accumulator.h"
#include "next/application/roster_availability_batch_read_query.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

bool asciiCaseInsensitiveEquals(
    const std::u16string_view left,
    const std::u16string_view right
    )
{
    if (left.size() != right.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index)
    {
        char16_t leftCharacter = left[index];
        char16_t rightCharacter = right[index];
        if (leftCharacter >= u'A' && leftCharacter <= u'Z')
        {
            leftCharacter = static_cast<char16_t>(leftCharacter - u'A' + u'a');
        }
        if (rightCharacter >= u'A' && rightCharacter <= u'Z')
        {
            rightCharacter = static_cast<char16_t>(rightCharacter - u'A' + u'a');
        }
        if (leftCharacter != rightCharacter)
        {
            return false;
        }
    }
    return true;
}

Domain::ClassId classId(const std::string& value)
{
    return *Domain::ClassId::fromString(value);
}

class FakeRosterAvailabilityPort final
    : public Application::RosterAvailabilityBatchReadPort
{
public:
    [[nodiscard]] Application::RosterAvailabilityBatchReadResult
    readRosterAvailability(
        const std::vector<Domain::ClassId>& classIds,
        const std::vector<std::u16string>& baseColumnNames
        ) const override
    {
        ++calls;
        receivedClassIds = classIds;
        receivedBaseColumnNames = baseColumnNames;
        if (error)
        {
            return Application::RosterAvailabilityBatchReadResult::failure(
                *error
                );
        }
        return Application::RosterAvailabilityBatchReadResult::success(
            records
            );
    }

    mutable int calls = 0;
    mutable std::vector<Domain::ClassId> receivedClassIds;
    mutable std::vector<std::u16string> receivedBaseColumnNames;
    std::vector<Application::RosterAvailabilityReadSnapshot> records;
    std::optional<Domain::OperationError> error;
};

}

class NextApplicationRosterAvailabilityBatchReadQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void projectionNormalizesAliasesAndFiltersIgnoredHeaders();
    void accumulatorCountsOnlyRetainedCellsInsideModeledRows();
    void batchQueryValidatesRequestsAndPreservesFailure();
    void batchQueryValidatesResponseShapeAndAvailabilityRange();
};

void NextApplicationRosterAvailabilityBatchReadQueryTests::
projectionNormalizesAliasesAndFiltersIgnoredHeaders()
{
    const std::vector<std::u16string> storedColumns{
        u" Note ",
        u"English",
        u"Autumn",
        u"Fall",
        u"NOTE",
        u" \t ",
        u"Second",
        u"English"
    };
    const std::vector<std::u16string> baseColumns{
        u"English",
        u"Korean",
        u"Winter",
        u"Fall"
    };

    const auto projection = Application::RosterColumnProjection::create(
        storedColumns,
        baseColumns,
        asciiCaseInsensitiveEquals
        );

    QCOMPARE(projection.columns().size(), std::size_t(6));
    QCOMPARE(projection.columns()[0].name, std::u16string(u"English"));
    QVERIFY(projection.columns()[0].sourceColumnIndex.has_value());
    QCOMPARE(*projection.columns()[0].sourceColumnIndex, std::size_t(1));
    QCOMPARE(projection.columns()[1].name, std::u16string(u"Korean"));
    QVERIFY(!projection.columns()[1].sourceColumnIndex.has_value());
    QCOMPARE(projection.columns()[2].name, std::u16string(u"Winter"));
    QVERIFY(!projection.columns()[2].sourceColumnIndex.has_value());
    QCOMPARE(projection.columns()[3].name, std::u16string(u"Fall"));
    // Autumn aliases Fall; RosterModel maps the first matching stored header.
    QVERIFY(projection.columns()[3].sourceColumnIndex.has_value());
    QCOMPARE(*projection.columns()[3].sourceColumnIndex, std::size_t(2));
    QCOMPARE(projection.columns()[4].name, std::u16string(u"Note"));
    QCOMPARE(*projection.columns()[4].sourceColumnIndex, std::size_t(0));
    QCOMPARE(projection.columns()[5].name, std::u16string(u"Second"));
    QCOMPARE(*projection.columns()[5].sourceColumnIndex, std::size_t(6));
}

void NextApplicationRosterAvailabilityBatchReadQueryTests::
accumulatorCountsOnlyRetainedCellsInsideModeledRows()
{
    const std::vector<std::u16string> storedColumns{
        u"English",
        u"Autumn",
        u"Memo",
        u" English ",
        u" \t "
    };
    const std::vector<std::u16string> baseColumns{
        u"English",
        u"Korean",
        u"Fall"
    };
    Application::RosterAvailabilityAccumulator accumulator(
        storedColumns,
        baseColumns,
        asciiCaseInsensitiveEquals
        );

    accumulator.observeCell(0, 3, u"duplicate English header");
    accumulator.observeCell(0, 4, u"blank header");
    accumulator.observeCell(0, 1, u" \u2003\t ");
    accumulator.observeCell(1, 0, u" \n\r ");
    accumulator.observeCell(1, 2, u" ");
    accumulator.observeCell(25, 2, u"beyond the model");
    accumulator.observeCell(-1, 2, u"invalid source row");
    QCOMPARE(accumulator.firstEmptyRow(), 0);

    accumulator.observeCell(0, 2, u"Custom column occupancy");
    accumulator.observeCell(1, 1, u"Alias maps to the Fall base column");
    QCOMPARE(accumulator.firstEmptyRow(), 2);

    for (int row = 2;
         row < static_cast<int>(Application::RosterModeledRowCount);
         ++row)
    {
        accumulator.observeCell(row, 0, u"student");
    }
    accumulator.observeCell(25, 0, u"ignored overflow row");
    QCOMPARE(accumulator.firstEmptyRow(), -1);
}

void NextApplicationRosterAvailabilityBatchReadQueryTests::
batchQueryValidatesRequestsAndPreservesFailure()
{
    FakeRosterAvailabilityPort port;
    const Application::RosterAvailabilityBatchReadQuery query(port);
    const std::vector<std::u16string> baseColumns{u"English", u"Korean"};

    const auto empty = query.execute({}, baseColumns);
    QVERIFY(empty);
    QVERIFY(empty.value().empty());
    QCOMPARE(port.calls, 0);

    const auto nonCanonical = query.execute({classId("01")}, baseColumns);
    QVERIFY(!nonCanonical);
    QCOMPARE(nonCanonical.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(port.calls, 0);

    const auto duplicate = query.execute(
        {classId("7"), classId("7")},
        baseColumns
        );
    QVERIFY(!duplicate);
    QCOMPARE(duplicate.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(port.calls, 0);

    port.error = Domain::OperationError{
        .code = Domain::ErrorCode::Technical,
        .message = "batch failed",
        .recoverable = false
    };
    const auto failed = query.execute({classId("7")}, baseColumns);
    QVERIFY(!failed);
    QCOMPARE(failed.error().message, std::string("batch failed"));
    QCOMPARE(port.calls, 1);
}

void NextApplicationRosterAvailabilityBatchReadQueryTests::
batchQueryValidatesResponseShapeAndAvailabilityRange()
{
    FakeRosterAvailabilityPort port;
    const Application::RosterAvailabilityBatchReadQuery query(port);
    const std::vector<Domain::ClassId> requested{
        classId("7"),
        classId("8")
    };

    port.records = {{.classId = classId("8"), .firstEmptyRow = 1}};
    auto result = query.execute(requested, {});
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);

    port.records = {
        {.classId = classId("8"), .firstEmptyRow = 1},
        {.classId = classId("7"), .firstEmptyRow = -1}
    };
    result = query.execute(requested, {});
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);

    port.records = {
        {.classId = classId("7"), .firstEmptyRow = 25},
        {.classId = classId("8"), .firstEmptyRow = -1}
    };
    result = query.execute(requested, {});
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);

    port.records = {
        {.classId = classId("7"), .firstEmptyRow = 0},
        {.classId = classId("8"), .firstEmptyRow = -1}
    };
    result = query.execute(requested, {u"English"});
    QVERIFY(result);
    QCOMPARE(result.value()[0].firstEmptyRow, 0);
    QCOMPARE(result.value()[1].firstEmptyRow, -1);
    QCOMPARE(port.receivedBaseColumnNames, (std::vector<std::u16string>{u"English"}));
}

QTEST_APPLESS_MAIN(NextApplicationRosterAvailabilityBatchReadQueryTests)

#include "next_application_roster_availability_batch_read_query_tests.moc"
