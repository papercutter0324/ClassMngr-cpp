#include "next/application/native_english_teacher_directory_read_query.h"
#include "next/domain/domain_types.h"

#include <QtTest/QtTest>

#include <string>
#include <type_traits>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

static_assert(!std::is_same_v<
    Domain::NativeEnglishTeacherId,
    Domain::TeacherId>);
static_assert(!std::is_convertible_v<
    Domain::NativeEnglishTeacherId,
    Domain::TeacherId>);

NativeEnglishTeacherDirectoryEntry teacher(
    const int id,
    const std::u16string& name
    )
{
    return {
        .id = Domain::NativeEnglishTeacherId(id),
        .name = name,
        .position = u"NET",
        .phoneNumber = u"010-1234-5678",
        .email = u"teacher@example.test",
        .birthday = u"03-14",
        .nationality = u"Canada"
    };
}

class RecordingReadPort final : public NativeEnglishTeacherDirectoryReadPort
{
public:
    [[nodiscard]] NativeEnglishTeacherDirectoryReadResult
    readNativeEnglishTeacherDirectory() const override
    {
        ++callCount;
        return result;
    }

    mutable int callCount = 0;
    NativeEnglishTeacherDirectoryReadResult result =
        NativeEnglishTeacherDirectoryReadResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unconfigured Native English Teacher directory read",
            .recoverable = false
        });
};

}

class NextApplicationNativeEnglishTeacherDirectoryReadQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void callsPortOnceAndPreservesRepositoryOrderAndAllFields();
    void propagatesPortError();
};

void NextApplicationNativeEnglishTeacherDirectoryReadQueryTests::
callsPortOnceAndPreservesRepositoryOrderAndAllFields()
{
    RecordingReadPort port;
    const NativeEnglishTeacherDirectorySnapshot expected{
        teacher(7, u"Alpha"),
        teacher(3, u"Zulu")
    };
    port.result = NativeEnglishTeacherDirectoryReadResult::success(expected);
    const NativeEnglishTeacherDirectoryReadQuery query(port);

    const NativeEnglishTeacherDirectoryReadResult result = query.execute();

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(result.value() == expected);
    QCOMPARE(result.value().at(0).id.value(), 7);
    QCOMPARE(result.value().at(0).name, std::u16string(u"Alpha"));
    QCOMPARE(result.value().at(0).position, std::u16string(u"NET"));
    QCOMPARE(result.value().at(0).phoneNumber,
        std::u16string(u"010-1234-5678"));
    QCOMPARE(result.value().at(0).email,
        std::u16string(u"teacher@example.test"));
    QCOMPARE(result.value().at(0).birthday, std::u16string(u"03-14"));
    QCOMPARE(result.value().at(0).nationality, std::u16string(u"Canada"));
    QCOMPARE(result.value().at(1).id.value(), 3);
}

void NextApplicationNativeEnglishTeacherDirectoryReadQueryTests::
propagatesPortError()
{
    RecordingReadPort port;
    const Domain::OperationError expectedError{
        .code = Domain::ErrorCode::Technical,
        .message = "native directory read failed",
        .recoverable = true
    };
    port.result = NativeEnglishTeacherDirectoryReadResult::failure(
        expectedError);
    const NativeEnglishTeacherDirectoryReadQuery query(port);

    const NativeEnglishTeacherDirectoryReadResult result = query.execute();

    QVERIFY(!result);
    QVERIFY(result.error() == expectedError);
    QCOMPARE(port.callCount, 1);
}

QTEST_APPLESS_MAIN(NextApplicationNativeEnglishTeacherDirectoryReadQueryTests)

#include "next_application_native_english_teacher_directory_read_query_tests.moc"
