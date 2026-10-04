#include "next/application/my_classes_class_information_batch_read_query.h"

#include <QtTest/QtTest>

#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

Domain::ClassId classId(const std::string& value)
{
    return *Domain::ClassId::fromString(value);
}

Domain::TeacherId teacherId(const std::string& value)
{
    return *Domain::TeacherId::fromString(value);
}

Application::MyClassesClassInformationBatchReadEntry successfulEntry(
    const Domain::ClassId& id,
    Application::MyClassesClassInformationFields fields = {}
    )
{
    return {
        .classId = id,
        .information = Domain::Result<
            Application::MyClassesClassInformationFields
            >::success(std::move(fields))
    };
}

class FakeMyClassesClassInformationBatchReadPort final
    : public Application::MyClassesClassInformationBatchReadPort
{
public:
    mutable int callCount = 0;
    mutable std::vector<Domain::ClassId> requestedIds;
    Application::MyClassesClassInformationBatchReadResult result =
        Application::MyClassesClassInformationBatchReadResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "not configured",
            .recoverable = false
        });

    [[nodiscard]] Application::MyClassesClassInformationBatchReadResult
    readMyClassesClassInformationBatch(
        const std::vector<Domain::ClassId>& classIds
        ) const override
    {
        ++callCount;
        requestedIds = classIds;
        return result;
    }
};

}

class NextApplicationMyClassesClassInformationBatchReadQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void emptyInputDoesNotCallPort();
    void preservesOrderedPerClassSuccessAndFailure();
    void rejectsInvalidAndDuplicateClassIdsBeforePortRead();
    void rejectsEntryCountAndOrderMismatch();
    void rejectsInvalidTeacherIdInSuccessfulEntry();
    void propagatesBatchPortFailure();
};

void NextApplicationMyClassesClassInformationBatchReadQueryTests::
emptyInputDoesNotCallPort()
{
    FakeMyClassesClassInformationBatchReadPort port;
    const Application::MyClassesClassInformationBatchReadQuery query(port);
    const auto result = query.execute({});

    QVERIFY(result);
    QVERIFY(result.value().empty());
    QCOMPARE(port.callCount, 0);
    QVERIFY(port.requestedIds.empty());
}

void NextApplicationMyClassesClassInformationBatchReadQueryTests::
preservesOrderedPerClassSuccessAndFailure()
{
    FakeMyClassesClassInformationBatchReadPort port;
    const std::vector<Domain::ClassId> requested{
        classId("42"), classId("7"), classId("100")
    };
    port.result = Application::MyClassesClassInformationBatchReadResult::success({
        successfulEntry(requested[0], {
            .classGrade = u"E4",
            .classLevel = u"Theseus",
            .regularSchedule = {{u"Wednesday", u"04:00 pm", u"04:50 pm"}},
            .teacherId = teacherId("19")
        }),
        {
            .classId = requested[1],
            .information = Domain::Result<
                Application::MyClassesClassInformationFields
                >::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = "class 7 could not be read",
                    .recoverable = true
                })
        },
        successfulEntry(requested[2])
    });

    const Application::MyClassesClassInformationBatchReadQuery query(port);
    const auto result = query.execute(requested);

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.requestedIds == requested);
    QCOMPARE(result.value().size(), requested.size());
    QVERIFY(result.value()[0].classId == requested[0]);
    QVERIFY(result.value()[0].information);
    QVERIFY(result.value()[0].information.value().classGrade == u"E4");
    QVERIFY(result.value()[0].information.value().teacherId == teacherId("19"));
    QVERIFY(result.value()[1].classId == requested[1]);
    QVERIFY(!result.value()[1].information);
    QCOMPARE(result.value()[1].information.error().message,
             std::string("class 7 could not be read"));
    QVERIFY(result.value()[2].classId == requested[2]);
    QVERIFY(result.value()[2].information);
}

void NextApplicationMyClassesClassInformationBatchReadQueryTests::
rejectsInvalidAndDuplicateClassIdsBeforePortRead()
{
    FakeMyClassesClassInformationBatchReadPort port;
    const Application::MyClassesClassInformationBatchReadQuery query(port);
    const std::vector<std::string> invalidIds{
        "0", "07", "-1", "+4", " 4", "4 ", "2147483648"
    };

    for (const std::string& value : invalidIds)
    {
        const auto result = query.execute({classId(value)});
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }

    const auto duplicate = query.execute({classId("4"), classId("4")});
    QVERIFY(!duplicate);
    QCOMPARE(duplicate.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(port.callCount, 0);
}

void NextApplicationMyClassesClassInformationBatchReadQueryTests::
rejectsEntryCountAndOrderMismatch()
{
    FakeMyClassesClassInformationBatchReadPort port;
    const std::vector<Domain::ClassId> requested{
        classId("4"), classId("9")
    };
    const Application::MyClassesClassInformationBatchReadQuery query(port);

    port.result = Application::MyClassesClassInformationBatchReadResult::success({
        successfulEntry(requested.front())
    });
    const auto shortResult = query.execute(requested);
    QVERIFY(!shortResult);
    QCOMPARE(shortResult.error().code, Domain::ErrorCode::Validation);

    port.result = Application::MyClassesClassInformationBatchReadResult::success({
        successfulEntry(requested.back()),
        successfulEntry(requested.front())
    });
    const auto reorderedResult = query.execute(requested);
    QVERIFY(!reorderedResult);
    QCOMPARE(reorderedResult.error().code, Domain::ErrorCode::Validation);
}

void NextApplicationMyClassesClassInformationBatchReadQueryTests::
rejectsInvalidTeacherIdInSuccessfulEntry()
{
    FakeMyClassesClassInformationBatchReadPort port;
    port.result = Application::MyClassesClassInformationBatchReadResult::success({
        successfulEntry(classId("7"), {
            .teacherId = teacherId("019")
        })
    });

    const Application::MyClassesClassInformationBatchReadQuery query(port);
    const auto result = query.execute({classId("7")});

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
}

void NextApplicationMyClassesClassInformationBatchReadQueryTests::
propagatesBatchPortFailure()
{
    FakeMyClassesClassInformationBatchReadPort port;
    const Domain::OperationError expected{
        .code = Domain::ErrorCode::NotFound,
        .message = "The active database session is unavailable.",
        .recoverable = true
    };
    port.result =
        Application::MyClassesClassInformationBatchReadResult::failure(expected);

    const Application::MyClassesClassInformationBatchReadQuery query(port);
    const auto result = query.execute({classId("7")});

    QVERIFY(!result);
    QVERIFY(result.error() == expected);
    QCOMPARE(port.callCount, 1);
}

QTEST_APPLESS_MAIN(NextApplicationMyClassesClassInformationBatchReadQueryTests)

#include "next_application_my_classes_class_information_batch_read_query_tests.moc"
