#include "app/services/feature_services.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "features/my_info/ui/personal_details_page.h"
#include "features/calendar/ui/calendar_page.h"
#include "features/schedule/ui/schedule_page.h"
#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "features/my_info/data/signature_image_processor.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/navigation_tab_widget.h"

#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QImage>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

#include <utility>

namespace
{

constexpr auto SignatureImageKey = "myInfo/signatureImage";
constexpr auto CurrentCampusKey = "myInfo/campus";
constexpr auto DisplayNameKey = "myInfo/name";
constexpr auto ZoomLoginIdKey = "myInfo/zoomLoginId";
constexpr auto ZoomPasswordKey = "myInfo/zoomPassword";
constexpr auto ZoomNotAvailableKey = "myInfo/zoomNotAvailable";
constexpr auto LegacyZoomLoginIdKey = "subPrep/personalZoomEmail";
constexpr auto LegacyZoomPasswordKey = "subPrep/personalZoomPassword";
constexpr auto LegacyZoomNotAvailableKey = "subPrep/personalZoomNotAvailable";
constexpr auto SignatureModeKey = "myInfo/signatureMode";
constexpr auto TypedSignatureTextKey = "myInfo/typedSignatureText";
constexpr auto TypedSignatureFontKey = "myInfo/typedSignatureFont";
constexpr auto UnrelatedKey = "myInfo/unrelatedPreference";

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

QLineEdit* personalDetailsEditor(
    MyWorkspacePage& page,
    qsizetype index
    )
{
    const QList<QLineEdit*> editors =
        page.personalDetailsPage()->findChildren<QLineEdit*>();
    return index >= 0 && index < editors.size()
        ? editors.at(index)
        : nullptr;
}

QLineEdit* personalZoomLoginEditor(MyWorkspacePage& page)
{
    return personalDetailsEditor(page, 1);
}

QLineEdit* personalZoomPasswordEditor(MyWorkspacePage& page)
{
    return personalDetailsEditor(page, 2);
}

QCheckBox* personalZoomUnavailableCheck(MyWorkspacePage& page)
{
    return page.personalDetailsPage()->findChild<QCheckBox*>(
        QStringLiteral("zoomNotAvailableCheck"));
}

QPushButton* signatureModeButton(
    MyWorkspacePage& page,
    const QString& objectName
    )
{
    return page.personalDetailsPage()->findChild<QPushButton*>(objectName);
}

bool executeSql(
    ApplicationServices& services,
    const QString& statement
    )
{
    DataService* dataService = services.dataService();
    if (!dataService || !dataService->databaseSession())
    {
        return false;
    }

    QSqlQuery query(dataService->databaseSession()->database());
    return query.exec(statement);
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
    void storedZoomCredentialsPrefillAndRespectUnavailableState();
    void missingZoomValuesUseNaFallbackAndDisableFields();
    void legacyZoomValuesMigrateDuringPersonalDetailsLoad();
    void storedSignaturePreferencesPopulateModeTextAndFont();
    void unavailableSettingsLeavePersonalDetailsFieldsUnchangedOnLoad();
    void unavailablePersonalDetailsSaveDoesNotNormalizeOrClearDirtyValues();
    void aggregateSavePersistsAllPersonalDetailsKeys();
    void aggregateSaveFailureRollsBackAndPreservesUnrelatedSettings();
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

void MyWorkspacePageTests::storedZoomCredentialsPrefillAndRespectUnavailableState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(openDatabase(services, directory));
    QVERIFY(services.dataService());
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(ZoomLoginIdKey),
            QStringLiteral("teacher@example.com")
            )
        );
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(ZoomPasswordKey),
            QStringLiteral("secret")
            )
        );
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(ZoomNotAvailableKey),
            false
            )
        );

    MyWorkspacePage available(&services);
    refreshPersonalDetails(available);

    auto* login = personalZoomLoginEditor(available);
    auto* password = personalZoomPasswordEditor(available);
    auto* unavailable = personalZoomUnavailableCheck(available);
    QVERIFY(login);
    QVERIFY(password);
    QVERIFY(unavailable);
    QCOMPARE(login->text(), QStringLiteral("teacher@example.com"));
    QCOMPARE(password->text(), QStringLiteral("secret"));
    QVERIFY(!unavailable->isChecked());
    QVERIFY(login->isEnabled());
    QVERIFY(password->isEnabled());

    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(ZoomNotAvailableKey),
            true
            )
        );

    MyWorkspacePage unavailablePage(&services);
    refreshPersonalDetails(unavailablePage);

    login = personalZoomLoginEditor(unavailablePage);
    password = personalZoomPasswordEditor(unavailablePage);
    unavailable = personalZoomUnavailableCheck(unavailablePage);
    QVERIFY(login);
    QVERIFY(password);
    QVERIFY(unavailable);
    QCOMPARE(login->text(), QStringLiteral("teacher@example.com"));
    QCOMPARE(password->text(), QStringLiteral("secret"));
    QVERIFY(unavailable->isChecked());
    QVERIFY(!login->isEnabled());
    QVERIFY(!password->isEnabled());
}

void MyWorkspacePageTests::missingZoomValuesUseNaFallbackAndDisableFields()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(openDatabase(services, directory));

    MyWorkspacePage available(&services);
    refreshPersonalDetails(available);

    auto* login = personalZoomLoginEditor(available);
    auto* password = personalZoomPasswordEditor(available);
    auto* unavailable = personalZoomUnavailableCheck(available);
    QVERIFY(login);
    QVERIFY(password);
    QVERIFY(unavailable);
    QCOMPARE(login->text(), QStringLiteral("N/A"));
    QCOMPARE(password->text(), QStringLiteral("N/A"));
    QVERIFY(unavailable->isChecked());
    QVERIFY(!login->isEnabled());
    QVERIFY(!password->isEnabled());

    ApplicationServices unavailableServices;
    MyWorkspacePage unavailablePage(&unavailableServices);
    refreshPersonalDetails(unavailablePage);

    login = personalZoomLoginEditor(unavailablePage);
    password = personalZoomPasswordEditor(unavailablePage);
    unavailable = personalZoomUnavailableCheck(unavailablePage);
    QVERIFY(login);
    QVERIFY(password);
    QVERIFY(unavailable);
    QVERIFY(login->text().isEmpty());
    QVERIFY(password->text().isEmpty());
    QVERIFY(!unavailable->isChecked());
    QVERIFY(login->isEnabled());
    QVERIFY(password->isEnabled());
}

void MyWorkspacePageTests::legacyZoomValuesMigrateDuringPersonalDetailsLoad()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(openDatabase(services, directory));
    QVERIFY(services.dataService());
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(LegacyZoomLoginIdKey),
            QStringLiteral("legacy@example.com")
            )
        );
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(LegacyZoomPasswordKey),
            QStringLiteral("legacy secret")
            )
        );
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(LegacyZoomNotAvailableKey),
            false
            )
        );

    MyWorkspacePage page(&services);
    refreshPersonalDetails(page);

    auto* login = personalZoomLoginEditor(page);
    auto* password = personalZoomPasswordEditor(page);
    auto* unavailable = personalZoomUnavailableCheck(page);
    QVERIFY(login);
    QVERIFY(password);
    QVERIFY(unavailable);
    QCOMPARE(login->text(), QStringLiteral("legacy@example.com"));
    QCOMPARE(password->text(), QStringLiteral("legacy secret"));
    QVERIFY(!unavailable->isChecked());

    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(ZoomLoginIdKey))
            ->toString(),
        QStringLiteral("legacy@example.com")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(ZoomPasswordKey))
            ->toString(),
        QStringLiteral("legacy secret")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(ZoomNotAvailableKey))
            ->toBool(),
        false
        );
}

void MyWorkspacePageTests::storedSignaturePreferencesPopulateModeTextAndFont()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(openDatabase(services, directory));
    QVERIFY(services.dataService());

    const QString expectedText = QString::fromUtf8(
        "  \xEA\xB9\x80\xEC\x84\xA0\xEC\x83\x9D\xEB\x8B\x98 / "
        "\xF0\x9F\xA7\xAD  "
        );
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(SignatureModeKey),
            1
            )
        );
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(TypedSignatureTextKey),
            expectedText
            )
        );
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(TypedSignatureFontKey),
            2
            )
        );

    MyWorkspacePage page(&services);
    refreshPersonalDetails(page);

    auto* imageMode = signatureModeButton(
        page,
        QStringLiteral("signatureImageModeButton")
        );
    auto* typeMode = signatureModeButton(
        page,
        QStringLiteral("signatureTypeModeButton")
        );
    auto* typedText = page.personalDetailsPage()->findChild<QLineEdit*>(
        QStringLiteral("typedSignatureEdit")
        );
    const QList<QPushButton*> fontButtons =
        page.personalDetailsPage()->findChildren<QPushButton*>(
            QStringLiteral("typedSignatureFontButton")
            );

    QVERIFY(imageMode);
    QVERIFY(typeMode);
    QVERIFY(typedText);
    QCOMPARE(fontButtons.size(), 4);
    QVERIFY(!imageMode->isChecked());
    QVERIFY(typeMode->isChecked());
    QCOMPARE(typedText->text(), expectedText);
    for (int index = 0; index < fontButtons.size(); ++index)
    {
        QCOMPARE(fontButtons.at(index)->isChecked(), index == 2);
    }
}

void MyWorkspacePageTests::aggregateSavePersistsAllPersonalDetailsKeys()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(openDatabase(services, directory));
    QVERIFY(services.dataService());

    const QByteArray source = sourcePng();
    QVERIFY(!source.isEmpty());
    const QString expectedName = QString::fromUtf8(
        "  \xEA\xB0\x80\xEB\x82\x98  "
        );
    const QString expectedText = QString::fromUtf8(
        "  \xEA\xB9\x80\xEC\x84\xA0\xEC\x83\x9D\xEB\x8B\x98 / "
        "\xF0\x9F\xA7\xAD  "
        );

    for (const auto& setting : {
             std::pair{QString::fromUtf8(CurrentCampusKey),
                       QVariant(QStringLiteral("Jeongja"))},
             std::pair{QString::fromUtf8(SignatureImageKey),
                       QVariant(QString::fromLatin1(source.toBase64()))},
             std::pair{QString::fromUtf8(SignatureModeKey), QVariant(1)},
             std::pair{QString::fromUtf8(TypedSignatureTextKey),
                       QVariant(expectedText)},
             std::pair{QString::fromUtf8(TypedSignatureFontKey), QVariant(2)},
             std::pair{QString::fromUtf8(ZoomLoginIdKey),
                       QVariant(QStringLiteral("old login"))},
             std::pair{QString::fromUtf8(ZoomPasswordKey),
                       QVariant(QStringLiteral("old password"))},
             std::pair{QString::fromUtf8(ZoomNotAvailableKey), QVariant(false)}
         })
    {
        QVERIFY(
            services.dataService()->saveSetting(
                setting.first,
                setting.second
                )
            );
    }
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(UnrelatedKey),
            QStringLiteral("preserved")
            )
        );

    MyWorkspacePage page(&services);
    refreshPersonalDetails(page);

    auto* name = personalNameEditor(page);
    auto* campus = page.personalDetailsPage()->findChild<QComboBox*>();
    auto* login = personalZoomLoginEditor(page);
    auto* password = personalZoomPasswordEditor(page);
    auto* unavailable = personalZoomUnavailableCheck(page);
    QVERIFY(name);
    QVERIFY(campus);
    QVERIFY(login);
    QVERIFY(password);
    QVERIFY(unavailable);

    name->setText(expectedName);
    campus->setCurrentText(QStringLiteral("Jeongja"));
    login->setText(QStringLiteral(" teacher@example.com "));
    password->setText(QStringLiteral(" secret "));
    unavailable->setChecked(false);

    QVERIFY(page.personalDetailsPage()->saveChanges());

    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(DisplayNameKey))
            ->toString(),
        expectedName
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(CurrentCampusKey))
            ->toString(),
        QStringLiteral("Jeongja")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(ZoomLoginIdKey))
            ->toString(),
        QStringLiteral(" teacher@example.com ")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(ZoomPasswordKey))
            ->toString(),
        QStringLiteral(" secret ")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(ZoomNotAvailableKey))
            ->toBool(),
        false
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(SignatureImageKey))
            ->toString(),
        QString::fromLatin1(
            SignatureImage::prepareForEmbedding(
                SignatureImage::prepareForEmbedding(source)
                ).toBase64()
            )
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(SignatureModeKey))
            ->toInt(),
        1
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(TypedSignatureTextKey))
            ->toString(),
        expectedText
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(TypedSignatureFontKey))
            ->toInt(),
        2
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(UnrelatedKey))
            ->toString(),
        QStringLiteral("preserved")
        );
}

void MyWorkspacePageTests::
unavailableSettingsLeavePersonalDetailsFieldsUnchangedOnLoad()
{
    ApplicationServices services;
    QVERIFY(services.settingsService());
    QVERIFY(!services.settingsService()->isAvailable());

    MyWorkspacePage page(&services);
    auto* name = personalNameEditor(page);
    auto* campus = page.personalDetailsPage()->findChild<QComboBox*>();
    auto* login = personalZoomLoginEditor(page);
    auto* password = personalZoomPasswordEditor(page);
    auto* unavailable = personalZoomUnavailableCheck(page);
    auto* typedSignature = page.personalDetailsPage()->findChild<QLineEdit*>(
        QStringLiteral("typedSignatureEdit")
        );
    QVERIFY(name);
    QVERIFY(campus);
    QVERIFY(login);
    QVERIFY(password);
    QVERIFY(unavailable);
    QVERIFY(typedSignature);

    const QSignalBlocker nameBlocker(name);
    const QSignalBlocker campusBlocker(campus);
    const QSignalBlocker loginBlocker(login);
    const QSignalBlocker passwordBlocker(password);
    const QSignalBlocker unavailableBlocker(unavailable);
    const QSignalBlocker typedSignatureBlocker(typedSignature);
    name->setText(QStringLiteral("preexisting name"));
    campus->addItem(
        QStringLiteral("preexisting campus"),
        QStringLiteral("campus-id")
        );
    login->setText(QStringLiteral("preexisting login"));
    password->setText(QStringLiteral("preexisting password"));
    unavailable->setChecked(true);
    typedSignature->setText(QStringLiteral("preexisting signature"));

    refreshPersonalDetails(page);

    QCOMPARE(name->text(), QStringLiteral("preexisting name"));
    QCOMPARE(campus->count(), 1);
    QCOMPARE(campus->currentText(), QStringLiteral("preexisting campus"));
    QCOMPARE(campus->currentData().toString(), QStringLiteral("campus-id"));
    QCOMPARE(login->text(), QStringLiteral("preexisting login"));
    QCOMPARE(password->text(), QStringLiteral("preexisting password"));
    QVERIFY(unavailable->isChecked());
    QCOMPARE(typedSignature->text(), QStringLiteral("preexisting signature"));
}

void MyWorkspacePageTests::
unavailablePersonalDetailsSaveDoesNotNormalizeOrClearDirtyValues()
{
    ApplicationServices services;
    QVERIFY(services.settingsService());
    QVERIFY(!services.settingsService()->isAvailable());

    MyWorkspacePage page(&services);
    refreshPersonalDetails(page);

    auto* login = personalZoomLoginEditor(page);
    auto* password = personalZoomPasswordEditor(page);
    auto* unavailable = personalZoomUnavailableCheck(page);
    QVERIFY(login);
    QVERIFY(password);
    QVERIFY(unavailable);

    login->setText(QStringLiteral("  "));
    password->setText(QStringLiteral(" \t "));
    unavailable->setChecked(false);
    QVERIFY(page.personalDetailsPage()->hasUnsavedChanges());

    QVERIFY(!page.personalDetailsPage()->saveChanges());

    QCOMPARE(login->text(), QStringLiteral("  "));
    QCOMPARE(password->text(), QStringLiteral(" \t "));
    QVERIFY(page.personalDetailsPage()->hasUnsavedChanges());
}

void MyWorkspacePageTests::
aggregateSaveFailureRollsBackAndPreservesUnrelatedSettings()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(openDatabase(services, directory));
    QVERIFY(services.dataService());

    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(DisplayNameKey),
            QStringLiteral("old name")
            )
        );
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(CurrentCampusKey),
            QStringLiteral("Jeongja")
            )
        );
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(ZoomLoginIdKey),
            QStringLiteral("old login")
            )
        );
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(ZoomPasswordKey),
            QStringLiteral("old password")
            )
        );
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(ZoomNotAvailableKey),
            true
            )
        );
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(SignatureImageKey),
            QStringLiteral("old-image")
            )
        );
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(SignatureModeKey),
            0
            )
        );
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(TypedSignatureTextKey),
            QStringLiteral("old text")
            )
        );
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(TypedSignatureFontKey),
            1
            )
        );
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(UnrelatedKey),
            QStringLiteral("preserved")
            )
        );
    QVERIFY(executeSql(
        services,
        QStringLiteral(R"(
            CREATE TRIGGER fail_personal_details_page_save
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'myInfo/typedSignatureFont'
            BEGIN
                SELECT RAISE(ABORT, 'forced personal details page save failure');
            END
        )")
        ));

    MyWorkspacePage page(&services);
    refreshPersonalDetails(page);
    auto* name = personalNameEditor(page);
    QVERIFY(name);
    name->setText(QStringLiteral("new name"));

    QVERIFY(!page.personalDetailsPage()->saveChanges());

    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(DisplayNameKey))
            ->toString(),
        QStringLiteral("old name")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(CurrentCampusKey))
            ->toString(),
        QStringLiteral("Jeongja")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(ZoomLoginIdKey))
            ->toString(),
        QStringLiteral("old login")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(ZoomPasswordKey))
            ->toString(),
        QStringLiteral("old password")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(ZoomNotAvailableKey))
            ->toBool(),
        true
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(SignatureImageKey))
            ->toString(),
        QStringLiteral("old-image")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(SignatureModeKey))
            ->toInt(),
        0
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(TypedSignatureTextKey))
            ->toString(),
        QStringLiteral("old text")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(TypedSignatureFontKey))
            ->toInt(),
        1
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(UnrelatedKey))
            ->toString(),
        QStringLiteral("preserved")
        );
}

QTEST_MAIN(MyWorkspacePageTests)

#include "my_workspace_page_tests.moc"
