#include "next/application/class_transfer_apply_use_case.h"

#include <QtTest/QtTest>

using namespace ClassMngr::Next;

namespace
{
class RecordingPort final : public Application::ClassTransferApplyPort
{
public:
    [[nodiscard]] Application::ClassTransferApplyResult apply(
        const Application::ClassTransferApplyCommand& command
        ) const override
    {
        ++calls;
        received = command;
        return result;
    }

    mutable int calls = 0;
    mutable Application::ClassTransferApplyCommand received;
    Application::ClassTransferApplyResult result =
        Application::ClassTransferApplyResult::success({});
};

Application::ClassTransferApplyCommand command()
{
    ::ClassTransferPackage package;
    ClassTransferClass transferClass;
    transferClass.key = QStringLiteral("class-1");
    transferClass.name = QStringLiteral("Incoming Class");
    package.classes.append(transferClass);

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
    void forwardsPackageChoicesAndSummary();
    void preservesPortFailure();
};

void NextApplicationClassTransferApplyUseCaseTests::
forwardsPackageChoicesAndSummary()
{
    RecordingPort port;
    port.result = Application::ClassTransferApplyResult::success({
        .createdClassIds = {*Domain::ClassId::fromString("14")},
        .replacedClassIds = {*Domain::ClassId::fromString("22")},
        .skippedClassCount = 3
    });
    const auto request = command();

    const auto result = Application::ClassTransferApplyUseCase::execute(
        request,
        port
        );

    QVERIFY(result);
    QCOMPARE(port.calls, 1);
    QCOMPARE(port.received.package.classes.size(), 1);
    QCOMPARE(port.received.package.classes.first().key,
        QStringLiteral("class-1"));
    QCOMPARE(port.received.choices.classes.size(), std::size_t(1));
    QCOMPARE(port.received.choices.classes.front().packageClassIndex, 0);
    QCOMPARE(port.received.choices.classes.front().action,
        Application::ClassTransferReviewClassAction::Create);
    QCOMPARE(result.value().createdClassIds.size(), std::size_t(1));
    QCOMPARE(result.value().createdClassIds.front().value(), std::string("14"));
    QCOMPARE(result.value().replacedClassIds.front().value(), std::string("22"));
    QCOMPARE(result.value().skippedClassCount, 3);
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
