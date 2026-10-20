#include "next/application/my_classes_student_count_batch_read_query.h"

#include <QtTest/QtTest>

#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

static_assert(!std::is_convertible_v<
    int,
    Application::MyClassesStudentCount
    >);

namespace
{

Domain::ClassId classId(const std::string& value)
{
    return *Domain::ClassId::fromString(value);
}

Application::MyClassesStudentCountBatchReadEntry successfulEntry(
    const Domain::ClassId& id,
    const int count
    )
{
    return {
        .classId = id,
        .studentCount = Domain::Result<
            Application::MyClassesStudentCount
            >::success(Application::MyClassesStudentCount(count))
    };
}

class FakeMyClassesStudentCountBatchReadPort final
    : public Application::MyClassesStudentCountBatchReadPort
{
public:
    mutable int callCount = 0;
    mutable std::vector<Domain::ClassId> requestedIds;
    Application::MyClassesStudentCountBatchReadResult result =
        Application::MyClassesStudentCountBatchReadResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "not configured",
            .recoverable = false
        });

    [[nodiscard]] Application::MyClassesStudentCountBatchReadResult
    readMyClassesStudentCounts(
        const std::vector<Domain::ClassId>& ids
        ) const override
    {
        ++callCount;
        requestedIds = ids;
        return result;
    }
};

}

class NextApplicationMyClassesStudentCountBatchReadQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void emptyInputDoesNotCallPort();
    void preservesOrderedPerClassSuccessAndFailure();
    void rejectsInvalidAndDuplicateClassIdsBeforePortRead();
    void rejectsEntryCountOrderAndNegativeCount();
    void propagatesBatchPortFailure();
};

void NextApplicationMyClassesStudentCountBatchReadQueryTests::
emptyInputDoesNotCallPort()
{
    FakeMyClassesStudentCountBatchReadPort port;
    const Application::MyClassesStudentCountBatchReadQuery query(port);
    const auto result = query.execute({});

    QVERIFY(result);
    QVERIFY(result.value().empty());
    QCOMPARE(port.callCount, 0);
    QVERIFY(port.requestedIds.empty());
}

void NextApplicationMyClassesStudentCountBatchReadQueryTests::
preservesOrderedPerClassSuccessAndFailure()
{
    FakeMyClassesStudentCountBatchReadPort port;
    const std::vector<Domain::ClassId> requested{
        classId("42"), classId("7"), classId("100"), classId("101")
    };
    const Domain::OperationError expectedRowError{
        .code = Domain::ErrorCode::Technical,
        .message = "class 7 roster failed",
        .recoverable = true
    };
    port.result = Application::MyClassesStudentCountBatchReadResult::success({
        successfulEntry(requested[0], 4),
        {
            .classId = requested[1],
            .studentCount = Domain::Result<
                Application::MyClassesStudentCount
                >::failure(expectedRowError)
        },
        successfulEntry(requested[2], 0),
        successfulEntry(requested[3], std::numeric_limits<int>::max())
    });

    const Application::MyClassesStudentCountBatchReadQuery query(port);
    const auto result = query.execute(requested);

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.requestedIds == requested);
    QCOMPARE(result.value().size(), requested.size());
    QVERIFY(result.value()[0].classId == requested[0]);
    QVERIFY(result.value()[0].studentCount);
    QCOMPARE(result.value()[0].studentCount.value().rawValue(), 4);
    QVERIFY(result.value()[1].classId == requested[1]);
    QVERIFY(!result.value()[1].studentCount);
    QVERIFY(result.value()[1].studentCount.error() == expectedRowError);
    QVERIFY(result.value()[2].classId == requested[2]);
    QVERIFY(result.value()[2].studentCount);
    QCOMPARE(result.value()[2].studentCount.value().rawValue(), 0);
    QVERIFY(result.value()[3].classId == requested[3]);
    QVERIFY(result.value()[3].studentCount);
    QCOMPARE(
        result.value()[3].studentCount.value().rawValue(),
        std::numeric_limits<int>::max()
        );
}

void NextApplicationMyClassesStudentCountBatchReadQueryTests::
rejectsInvalidAndDuplicateClassIdsBeforePortRead()
{
    FakeMyClassesStudentCountBatchReadPort port;
    const Application::MyClassesStudentCountBatchReadQuery query(port);
    const std::vector<std::string> invalidIds{
        "0", "07", "-1", "+4", " 4", "4 ", "42x", "2147483648"
    };

    for (const std::string& value : invalidIds)
    {
        const auto result = query.execute({classId(value)});
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
        QCOMPARE(
            result.error().message,
            std::string(
                "My Classes class IDs must be canonical positive integers."
                )
            );
        QVERIFY(!result.error().recoverable);
    }
    QCOMPARE(port.callCount, 0);

    const auto duplicate = query.execute({classId("4"), classId("4")});
    QVERIFY(!duplicate);
    QCOMPARE(duplicate.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(
        duplicate.error().message,
        std::string("My Classes class IDs must be unique.")
        );
    QVERIFY(!duplicate.error().recoverable);
    QCOMPARE(port.callCount, 0);
}

void NextApplicationMyClassesStudentCountBatchReadQueryTests::
rejectsEntryCountOrderAndNegativeCount()
{
    FakeMyClassesStudentCountBatchReadPort port;
    const std::vector<Domain::ClassId> requested{
        classId("4"), classId("9")
    };
    const Application::MyClassesStudentCountBatchReadQuery query(port);

    port.result = Application::MyClassesStudentCountBatchReadResult::success({
        successfulEntry(requested.front(), 2)
    });
    const auto shortResult = query.execute(requested);
    QVERIFY(!shortResult);
    QCOMPARE(shortResult.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(
        shortResult.error().message,
        std::string(
            "A My Classes student-count batch returned a different number of entries."
            )
        );
    QVERIFY(!shortResult.error().recoverable);

    port.result = Application::MyClassesStudentCountBatchReadResult::success({
        successfulEntry(requested.back(), 2),
        successfulEntry(requested.front(), 1)
    });
    const auto reorderedResult = query.execute(requested);
    QVERIFY(!reorderedResult);
    QCOMPARE(reorderedResult.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(
        reorderedResult.error().message,
        std::string(
            "A My Classes student-count batch returned entries in a different identifier order."
            )
        );
    QVERIFY(!reorderedResult.error().recoverable);

    port.result = Application::MyClassesStudentCountBatchReadResult::success({
        successfulEntry(requested.front(), -1),
        successfulEntry(requested.back(), 1)
    });
    const auto negativeResult = query.execute(requested);
    QVERIFY(!negativeResult);
    QCOMPARE(negativeResult.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(
        negativeResult.error().message,
        std::string("A My Classes student-count batch returned a negative count.")
        );
    QVERIFY(!negativeResult.error().recoverable);
}

void NextApplicationMyClassesStudentCountBatchReadQueryTests::
propagatesBatchPortFailure()
{
    FakeMyClassesStudentCountBatchReadPort port;
    const Domain::OperationError expected{
        .code = Domain::ErrorCode::NotFound,
        .message = "The active roster database session is unavailable.",
        .recoverable = true
    };
    port.result =
        Application::MyClassesStudentCountBatchReadResult::failure(expected);

    const Application::MyClassesStudentCountBatchReadQuery query(port);
    const auto result = query.execute({classId("7")});

    QVERIFY(!result);
    QVERIFY(result.error() == expected);
    QCOMPARE(port.callCount, 1);
}

QTEST_APPLESS_MAIN(NextApplicationMyClassesStudentCountBatchReadQueryTests)

#include "next_application_my_classes_student_count_batch_read_query_tests.moc"
