#include "ui/shared/pages/basepage.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QtTest>

class TestPage : public BasePage
{
public:
    QWidget* addContentWidget()
    {
        auto* widget = new QWidget(this);
        widget->setFixedSize(180, 80);

        contentLayout()->addWidget(widget);

        return widget;
    }

    QWidget* addBottomWidget()
    {
        auto* widget = new QWidget(this);
        widget->setFixedSize(120, 24);

        bottomLayout()->addWidget(widget);

        return widget;
    }
};

class BasePageTests : public QObject
{
    Q_OBJECT

private slots:
    void noDatabaseBannerVisibilityAndActions();
    void noDatabaseBannerOffsetsExistingLayout();
};

void BasePageTests::noDatabaseBannerVisibilityAndActions()
{
    TestPage page;
    page.resize(800, 600);
    page.show();

    QCoreApplication::processEvents();

    auto* banner =
        page.findChild<QFrame*>(
            QStringLiteral("noDatabaseBanner")
            );

    auto* openButton =
        page.findChild<QPushButton*>(
            QStringLiteral("noDatabaseOpenButton")
            );

    auto* newButton =
        page.findChild<QPushButton*>(
            QStringLiteral("noDatabaseNewButton")
            );

    QVERIFY(banner);
    QVERIFY(openButton);
    QVERIFY(newButton);
    QVERIFY(!banner->isVisible());

    page.hide();

    page.setDatabaseOpen(false);
    page.show();
    QCoreApplication::processEvents();

    QVERIFY(banner->isVisible());

    QSignalSpy openSpy(
        &page,
        &BasePage::openDatabaseRequested
        );

    QSignalSpy newSpy(
        &page,
        &BasePage::newDatabaseRequested
        );

    QTest::mouseClick(
        openButton,
        Qt::LeftButton
        );

    QTest::mouseClick(
        newButton,
        Qt::LeftButton
        );

    QCOMPARE(openSpy.count(), 1);
    QCOMPARE(newSpy.count(), 1);

    page.setDatabaseOpen(true);
    QCoreApplication::processEvents();

    QVERIFY(!banner->isVisible());
}

void BasePageTests::noDatabaseBannerOffsetsExistingLayout()
{
    TestPage page;
    QWidget* contentWidget = page.addContentWidget();
    QWidget* bottomWidget = page.addBottomWidget();

    page.resize(800, 600);
    page.show();

    QCoreApplication::processEvents();

    const QRect contentGeometry =
        contentWidget->geometry();

    const QRect bottomGeometry =
        bottomWidget->geometry();

    auto* banner =
        page.findChild<QFrame*>(
            QStringLiteral("noDatabaseBanner")
            );

    QVERIFY(banner);

    page.hide();

    page.setDatabaseOpen(false);
    page.show();
    QCoreApplication::processEvents();

    QCOMPARE(
        contentWidget->geometry().top(),
        contentGeometry.top()
            + banner->height()
            + 8
        );

    QCOMPARE(
        bottomWidget->geometry(),
        bottomGeometry
        );

    page.setDatabaseOpen(true);
    QCoreApplication::processEvents();

    QCOMPARE(
        contentWidget->geometry(),
        contentGeometry
        );
}

QTEST_MAIN(BasePageTests)

#include "basepage_tests.moc"
