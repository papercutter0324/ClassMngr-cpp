#include "next/application/selected_class_grade_read_query.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <type_traits>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

Domain::ClassId classId(const std::string& value)
{
    return *Domain::ClassId::fromString(value);
}

class RecordingReadPort final : public SelectedClassGradeReadPort
{
public:
    [[nodiscard]] SelectedClassGradeReadResult readSelectedClassGrade(
        const Domain::ClassId& request
        ) const override
    {
        ++callCount;
        lastRequest = request;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<Domain::ClassId> lastRequest;
    SelectedClassGradeReadResult result =
        SelectedClassGradeReadResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unconfigured read port",
            .recoverable = false
        });
};

}

class NextApplicationSelectedClassGradeReadQueryTests final : public QObject
{
    Q_OBJECT

private slots:
    void contractUsesQtFreeTypedValues();
    void validatesSelectedClassIdBeforePortCall();
    void mapsTypedIdAndPreservesEmptyOrPopulatedGrade();
    void propagatesReadFailureAndRejectsMismatchedIdentity();
};

void NextApplicationSelectedClassGradeReadQueryTests::
contractUsesQtFreeTypedValues()
{
    static_assert(std::is_same_v<
        decltype(SelectedClassGradeReadSnapshot::classGrade),
        SelectedClassGradeValue
        >);
    static_assert(std::is_constructible_v<SelectedClassGradeValue, std::string>);
    static_assert(!std::is_convertible_v<std::string, SelectedClassGradeValue>);
    static_assert(!std::is_convertible_v<SelectedClassGradeValue, std::string>);
    static_assert(std::is_same_v<
        decltype(std::declval<const SelectedClassGradeValue&>().value()),
        const std::string&
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<SelectedClassGradeReadPort&>()
            .readSelectedClassGrade(std::declval<const Domain::ClassId&>())),
        SelectedClassGradeReadResult
        >);

    SelectedClassGradeReadSnapshot snapshot{
        classId("42"),
        SelectedClassGradeValue("M2")
    };
    QVERIFY(snapshot.classId == classId("42"));
    QVERIFY(snapshot.classGrade == SelectedClassGradeValue("M2"));
    QVERIFY(SelectedClassGradeValue{}.value().empty());

    const std::string arbitraryUtf8("\xE3\x81\x82\0\xF0\x9F\x8C\x8C", 8);
    const SelectedClassGradeValue preserved(arbitraryUtf8);
    QCOMPARE(preserved.value(), arbitraryUtf8);
    QVERIFY(preserved == SelectedClassGradeValue(arbitraryUtf8));
}

void NextApplicationSelectedClassGradeReadQueryTests::
validatesSelectedClassIdBeforePortCall()
{
    RecordingReadPort port;
    const SelectedClassGradeReadQuery query(port);

    for (const std::string& value : {
             std::string("0"),
             std::string("-1"),
             std::string("042"),
             std::string("+42"),
             std::string("42 "),
             std::string("42x"),
             std::string("2147483648")
         })
    {
        const auto result = query.execute(classId(value));
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
        QCOMPARE(
            result.error().message,
            std::string(
                "Selected class ID must be a canonical positive integer."
                )
            );
        QVERIFY(!result.error().recoverable);
    }

    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationSelectedClassGradeReadQueryTests::
mapsTypedIdAndPreservesEmptyOrPopulatedGrade()
{
    RecordingReadPort port;
    const Domain::ClassId requested = classId("42");
    port.result = SelectedClassGradeReadResult::success({
        requested,
        SelectedClassGradeValue(" M1 ")
    });

    const SelectedClassGradeReadQuery query(port);
    auto result = query.execute(requested);

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    QVERIFY(*port.lastRequest == requested);
    QVERIFY(result.value().classId == requested);
    QCOMPARE(result.value().classGrade.value(), std::string(" M1 "));

    const std::string arbitraryUtf8("\xE3\x81\x82\0\xF0\x9F\x8C\x8C", 8);
    port.result = SelectedClassGradeReadResult::success({
        requested,
        SelectedClassGradeValue(arbitraryUtf8)
        });
    result = query.execute(requested);

    QVERIFY(result);
    QCOMPARE(result.value().classGrade.value(), arbitraryUtf8);

    port.result = SelectedClassGradeReadResult::success({
        requested,
        SelectedClassGradeValue{}
        });
    result = query.execute(requested);

    QVERIFY(result);
    QVERIFY(result.value().classGrade.value().empty());
    QCOMPARE(port.callCount, 3);
}

void NextApplicationSelectedClassGradeReadQueryTests::
propagatesReadFailureAndRejectsMismatchedIdentity()
{
    RecordingReadPort port;
    const Domain::OperationError expected{
        .code = Domain::ErrorCode::Technical,
        .message = "class grade source failed",
        .recoverable = false
    };
    port.result = SelectedClassGradeReadResult::failure(expected);

    const SelectedClassGradeReadQuery query(port);
    auto result = query.execute(classId("42"));

    QVERIFY(!result);
    QVERIFY(result.error() == expected);
    QCOMPARE(port.callCount, 1);

    port.result = SelectedClassGradeReadResult::success({
        classId("43"),
        SelectedClassGradeValue("M3")
    });
    result = query.execute(classId("42"));

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(port.callCount, 2);
}

QTEST_APPLESS_MAIN(NextApplicationSelectedClassGradeReadQueryTests)

#include "next_application_selected_class_grade_read_query_tests.moc"
