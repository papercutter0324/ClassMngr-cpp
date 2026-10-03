#include "next/application/initial_setup_teacher_choices_read_query.h"

#include <QtTest/QtTest>

#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

Domain::TeacherId teacherId(const std::string& value)
{
    const auto parsed = Domain::TeacherId::fromString(value);
    if (!parsed)
    {
        qFatal("Test teacher ID must have a typed representation.");
    }
    return *parsed;
}

class RecordingInitialSetupTeacherChoicesReadPort final
    : public InitialSetupTeacherChoicesReadPort
{
public:
    [[nodiscard]] InitialSetupTeacherChoicesResult
    readInitialSetupTeacherChoices() const override
    {
        ++callCount;
        return result;
    }

    mutable int callCount = 0;
    InitialSetupTeacherChoicesResult result =
        InitialSetupTeacherChoicesResult::success({});
};

}

class NextApplicationInitialSetupTeacherChoicesReadQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void preservesOrderedTypedIdsAndAllUtf16NameFields();
    void preservesEmptySuccessAndStructuredFailure();
    void rejectsInvalidOrDuplicateTeacherIds();
};

void NextApplicationInitialSetupTeacherChoicesReadQueryTests::
preservesOrderedTypedIdsAndAllUtf16NameFields()
{
    RecordingInitialSetupTeacherChoicesReadPort port;
    const InitialSetupTeacherChoicesSnapshot expected{
        .teachers = {
            {
                .teacherId = teacherId("17"),
                .teacherKr = u"  \uAE40\uC120\uC0DD  ",
                .teacherEn = u"  Teacher Seventeen  ",
                .preferredRomanization = u"  Seonsaeng  ",
                .preferredName = u"  Teacher 17  "
            },
            {
                .teacherId = teacherId("3"),
                .teacherKr = u"\uC774\uC120\uC0DD",
                .teacherEn = u"Teacher Three",
                .preferredRomanization = u"I Seonsaeng",
                .preferredName = u"Teacher 3"
            }
        }
    };
    port.result = InitialSetupTeacherChoicesResult::success(expected);
    const InitialSetupTeacherChoicesReadQuery query(port);

    const auto result = query.execute();

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(result.value() == expected);
    QCOMPARE(result.value().teachers.size(), std::size_t(2));
    QCOMPARE(result.value().teachers[0].teacherId.value(), std::string("17"));
    QCOMPARE(result.value().teachers[0].teacherKr,
        std::u16string(u"  \uAE40\uC120\uC0DD  "));
    QCOMPARE(result.value().teachers[0].teacherEn,
        std::u16string(u"  Teacher Seventeen  "));
    QCOMPARE(result.value().teachers[0].preferredRomanization,
        std::u16string(u"  Seonsaeng  "));
    QCOMPARE(result.value().teachers[0].preferredName,
        std::u16string(u"  Teacher 17  "));
    QCOMPARE(result.value().teachers[1].teacherId.value(), std::string("3"));
    QCOMPARE(result.value().teachers[1].teacherKr,
        std::u16string(u"\uC774\uC120\uC0DD"));
    QCOMPARE(result.value().teachers[1].teacherEn,
        std::u16string(u"Teacher Three"));
    QCOMPARE(result.value().teachers[1].preferredRomanization,
        std::u16string(u"I Seonsaeng"));
    QCOMPARE(result.value().teachers[1].preferredName,
        std::u16string(u"Teacher 3"));
}

void NextApplicationInitialSetupTeacherChoicesReadQueryTests::
preservesEmptySuccessAndStructuredFailure()
{
    RecordingInitialSetupTeacherChoicesReadPort port;
    InitialSetupTeacherChoicesReadQuery query(port);

    const auto emptyResult = query.execute();
    QVERIFY(emptyResult);
    QVERIFY(emptyResult.value().teachers.empty());
    QCOMPARE(port.callCount, 1);

    const Domain::OperationError expectedError{
        .code = Domain::ErrorCode::Technical,
        .message = "initial setup teacher choices are unavailable",
        .recoverable = false
    };
    port.result = InitialSetupTeacherChoicesResult::failure(expectedError);

    const auto failedResult = query.execute();

    QVERIFY(!failedResult);
    QVERIFY(failedResult.error() == expectedError);
    QCOMPARE(port.callCount, 2);
}

void NextApplicationInitialSetupTeacherChoicesReadQueryTests::
rejectsInvalidOrDuplicateTeacherIds()
{
    const std::vector<std::vector<std::string>> invalidIdSets{
        {"0"},
        {"01"},
        {"-1"},
        {"2147483648"},
        {"17", "17"}
    };

    for (const auto& ids : invalidIdSets)
    {
        RecordingInitialSetupTeacherChoicesReadPort port;
        InitialSetupTeacherChoicesSnapshot snapshot;
        for (const std::string& id : ids)
        {
            snapshot.teachers.push_back({
                .teacherId = teacherId(id),
                .teacherKr = u"Teacher",
                .teacherEn = u"Teacher",
                .preferredRomanization = u"Teacher",
                .preferredName = u"Teacher"
            });
        }
        port.result = InitialSetupTeacherChoicesResult::success(
            std::move(snapshot)
            );
        const InitialSetupTeacherChoicesReadQuery query(port);

        const auto result = query.execute();

        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
        QVERIFY(!result.error().recoverable);
        QCOMPARE(port.callCount, 1);
    }
}

QTEST_APPLESS_MAIN(NextApplicationInitialSetupTeacherChoicesReadQueryTests)

#include "next_application_initial_setup_teacher_choices_read_query_tests.moc"
