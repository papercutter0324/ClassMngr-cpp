#include "next/ui/qt_user_prompt_presenter.h"

#include <QApplication>
#include <QWidget>
#include <QFileInfo>
#include <QPointer>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <optional>
#include <thread>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Ui;

namespace
{

PromptAutomationId automationId(const char* value)
{
    const auto id = PromptAutomationId::create(value);
    if (!id)
    {
        qFatal("Invalid test prompt automation ID.");
    }
    return *id;
}

PromptActionId actionId(const char* value)
{
    const auto id = PromptActionId::create(value);
    if (!id)
    {
        qFatal("Invalid test prompt action ID.");
    }
    return *id;
}

const PromptActionSnapshot* actionWithText(
    const PromptSnapshot& prompt,
    const std::string& text
    )
{
    for (const PromptActionSnapshot& action : prompt.actions)
    {
        if (action.text == text)
        {
            return &action;
        }
    }
    return nullptr;
}

void closePromptIfCommandFailed(const bool commandSucceeded)
{
    if (commandSucceeded)
    {
        return;
    }
    QPointer<QWidget> modal = QApplication::activeModalWidget();
    if (!modal)
    {
        return;
    }
    QTimer::singleShot(
        500,
        [modal]()
        {
            if (modal)
            {
                modal->close();
            }
        }
        );
}

}

class QtUserPromptPresenterTests final : public QObject
{
    Q_OBJECT

private slots:
    void messageMapsValuesAndSupportsCaptureAndAcknowledge();
    void confirmationMapsTypedResponses_data();
    void confirmationMapsTypedResponses();
    void unsavedChangesMapsTypedChoices_data();
    void unsavedChangesMapsTypedChoices();
    void namedActionsMapStableIdsAndRoles_data();
    void namedActionsMapStableIdsAndRoles();
    void invalidNamedActionRequestsFailBeforePresentation();
    void automationIdsAreStableValues();
    void presenterReportsWrongThreadAsTechnicalFailure();
};

void QtUserPromptPresenterTests::messageMapsValuesAndSupportsCaptureAndAcknowledge()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QtUserPromptPresenter presenter;
    QtPromptAutomationAdapter automation;
    const PromptAutomationId id = automationId("next.prompt.message");
    const std::string capturePath = directory.filePath("message.png").toStdString();
    std::optional<PromptSnapshot> observed;
    bool captured = false;
    bool acknowledged = false;

    QTimer::singleShot(
        0,
        [&]()
        {
            observed = automation.activePrompt(id);
            captured = automation.capture(id, capturePath);
            acknowledged = automation.acknowledge(id);
            closePromptIfCommandFailed(acknowledged);
        }
        );

    const auto result = presenter.showMessage(MessagePromptRequest{
        .title = "Saved",
        .message = "The report was saved.",
        .details = "Report id: 42",
        .severity = PromptSeverity::Information,
        .acknowledgeText = "Close"
    });

    QVERIFY(result.hasValue());
    QVERIFY(observed);
    QCOMPARE(observed->kind, PromptKind::Message);
    QCOMPARE(observed->id, id);
    QCOMPARE(observed->title, std::string("Saved"));
    QCOMPARE(observed->text, std::string("The report was saved."));
    QCOMPARE(observed->details, std::string("Report id: 42"));
    QCOMPARE(observed->severity, PromptSeverity::Information);
    QVERIFY(observed->visible);
    const auto* closeAction = actionWithText(*observed, "Close");
    QVERIFY(closeAction);
    QVERIFY(closeAction->isDefault);
    QVERIFY(closeAction->isEscape);
    QVERIFY(captured);
    QVERIFY(QFileInfo::exists(QString::fromStdString(capturePath)));
    QVERIFY(acknowledged);
    QVERIFY(!automation.activePrompt(id));
}

void QtUserPromptPresenterTests::confirmationMapsTypedResponses_data()
{
    QTest::addColumn<int>("command");
    QTest::addColumn<int>("expected");
    QTest::addColumn<int>("defaultAction");
    QTest::addColumn<int>("escapeAction");
    QTest::addColumn<int>("buttonRole");

    QTest::newRow("accept")
        << 0 << static_cast<int>(ConfirmationResponse::Accepted)
        << static_cast<int>(ConfirmationAction::Accept)
        << static_cast<int>(ConfirmationAction::Decline)
        << static_cast<int>(PromptActionRole::Accept);
    QTest::newRow("decline-by-escape")
        << 1 << static_cast<int>(ConfirmationResponse::Declined)
        << static_cast<int>(ConfirmationAction::Accept)
        << static_cast<int>(ConfirmationAction::Decline)
        << static_cast<int>(PromptActionRole::Accept);
    QTest::newRow("dismiss")
        << 2 << static_cast<int>(ConfirmationResponse::Dismissed)
        << static_cast<int>(ConfirmationAction::Accept)
        << static_cast<int>(ConfirmationAction::Decline)
        << static_cast<int>(PromptActionRole::Accept);
    QTest::newRow("destructive-role-default-declines")
        << 3 << static_cast<int>(ConfirmationResponse::Declined)
        << static_cast<int>(ConfirmationAction::Decline)
        << static_cast<int>(ConfirmationAction::Decline)
        << static_cast<int>(PromptActionRole::Destructive);
    QTest::newRow("destructive-accept-is-still-accepted")
        << 4 << static_cast<int>(ConfirmationResponse::Accepted)
        << static_cast<int>(ConfirmationAction::Decline)
        << static_cast<int>(ConfirmationAction::Decline)
        << static_cast<int>(PromptActionRole::Destructive);
    QTest::newRow("accept-by-escape")
        << 5 << static_cast<int>(ConfirmationResponse::Accepted)
        << static_cast<int>(ConfirmationAction::Accept)
        << static_cast<int>(ConfirmationAction::Accept)
        << static_cast<int>(PromptActionRole::Accept);
}

void QtUserPromptPresenterTests::confirmationMapsTypedResponses()
{
    QFETCH(int, command);
    QFETCH(int, expected);
    QFETCH(int, defaultAction);
    QFETCH(int, escapeAction);
    QFETCH(int, buttonRole);

    QtUserPromptPresenter presenter;
    QtPromptAutomationAdapter automation;
    const PromptAutomationId id = automationId("next.prompt.confirmation");
    bool inspected = false;
    bool commanded = false;
    QTimer::singleShot(
        0,
        [&]()
        {
            const auto prompt = automation.activePrompt(id);
            const auto* accept = prompt
                ? actionWithText(*prompt, "Delete")
                : nullptr;
            const auto* decline = prompt
                ? actionWithText(*prompt, "Keep")
                : nullptr;
            inspected = prompt
                && prompt->kind == PromptKind::Confirmation
                && prompt->visible
                && accept
                && decline
                && static_cast<int>(accept->role) == buttonRole
                && accept->isDefault
                    == (defaultAction == static_cast<int>(ConfirmationAction::Accept))
                && decline->isDefault
                    == (defaultAction == static_cast<int>(ConfirmationAction::Decline))
                && accept->isEscape
                    == (escapeAction == static_cast<int>(ConfirmationAction::Accept))
                && decline->isEscape
                    == (escapeAction == static_cast<int>(ConfirmationAction::Decline));

            switch (command)
            {
                case 0:
                    commanded = automation.chooseConfirmation(
                        id,
                        ConfirmationAction::Accept
                        );
                    break;
                case 1:
                    commanded = automation.reject(id);
                    break;
                case 2:
                    commanded = automation.dismiss(id);
                    break;
                case 3:
                    commanded = automation.activateDefault(id);
                    break;
                case 4:
                    commanded = automation.chooseConfirmation(
                        id,
                        ConfirmationAction::Accept
                        );
                    break;
                case 5:
                    commanded = automation.reject(id);
                    break;
            }
            closePromptIfCommandFailed(commanded);
        }
        );

    ConfirmationPromptRequest request{
        .title = "Delete report",
        .message = "Delete this report?",
        .severity = PromptSeverity::Warning,
        .acceptText = "Delete",
        .declineText = "Keep"
    };
    if (command == 3 || command == 4)
    {
        request.acceptButtonRole = ConfirmationButtonRole::Destructive;
        request.defaultAction = ConfirmationAction::Decline;
    }
    if (command == 5)
    {
        request.escapeAction = ConfirmationAction::Accept;
    }

    const auto result = presenter.confirm(request);
    QVERIFY(result.hasValue());
    QVERIFY(inspected);
    QVERIFY(commanded);
    QCOMPARE(static_cast<int>(result.value()), expected);
}

void QtUserPromptPresenterTests::unsavedChangesMapsTypedChoices_data()
{
    QTest::addColumn<int>("command");
    QTest::addColumn<int>("expected");
    QTest::addColumn<int>("escapeAction");

    QTest::newRow("save")
        << 0 << static_cast<int>(UnsavedChangesAction::Save)
        << static_cast<int>(UnsavedChangesAction::Cancel);
    QTest::newRow("discard")
        << 1 << static_cast<int>(UnsavedChangesAction::Discard)
        << static_cast<int>(UnsavedChangesAction::Cancel);
    QTest::newRow("cancel-default")
        << 2 << static_cast<int>(UnsavedChangesAction::Cancel)
        << static_cast<int>(UnsavedChangesAction::Cancel);
    QTest::newRow("escape-action-is-explicit")
        << 3 << static_cast<int>(UnsavedChangesAction::Discard)
        << static_cast<int>(UnsavedChangesAction::Discard);
    QTest::newRow("close-is-cancel")
        << 4 << static_cast<int>(UnsavedChangesAction::Cancel)
        << static_cast<int>(UnsavedChangesAction::Cancel);
    QTest::newRow("default-action")
        << 5 << static_cast<int>(UnsavedChangesAction::Save)
        << static_cast<int>(UnsavedChangesAction::Cancel);
}

void QtUserPromptPresenterTests::unsavedChangesMapsTypedChoices()
{
    QFETCH(int, command);
    QFETCH(int, expected);
    QFETCH(int, escapeAction);

    QtUserPromptPresenter presenter;
    QtPromptAutomationAdapter automation;
    const PromptAutomationId id = automationId("next.prompt.unsaved-changes");
    bool inspected = false;
    bool commanded = false;
    QTimer::singleShot(
        0,
        [&]()
        {
            const auto prompt = automation.activePrompt(id);
            const auto* save = prompt ? actionWithText(*prompt, "Save") : nullptr;
            const auto* discard = prompt
                ? actionWithText(*prompt, "Discard Changes")
                : nullptr;
            const auto* cancel = prompt
                ? actionWithText(*prompt, "Cancel")
                : nullptr;
            inspected = prompt
                && prompt->kind == PromptKind::UnsavedChanges
                && prompt->severity == PromptSeverity::Warning
                && save
                && discard
                && cancel
                && discard->role == PromptActionRole::Destructive
                && save->isDefault == (command == 5)
                && cancel->isDefault == (command != 5)
                && (escapeAction == static_cast<int>(UnsavedChangesAction::Save)
                    ? save->isEscape
                    : escapeAction == static_cast<int>(UnsavedChangesAction::Discard)
                        ? discard->isEscape
                        : cancel->isEscape);

            switch (command)
            {
                case 0:
                    commanded = automation.chooseUnsavedChanges(
                        id,
                        UnsavedChangesAction::Save
                        );
                    break;
                case 1:
                    commanded = automation.chooseUnsavedChanges(
                        id,
                        UnsavedChangesAction::Discard
                        );
                    break;
                case 2:
                    commanded = automation.reject(id);
                    break;
                case 3:
                    commanded = automation.reject(id);
                    break;
                case 4:
                    commanded = automation.dismiss(id);
                    break;
                case 5:
                    commanded = automation.activateDefault(id);
                    break;
            }
            closePromptIfCommandFailed(commanded);
        }
        );

    UnsavedChangesPromptRequest request{
        .title = "Unsaved changes",
        .message = "This page has unsaved changes.",
        .question = "Save before leaving?"
    };
    if (command == 3)
    {
        request.escapeAction = UnsavedChangesAction::Discard;
    }
    if (command == 5)
    {
        request.defaultAction = UnsavedChangesAction::Save;
    }

    const auto result = presenter.confirmUnsavedChanges(request);
    QVERIFY(result.hasValue());
    QVERIFY(inspected);
    QVERIFY(commanded);
    QCOMPARE(static_cast<int>(result.value()), expected);
}

void QtUserPromptPresenterTests::namedActionsMapStableIdsAndRoles_data()
{
    QTest::addColumn<int>("command");
    QTest::addColumn<QString>("expected");

    QTest::newRow("named-action") << 0 << QStringLiteral("open");
    QTest::newRow("default-action") << 1 << QStringLiteral("delete");
    QTest::newRow("escape-action") << 2 << QStringLiteral("cancel");
    QTest::newRow("dismissed") << 3 << QString();
}

void QtUserPromptPresenterTests::namedActionsMapStableIdsAndRoles()
{
    QFETCH(int, command);
    QFETCH(QString, expected);

    const PromptActionId apply = actionId("apply");
    const PromptActionId open = actionId("open");
    const PromptActionId remove = actionId("delete");
    const PromptActionId cancel = actionId("cancel");
    const PromptAutomationId id = automationId("next.prompt.named-action");
    QtUserPromptPresenter presenter;
    QtPromptAutomationAdapter automation;
    bool inspected = false;
    bool commanded = false;
    QTimer::singleShot(
        0,
        [&]()
        {
            const auto prompt = automation.activePrompt(id);
            const auto* applyAction = prompt
                ? actionWithText(*prompt, "Apply")
                : nullptr;
            const auto* deleteAction = prompt
                ? actionWithText(*prompt, "Delete")
                : nullptr;
            const auto* openAction = prompt
                ? actionWithText(*prompt, "Open")
                : nullptr;
            const auto* cancelAction = prompt
                ? actionWithText(*prompt, "Cancel")
                : nullptr;
            inspected = prompt
                && prompt->kind == PromptKind::NamedAction
                && prompt->id == id
                && prompt->visible
                && applyAction
                && deleteAction
                && openAction
                && cancelAction
                && !applyAction->enabled
                && deleteAction->role == PromptActionRole::Destructive
                && deleteAction->id == remove
                && openAction->id == open
                && cancelAction->id == cancel
                && deleteAction->isDefault
                && cancelAction->isEscape;

            switch (command)
            {
                case 0:
                    commanded = !automation.chooseAction(id, apply)
                        && automation.chooseAction(id, open);
                    break;
                case 1:
                    commanded = automation.activateDefault(id);
                    break;
                case 2:
                    commanded = automation.reject(id);
                    break;
                case 3:
                    commanded = automation.dismiss(id);
                    break;
            }
            closePromptIfCommandFailed(commanded);
        }
        );

    const auto result = presenter.chooseAction(NamedActionPromptRequest{
        .title = "Choose an action",
        .message = "Choose one action.",
        .informativeText = "Review the available actions.",
        .severity = PromptSeverity::Warning,
        .actions = {
            PromptAction{
                .id = apply,
                .text = "Apply",
                .role = PromptActionRole::Accept,
                .enabled = false
            },
            PromptAction{
                .id = remove,
                .text = "Delete",
                .role = PromptActionRole::Destructive
            },
            PromptAction{
                .id = open,
                .text = "Open",
                .role = PromptActionRole::Action
            },
            PromptAction{
                .id = cancel,
                .text = "Cancel",
                .role = PromptActionRole::Reject
            }
        },
        .defaultActionId = remove,
        .escapeActionId = cancel
    });

    QVERIFY(result.hasValue());
    QVERIFY(inspected);
    QVERIFY(commanded);
    if (expected.isEmpty())
    {
        QVERIFY(!result.value());
    }
    else
    {
        QVERIFY(result.value());
        QCOMPARE(QString::fromStdString(result.value()->value()), expected);
    }
}

void QtUserPromptPresenterTests::invalidNamedActionRequestsFailBeforePresentation()
{
    QtUserPromptPresenter presenter;
    const PromptActionId available = actionId("available");
    const PromptActionId missing = actionId("missing");

    const auto empty = presenter.chooseAction(NamedActionPromptRequest{});
    QVERIFY(!empty.hasValue());
    QCOMPARE(empty.error().code, Domain::ErrorCode::InvalidInput);

    const auto unavailableDefault = presenter.chooseAction(
        NamedActionPromptRequest{
            .title = "Invalid",
            .message = "No prompt should appear.",
            .actions = {
                PromptAction{
                    .id = available,
                    .text = "Available"
                }
            },
            .defaultActionId = missing
        }
        );
    QVERIFY(!unavailableDefault.hasValue());
    QCOMPARE(unavailableDefault.error().code, Domain::ErrorCode::InvalidInput);

    const auto duplicateIds = presenter.chooseAction(
        NamedActionPromptRequest{
            .title = "Invalid",
            .message = "Duplicate IDs should fail.",
            .actions = {
                PromptAction{
                    .id = available,
                    .text = "First"
                },
                PromptAction{
                    .id = available,
                    .text = "Second"
                }
            }
        }
        );
    QVERIFY(!duplicateIds.hasValue());
    QCOMPARE(duplicateIds.error().code, Domain::ErrorCode::InvalidInput);
}

void QtUserPromptPresenterTests::automationIdsAreStableValues()
{
    const auto id = PromptAutomationId::create("feature.delete.confirmation");
    QVERIFY(id);
    QCOMPARE(id->value(), std::string("feature.delete.confirmation"));
    QVERIFY(!PromptAutomationId::create("Feature delete"));
}

void QtUserPromptPresenterTests::presenterReportsWrongThreadAsTechnicalFailure()
{
    QtUserPromptPresenter presenter;
    std::optional<Domain::Result<ConfirmationResponse>> result;
    std::thread worker(
        [&]()
        {
            result = presenter.confirm(ConfirmationPromptRequest{});
        }
        );
    worker.join();

    QVERIFY(result);
    QVERIFY(!result->hasValue());
    QCOMPARE(result->error().code, Domain::ErrorCode::Technical);
}

QTEST_MAIN(QtUserPromptPresenterTests)

#include "qt_user_prompt_presenter_tests.moc"
