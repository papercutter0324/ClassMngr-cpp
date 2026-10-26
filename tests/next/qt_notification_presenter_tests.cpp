#include "next/ui/qt_notification_presenter.h"

#include <QApplication>
#include <QMainWindow>
#include <QStatusBar>
#include <QTest>
#include <QWidget>
#include <QtTest/QtTest>

#include <chrono>
#include <memory>
#include <thread>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Ui;
using namespace std::chrono_literals;

class QtNotificationPresenterTests final : public QObject
{
    Q_OBJECT

private slots:
    void severityIsPresentedAndMessageExpires();
    void replacementRestartsTheTransientTimeout();
    void clearAndPresenterDestructionHideTheStatusBar();
    void invalidRequestsAreRejectedWithoutChangingPresentation();
    void presenterReportsWrongThreadAsTechnicalFailure();
    void presenterHandlesStatusBarDestruction();
};

void QtNotificationPresenterTests::severityIsPresentedAndMessageExpires()
{
    QStatusBar statusBar;
    QtNotificationPresenter presenter(statusBar, 60ms);

    QVERIFY(statusBar.isHidden());
    const auto result = presenter.show(NotificationRequest{
        .message = "Check the updated record.",
        .severity = NotificationSeverity::Warning
    });

    QVERIFY(result.hasValue());
    QVERIFY(statusBar.isVisible());
    QCOMPARE(statusBar.currentMessage(), QStringLiteral("Warning: Check the updated record."));
    QCOMPARE(
        statusBar.property("classMngrNextNotificationSeverity").toInt(),
        static_cast<int>(NotificationSeverity::Warning)
        );

    QTRY_VERIFY_WITH_TIMEOUT(statusBar.isHidden(), 1000);
    QVERIFY(statusBar.currentMessage().isEmpty());
}

void QtNotificationPresenterTests::replacementRestartsTheTransientTimeout()
{
    QStatusBar statusBar;
    QtNotificationPresenter presenter(statusBar, 120ms);

    QVERIFY(presenter.show(NotificationRequest{
        .message = "First notice"
    }).hasValue());
    QTest::qWait(70);
    QVERIFY(presenter.show(NotificationRequest{
        .message = "Second notice",
        .severity = NotificationSeverity::Error
    }).hasValue());

    QTest::qWait(70);
    QVERIFY(!statusBar.isHidden());
    QCOMPARE(statusBar.currentMessage(), QStringLiteral("Error: Second notice"));
    QTRY_VERIFY_WITH_TIMEOUT(statusBar.isHidden(), 1000);
}

void QtNotificationPresenterTests::clearAndPresenterDestructionHideTheStatusBar()
{
    QStatusBar statusBar;
    {
        QtNotificationPresenter presenter(statusBar, 500ms);
        QVERIFY(presenter.show(NotificationRequest{
            .message = "Dismiss me"
        }).hasValue());
        QVERIFY(statusBar.isVisible());

        QVERIFY(presenter.clear().hasValue());
        QVERIFY(statusBar.isHidden());
        QVERIFY(statusBar.currentMessage().isEmpty());

        QVERIFY(presenter.show(NotificationRequest{
            .message = "Presenter cleanup"
        }).hasValue());
        QVERIFY(statusBar.isVisible());
    }

    QVERIFY(statusBar.isHidden());
    QVERIFY(statusBar.currentMessage().isEmpty());
}

void QtNotificationPresenterTests::invalidRequestsAreRejectedWithoutChangingPresentation()
{
    QStatusBar statusBar;
    QtNotificationPresenter presenter(statusBar, 100ms);

    const auto emptyText = presenter.show(NotificationRequest{});
    const auto whitespace = presenter.show(NotificationRequest{
        .message = " \t\n"
    });
    const auto invalidSeverity = presenter.show(NotificationRequest{
        .message = "Invalid severity",
        .severity = static_cast<NotificationSeverity>(99)
    });
    QtNotificationPresenter invalidTimeoutPresenter(statusBar, 0ms);
    const auto invalidTimeout = invalidTimeoutPresenter.show(NotificationRequest{
        .message = "Valid message"
    });

    QVERIFY(!emptyText.hasValue());
    QCOMPARE(emptyText.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(!whitespace.hasValue());
    QCOMPARE(whitespace.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(!invalidSeverity.hasValue());
    QCOMPARE(invalidSeverity.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(!invalidTimeout.hasValue());
    QCOMPARE(invalidTimeout.error().code, Domain::ErrorCode::Technical);
    QVERIFY(statusBar.isHidden());
    QVERIFY(statusBar.currentMessage().isEmpty());
}

void QtNotificationPresenterTests::presenterReportsWrongThreadAsTechnicalFailure()
{
    QStatusBar statusBar;
    QtNotificationPresenter presenter(statusBar, 100ms);
    bool showFailedOnForeignThread = false;
    bool clearFailedOnForeignThread = false;

    std::thread foreignThread([&] {
        const auto result = presenter.show(NotificationRequest{
            .message = "Wrong thread"
        });
        showFailedOnForeignThread = !result.hasValue()
            && result.error().code == Domain::ErrorCode::Technical;
        const auto clearResult = presenter.clear();
        clearFailedOnForeignThread = !clearResult.hasValue()
            && clearResult.error().code == Domain::ErrorCode::Technical;
    });
    foreignThread.join();

    QVERIFY(showFailedOnForeignThread);
    QVERIFY(clearFailedOnForeignThread);
    QVERIFY(statusBar.isHidden());
}

void QtNotificationPresenterTests::presenterHandlesStatusBarDestruction()
{
    auto window = std::make_unique<QMainWindow>();
    QStatusBar* const statusBar = window->statusBar();
    window->show();
    QApplication::processEvents();
    auto presenter = std::make_unique<QtNotificationPresenter>(*statusBar, 500ms);
    QVERIFY(presenter->show(NotificationRequest{
        .message = "Temporary"
    }).hasValue());
    QVERIFY(statusBar->isVisible());

    window.reset();
    const auto cleared = presenter->clear();
    QVERIFY(!cleared.hasValue());
    QCOMPARE(cleared.error().code, Domain::ErrorCode::Technical);
    presenter.reset();
}

QTEST_MAIN(QtNotificationPresenterTests)
#include "qt_notification_presenter_tests.moc"
