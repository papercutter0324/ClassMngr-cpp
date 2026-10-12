#include "next/application/my_classes_class_summary_batch_read_query.h"

#include <QtTest/QtTest>

#include <cstddef>
#include <optional>
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

Domain::TeacherId teacherId(std::string value)
{
    return *Domain::TeacherId::fromString(std::move(value));
}

MyClassesClassSummaryBatchReadEntry entry(
    std::string id,
    std::optional<Domain::TeacherId> teacher = std::nullopt
    )
{
    return {
        .classId = classId(std::move(id)),
        .summary = Domain::Result<MyClassesClassSummaryFields>::success({
            .classGrade = u"Grade 4",
            .classLevel = u"Level B",
            .teacherId = std::move(teacher)
        })
    };
}

class RecordingPort final : public MyClassesClassSummaryBatchReadPort
{
public:
    [[nodiscard]] MyClassesClassSummaryBatchReadResult
    readMyClassesClassSummaries(
        const std::vector<Domain::ClassId>& classIds
        ) const override
    {
        ++callCount;
        requestedClassIds = classIds;
        return result;
    }

    mutable int callCount = 0;
    mutable std::vector<Domain::ClassId> requestedClassIds;
    MyClassesClassSummaryBatchReadResult result =
        MyClassesClassSummaryBatchReadResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unconfigured class-summary batch read port.",
            .recoverable = false
        });
};

void verifyFailure(
    const MyClassesClassSummaryBatchReadResult& result,
    const Domain::ErrorCode code,
    const std::string& message,
    const bool recoverable
    )
{
    QVERIFY(!result);
    QCOMPARE(result.error().code, code);
    QCOMPARE(result.error().message, message);
    QCOMPARE(result.error().recoverable, recoverable);
}

}

class NextApplicationMyClassesClassSummaryBatchReadQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void emptyRequestSucceedsWithoutReading();
    void rejectsInvalidClassIdsBeforeReading();
    void rejectsDuplicateClassIdsBeforeReading();
    void forwardsMaximumClassIdAndPreservesEntryOrder();
    void rejectsWrongResponseCount();
    void rejectsWrongResponseOrder();
    void rejectsInvalidReturnedTeacherIds();
    void propagatesPortFailureExactly();
};

void NextApplicationMyClassesClassSummaryBatchReadQueryTests::
emptyRequestSucceedsWithoutReading()
{
    RecordingPort port;
    const MyClassesClassSummaryBatchReadQuery query(port);

    const auto result = query.execute({});

    QVERIFY(result);
    QVERIFY(result.value().empty());
    QCOMPARE(port.callCount, 0);
    QVERIFY(port.requestedClassIds.empty());
}

void NextApplicationMyClassesClassSummaryBatchReadQueryTests::
rejectsInvalidClassIdsBeforeReading()
{
    const std::vector<std::string> invalidIds{
        "0", "042", "42x", "-1", "2147483648"
    };
    for (const std::string& rawId : invalidIds)
    {
        RecordingPort port;
        const MyClassesClassSummaryBatchReadQuery query(port);
        const auto result = query.execute({classId(rawId)});

        verifyFailure(
            result,
            Domain::ErrorCode::InvalidInput,
            "My Classes class IDs must be canonical positive integers.",
            false
            );
        QCOMPARE(port.callCount, 0);
        QVERIFY(port.requestedClassIds.empty());
    }
}

void NextApplicationMyClassesClassSummaryBatchReadQueryTests::
rejectsDuplicateClassIdsBeforeReading()
{
    RecordingPort port;
    const MyClassesClassSummaryBatchReadQuery query(port);

    const auto result = query.execute({classId("42"), classId("42")});

    verifyFailure(
        result,
        Domain::ErrorCode::InvalidInput,
        "My Classes class IDs must be unique.",
        false
        );
    QCOMPARE(port.callCount, 0);
    QVERIFY(port.requestedClassIds.empty());
}

void NextApplicationMyClassesClassSummaryBatchReadQueryTests::
forwardsMaximumClassIdAndPreservesEntryOrder()
{
    RecordingPort port;
    port.result = MyClassesClassSummaryBatchReadResult::success({
        entry("2147483647", teacherId("2147483647")),
        entry("7")
    });
    const MyClassesClassSummaryBatchReadQuery query(port);
    const std::vector<Domain::ClassId> requestedIds{
        classId("2147483647"), classId("7")
    };

    const auto result = query.execute(requestedIds);

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.requestedClassIds == requestedIds);
    QCOMPARE(result.value().size(), std::size_t(2));
    QCOMPARE(result.value()[0].classId.value(), std::string("2147483647"));
    QCOMPARE(result.value()[1].classId.value(), std::string("7"));
    QVERIFY(result.value()[0].summary);
    QVERIFY(result.value()[0].summary.value().teacherId.has_value());
    QCOMPARE(
        result.value()[0].summary.value().teacherId->value(),
        std::string("2147483647")
        );
}

void NextApplicationMyClassesClassSummaryBatchReadQueryTests::
rejectsWrongResponseCount()
{
    RecordingPort port;
    port.result = MyClassesClassSummaryBatchReadResult::success({
        entry("42")
    });
    const MyClassesClassSummaryBatchReadQuery query(port);

    const auto result = query.execute({classId("42"), classId("7")});

    verifyFailure(
        result,
        Domain::ErrorCode::Validation,
        "A My Classes class-summary batch returned a different number of entries.",
        false
        );
    QCOMPARE(port.callCount, 1);
}

void NextApplicationMyClassesClassSummaryBatchReadQueryTests::
rejectsWrongResponseOrder()
{
    RecordingPort port;
    port.result = MyClassesClassSummaryBatchReadResult::success({
        entry("7"),
        entry("42")
    });
    const MyClassesClassSummaryBatchReadQuery query(port);

    const auto result = query.execute({classId("42"), classId("7")});

    verifyFailure(
        result,
        Domain::ErrorCode::Validation,
        "A My Classes class-summary batch returned entries in a different identifier order.",
        false
        );
    QCOMPARE(port.callCount, 1);
}

void NextApplicationMyClassesClassSummaryBatchReadQueryTests::
rejectsInvalidReturnedTeacherIds()
{
    const std::vector<std::string> invalidTeacherIds{
        "0", "042", "42x", "-1", "2147483648"
    };
    for (const std::string& rawId : invalidTeacherIds)
    {
        RecordingPort port;
        port.result = MyClassesClassSummaryBatchReadResult::success({
            entry("42", teacherId(rawId))
        });
        const MyClassesClassSummaryBatchReadQuery query(port);

        const auto result = query.execute({classId("42")});

        verifyFailure(
            result,
            Domain::ErrorCode::Validation,
            "A My Classes class-summary batch returned an invalid teacher identifier.",
            false
            );
        QCOMPARE(port.callCount, 1);
    }
}

void NextApplicationMyClassesClassSummaryBatchReadQueryTests::
propagatesPortFailureExactly()
{
    RecordingPort port;
    const Domain::OperationError expected{
        .code = Domain::ErrorCode::Technical,
        .message = "class-summary batch read failed",
        .recoverable = true
    };
    port.result = MyClassesClassSummaryBatchReadResult::failure(expected);
    const MyClassesClassSummaryBatchReadQuery query(port);

    const auto result = query.execute({classId("42")});

    QVERIFY(!result);
    QVERIFY(result.error() == expected);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.requestedClassIds == std::vector<Domain::ClassId>{classId("42")});
}

QTEST_APPLESS_MAIN(NextApplicationMyClassesClassSummaryBatchReadQueryTests)

#include "next_application_my_classes_class_summary_batch_read_query_tests.moc"
