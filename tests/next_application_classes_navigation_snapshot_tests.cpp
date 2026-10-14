#include "next/application/classes_navigation_snapshot.h"

#include <QtTest/QtTest>

#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

Domain::ClassId classId(std::string value)
{
    return *Domain::ClassId::fromString(std::move(value));
}

ClassesNavigationClass sourceClass(
    std::string id,
    std::u16string name
    )
{
    return {
        .classId = classId(std::move(id)),
        .className = std::move(name)
    };
}

class RecordingClassesNavigationReadPort final
    : public ClassesNavigationSnapshotReadPort
{
public:
    [[nodiscard]] ClassesNavigationSnapshotResult readClasses(
        const ClassesNavigationSnapshotQuery& query
        ) const override
    {
        requests.push_back(query);
        return result;
    }

    mutable std::vector<ClassesNavigationSnapshotQuery> requests;
    ClassesNavigationSnapshotResult result =
        ClassesNavigationSnapshotResult::success({});
};

}

class NextApplicationClassesNavigationSnapshotTests final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsNonCanonicalAndDuplicateIdsWithoutCallingPort();
    void returnsSuccessfulEmptySnapshotWithoutCallingPort();
    void preservesRequestedOrderExactUnicodeAndRawScheduleRows();
    void rejectsIdentityMismatchWhenDetailsFailed();
    void propagatesPortErrorAndRejectsMalformedSnapshots();
};

void NextApplicationClassesNavigationSnapshotTests::
rejectsNonCanonicalAndDuplicateIdsWithoutCallingPort()
{
    RecordingClassesNavigationReadPort port;
    const auto malformedResult = ClassesNavigationSnapshotQueryHandler::execute(
        {.classes = {sourceClass("42x", u"Name")}},
        port
        );
    QVERIFY(!malformedResult);
    QCOMPARE(malformedResult.error().message,
             std::string("Class IDs must be canonical positive integers."));
    QCOMPARE(malformedResult.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(malformedResult.error().recoverable);
    QVERIFY(port.requests.empty());

    for (const std::string& invalid : {
             "0", "-42", "+42", "042", "42 ", "class-42",
             "999999999999999999999"
         })
    {
        ClassesNavigationSnapshotQuery query{
            .classes = {sourceClass(invalid, u"Name")}
        };
        const auto result =
            ClassesNavigationSnapshotQueryHandler::execute(query, port);
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }

    ClassesNavigationSnapshotQuery duplicateQuery{
        .classes = {
            sourceClass("42", u"First"),
            sourceClass("42", u"Second")
        }
    };
    const auto duplicateResult =
        ClassesNavigationSnapshotQueryHandler::execute(duplicateQuery, port);
    QVERIFY(!duplicateResult);
    QCOMPARE(duplicateResult.error().message,
             std::string("Class IDs must be unique."));
    QCOMPARE(duplicateResult.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(duplicateResult.error().recoverable);
    QVERIFY(port.requests.empty());
}

void NextApplicationClassesNavigationSnapshotTests::
returnsSuccessfulEmptySnapshotWithoutCallingPort()
{
    RecordingClassesNavigationReadPort port;
    const auto result = ClassesNavigationSnapshotQueryHandler::execute(
        {},
        port
        );

    QVERIFY(result);
    QVERIFY(result.value().classes.empty());
    QVERIFY(port.requests.empty());
}

void NextApplicationClassesNavigationSnapshotTests::
preservesRequestedOrderExactUnicodeAndRawScheduleRows()
{
    RecordingClassesNavigationReadPort port;
    const ClassesNavigationSnapshotQuery requested{
        .classes = {
            sourceClass("43", u"  안녕 \U0001F9ED  "),
            sourceClass("42", u"Class Ω")
        }
    };
    port.result = ClassesNavigationSnapshotResult::success({
        .classes = {
            {
                .classId = requested.classes[0].classId,
                .className = requested.classes[0].className,
                .details = ClassesNavigationDetailsResult::success({
                    .grade = u"E4",
                    .level = u"Perseus",
                    .regularSchedule = {
                        {u"Raw Friday", u" 4:00 PM ", u"4:55 PM"},
                        {u"Monday", u"4:00 PM", u"4:50 PM"}
                    },
                    .intensiveSchedule = {
                        {u"Holiday", u"09:00", u"09:55"}
                    },
                    .teacherEnglishName = u"Teacher \U0001F9ED",
                    .teacherKoreanName = u"김민지"
                })
            },
            {
                .classId = requested.classes[1].classId,
                .className = requested.classes[1].className,
                .details = ClassesNavigationDetailsResult::failure({
                    .code = Domain::ErrorCode::NotFound,
                    .message = "Class no longer exists.",
                    .recoverable = true
                })
            }
        }
    });

    const auto result =
        ClassesNavigationSnapshotQueryHandler::execute(requested, port);

    QVERIFY(result);
    QCOMPARE(port.requests.size(), std::size_t(1));
    QVERIFY(port.requests.front() == requested);
    QCOMPARE(result.value().classes.size(), std::size_t(2));
    QVERIFY(result.value().classes[0].classId == classId("43"));
    QCOMPARE(result.value().classes[0].className,
             std::u16string(u"  안녕 \U0001F9ED  "));
    QCOMPARE(result.value().classes[0].details.value().grade,
             std::u16string(u"E4"));
    QCOMPARE(
        result.value().classes[0].details.value().regularSchedule.size(),
        std::size_t(2)
        );
    QCOMPARE(result.value().classes[0].details.value().regularSchedule[0].day,
             std::u16string(u"Raw Friday"));
    QCOMPARE(
        result.value().classes[0].details.value().regularSchedule[0].startTime,
        std::u16string(u" 4:00 PM ")
        );
    QCOMPARE(result.value().classes[0].details.value().regularSchedule[1].day,
             std::u16string(u"Monday"));
    QCOMPARE(
        result.value().classes[0].details.value().intensiveSchedule.size(),
        std::size_t(1)
        );
    QCOMPARE(result.value().classes[0].details.value().intensiveSchedule[0].endTime,
             std::u16string(u"09:55"));
    QCOMPARE(result.value().classes[0].details.value().teacherEnglishName,
             std::u16string(u"Teacher \U0001F9ED"));
    QCOMPARE(result.value().classes[0].details.value().teacherKoreanName,
             std::u16string(u"김민지"));
    QVERIFY(result.value().classes[1].classId == classId("42"));
    QCOMPARE(result.value().classes[1].className,
             requested.classes[1].className);
    QCOMPARE(result.value().classes[1].details.error().code,
             Domain::ErrorCode::NotFound);
    QVERIFY(result.value().classes[1].details.error().recoverable);
}

void NextApplicationClassesNavigationSnapshotTests::
rejectsIdentityMismatchWhenDetailsFailed()
{
    RecordingClassesNavigationReadPort port;
    const ClassesNavigationSnapshotQuery requested{
        .classes = {
            sourceClass("42", u"Class 42"),
            sourceClass("43", u"Class 43")
        }
    };
    port.result = ClassesNavigationSnapshotResult::success({
        .classes = {
            {
                .classId = requested.classes[0].classId,
                .className = requested.classes[0].className,
                .details = ClassesNavigationDetailsResult::failure({
                    .code = Domain::ErrorCode::NotFound,
                    .message = "Missing parent.",
                    .recoverable = true
                })
            },
            {
                .classId = requested.classes[1].classId,
                .className = u"Changed name",
                .details = ClassesNavigationDetailsResult::failure({
                    .code = Domain::ErrorCode::NotFound,
                    .message = "Missing parent.",
                    .recoverable = true
                })
            }
        }
    });

    const auto result =
        ClassesNavigationSnapshotQueryHandler::execute(requested, port);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(result.error().message,
             std::string(
                 "A classes navigation read changed class identity or order."
                 ));

    port.result = ClassesNavigationSnapshotResult::success({
        .classes = {
            {
                .classId = requested.classes[1].classId,
                .className = requested.classes[1].className,
                .details = ClassesNavigationDetailsResult::failure({
                    .code = Domain::ErrorCode::NotFound,
                    .message = "Missing parent.",
                    .recoverable = true
                })
            },
            {
                .classId = requested.classes[0].classId,
                .className = requested.classes[0].className,
                .details = ClassesNavigationDetailsResult::failure({
                    .code = Domain::ErrorCode::NotFound,
                    .message = "Missing parent.",
                    .recoverable = true
                })
            }
        }
    });
    const auto reorderedResult =
        ClassesNavigationSnapshotQueryHandler::execute(requested, port);
    QVERIFY(!reorderedResult);
    QCOMPARE(reorderedResult.error().code, Domain::ErrorCode::Validation);
}

void NextApplicationClassesNavigationSnapshotTests::
propagatesPortErrorAndRejectsMalformedSnapshots()
{
    RecordingClassesNavigationReadPort port;
    const ClassesNavigationSnapshotQuery requested{
        .classes = {sourceClass("42", u"Class 42")}
    };
    const Domain::OperationError sourceError{
        .code = Domain::ErrorCode::Technical,
        .message = "batch read failed",
        .recoverable = false
    };
    port.result = ClassesNavigationSnapshotResult::failure(sourceError);

    port.requests.clear();
    auto result = ClassesNavigationSnapshotQueryHandler::execute(
        requested,
        port
        );
    QVERIFY(!result);
    QVERIFY(result.error() == sourceError);
    QCOMPARE(port.requests.size(), std::size_t(1));

    port.result = ClassesNavigationSnapshotResult::success({});
    port.requests.clear();
    result = ClassesNavigationSnapshotQueryHandler::execute(requested, port);
    QVERIFY(!result);
    QCOMPARE(result.error().message,
             std::string(
                 "A classes navigation read returned a different number of entries."
                 ));
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QVERIFY(!result.error().recoverable);
    QCOMPARE(port.requests.size(), std::size_t(1));

    port.result = ClassesNavigationSnapshotResult::success({
        .classes = {{
            .classId = classId("43"),
            .className = u"Class 42"
        }}
    });
    port.requests.clear();
    result = ClassesNavigationSnapshotQueryHandler::execute(requested, port);
    QVERIFY(!result);
    QCOMPARE(result.error().message,
             std::string(
                 "A classes navigation read changed class identity or order."
                 ));
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QVERIFY(!result.error().recoverable);
    QCOMPARE(port.requests.size(), std::size_t(1));
}

QTEST_APPLESS_MAIN(NextApplicationClassesNavigationSnapshotTests)

#include "next_application_classes_navigation_snapshot_tests.moc"
