#include "next/application/roster_availability_accumulator.h"
#include "next/application/roster_availability_batch_read_query.h"

#include <QtTest/QtTest>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

using namespace ClassMngr::Next;

static_assert(
    Application::RosterSourceColumnIndex{}
        == Application::RosterSourceColumnIndex{0}
    );
static_assert(std::is_constructible_v<
    Application::RosterSourceColumnIndex,
    std::size_t
    >);
static_assert(!std::is_convertible_v<
    std::size_t,
    Application::RosterSourceColumnIndex
    >);
static_assert(!std::is_convertible_v<
    Application::RosterSourceColumnIndex,
    std::size_t
    >);
static_assert(
    Application::RosterAvailabilityAccumulatorRowIndex{}
        == Application::RosterAvailabilityAccumulatorRowIndex{0}
    );
static_assert(
    Application::RosterAvailabilityAccumulatorRowIndex{-1}.rawValue() == -1
    );
static_assert(std::is_constructible_v<
    Application::RosterAvailabilityAccumulatorRowIndex,
    int
    >);
static_assert(!std::is_convertible_v<
    int,
    Application::RosterAvailabilityAccumulatorRowIndex
    >);
static_assert(!std::is_convertible_v<
    Application::RosterAvailabilityAccumulatorRowIndex,
    int
    >);
static_assert(
    Application::RosterAvailabilityAccumulatorSourceColumnIndex{}
        == Application::RosterAvailabilityAccumulatorSourceColumnIndex{0}
    );
static_assert(
    Application::RosterAvailabilityAccumulatorSourceColumnIndex{-1}.rawValue()
        == -1
    );
static_assert(std::is_constructible_v<
    Application::RosterAvailabilityAccumulatorSourceColumnIndex,
    int
    >);
static_assert(!std::is_convertible_v<
    int,
    Application::RosterAvailabilityAccumulatorSourceColumnIndex
    >);
static_assert(!std::is_convertible_v<
    Application::RosterAvailabilityAccumulatorSourceColumnIndex,
    int
    >);

namespace
{

using FirstEmptyRow = Application::RosterAvailabilityFirstEmptyRow;

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
    const Application::RosterSourceColumnIndex defaultSourceColumnIndex;
    QCOMPARE(defaultSourceColumnIndex.rawValue(), std::size_t(0));

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
    QCOMPARE(
        projection.columns()[0].sourceColumnIndex->rawValue(),
        std::size_t(1)
        );
    QCOMPARE(projection.columns()[1].name, std::u16string(u"Korean"));
    QVERIFY(!projection.columns()[1].sourceColumnIndex.has_value());
    QCOMPARE(projection.columns()[2].name, std::u16string(u"Winter"));
    QVERIFY(!projection.columns()[2].sourceColumnIndex.has_value());
    QCOMPARE(projection.columns()[3].name, std::u16string(u"Fall"));
    // Autumn aliases Fall; RosterModel maps the first matching stored header.
    QVERIFY(projection.columns()[3].sourceColumnIndex.has_value());
    QCOMPARE(
        projection.columns()[3].sourceColumnIndex->rawValue(),
        std::size_t(2)
        );
    QCOMPARE(projection.columns()[4].name, std::u16string(u"Note"));
    QCOMPARE(
        projection.columns()[4].sourceColumnIndex->rawValue(),
        std::size_t(0)
        );
    QCOMPARE(projection.columns()[5].name, std::u16string(u"Second"));
    QCOMPARE(
        projection.columns()[5].sourceColumnIndex->rawValue(),
        std::size_t(6)
        );
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

    using AccumulatorRowIndex =
        Application::RosterAvailabilityAccumulatorRowIndex;
    using AccumulatorSourceColumnIndex =
        Application::RosterAvailabilityAccumulatorSourceColumnIndex;

    accumulator.observeCell(
        AccumulatorRowIndex{0},
        AccumulatorSourceColumnIndex{3},
        u"duplicate English header"
        );
    accumulator.observeCell(
        AccumulatorRowIndex{0},
        AccumulatorSourceColumnIndex{4},
        u"blank header"
        );
    accumulator.observeCell(
        AccumulatorRowIndex{0},
        AccumulatorSourceColumnIndex{1},
        u" \u2003\t "
        );
    accumulator.observeCell(
        AccumulatorRowIndex{1},
        AccumulatorSourceColumnIndex{0},
        u" \n\r "
        );
    accumulator.observeCell(
        AccumulatorRowIndex{1},
        AccumulatorSourceColumnIndex{2},
        u" "
        );
    accumulator.observeCell(
        AccumulatorRowIndex{25},
        AccumulatorSourceColumnIndex{2},
        u"beyond the model"
        );
    accumulator.observeCell(
        AccumulatorRowIndex{-1},
        AccumulatorSourceColumnIndex{2},
        u"invalid source row"
        );
    accumulator.observeCell(
        AccumulatorRowIndex{0},
        AccumulatorSourceColumnIndex{-1},
        u"invalid negative source column"
        );
    QCOMPARE(accumulator.firstEmptyRow().rawValue(), 0);
    accumulator.observeCell(
        AccumulatorRowIndex{0},
        AccumulatorSourceColumnIndex{99},
        u"invalid out-of-range source column"
        );
    QCOMPARE(accumulator.firstEmptyRow().rawValue(), 0);

    accumulator.observeCell(
        AccumulatorRowIndex{0},
        AccumulatorSourceColumnIndex{2},
        u"Custom column occupancy"
        );
    accumulator.observeCell(
        AccumulatorRowIndex{1},
        AccumulatorSourceColumnIndex{1},
        u"Alias maps to the Fall base column"
        );
    QCOMPARE(accumulator.firstEmptyRow().rawValue(), 2);

    for (int row = 2;
         row < static_cast<int>(Application::RosterModeledRowCount);
         ++row)
    {
        accumulator.observeCell(
            AccumulatorRowIndex{row},
            AccumulatorSourceColumnIndex{0},
            u"student"
            );
    }
    accumulator.observeCell(
        AccumulatorRowIndex{25},
        AccumulatorSourceColumnIndex{0},
        u"ignored overflow row"
        );
    QVERIFY(accumulator.firstEmptyRow().isFull());
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
    QCOMPARE(
        nonCanonical.error().message,
        std::string("Class IDs must be canonical positive integers.")
        );
    QVERIFY(!nonCanonical.error().recoverable);
    QCOMPARE(port.calls, 0);

    const auto malformed = query.execute({classId("42x")}, baseColumns);
    QVERIFY(!malformed);
    QCOMPARE(malformed.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(
        malformed.error().message,
        std::string("Class IDs must be canonical positive integers.")
        );
    QVERIFY(!malformed.error().recoverable);
    QCOMPARE(port.calls, 0);

    const auto duplicate = query.execute(
        {classId("7"), classId("7")},
        baseColumns
        );
    QVERIFY(!duplicate);
    QCOMPARE(duplicate.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(
        duplicate.error().message,
        std::string("Class IDs must be unique.")
        );
    QVERIFY(!duplicate.error().recoverable);
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
    const Application::RosterAvailabilityReadSnapshot defaultSnapshot{
        .classId = classId("7")
    };
    QCOMPARE(defaultSnapshot.firstEmptyRowResult.value().rawValue(), 0);

    port.records = {{
        .classId = classId("8"),
        .firstEmptyRowResult =
            Domain::Result<FirstEmptyRow>::success(FirstEmptyRow(1))
    }};
    auto result = query.execute(requested, {});
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);

    port.records = {
        {
            .classId = classId("8"),
            .firstEmptyRowResult =
                Domain::Result<FirstEmptyRow>::success(FirstEmptyRow(1))
        },
        {
            .classId = classId("7"),
            .firstEmptyRowResult =
                Domain::Result<FirstEmptyRow>::success(FirstEmptyRow(-1))
        }
    };
    result = query.execute(requested, {});
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);

    port.records = {
        {
            .classId = classId("7"),
            .firstEmptyRowResult =
                Domain::Result<FirstEmptyRow>::success(FirstEmptyRow(25))
        },
        {
            .classId = classId("8"),
            .firstEmptyRowResult =
                Domain::Result<FirstEmptyRow>::success(FirstEmptyRow(-1))
        }
    };
    result = query.execute(requested, {});
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);

    port.records = {
        {
            .classId = classId("7"),
            .firstEmptyRowResult =
                Domain::Result<FirstEmptyRow>::success(FirstEmptyRow(-2))
        },
        {
            .classId = classId("8"),
            .firstEmptyRowResult =
                Domain::Result<FirstEmptyRow>::success(FirstEmptyRow(24))
        }
    };
    result = query.execute(requested, {});
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);

    port.records = {
        {
            .classId = classId("7"),
            .firstEmptyRowResult =
                Domain::Result<FirstEmptyRow>::success(FirstEmptyRow(0))
        },
        {
            .classId = classId("8"),
            .firstEmptyRowResult =
                Domain::Result<FirstEmptyRow>::success(FirstEmptyRow(-1))
        }
    };
    result = query.execute(requested, {u"English"});
    QVERIFY(result);
    QCOMPARE(result.value()[0].firstEmptyRowResult.value().rawValue(), 0);
    QVERIFY(result.value()[1].firstEmptyRowResult.value().isFull());
    QCOMPARE(port.receivedBaseColumnNames, (std::vector<std::u16string>{u"English"}));

    const std::vector<Domain::ClassId> mixedRequested{
        classId("7"),
        classId("8"),
        classId("9")
    };
    const Domain::OperationError missingParent{
        .code = Domain::ErrorCode::NotFound,
        .message = "The requested class does not exist.",
        .recoverable = true
    };
    port.records = {
        {
            .classId = classId("7"),
            .firstEmptyRowResult =
                Domain::Result<FirstEmptyRow>::success(FirstEmptyRow(24))
        },
        {
            .classId = classId("8"),
            .firstEmptyRowResult =
                Domain::Result<FirstEmptyRow>::failure(missingParent)
        },
        {
            .classId = classId("9"),
            .firstEmptyRowResult =
                Domain::Result<FirstEmptyRow>::success(FirstEmptyRow(-1))
        }
    };
    result = query.execute(mixedRequested, {});
    QVERIFY(result);
    QCOMPARE(result.value().size(), std::size_t(3));
    QCOMPARE(result.value()[0].classId, mixedRequested[0]);
    QVERIFY(result.value()[0].firstEmptyRowResult);
    QCOMPARE(result.value()[0].firstEmptyRowResult.value().rawValue(), 24);
    QCOMPARE(result.value()[1].classId, mixedRequested[1]);
    QVERIFY(!result.value()[1].firstEmptyRowResult);
    QCOMPARE(
        result.value()[1].firstEmptyRowResult.error(),
        missingParent
        );
    QCOMPARE(result.value()[2].classId, mixedRequested[2]);
    QVERIFY(result.value()[2].firstEmptyRowResult);
    QVERIFY(result.value()[2].firstEmptyRowResult.value().isFull());
}

QTEST_APPLESS_MAIN(NextApplicationRosterAvailabilityBatchReadQueryTests)

#include "next_application_roster_availability_batch_read_query_tests.moc"
