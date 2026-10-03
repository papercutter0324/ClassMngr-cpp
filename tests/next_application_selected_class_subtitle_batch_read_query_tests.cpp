#include "next/application/selected_class_subtitle_batch_read_query.h"

#include <QtTest/QtTest>

#include <string>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

Domain::ClassId classId(const std::string& value)
{
    return *Domain::ClassId::fromString(value);
}

Application::SelectedClassSubtitleReadSnapshot snapshot(
    const Domain::ClassId& id
    )
{
    return {
        id,
        Domain::Result<Application::SelectedClassSubtitleFields>::success({}),
        Domain::Result<std::optional<
            Application::SelectedClassSubtitleTeacherFields
            >>::success(std::nullopt)
    };
}

class FakeBatchReadPort final
    : public Application::SelectedClassSubtitleBatchReadPort
{
public:
    mutable int callCount = 0;
    mutable std::vector<Domain::ClassId> requestedIds;
    Application::SelectedClassSubtitleBatchReadResult result =
        Application::SelectedClassSubtitleBatchReadResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "not configured",
            .recoverable = false
        });

    [[nodiscard]] Application::SelectedClassSubtitleBatchReadResult
    readSelectedClassSubtitles(
        const std::vector<Domain::ClassId>& classIds
        ) const override
    {
        ++callCount;
        requestedIds = classIds;
        return result;
    }
};

}

class NextApplicationSelectedClassSubtitleBatchReadQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void emptyInputDoesNotCallPort();
    void nonemptyInputMakesOneOrderedBatchCall();
    void rejectsNoncanonicalAndDuplicateIdsBeforePortRead();
    void propagatesStructuredPortFailure();
    void rejectsCountAndIdentifierOrderMismatches();
};

void NextApplicationSelectedClassSubtitleBatchReadQueryTests::
emptyInputDoesNotCallPort()
{
    FakeBatchReadPort port;
    const Application::SelectedClassSubtitleBatchReadQuery query(port);
    const auto result = query.execute({});

    QVERIFY(result);
    QVERIFY(result.value().empty());
    QCOMPARE(port.callCount, 0);
    QVERIFY(port.requestedIds.empty());
}

void NextApplicationSelectedClassSubtitleBatchReadQueryTests::
nonemptyInputMakesOneOrderedBatchCall()
{
    FakeBatchReadPort port;
    const std::vector<Domain::ClassId> requested{
        classId("42"), classId("7"), classId("100")
    };
    port.result = Application::SelectedClassSubtitleBatchReadResult::success({
        snapshot(requested[0]),
        snapshot(requested[1]),
        snapshot(requested[2])
    });

    const Application::SelectedClassSubtitleBatchReadQuery query(port);
    const auto result = query.execute(requested);

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.requestedIds == requested);
    QCOMPARE(result.value().size(), requested.size());
    for (std::size_t index = 0; index < requested.size(); ++index)
    {
        QVERIFY(result.value()[index].classId == requested[index]);
    }
}

void NextApplicationSelectedClassSubtitleBatchReadQueryTests::
rejectsNoncanonicalAndDuplicateIdsBeforePortRead()
{
    FakeBatchReadPort port;
    const Application::SelectedClassSubtitleBatchReadQuery query(port);
    const std::vector<std::string> invalidIds{
        "0", "01", "-1", "+4", " 4", "2147483648"
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

void NextApplicationSelectedClassSubtitleBatchReadQueryTests::
propagatesStructuredPortFailure()
{
    FakeBatchReadPort port;
    const Domain::OperationError expected{
        .code = Domain::ErrorCode::NotFound,
        .message = "The active database session is unavailable.",
        .recoverable = true
    };
    port.result = Application::SelectedClassSubtitleBatchReadResult::failure(
        expected
        );

    const Application::SelectedClassSubtitleBatchReadQuery query(port);
    const auto result = query.execute({classId("42")});

    QVERIFY(!result);
    QVERIFY(result.error() == expected);
    QCOMPARE(port.callCount, 1);
}

void NextApplicationSelectedClassSubtitleBatchReadQueryTests::
rejectsCountAndIdentifierOrderMismatches()
{
    FakeBatchReadPort port;
    const std::vector<Domain::ClassId> requested{
        classId("4"), classId("9")
    };
    const Application::SelectedClassSubtitleBatchReadQuery query(port);

    port.result = Application::SelectedClassSubtitleBatchReadResult::success({
        snapshot(requested.front())
    });
    const auto shortResult = query.execute(requested);
    QVERIFY(!shortResult);
    QCOMPARE(shortResult.error().code, Domain::ErrorCode::Validation);

    port.result = Application::SelectedClassSubtitleBatchReadResult::success({
        snapshot(requested.front()),
        snapshot(requested.front()),
        snapshot(requested.back())
    });
    const auto extraResult = query.execute(requested);
    QVERIFY(!extraResult);
    QCOMPARE(extraResult.error().code, Domain::ErrorCode::Validation);

    port.result = Application::SelectedClassSubtitleBatchReadResult::success({
        snapshot(requested.back()),
        snapshot(requested.front())
    });
    const auto reorderedResult = query.execute(requested);
    QVERIFY(!reorderedResult);
    QCOMPARE(reorderedResult.error().code, Domain::ErrorCode::Validation);
}

QTEST_APPLESS_MAIN(NextApplicationSelectedClassSubtitleBatchReadQueryTests)

#include "next_application_selected_class_subtitle_batch_read_query_tests.moc"
