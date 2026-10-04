#include "next/application/native_english_teacher_birthday_directory_read_query.h"

#include <QtTest/QtTest>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

class RecordingPort final : public NativeEnglishTeacherBirthdayDirectoryReadPort
{
public:
    [[nodiscard]] NativeEnglishTeacherBirthdayDirectoryReadResult
    readNativeEnglishTeacherBirthdayDirectory() const override
    {
        ++callCount;
        return result;
    }

    mutable int callCount = 0;
    NativeEnglishTeacherBirthdayDirectoryReadResult result =
        NativeEnglishTeacherBirthdayDirectoryReadResult::success({});
};

}

class NextApplicationNativeEnglishTeacherBirthdayDirectoryReadQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void preservesRawRowsAndOrder();
    void preservesSuccessfulEmptyDirectory();
    void propagatesTypedFailure();
};

void NextApplicationNativeEnglishTeacherBirthdayDirectoryReadQueryTests::
preservesRawRowsAndOrder()
{
    RecordingPort port;
    port.result = NativeEnglishTeacherBirthdayDirectoryReadResult::success({
        {
            QStringLiteral("  Zulu  ").toStdU16String(),
            u" NET ",
            u" 12-31 "
        },
        {u"Alpha", u"Co-ordinator", u"01-02"}
    });
    const NativeEnglishTeacherBirthdayDirectoryReadQuery query(port);

    const auto result = query.execute();

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QCOMPARE(result.value().size(), std::size_t(2));
    QCOMPARE(result.value()[0].name, std::u16string(u"  Zulu  "));
    QCOMPARE(result.value()[0].position, std::u16string(u" NET "));
    QCOMPARE(result.value()[0].birthday, std::u16string(u" 12-31 "));
    QCOMPARE(result.value()[1].name, std::u16string(u"Alpha"));
}

void NextApplicationNativeEnglishTeacherBirthdayDirectoryReadQueryTests::
preservesSuccessfulEmptyDirectory()
{
    RecordingPort port;
    port.result = NativeEnglishTeacherBirthdayDirectoryReadResult::success({});
    const NativeEnglishTeacherBirthdayDirectoryReadQuery query(port);

    const auto result = query.execute();

    QVERIFY(result);
    QVERIFY(result.value().empty());
    QCOMPARE(port.callCount, 1);
}

void NextApplicationNativeEnglishTeacherBirthdayDirectoryReadQueryTests::
propagatesTypedFailure()
{
    RecordingPort port;
    const Domain::OperationError expected{
        .code = Domain::ErrorCode::Technical,
        .message = "Native birthday read failed",
        .recoverable = false
    };
    port.result = NativeEnglishTeacherBirthdayDirectoryReadResult::failure(
        expected);
    const NativeEnglishTeacherBirthdayDirectoryReadQuery query(port);

    const auto result = query.execute();

    QVERIFY(!result);
    QVERIFY(result.error() == expected);
    QCOMPARE(port.callCount, 1);
}

QTEST_APPLESS_MAIN(
    NextApplicationNativeEnglishTeacherBirthdayDirectoryReadQueryTests)

#include "next_application_native_english_teacher_birthday_directory_read_query_tests.moc"
