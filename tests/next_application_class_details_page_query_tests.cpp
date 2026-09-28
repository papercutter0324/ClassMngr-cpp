#include "next/application/class_details_page_query.h"

#include <QtTest/QtTest>

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

Domain::ClassId classId(std::string value)
{
    return *Domain::ClassId::fromString(value);
}

ClassDetailsPageReadSnapshot snapshot(
    const Domain::ClassId& id,
    Domain::Result<ClassDetailsPageFields> classFields,
    Domain::Result<std::string> teacherName,
    Domain::Result<int> studentCount
    )
{
    return {
        id,
        std::move(classFields),
        std::move(teacherName),
        std::move(studentCount)
    };
}

class FakeReadPort final : public ClassDetailsPageReadPort
{
public:
    ClassDetailsPageReadResult result = ClassDetailsPageReadResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "Unconfigured fake read port",
        .recoverable = false
    });
    std::vector<Domain::ClassId> requests;

    [[nodiscard]] ClassDetailsPageReadResult readClassDetailsPage(
        const Domain::ClassId& id
        ) override
    {
        requests.push_back(id);
        return result;
    }
};

}

class NextApplicationClassDetailsPageQueryTests final : public QObject
{
    Q_OBJECT

private slots:
    void contractUsesOwningNonQtTextAndRawScheduleRows();
    void invalidTypedIdsAreRejectedBeforePortCall();
    void returnsSuccessfulSnapshotAndIndependentSourceOutcomes();
    void propagatesOverallPortError();
    void rejectsSnapshotClassIdMismatch();
};

void NextApplicationClassDetailsPageQueryTests::
contractUsesOwningNonQtTextAndRawScheduleRows()
{
    static_assert(std::is_same_v<
        decltype(ClassDetailsPageFields::classGrade),
        std::string
        >);
    static_assert(std::is_same_v<
        decltype(ClassDetailsPageScheduleRow::day),
        std::string
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<ClassDetailsPageReadPort&>()
            .readClassDetailsPage(std::declval<const Domain::ClassId&>())),
        ClassDetailsPageReadResult
        >);

    const ClassDetailsPageScheduleRow malformed{
        "Legacy-Funday", "not a time", "also invalid"
    };
    QCOMPARE(malformed.day, std::string("Legacy-Funday"));
    QCOMPARE(malformed.startTime, std::string("not a time"));
    QCOMPARE(malformed.endTime, std::string("also invalid"));
}

void NextApplicationClassDetailsPageQueryTests::
invalidTypedIdsAreRejectedBeforePortCall()
{
    FakeReadPort readPort;
    const ClassDetailsPageQuery query(readPort);
    const std::vector<Domain::ClassId> invalidIds{
        classId(" \t "),
        classId(std::string(kClassDetailsPageMaxIdentifierLength + 1, 'x'))
    };

    for (const auto& id : invalidIds)
    {
        const auto result = query.execute(id);
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
        QVERIFY(!result.error().recoverable);
    }
    QVERIFY(readPort.requests.empty());
}

void NextApplicationClassDetailsPageQueryTests::
returnsSuccessfulSnapshotAndIndependentSourceOutcomes()
{
    FakeReadPort readPort;
    const Domain::ClassId id = classId("class-42");
    ClassDetailsPageFields fields;
    fields.classGrade = "E4";
    fields.classLevel = "Theseus";
    fields.classColor = "#112233";
    fields.fontColor = "#445566";
    fields.regularSchedule = {
        {"Friday", "legacy start text", "legacy end text"},
        {"Monday", "9:00 AM", "9:55 AM"}
    };
    fields.intensiveSchedule = {
        {"bad weekday", "??", "n/a"}
    };
    const auto expectedFields = fields;
    readPort.result = ClassDetailsPageReadResult::success(snapshot(
        id,
        Domain::Result<ClassDetailsPageFields>::success(std::move(fields)),
        Domain::Result<std::string>::success("Teacher display"),
        Domain::Result<int>::success(18)
        ));

    const ClassDetailsPageQuery query(readPort);
    const auto result = query.execute(id);

    QVERIFY(result);
    QVERIFY(result.value().classId == id);
    QVERIFY(result.value().classFields);
    QVERIFY(result.value().teacherDisplayName);
    QVERIFY(result.value().studentCount);
    QVERIFY(result.value().classFields.value() == expectedFields);
    QCOMPARE(result.value().teacherDisplayName.value(),
             std::string("Teacher display"));
    QCOMPARE(result.value().studentCount.value(), 18);
    QVERIFY(readPort.requests == std::vector<Domain::ClassId>{id});

    const Domain::OperationError classError{
        .code = Domain::ErrorCode::Technical,
        .message = "class-info source failed",
        .recoverable = true
    };
    readPort.result = ClassDetailsPageReadResult::success(snapshot(
        id,
        Domain::Result<ClassDetailsPageFields>::failure(classError),
        Domain::Result<std::string>::success("Teacher still available"),
        Domain::Result<int>::success(7)
        ));
    const auto partial = query.execute(id);

    QVERIFY(partial);
    QVERIFY(!partial.value().classFields);
    QVERIFY(partial.value().classFields.error() == classError);
    QVERIFY(partial.value().teacherDisplayName);
    QCOMPARE(partial.value().teacherDisplayName.value(),
             std::string("Teacher still available"));
    QVERIFY(partial.value().studentCount);
    QCOMPARE(partial.value().studentCount.value(), 7);

    readPort.result = ClassDetailsPageReadResult::success(snapshot(
        id,
        Domain::Result<ClassDetailsPageFields>::success(expectedFields),
        Domain::Result<std::string>::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "teacher source failed",
            .recoverable = false
        }),
        Domain::Result<int>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "roster source failed",
            .recoverable = true
        })
        ));
    const auto sourceFailures = query.execute(id);

    QVERIFY(sourceFailures);
    QVERIFY(sourceFailures.value().classFields);
    QVERIFY(!sourceFailures.value().teacherDisplayName);
    QCOMPARE(sourceFailures.value().teacherDisplayName.error().message,
             std::string("teacher source failed"));
    QVERIFY(!sourceFailures.value().studentCount);
    QCOMPARE(sourceFailures.value().studentCount.error().message,
             std::string("roster source failed"));
}

void NextApplicationClassDetailsPageQueryTests::propagatesOverallPortError()
{
    FakeReadPort readPort;
    const Domain::OperationError expected{
        .code = Domain::ErrorCode::Technical,
        .message = "snapshot source failed",
        .recoverable = true
    };
    readPort.result = ClassDetailsPageReadResult::failure(expected);

    const ClassDetailsPageQuery query(readPort);
    const auto result = query.execute(classId("42"));

    QVERIFY(!result);
    QVERIFY(result.error() == expected);
    QCOMPARE(readPort.requests.size(), std::size_t(1));
}

void NextApplicationClassDetailsPageQueryTests::
rejectsSnapshotClassIdMismatch()
{
    FakeReadPort readPort;
    readPort.result = ClassDetailsPageReadResult::success(snapshot(
        classId("someone-else"),
        Domain::Result<ClassDetailsPageFields>::success({}),
        Domain::Result<std::string>::success({}),
        Domain::Result<int>::success(0)
        ));

    const ClassDetailsPageQuery query(readPort);
    const auto result = query.execute(classId("selected-class"));

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(readPort.requests.size(), std::size_t(1));
}

QTEST_APPLESS_MAIN(NextApplicationClassDetailsPageQueryTests)

#include "next_application_class_details_page_query_tests.moc"
