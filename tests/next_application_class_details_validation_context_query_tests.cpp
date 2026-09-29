#include "next/application/class_details_validation_context_query.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <type_traits>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

Domain::ClassId classId(const char* value)
{
    return *Domain::ClassId::fromString(value);
}

class RecordingPort final
    : public Application::ClassDetailsValidationContextPort
{
public:
    [[nodiscard]] Application::ClassDetailsValidationContextPortResult
    loadClassDetailsValidationContext(
        const Domain::ClassId& classIdValue
        ) const override
    {
        ++callCount;
        requestedClassId = classIdValue;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<Domain::ClassId> requestedClassId;
    Application::ClassDetailsValidationContextPortResult result =
        Application::ClassDetailsValidationContextPortResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unconfigured validation context port.",
            .recoverable = false
        });
};

}

class NextApplicationClassDetailsValidationContextQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void contractUsesTypedQtFreeContextValues();
    void callsPortOnceAndPreservesIdentitySentinelsAndRawText();
    void propagatesPortFailureExactly();
    void rejectsMismatchedClassIdentity();
    void rejectsNonCanonicalIdsBeforePortCall();
};

void NextApplicationClassDetailsValidationContextQueryTests::
contractUsesTypedQtFreeContextValues()
{
    static_assert(std::is_same_v<
        decltype(Application::ClassDetailsValidationContextData::matchedClassId),
        Domain::ClassId
        >);
    static_assert(std::is_same_v<
        decltype(Application::ClassDetailsValidationContextData::teacherId),
        int
        >);
    static_assert(std::is_same_v<
        decltype(Application::ClassDetailsValidationContextSnapshot::notes),
        std::u16string
        >);
    static_assert(std::is_same_v<
        decltype(Application::ClassDetailsValidationContextSnapshot::timeFillerActivities),
        std::u16string
        >);

    const Application::ClassDetailsValidationContextData context{
        .matchedClassId = classId("42"),
        .teacherId = -1,
        .notes = u" raw notes ",
        .timeFillerActivities = u" raw activities "
    };
    QCOMPARE(context.matchedClassId.value(), std::string("42"));
    QCOMPARE(context.teacherId, -1);
    QCOMPARE(context.notes, std::u16string(u" raw notes "));
    QCOMPARE(
        context.timeFillerActivities,
        std::u16string(u" raw activities ")
        );
}

void NextApplicationClassDetailsValidationContextQueryTests::
callsPortOnceAndPreservesIdentitySentinelsAndRawText()
{
    RecordingPort port;
    port.result = Application::ClassDetailsValidationContextPortResult::success({
        .matchedClassId = classId("42"),
        .teacherId = -2,
        .notes = u"  \uC548\uB155\uD558\uC138\uC694\nnotes  ",
        .timeFillerActivities = u"\u00a0activities\u00a0"
    });

    const auto result = Application::ClassDetailsValidationContextQuery::execute(
        classId("42"),
        port
        );

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.requestedClassId.has_value());
    QCOMPARE(port.requestedClassId->value(), std::string("42"));
    QCOMPARE(result.value().requestedClassId.value(), std::string("42"));
    QCOMPARE(result.value().matchedClassId.value(), std::string("42"));
    QCOMPARE(result.value().teacherId, -2);
    QCOMPARE(
        result.value().notes,
        std::u16string(u"  \uC548\uB155\uD558\uC138\uC694\nnotes  ")
        );
    QCOMPARE(
        result.value().timeFillerActivities,
        std::u16string(u"\u00a0activities\u00a0")
        );
}

void NextApplicationClassDetailsValidationContextQueryTests::
propagatesPortFailureExactly()
{
    RecordingPort port;
    const Domain::OperationError expected{
        .code = Domain::ErrorCode::Technical,
        .message = "class info repository read failed",
        .recoverable = true
    };
    port.result = Application::ClassDetailsValidationContextPortResult::failure(
        expected
        );

    const auto result = Application::ClassDetailsValidationContextQuery::execute(
        classId("42"),
        port
        );

    QVERIFY(!result);
    QVERIFY(result.error() == expected);
    QCOMPARE(port.callCount, 1);
}

void NextApplicationClassDetailsValidationContextQueryTests::
rejectsMismatchedClassIdentity()
{
    RecordingPort port;
    port.result = Application::ClassDetailsValidationContextPortResult::success({
        .matchedClassId = classId("43"),
        .teacherId = 7,
        .notes = u"wrong class",
        .timeFillerActivities = u"wrong class"
    });

    const auto result = Application::ClassDetailsValidationContextQuery::execute(
        classId("42"),
        port
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QVERIFY(!result.error().recoverable);
    QCOMPARE(port.callCount, 1);
}

void NextApplicationClassDetailsValidationContextQueryTests::
rejectsNonCanonicalIdsBeforePortCall()
{
    RecordingPort port;
    const std::vector<std::string> values{"0", "042", "42x", "-1"};
    for (const std::string& value : values)
    {
        const auto result = Application::ClassDetailsValidationContextQuery::execute(
            *Domain::ClassId::fromString(value),
            port
            );
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }
    QCOMPARE(port.callCount, 0);
}

QTEST_APPLESS_MAIN(NextApplicationClassDetailsValidationContextQueryTests)

#include "next_application_class_details_validation_context_query_tests.moc"
