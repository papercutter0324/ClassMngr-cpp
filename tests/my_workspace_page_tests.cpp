#include "features/my_info/ui/my_workspace_page.h"
#include "features/my_info/ui/personal_details_page.h"
#include "features/calendar/ui/calendar_page.h"
#include "features/schedule/ui/schedule_page.h"
#include "core/application_services.h"
#include "data/data_service.h"
#include "features/my_info/data/signature_image_processor.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/navigation_tab_widget.h"

#include <QApplication>
#include <QBuffer>
#include <QImage>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

namespace
{

constexpr auto SignatureImageKey = "myInfo/signatureImage";
constexpr auto CurrentCampusKey = "myInfo/campus";
constexpr auto DisplayNameKey = "myInfo/name";

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("my-workspace-signature-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

bool openDatabase(
    ApplicationServices& services,
    QTemporaryDir& directory
    )
{
    return services.openDatabase(databasePath(directory)).has_value();
}

QByteArray sourcePng()
{
    QImage source(2, 1, QImage::Format_RGB32);
    source.setPixelColor(0, 0, Qt::white);
    source.setPixelColor(1, 0, Qt::black);

    QByteArray encoded;
    QBuffer buffer(&encoded);
    if (!buffer.open(QIODevice::WriteOnly) || !source.save(&buffer, "PNG"))
    {
        return {};
    }

    return encoded;
}

QLabel* signaturePreview(MyWorkspacePage& page)
{
    return page.personalDetailsPage()->findChild<QLabel*>(
        QStringLiteral("signatureImagePreview"));
}

QLineEdit* personalNameEditor(MyWorkspacePage& page)
{
    const QList<QLineEdit*> editors =
        page.personalDetailsPage()->findChildren<QLineEdit*>();
    return editors.isEmpty() ? nullptr : editors.constFirst();
}

void refreshPersonalDetails(MyWorkspacePage& page)
{
    page.show();
    page.openTab(WorkspaceTab::Details);
    QApplication::processEvents();
    page.personalDetailsPage()->refresh();
    QApplication::processEvents();
}

} // namespace

class MyWorkspacePageTests : public QObject
{
    Q_OBJECT

private slots:
    void createsNamedTabsWithScheduleSelectedByDefault();
    void opensTabsThroughNamedIdentifiers();
    void preservesChildPagesWhenSwitchingTabs();
    void defersCalendarConstructionWhileOtherTabsAreOpen();
    void initializesCalendarWithoutChangingTheActiveWorkspaceTab();
    void isAvailableAsTheDefaultTopLevelPage();
    void receivesTheTopLevelDatabaseState();
    void storedSignatureImageIsPreparedForPreview();
    void missingCorruptAndUnavailableSignatureImagesStayEmpty();
    void storedCampusPrefillsAndCorrectsThroughTypedPort();
    void storedDisplayNamePrefillsWithUtf8AndWhitespace();
    void missingAndUnavailableDisplayNamePrefillEmpty();
};

void MyWorkspacePageTests::createsNamedTabsWithScheduleSelectedByDefault()
{
    MyWorkspacePage page(nullptr);

    auto* tabs = page.findChild<NavigationTabWidget*>(
        QStringLiteral("myWorkspaceTabs")
        );
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 3);
    QCOMPARE(tabs->tabText(0), QStringLiteral("My Details"));
    QCOMPARE(tabs->tabText(1), QStringLiteral("My Schedule"));
    QCOMPARE(tabs->tabText(2), QStringLiteral("Calendar"));
    QCOMPARE(page.currentTab(), WorkspaceTab::Schedule);
    QCOMPARE(tabs->currentIndex(), 1);
    QVERIFY(page.personalDetailsPage());
    QVERIFY(page.schedulePage());
}

void MyWorkspacePageTests::opensTabsThroughNamedIdentifiers()
{
    MyWorkspacePage page(nullptr);

    page.openTab(WorkspaceTab::Details);
    QCOMPARE(page.currentTab(), WorkspaceTab::Details);

    page.openTab(WorkspaceTab::Schedule);
    QCOMPARE(page.currentTab(), WorkspaceTab::Schedule);
}

void MyWorkspacePageTests::preservesChildPagesWhenSwitchingTabs()
{
    MyWorkspacePage page(nullptr);
    PersonalDetailsPage* const details = page.personalDetailsPage();
    SchedulePage* const schedule = page.schedulePage();

    page.openTab(WorkspaceTab::Details);
    page.openTab(WorkspaceTab::Schedule);

    QCOMPARE(page.personalDetailsPage(), details);
    QCOMPARE(page.schedulePage(), schedule);
}

void MyWorkspacePageTests::defersCalendarConstructionWhileOtherTabsAreOpen()
{
    MyWorkspacePage page(nullptr);

    QVERIFY(!page.calendarPage());

    page.openTab(WorkspaceTab::Details);
    page.openTab(WorkspaceTab::Schedule);

    QVERIFY(!page.calendarPage());
}

void MyWorkspacePageTests::initializesCalendarWithoutChangingTheActiveWorkspaceTab()
{
    ApplicationServices services;
    PageManager pages;
    pages.initialize(&services, false);

    MyWorkspacePage* const workspace = pages.myWorkspacePage();
    QVERIFY(workspace);

    for (const WorkspaceTab tab : {
             WorkspaceTab::Details,
             WorkspaceTab::Schedule,
             WorkspaceTab::Calendar
         })
    {
        workspace->openTab(tab);

        QVERIFY(pages.ensureCalendarPage());
        QCOMPARE(workspace->currentTab(), tab);
    }
}

void MyWorkspacePageTests::isAvailableAsTheDefaultTopLevelPage()
{
    ApplicationServices services;
    PageManager pages;
    pages.initialize(&services, false);

    QVERIFY(pages.isPageInstantiated(PageType::MyWorkspace));

    QVERIFY(pages.isCurrentPage(PageType::MyWorkspace));
    QVERIFY(pages.myWorkspacePage());
    QCOMPARE(
        pages.myWorkspacePage()->currentTab(),
        WorkspaceTab::Schedule
        );
}

void MyWorkspacePageTests::receivesTheTopLevelDatabaseState()
{
    ApplicationServices services;
    PageManager pages;
    pages.initialize(&services, false);
    pages.setDatabaseOpen(true);

    pages.showPage(PageType::MyWorkspace);

    QVERIFY(pages.outputCapabilities().printEnabled);
    QVERIFY(pages.outputCapabilities().saveAsEnabled);
}

void MyWorkspacePageTests::storedSignatureImageIsPreparedForPreview()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(openDatabase(services, directory));
    QVERIFY(services.dataService());

    const QByteArray source = sourcePng();
    QVERIFY(!source.isEmpty());
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(SignatureImageKey),
            QString::fromLatin1(source.toBase64())
            )
        );

    MyWorkspacePage page(&services);
    refreshPersonalDetails(page);

    auto* preview = signaturePreview(page);
    QVERIFY(preview);
    QVERIFY(preview->text().isEmpty());
    QVERIFY(!preview->pixmap().isNull());

    const QByteArray expected =
        SignatureImage::prepareForEmbedding(source);
    QVERIFY(!expected.isEmpty());
}

void MyWorkspacePageTests::missingCorruptAndUnavailableSignatureImagesStayEmpty()
{
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        ApplicationServices services;
        QVERIFY(openDatabase(services, directory));

        MyWorkspacePage page(&services);
        refreshPersonalDetails(page);

        auto* preview = signaturePreview(page);
        QVERIFY(preview);
        QVERIFY(!preview->text().isEmpty());
        QVERIFY(preview->pixmap().isNull());
    }

    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        ApplicationServices services;
        QVERIFY(openDatabase(services, directory));
        QVERIFY(services.dataService());
        QVERIFY(
            services.dataService()->saveSetting(
                QString::fromUtf8(SignatureImageKey),
                QStringLiteral("%%%not-base64%%")
                )
            );

        MyWorkspacePage page(&services);
        refreshPersonalDetails(page);

        auto* preview = signaturePreview(page);
        QVERIFY(preview);
        QVERIFY(!preview->text().isEmpty());
        QVERIFY(preview->pixmap().isNull());
    }

    {
        ApplicationServices services;
        MyWorkspacePage page(&services);
        refreshPersonalDetails(page);

        auto* preview = signaturePreview(page);
        QVERIFY(preview);
        QVERIFY(!preview->text().isEmpty());
        QVERIFY(preview->pixmap().isNull());
    }
}

void MyWorkspacePageTests::storedCampusPrefillsAndCorrectsThroughTypedPort()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(openDatabase(services, directory));
    QVERIFY(services.dataService());
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(CurrentCampusKey),
            QStringLiteral(" J ")
            )
        );

    MyWorkspacePage page(&services);
    refreshPersonalDetails(page);

    auto* campus = page.personalDetailsPage()->findChild<QComboBox*>();
    QVERIFY(campus);
    QCOMPARE(campus->currentData().toString(), QStringLiteral("j"));
    QCOMPARE(campus->currentText(), QStringLiteral("Jeongja"));

    const auto stored = services.dataService()->loadSetting(
        QString::fromUtf8(CurrentCampusKey)
        );
    QVERIFY(stored);
    QCOMPARE(stored->toString(), QStringLiteral("Jeongja"));
}

void MyWorkspacePageTests::storedDisplayNamePrefillsWithUtf8AndWhitespace()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(openDatabase(services, directory));
    QVERIFY(services.dataService());

    const QString expected = QStringLiteral("  홍길동  ");
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(DisplayNameKey),
            expected
            )
        );

    MyWorkspacePage page(&services);
    refreshPersonalDetails(page);

    auto* name = personalNameEditor(page);
    QVERIFY(name);
    QCOMPARE(name->text(), expected);
}

void MyWorkspacePageTests::missingAndUnavailableDisplayNamePrefillEmpty()
{
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        ApplicationServices services;
        QVERIFY(openDatabase(services, directory));

        MyWorkspacePage page(&services);
        refreshPersonalDetails(page);

        auto* name = personalNameEditor(page);
        QVERIFY(name);
        QVERIFY(name->text().isEmpty());
    }

    {
        ApplicationServices services;
        MyWorkspacePage page(&services);
        refreshPersonalDetails(page);

        auto* name = personalNameEditor(page);
        QVERIFY(name);
        QVERIFY(name->text().isEmpty());
    }
}

QTEST_MAIN(MyWorkspacePageTests)

#include "my_workspace_page_tests.moc"
