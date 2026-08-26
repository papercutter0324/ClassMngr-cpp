#include "ui/shared/pages/basepage.h"
#include "core/fontmanager.h"

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

class LifecycleTestPage : public BasePage
{
public:
    void refresh() override
    {
        ++refreshCount;
    }

    void releaseFeatureResources() override
    {
        ++releaseCount;
    }

    int refreshCount = 0;
    int releaseCount = 0;
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
                "— A guided setup to help you import or add your schedule, "
                "classes, and co-teachers."
                ) == 0)
        {
            return QStringLiteral("Translated setup description");
        }

        if (qstrcmp(sourceText, "New Profile") == 0)
        {
            return QStringLiteral("Translated new Profile");
        }

        if (qstrcmp(sourceText, "Initial Setup") == 0)
        {
            return QStringLiteral("Translated initial setup");
        }

        if (qstrcmp(
                sourceText,
                "— Create a new Teacher Profile and manually enter your schedule, "
                "classes, and co-teachers."
                ) == 0)
        {
            return QStringLiteral("Translated new description");
        }

        if (qstrcmp(sourceText, "Open Profile") == 0)
        {
            return QStringLiteral("Translated open Profile");
        }

        if (qstrcmp(
                sourceText,
                "— Open an existing Teacher Profile file."
                ) == 0)
        {
            return QStringLiteral("Translated open description");
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
    void noDatabaseBannerScalesWithFontSize();
    void databaseStateClearHookIsDispatched();
    void outputContractDefaultsToUnsupportedAndSignalsDatabaseChanges();
    void lifecycleRefreshesOnlyWhenStale();
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

    auto* setupDescription =
        page.findChild<QLabel*>(
            QStringLiteral("noDatabaseSetupDescription")
            );

    auto* newDescription =
        page.findChild<QLabel*>(
            QStringLiteral("noDatabaseNewDescription")
            );

    auto* openDescription =
        page.findChild<QLabel*>(
            QStringLiteral("noDatabaseOpenDescription")
            );

    QVERIFY(title);
    QVERIFY(setupDescription);
    QVERIFY(newDescription);
    QVERIFY(openDescription);

    QCOMPARE(title->text(), QStringLiteral("Getting Started"));
    QCOMPARE(
        setupDescription->text(),
        QStringLiteral(
            "— A guided setup to help you import or add your schedule, "
            "classes, and co-teachers."
            )
        );
    QCOMPARE(newButton->text(), QStringLiteral("New Profile"));
    QCOMPARE(
        newDescription->text(),
        QStringLiteral(
            "— Create a new Teacher Profile and manually enter your schedule, "
            "classes, and co-teachers."
            )
        );
    QCOMPARE(openButton->text(), QStringLiteral("Open Profile"));
    QCOMPARE(
        openDescription->text(),
        QStringLiteral("— Open an existing Teacher Profile file.")
        );
    QCOMPARE(setupButton->text(), QStringLiteral("Initial Setup"));
    QVERIFY(setupButton->styleSheet().isEmpty());
    QVERIFY(newButton->styleSheet().isEmpty());
    QVERIFY(openButton->styleSheet().isEmpty());

    page.hide();

    page.setDatabaseOpen(false);
    page.show();
    QCoreApplication::processEvents();

    QVERIFY(banner->isVisible());
    QCOMPARE(setupButton->geometry().left(), newButton->geometry().left());
    QCOMPARE(newButton->geometry().left(), openButton->geometry().left());
    QCOMPARE(setupButton->width(), newButton->width());
    QCOMPARE(newButton->width(), openButton->width());
    QVERIFY(setupButton->geometry().top() < newButton->geometry().top());
    QVERIFY(newButton->geometry().top() < openButton->geometry().top());
    QVERIFY(setupDescription->geometry().left() > setupButton->geometry().right());
    QVERIFY(newDescription->geometry().left() > newButton->geometry().right());
    QVERIFY(openDescription->geometry().left() > openButton->geometry().right());
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
        QStringLiteral("noDatabaseSetupButton"),
        QStringLiteral("noDatabaseSetupDescription"),
        QStringLiteral("noDatabaseNewButton"),
        QStringLiteral("noDatabaseNewDescription"),
        QStringLiteral("noDatabaseOpenButton"),
        QStringLiteral("noDatabaseOpenDescription")
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

    auto* setupDescription =
        page.findChild<QLabel*>(
            QStringLiteral("noDatabaseSetupDescription")
            );

    auto* newDescription =
        page.findChild<QLabel*>(
            QStringLiteral("noDatabaseNewDescription")
            );

    auto* openDescription =
        page.findChild<QLabel*>(
            QStringLiteral("noDatabaseOpenDescription")
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
    QVERIFY(setupDescription);
    QVERIFY(newDescription);
    QVERIFY(openDescription);
    QVERIFY(openButton);
    QVERIFY(newButton);
    QVERIFY(setupButton);

    NoDatabaseBannerTranslator translator;
    qApp->installTranslator(&translator);

    QEvent languageChange(QEvent::LanguageChange);
    QCoreApplication::sendEvent(&page, &languageChange);
    QCoreApplication::processEvents();

    QCOMPARE(title->text(), QStringLiteral("Translated title"));
    QCOMPARE(setupDescription->text(), QStringLiteral("Translated setup description"));
    QCOMPARE(newButton->text(), QStringLiteral("Translated new Profile"));
    QCOMPARE(newDescription->text(), QStringLiteral("Translated new description"));
    QCOMPARE(openButton->text(), QStringLiteral("Translated open Profile"));
    QCOMPARE(openDescription->text(), QStringLiteral("Translated open description"));
    QCOMPARE(setupButton->text(), QStringLiteral("Translated initial setup"));
    QCOMPARE(setupButton->width(), newButton->width());
    QCOMPARE(newButton->width(), openButton->width());

    QVERIFY(banner->rect().contains(title->geometry()));

    qApp->removeTranslator(&translator);

    QCoreApplication::sendEvent(&page, &languageChange);
    QCoreApplication::processEvents();

    QCOMPARE(title->text(), QStringLiteral("Getting Started"));
    QCOMPARE(
        setupDescription->text(),
        QStringLiteral(
            "— A guided setup to help you import or add your schedule, "
            "classes, and co-teachers."
            )
        );
    QCOMPARE(newButton->text(), QStringLiteral("New Profile"));
    QCOMPARE(
        newDescription->text(),
        QStringLiteral(
            "— Create a new Teacher Profile and manually enter your schedule, "
            "classes, and co-teachers."
            )
        );
    QCOMPARE(openButton->text(), QStringLiteral("Open Profile"));
    QCOMPARE(
        openDescription->text(),
        QStringLiteral("— Open an existing Teacher Profile file.")
        );
    QCOMPARE(setupButton->text(), QStringLiteral("Initial Setup"));
}

void BasePageTests::noDatabaseBannerScalesWithFontSize()
{
    TestPage page;
    page.resize(800, 600);
    page.setDatabaseOpen(false);
    page.show();

    auto* title =
        page.findChild<QLabel*>(
            QStringLiteral("noDatabaseTitle")
            );

    auto* setupButton =
        page.findChild<QPushButton*>(
            QStringLiteral("noDatabaseSetupButton")
            );

    auto* newButton =
        page.findChild<QPushButton*>(
            QStringLiteral("noDatabaseNewButton")
            );

    auto* openButton =
        page.findChild<QPushButton*>(
            QStringLiteral("noDatabaseOpenButton")
            );

    QVERIFY(title);
    QVERIFY(setupButton);
    QVERIFY(newButton);
    QVERIFY(openButton);

    QCoreApplication::processEvents();

    const int originalFontSizeOffset =
        FontManager::sizeOffset();

    const QFont originalFont =
        page.font();

    const int standardFontSizeAtLarge =
        originalFont.pointSize()
        - originalFontSizeOffset
        + fontSizeOffset(FontSize::Large);

    const int titleFontSizeDifference =
        20 - standardFontSizeAtLarge;

    QCOMPARE(
        title->font().pointSize(),
        originalFont.pointSize() + titleFontSizeDifference
        );

    const int normalButtonWidth =
        setupButton->width();

    QFont largeFont =
        originalFont;

    largeFont.setPointSize(
        originalFont.pointSize()
        - originalFontSizeOffset
        + fontSizeOffset(FontSize::Large)
        );

    FontManager::setSizeOffset(
        fontSizeOffset(FontSize::Large)
        );

    page.setFont(largeFont);
    QCoreApplication::processEvents();

    QCOMPARE(title->font().pointSize(), 20);
    QVERIFY(setupButton->width() > normalButtonWidth);
    QCOMPARE(setupButton->width(), newButton->width());
    QCOMPARE(newButton->width(), openButton->width());

    FontManager::setSizeOffset(originalFontSizeOffset);
    page.setFont(originalFont);
    QCoreApplication::processEvents();
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

void BasePageTests::lifecycleRefreshesOnlyWhenStale()
{
    LifecycleTestPage page;

    QVERIFY(!page.needsRefresh());

    page.setDatabaseOpen(true);
    QVERIFY(page.needsRefresh());

    page.activate();
    QCOMPARE(page.refreshCount, 1);
    QVERIFY(!page.needsRefresh());

    page.activate();
    QCOMPARE(page.refreshCount, 1);

    page.markStale();
    page.activate();
    QCOMPARE(page.refreshCount, 2);

    page.deactivate();
    QCOMPARE(page.releaseCount, 1);
}

QTEST_MAIN(BasePageTests)

#include "basepage_tests.moc"
