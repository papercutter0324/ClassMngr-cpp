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
        std::string
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<SelectedClassGradeReadPort&>()
            .readSelectedClassGrade(std::declval<const Domain::ClassId&>())),
        SelectedClassGradeReadResult
        >);

    SelectedClassGradeReadSnapshot snapshot{
        classId("42"),
        "M2"
    };
    QVERIFY(snapshot.classId == classId("42"));
    QVERIFY(snapshot.classGrade == "M2");
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
             std::string("2147483648")
         })
    {
        const auto result = query.execute(classId(value));
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
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
        " M1 "
    });

    const SelectedClassGradeReadQuery query(port);
    auto result = query.execute(requested);

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    QVERIFY(*port.lastRequest == requested);
    QVERIFY(result.value().classId == requested);
    QVERIFY(result.value().classGrade == " M1 ");

    port.result = SelectedClassGradeReadResult::success({requested, {}});
    result = query.execute(requested);

    QVERIFY(result);
    QVERIFY(result.value().classGrade.empty());
    QCOMPARE(port.callCount, 2);
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
        "M3"
    });
    result = query.execute(classId("42"));

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(port.callCount, 2);
}

QTEST_APPLESS_MAIN(NextApplicationSelectedClassGradeReadQueryTests)

#include "next_application_selected_class_grade_read_query_tests.moc"
