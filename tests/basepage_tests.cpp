#include "ui/shared/pages/basepage.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTranslator>
#include <QVBoxLayout>
#include <QWidget>
#include <QtTest>

class TestPage : public BasePage
{
public:
    void clearDatabaseState() override
    {
        ++databaseClearCount;
    }

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

    int databaseClearCount = 0;
};

class NoDatabaseBannerTranslator : public QTranslator
{
public:
    QString translate(
        const char* context,
        const char* sourceText,
        const char* disambiguation = nullptr,
        int n = -1
        ) const override
    {
        Q_UNUSED(disambiguation);
        Q_UNUSED(n);

        if (qstrcmp(context, "BasePage") != 0)
        {
            return {};
        }

        if (qstrcmp(sourceText, "Getting Started") == 0)
        {
            return QStringLiteral("Translated title");
        }

        if (qstrcmp(
                sourceText,
                "No Teacher Profile is open. Set up ClassMngr in this order:"
                ) == 0)
        {
            return QStringLiteral("Translated introduction");
        }

        if (qstrcmp(
                sourceText,
                "1. Create a new Teacher Profile, or open an existing one."
                ) == 0)
        {
            return QStringLiteral("Translated first step");
        }

        if (qstrcmp(
                sourceText,
                "2. Create or import your Korean teachers."
                ) == 0)
        {
            return QStringLiteral("Translated second step");
        }

        if (qstrcmp(
                sourceText,
                "3. Create your classes and assign their teachers."
                ) == 0)
        {
            return QStringLiteral("Translated third step");
        }

        if (qstrcmp(
                sourceText,
                "Next, add schedules and rosters, then fill in any other information you need."
                ) == 0)
        {
            return QStringLiteral("Translated next steps");
        }

        if (qstrcmp(sourceText, "Open Teacher Profile...") == 0)
        {
            return QStringLiteral("Translated open Teacher Profile");
        }

        if (qstrcmp(sourceText, "New Teacher Profile...") == 0)
        {
            return QStringLiteral("Translated new Teacher Profile");
        }

        if (qstrcmp(sourceText, "Initial Setup...") == 0)
        {
            return QStringLiteral("Translated initial setup");
        }

        return {};
    }
};

class BasePageTests : public QObject
{
    Q_OBJECT

private slots:
    void noDatabaseBannerVisibilityAndActions();
    void noDatabaseBannerOffsetsExistingLayout();
    void noDatabaseBannerFitsNarrowPage();
    void noDatabaseBannerRetranslatesOnLanguageChange();
    void databaseStateClearHookIsDispatched();
    void outputContractDefaultsToUnsupportedAndSignalsDatabaseChanges();
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

    auto* setupButton =
        page.findChild<QPushButton*>(
            QStringLiteral("noDatabaseSetupButton")
            );

    QVERIFY(banner);
    QVERIFY(openButton);
    QVERIFY(newButton);
    QVERIFY(setupButton);
    QVERIFY(!banner->isVisible());

    auto* title =
        page.findChild<QLabel*>(
            QStringLiteral("noDatabaseTitle")
            );

    auto* message =
        page.findChild<QLabel*>(
            QStringLiteral("noDatabaseMessage")
            );

    auto* stepOne =
        page.findChild<QLabel*>(
            QStringLiteral("noDatabaseStepOne")
            );

    auto* stepTwo =
        page.findChild<QLabel*>(
            QStringLiteral("noDatabaseStepTwo")
            );

    auto* stepThree =
        page.findChild<QLabel*>(
            QStringLiteral("noDatabaseStepThree")
            );

    auto* nextSteps =
        page.findChild<QLabel*>(
            QStringLiteral("noDatabaseNextSteps")
            );

    QVERIFY(title);
    QVERIFY(message);
    QVERIFY(stepOne);
    QVERIFY(stepTwo);
    QVERIFY(stepThree);
    QVERIFY(nextSteps);

    QCOMPARE(title->text(), QStringLiteral("Getting Started"));
    QCOMPARE(
        message->text(),
        QStringLiteral(
            "No Teacher Profile is open. Set up ClassMngr in this order:"
            )
        );
    QCOMPARE(
        stepOne->text(),
        QStringLiteral(
            "1. Create a new Teacher Profile, or open an existing one."
            )
        );
    QCOMPARE(
        stepTwo->text(),
        QStringLiteral(
            "2. Create or import your Korean teachers."
            )
        );
    QCOMPARE(
        stepThree->text(),
        QStringLiteral(
            "3. Create your classes and assign their teachers."
            )
        );
    QCOMPARE(
        nextSteps->text(),
        QStringLiteral(
            "Next, add schedules and rosters, then fill in any other information you need."
            )
        );
    QCOMPARE(newButton->text(), QStringLiteral("New Teacher Profile..."));
    QCOMPARE(openButton->text(), QStringLiteral("Open Teacher Profile..."));
    QCOMPARE(setupButton->text(), QStringLiteral("Initial Setup..."));

    page.hide();

    page.setDatabaseOpen(false);
    page.show();
    QCoreApplication::processEvents();

    QVERIFY(banner->isVisible());
    QVERIFY(newButton->geometry().left() < openButton->geometry().left());
    QVERIFY(setupButton->isDefault());

    QSignalSpy setupSpy(
        &page,
        &BasePage::initialSetupRequested
        );

    QSignalSpy openSpy(
        &page,
        &BasePage::openDatabaseRequested
        );

    QSignalSpy newSpy(
        &page,
        &BasePage::newDatabaseRequested
        );

    QTest::mouseClick(
        setupButton,
        Qt::LeftButton
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
    QCOMPARE(setupSpy.count(), 1);

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

void BasePageTests::noDatabaseBannerFitsNarrowPage()
{
    TestPage page;
    QWidget* contentWidget = page.addContentWidget();

    page.resize(360, 600);
    page.show();

    QCoreApplication::processEvents();

    const int databaseOpenContentTop =
        contentWidget->geometry().top();

    page.hide();
    page.setDatabaseOpen(false);
    page.show();
    QCoreApplication::processEvents();

    auto* banner =
        page.findChild<QFrame*>(
            QStringLiteral("noDatabaseBanner")
            );

    QVERIFY(banner);
    QVERIFY(banner->isVisible());
    QCOMPARE(banner->geometry().left(), 0);
    QCOMPARE(banner->width(), page.width());

    const QStringList containedWidgetNames{
        QStringLiteral("noDatabaseTitle"),
        QStringLiteral("noDatabaseMessage"),
        QStringLiteral("noDatabaseStepOne"),
        QStringLiteral("noDatabaseStepTwo"),
        QStringLiteral("noDatabaseStepThree"),
        QStringLiteral("noDatabaseNextSteps"),
        QStringLiteral("noDatabaseSetupButton"),
        QStringLiteral("noDatabaseNewButton"),
        QStringLiteral("noDatabaseOpenButton")
    };

    for (const QString& objectName : containedWidgetNames)
    {
        QWidget* widget =
            banner->findChild<QWidget*>(objectName);

        QVERIFY2(widget, qPrintable(objectName));
        QVERIFY2(
            banner->rect().contains(widget->geometry()),
            qPrintable(objectName)
            );
    }

    QCOMPARE(
        contentWidget->geometry().top(),
        databaseOpenContentTop
            + banner->height()
            + 8
        );
}

void BasePageTests::noDatabaseBannerRetranslatesOnLanguageChange()
{
    TestPage page;
    page.resize(360, 600);
    page.setDatabaseOpen(false);
    page.show();

    auto* banner =
        page.findChild<QFrame*>(
            QStringLiteral("noDatabaseBanner")
            );

    auto* title =
        page.findChild<QLabel*>(
            QStringLiteral("noDatabaseTitle")
            );

    auto* message =
        page.findChild<QLabel*>(
            QStringLiteral("noDatabaseMessage")
            );

    auto* stepOne =
        page.findChild<QLabel*>(
            QStringLiteral("noDatabaseStepOne")
            );

    auto* stepTwo =
        page.findChild<QLabel*>(
            QStringLiteral("noDatabaseStepTwo")
            );

    auto* stepThree =
        page.findChild<QLabel*>(
            QStringLiteral("noDatabaseStepThree")
            );

    auto* nextSteps =
        page.findChild<QLabel*>(
            QStringLiteral("noDatabaseNextSteps")
            );

    auto* openButton =
        page.findChild<QPushButton*>(
            QStringLiteral("noDatabaseOpenButton")
            );

    auto* newButton =
        page.findChild<QPushButton*>(
            QStringLiteral("noDatabaseNewButton")
            );

    auto* setupButton =
        page.findChild<QPushButton*>(
            QStringLiteral("noDatabaseSetupButton")
            );

    QVERIFY(banner);
    QVERIFY(title);
    QVERIFY(message);
    QVERIFY(stepOne);
    QVERIFY(stepTwo);
    QVERIFY(stepThree);
    QVERIFY(nextSteps);
    QVERIFY(openButton);
    QVERIFY(newButton);
    QVERIFY(setupButton);

    NoDatabaseBannerTranslator translator;
    qApp->installTranslator(&translator);

    QEvent languageChange(QEvent::LanguageChange);
    QCoreApplication::sendEvent(&page, &languageChange);
    QCoreApplication::processEvents();

    QCOMPARE(title->text(), QStringLiteral("Translated title"));
    QCOMPARE(message->text(), QStringLiteral("Translated introduction"));
    QCOMPARE(stepOne->text(), QStringLiteral("Translated first step"));
    QCOMPARE(stepTwo->text(), QStringLiteral("Translated second step"));
    QCOMPARE(stepThree->text(), QStringLiteral("Translated third step"));
    QCOMPARE(nextSteps->text(), QStringLiteral("Translated next steps"));
    QCOMPARE(openButton->text(), QStringLiteral("Translated open Teacher Profile"));
    QCOMPARE(newButton->text(), QStringLiteral("Translated new Teacher Profile"));
    QCOMPARE(setupButton->text(), QStringLiteral("Translated initial setup"));

    for (QLabel* label : {
             title,
             message,
             stepOne,
             stepTwo,
             stepThree,
             nextSteps
             })
    {
        QVERIFY(banner->rect().contains(label->geometry()));
    }

    qApp->removeTranslator(&translator);

    QCoreApplication::sendEvent(&page, &languageChange);
    QCoreApplication::processEvents();

    QCOMPARE(title->text(), QStringLiteral("Getting Started"));
    QCOMPARE(
        message->text(),
        QStringLiteral(
            "No Teacher Profile is open. Set up ClassMngr in this order:"
            )
        );
    QCOMPARE(
        stepOne->text(),
        QStringLiteral(
            "1. Create a new Teacher Profile, or open an existing one."
            )
        );
    QCOMPARE(
        stepTwo->text(),
        QStringLiteral(
            "2. Create or import your Korean teachers."
            )
        );
    QCOMPARE(
        stepThree->text(),
        QStringLiteral(
            "3. Create your classes and assign their teachers."
            )
        );
    QCOMPARE(
        nextSteps->text(),
        QStringLiteral(
            "Next, add schedules and rosters, then fill in any other information you need."
            )
        );
    QCOMPARE(openButton->text(), QStringLiteral("Open Teacher Profile..."));
    QCOMPARE(newButton->text(), QStringLiteral("New Teacher Profile..."));
    QCOMPARE(setupButton->text(), QStringLiteral("Initial Setup..."));
}

void BasePageTests::databaseStateClearHookIsDispatched()
{
    TestPage page;

    QCOMPARE(page.databaseClearCount, 0);
    page.clearDatabaseState();
    QCOMPARE(page.databaseClearCount, 1);
}

void BasePageTests
    ::outputContractDefaultsToUnsupportedAndSignalsDatabaseChanges()
{
    TestPage page;
    QSignalSpy capabilitySpy(
        &page,
        &BasePage::outputCapabilitiesChanged
        );

    const PageOutputCapabilities capabilities =
        page.outputCapabilities();
    QVERIFY(!capabilities.printEnabled);
    QVERIFY(!capabilities.saveAsEnabled);

    page.printCurrentPage();
    page.saveCurrentPageAs();

    page.setDatabaseOpen(true);
    QCOMPARE(capabilitySpy.count(), 1);

    page.setDatabaseOpen(true);
    QCOMPARE(capabilitySpy.count(), 1);

    page.setDatabaseOpen(false);
    QCOMPARE(capabilitySpy.count(), 2);
}

QTEST_MAIN(BasePageTests)

#include "basepage_tests.moc"
