#include "next/application/korean_teacher_birthday_directory_read_query.h"

#include <QtTest/QtTest>

#include <string>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

class RecordingReadPort final : public KoreanTeacherBirthdayDirectoryReadPort
{
public:
    [[nodiscard]] KoreanTeacherBirthdayDirectoryReadResult
    readKoreanTeacherBirthdayDirectory() const override
    {
        ++callCount;
        return result;
    }

    mutable int callCount = 0;
    KoreanTeacherBirthdayDirectoryReadResult result =
        KoreanTeacherBirthdayDirectoryReadResult::success({});
};

}

class NextApplicationKoreanTeacherBirthdayDirectoryReadQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void callsPortOnceAndPreservesOrderAndRawNameAndBirthdayFields();
    void preservesEmptySuccessAndStructuredFailure();
};

void NextApplicationKoreanTeacherBirthdayDirectoryReadQueryTests::
callsPortOnceAndPreservesOrderAndRawNameAndBirthdayFields()
{
    RecordingReadPort port;
    const KoreanTeacherBirthdayDirectorySnapshot expected{
        {
            .birthday = u" 03-14 ",
            .teacherKr = u"  \uAE40\uC120\uC0DD  ",
            .teacherEn = u"  Teacher One  ",
            .preferredRomanization = u"  Seonsaeng  ",
            .preferredName = u"  Preferred One  "
        },
        {
            .birthday = u"not-a-date",
            .teacherKr = u"Teacher Two Korean",
            .teacherEn = u"Teacher Two",
            .preferredRomanization = u"Teacher Two Romanized",
            .preferredName = u""
        }
    };
    port.result = KoreanTeacherBirthdayDirectoryReadResult::success(expected);
    const KoreanTeacherBirthdayDirectoryReadQuery query(port);

    const KoreanTeacherBirthdayDirectoryReadResult result = query.execute();

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(result.value() == expected);
    QCOMPARE(result.value().size(), std::size_t(2));
    QCOMPARE(result.value()[0].birthday, std::u16string(u" 03-14 "));
    QCOMPARE(result.value()[0].teacherKr,
        std::u16string(u"  \uAE40\uC120\uC0DD  "));
    QCOMPARE(result.value()[0].teacherEn,
        std::u16string(u"  Teacher One  "));
    QCOMPARE(result.value()[0].preferredRomanization,
        std::u16string(u"  Seonsaeng  "));
    QCOMPARE(result.value()[0].preferredName,
        std::u16string(u"  Preferred One  "));
    QCOMPARE(result.value()[1].birthday, std::u16string(u"not-a-date"));
    QCOMPARE(result.value()[1].preferredName, std::u16string());
}

void NextApplicationKoreanTeacherBirthdayDirectoryReadQueryTests::
preservesEmptySuccessAndStructuredFailure()
{
    RecordingReadPort port;
    const KoreanTeacherBirthdayDirectoryReadQuery query(port);

    const auto emptyResult = query.execute();
    QVERIFY(emptyResult);
    QVERIFY(emptyResult.value().empty());
    QCOMPARE(port.callCount, 1);

    const Domain::OperationError expectedError{
        .code = Domain::ErrorCode::Technical,
        .message = "Korean teacher birthday directory read failed",
        .recoverable = false
    };
    port.result = KoreanTeacherBirthdayDirectoryReadResult::failure(
        expectedError);

    const auto failedResult = query.execute();

    QVERIFY(!failedResult);
    QVERIFY(failedResult.error() == expectedError);
    QCOMPARE(port.callCount, 2);
}

QTEST_APPLESS_MAIN(
    NextApplicationKoreanTeacherBirthdayDirectoryReadQueryTests)

#include "next_application_korean_teacher_birthday_directory_read_query_tests.moc"
