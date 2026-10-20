#include "next/application/class_transfer_apply_use_case.h"

#include <QtTest/QtTest>

#include <limits>
#include <type_traits>
#include <utility>

using namespace ClassMngr::Next;

namespace
{
class RecordingPort final : public Application::ClassTransferApplyPort
{
public:
    [[nodiscard]] Application::ClassTransferApplyResult apply(
        Application::ClassTransferApplyCommand command
        ) const override
    {
        ++calls;
        received = std::move(command);
        return result;
    }

    mutable int calls = 0;
    mutable Application::ClassTransferApplyCommand received;
    Application::ClassTransferApplyResult result =
        Application::ClassTransferApplyResult::success({});
};

Application::ClassTransferApplyCommand command()
{
    Application::ClassTransferPortablePackage package;
    Application::ClassExportClass transferClass;
    transferClass.key = Application::ClassExportClassKey{u"class-1"};
    transferClass.name = u"Incoming Class";
    package.classes.push_back(std::move(transferClass));

    Application::ClassTransferApplyRequest choices;
    choices.classes.push_back({
        0,
        Application::ClassTransferReviewClassAction::Create,
        std::nullopt
    });
    return {.package = std::move(package), .choices = std::move(choices)};
}
}

class NextApplicationClassTransferApplyUseCaseTests final : public QObject
{
    Q_OBJECT

private slots:
    void skippedClassCountPreservesSignedIntAndExplicitType();
    void forwardsPackageChoicesAndSummary();
    void preservesPortFailure();
};

void NextApplicationClassTransferApplyUseCaseTests::
skippedClassCountPreservesSignedIntAndExplicitType()
{
    using Count = Application::ClassTransferSkippedClassCount;
    static_assert(std::is_default_constructible_v<Count>);
    static_assert(std::is_constructible_v<Count, int>);
    static_assert(!std::is_convertible_v<int, Count>);
    static_assert(!std::is_convertible_v<Count, int>);

    const Count defaultCount;
    QCOMPARE(defaultCount.rawValue(), 0);

    constexpr int minimum = std::numeric_limits<int>::min();
    constexpr int maximum = std::numeric_limits<int>::max();
    const Count minimumCount{minimum};
    const Count maximumCount{maximum};
    QCOMPARE(minimumCount.rawValue(), minimum);
    QCOMPARE(maximumCount.rawValue(), maximum);
    QVERIFY(minimumCount == Count{minimum});
    QVERIFY(maximumCount == Count{maximum});
    QVERIFY(minimumCount != maximumCount);
}

void NextApplicationClassTransferApplyUseCaseTests::
forwardsPackageChoicesAndSummary()
{
    RecordingPort port;
    port.result = Application::ClassTransferApplyResult::success({
        .createdClassIds = {*Domain::ClassId::fromString("14")},
        .replacedClassIds = {*Domain::ClassId::fromString("22")},
        .skippedClassCount = Application::ClassTransferSkippedClassCount{3}
    });
    auto request = command();

    const auto result = Application::ClassTransferApplyUseCase::execute(
        std::move(request),
        port
        );

    QVERIFY(result);
    QCOMPARE(port.calls, 1);
    QCOMPARE(port.received.package.classes.size(), std::size_t(1));
    QCOMPARE(port.received.package.classes.front().key.value(),
        std::u16string(u"class-1"));
    QCOMPARE(port.received.choices.classes.size(), std::size_t(1));
    QCOMPARE(port.received.choices.classes.front().packageClassIndex, 0);
    QCOMPARE(port.received.choices.classes.front().action,
        Application::ClassTransferReviewClassAction::Create);
    QCOMPARE(result.value().createdClassIds.size(), std::size_t(1));
    QCOMPARE(result.value().createdClassIds.front().value(), std::string("14"));
    QCOMPARE(result.value().replacedClassIds.front().value(), std::string("22"));
    QCOMPARE(result.value().skippedClassCount.rawValue(), 3);
    QVERIFY(result.value().skippedClassCount
        == Application::ClassTransferSkippedClassCount{3});
}

void NextApplicationClassTransferApplyUseCaseTests::preservesPortFailure()
{
    RecordingPort port;
    port.result = Application::ClassTransferApplyResult::failure({
        .code = Domain::ErrorCode::Conflict,
        .message = "stale transfer choice",
        .recoverable = true
    });

    const auto result = Application::ClassTransferApplyUseCase::execute(
        command(),
        port
        );

    QVERIFY(!result);
    QCOMPARE(port.calls, 1);
    QCOMPARE(result.error().code, Domain::ErrorCode::Conflict);
    QCOMPARE(result.error().message, std::string("stale transfer choice"));
    QVERIFY(result.error().recoverable);
}

QTEST_APPLESS_MAIN(NextApplicationClassTransferApplyUseCaseTests)

#include "next_application_class_transfer_apply_use_case_tests.moc"
