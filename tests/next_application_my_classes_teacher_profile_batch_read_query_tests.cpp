#include "next/application/my_classes_teacher_profile_batch_read_query.h"

#include <QtTest/QtTest>

#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

Domain::TeacherId teacherId(const std::string& value)
{
    return *Domain::TeacherId::fromString(value);
}

Application::MyClassesTeacherProfileBatchReadEntry successfulEntry(
    const Domain::TeacherId& id,
    Domain::TeacherProfileFields fields = {}
    )
{
    return {
        .teacherId = id,
        .profile = Domain::Result<Domain::TeacherProfileFields>::success(
            std::move(fields)
            )
    };
}

class FakeMyClassesTeacherProfileBatchReadPort final
    : public Application::MyClassesTeacherProfileBatchReadPort
{
public:
    mutable int callCount = 0;
    mutable std::vector<Domain::TeacherId> requestedIds;
    Application::MyClassesTeacherProfileBatchReadResult result =
        Application::MyClassesTeacherProfileBatchReadResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "not configured",
            .recoverable = false
        });

    [[nodiscard]] Application::MyClassesTeacherProfileBatchReadResult
    readMyClassesTeacherProfiles(
        const std::vector<Domain::TeacherId>& ids
        ) const override
    {
        ++callCount;
        requestedIds = ids;
        return result;
    }
};

}

class NextApplicationMyClassesTeacherProfileBatchReadQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void emptyInputDoesNotCallPort();
    void preservesOrderedPerTeacherSuccessAndFailure();
    void rejectsInvalidAndDuplicateTeacherIdsBeforePortRead();
    void rejectsEntryCountAndOrderMismatch();
    void propagatesBatchPortFailure();
};

void NextApplicationMyClassesTeacherProfileBatchReadQueryTests::
emptyInputDoesNotCallPort()
{
    FakeMyClassesTeacherProfileBatchReadPort port;
    const Application::MyClassesTeacherProfileBatchReadQuery query(port);
    const auto result = query.execute({});

    QVERIFY(result);
    QVERIFY(result.value().empty());
    QCOMPARE(port.callCount, 0);
    QVERIFY(port.requestedIds.empty());
}

void NextApplicationMyClassesTeacherProfileBatchReadQueryTests::
preservesOrderedPerTeacherSuccessAndFailure()
{
    FakeMyClassesTeacherProfileBatchReadPort port;
    const std::vector<Domain::TeacherId> requested{
        teacherId("42"), teacherId("7"), teacherId("100")
    };
    Domain::TeacherProfileFields fields{
        .teacherKr = u"\uD55C\uAD6D\uC5B4",
        .teacherEn = u"English teacher",
        .preferredRomanization = u"Romanized name",
        .preferredName = u"Preferred name",
        .roomNumber = u"Room 12",
        .birthday = u"03-14",
        .phoneNumber = u"010-1234-5678",
        .wifiName = u"Class WiFi",
        .wifiPassword = u"WiFi password",
        .internetType = u"Both",
        .zoomId = u"Zoom ID",
        .zoomPassword = u"Zoom password",
        .projectionType = u"Zoom",
        .notes = u"Profile notes \U0001F9ED"
    };
    port.result = Application::MyClassesTeacherProfileBatchReadResult::success({
        successfulEntry(requested[0], fields),
        {
            .teacherId = requested[1],
            .profile = Domain::Result<Domain::TeacherProfileFields>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "teacher 7 could not be read",
                .recoverable = false
            })
        },
        successfulEntry(requested[2])
    });

    const Application::MyClassesTeacherProfileBatchReadQuery query(port);
    const auto result = query.execute(requested);

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.requestedIds == requested);
    QCOMPARE(result.value().size(), requested.size());
    QVERIFY(result.value()[0].teacherId == requested[0]);
    QVERIFY(result.value()[0].profile);
    QVERIFY(result.value()[0].profile.value() == fields);
    QVERIFY(result.value()[1].teacherId == requested[1]);
    QVERIFY(!result.value()[1].profile);
    QCOMPARE(result.value()[1].profile.error().message,
             std::string("teacher 7 could not be read"));
    QVERIFY(result.value()[2].teacherId == requested[2]);
    QVERIFY(result.value()[2].profile);
}

void NextApplicationMyClassesTeacherProfileBatchReadQueryTests::
rejectsInvalidAndDuplicateTeacherIdsBeforePortRead()
{
    FakeMyClassesTeacherProfileBatchReadPort port;
    const Application::MyClassesTeacherProfileBatchReadQuery query(port);
    const std::vector<std::string> invalidIds{
        "0", "07", "-1", "+4", " 4", "4 ", "2147483648"
    };

    for (const std::string& value : invalidIds)
    {
        const auto result = query.execute({teacherId(value)});
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }

    const auto duplicate = query.execute({teacherId("4"), teacherId("4")});
    QVERIFY(!duplicate);
    QCOMPARE(duplicate.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(port.callCount, 0);
}

void NextApplicationMyClassesTeacherProfileBatchReadQueryTests::
rejectsEntryCountAndOrderMismatch()
{
    FakeMyClassesTeacherProfileBatchReadPort port;
    const std::vector<Domain::TeacherId> requested{
        teacherId("4"), teacherId("9")
    };
    const Application::MyClassesTeacherProfileBatchReadQuery query(port);

    port.result = Application::MyClassesTeacherProfileBatchReadResult::success({
        successfulEntry(requested.front())
    });
    const auto shortResult = query.execute(requested);
    QVERIFY(!shortResult);
    QCOMPARE(shortResult.error().code, Domain::ErrorCode::Validation);

    port.result = Application::MyClassesTeacherProfileBatchReadResult::success({
        successfulEntry(requested.back()),
        successfulEntry(requested.front())
    });
    const auto reorderedResult = query.execute(requested);
    QVERIFY(!reorderedResult);
    QCOMPARE(reorderedResult.error().code, Domain::ErrorCode::Validation);
}

void NextApplicationMyClassesTeacherProfileBatchReadQueryTests::
propagatesBatchPortFailure()
{
    FakeMyClassesTeacherProfileBatchReadPort port;
    const Domain::OperationError expected{
        .code = Domain::ErrorCode::NotFound,
        .message = "The active database session is unavailable.",
        .recoverable = true
    };
    port.result =
        Application::MyClassesTeacherProfileBatchReadResult::failure(expected);

    const Application::MyClassesTeacherProfileBatchReadQuery query(port);
    const auto result = query.execute({teacherId("7")});

    QVERIFY(!result);
    QVERIFY(result.error() == expected);
    QCOMPARE(port.callCount, 1);
}

QTEST_APPLESS_MAIN(NextApplicationMyClassesTeacherProfileBatchReadQueryTests)

#include "next_application_my_classes_teacher_profile_batch_read_query_tests.moc"
