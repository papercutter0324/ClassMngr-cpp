#include "next/application/class_notes_page_read_query.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

Domain::ClassId classId(const std::string& value)
{
    return *Domain::ClassId::fromString(value);
}

class RecordingReadPort final : public ClassNotesPageReadPort
{
public:
    [[nodiscard]] ClassNotesPageReadResult readClassNotesPage(
        const Domain::ClassId& request
        ) const override
    {
        ++callCount;
        lastRequest = request;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<Domain::ClassId> lastRequest;
    ClassNotesPageReadResult result = ClassNotesPageReadResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "Unconfigured read port",
        .recoverable = false
    });
};

ClassNotesPageReadSnapshot snapshot(
    const Domain::ClassId& id,
    Domain::Result<ClassNotesPageFields> fields,
    Domain::Result<std::u16string> teacherName
    )
{
    return {id, std::move(fields), std::move(teacherName)};
}

}

class NextApplicationClassNotesPageReadQueryTests final : public QObject
{
    Q_OBJECT

private slots:
    void contractUsesQtFreeTypedValues();
    void rejectsNonCanonicalOrOutOfRangeIdsBeforePortCall();
    void callsPortOnceWithTypedIdAndPreservesIndependentResults();
    void propagatesPortError();
    void rejectsMismatchedResponseIdentity();
};

void NextApplicationClassNotesPageReadQueryTests::
contractUsesQtFreeTypedValues()
{
    static_assert(std::is_same_v<
        decltype(ClassNotesPageFields::notes),
        std::u16string
        >);
    static_assert(std::is_same_v<
        decltype(ClassNotesPageScheduleRow::day),
        std::u16string
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<ClassNotesPageReadPort&>()
            .readClassNotesPage(std::declval<const Domain::ClassId&>())),
        ClassNotesPageReadResult
        >);

    ClassNotesPageFields fields;
    fields.classGrade = u"E4";
    fields.classLevel = u"Theseus";
    fields.regularSchedule = {{u"Friday", u"9:00 AM"}};
    fields.notes = u"Exact notes \U0001F642";
    fields.timeFillerActivities = u"Exact activities";
    QVERIFY(fields.regularSchedule.front().day == u"Friday");
    QVERIFY(fields.notes == u"Exact notes \U0001F642");
}

void NextApplicationClassNotesPageReadQueryTests::
rejectsNonCanonicalOrOutOfRangeIdsBeforePortCall()
{
    RecordingReadPort port;
    const std::vector<Domain::ClassId> invalid{
        classId("0"),
        classId("-1"),
        classId("01"),
        classId("+1"),
        classId(" 1"),
        classId("2147483648")
    };
    const ClassNotesPageReadQuery query(port);

    for (const auto& id : invalid)
    {
        const auto result = query.execute(id);
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }
    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationClassNotesPageReadQueryTests::
callsPortOnceWithTypedIdAndPreservesIndependentResults()
{
    RecordingReadPort port;
    const Domain::ClassId requested = classId("42");
    ClassNotesPageFields fields;
    fields.classGrade = u"E4";
    fields.classLevel = u"Theseus";
    fields.regularSchedule = {{u"Monday", u"4:00 PM"}};
    fields.notes = u"  stored notes  ";
    fields.timeFillerActivities = u" stored activities ";
    const ClassNotesPageFields expected = fields;
    port.result = ClassNotesPageReadResult::success(snapshot(
        requested,
        Domain::Result<ClassNotesPageFields>::success(std::move(fields)),
        Domain::Result<std::u16string>::success(u"Teacher Display")
        ));
    const ClassNotesPageReadQuery query(port);

    const auto result = query.execute(requested);

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    QVERIFY(*port.lastRequest == requested);
    QVERIFY(result.value().classId == requested);
    QVERIFY(result.value().classFields);
    QVERIFY(result.value().classFields.value() == expected);
    QVERIFY(result.value().teacherDisplayName);
    QVERIFY(result.value().teacherDisplayName.value() == u"Teacher Display");

    const Domain::OperationError teacherError{
        .code = Domain::ErrorCode::NotFound,
        .message = "teacher unavailable",
        .recoverable = true
    };
    port.result = ClassNotesPageReadResult::success(snapshot(
        requested,
        Domain::Result<ClassNotesPageFields>::success(expected),
        Domain::Result<std::u16string>::failure(teacherError)
        ));
    const auto independent = query.execute(requested);
    QVERIFY(independent);
    QVERIFY(independent.value().classFields);
    QVERIFY(!independent.value().teacherDisplayName);
    QVERIFY(independent.value().teacherDisplayName.error() == teacherError);
    QCOMPARE(port.callCount, 2);
}

void NextApplicationClassNotesPageReadQueryTests::propagatesPortError()
{
    RecordingReadPort port;
    const Domain::OperationError expected{
        .code = Domain::ErrorCode::Technical,
        .message = "snapshot read failed",
        .recoverable = false
    };
    port.result = ClassNotesPageReadResult::failure(expected);

    const ClassNotesPageReadQuery query(port);
    const auto result = query.execute(classId("7"));

    QVERIFY(!result);
    QVERIFY(result.error() == expected);
    QCOMPARE(port.callCount, 1);
}

void NextApplicationClassNotesPageReadQueryTests::
rejectsMismatchedResponseIdentity()
{
    RecordingReadPort port;
    port.result = ClassNotesPageReadResult::success(snapshot(
        classId("8"),
        Domain::Result<ClassNotesPageFields>::success({}),
        Domain::Result<std::u16string>::success({})
        ));

    const ClassNotesPageReadQuery query(port);
    const auto result = query.execute(classId("7"));

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(port.callCount, 1);
}

QTEST_APPLESS_MAIN(NextApplicationClassNotesPageReadQueryTests)

#include "next_application_class_notes_page_read_query_tests.moc"
