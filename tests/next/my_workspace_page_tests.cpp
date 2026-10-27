#include "core/resource_paths.h"
#include "data/database/workspace_store.h"
#include "features/my_info/data/signature_image_processor.h"
#include "features/my_info/data/typed_signature_renderer.h"
#include "next/application/personal_details_save_use_case.h"
#include "next/platform/workspace_store_personal_details_port.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/my_workspace_page.h"
#include "next/ui/page_host.h"
#include "next/ui/page_host_active_page_changes_adapter.h"
#include "preferences_test_ports.h"

#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <memory>
#include <optional>
#include <string>
#include <utility>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory, const QString& suffix)
{
    return directory.filePath(
        QStringLiteral("my-workspace-%1-%2.tps")
            .arg(suffix, QUuid::createUuid().toString(QUuid::WithoutBraces))
        );
}

bool applyFixture(WorkspaceStore& store)
{
    QFile fixture(QStringLiteral(CLASSMNGR_SOURCE_DIR)
                  + QStringLiteral(
                      "/tests/fixtures/workspaces/initial_setup_personal_details.sql"
                      ));
    if (!fixture.open(QIODevice::ReadOnly))
    {
        return false;
    }
    QSqlQuery query(store.database());
    return query.exec(QString::fromUtf8(fixture.readAll()));
}

bool executeSql(WorkspaceStore& store, const QString& statement)
{
    QSqlQuery query(store.database());
    return query.exec(statement);
}

std::string utf8(const QString& value)
{
    const QByteArray encoded = value.toUtf8();
    return std::string(encoded.constData(), static_cast<std::size_t>(encoded.size()));
}

std::string opaqueBytes(const QByteArray& value)
{
    return value.isEmpty()
        ? std::string{}
        : std::string(value.constData(), static_cast<std::size_t>(value.size()));
}

struct PageFixture final
{
    QTemporaryDir directory;
    WorkspaceStore firstStore;
    WorkspaceStore secondStore;
    WorkspaceStore* activeStore = nullptr;
    QString activeIdentity = QStringLiteral("first");
    bool readFailure = false;
    bool saveFailure = false;
    QByteArray chosenImage;
    int warningCount = 0;
    Test::MemoryPreferences preferences;
    std::unique_ptr<Resources::ResourceCatalog> catalog;
    std::unique_ptr<Resources::ResourceLoader> loader;
    std::unique_ptr<Ui::PageHost> host;

    bool initialize()
    {
        if (!directory.isValid()
            || !firstStore.open(databasePath(directory, QStringLiteral("first")))
            || !applyFixture(firstStore)
            || !secondStore.open(databasePath(directory, QStringLiteral("second")))
            || !applyFixture(secondStore))
        {
            return false;
        }

        const auto loadedCatalog = Resources::ResourceCatalog::load(
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT)
            );
        if (!loadedCatalog)
        {
            return false;
        }
        catalog = std::make_unique<Resources::ResourceCatalog>(
            std::move(*loadedCatalog));
        loader = std::make_unique<Resources::ResourceLoader>(*catalog);
        activeStore = &firstStore;
        host = std::make_unique<Ui::PageHost>(*loader, preferences.model);

        Ui::PageDescriptor descriptor{
            .id = *Ui::PageId::fromString(QStringLiteral("my_workspace")),
            .label = QStringLiteral("My Workspace"),
            .iconResourceId = std::nullopt,
            .requiredPermissions = {},
            .factory = [this](const Ui::PageContext& context)
                -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
                Ui::MyWorkspacePageActions actions;
                actions.saveMode = Application::SaveMode::Manual;
                actions.readDetails = [this] {
                    if (readFailure || !activeStore)
                    {
                        return Application::PersonalDetailsReadResult::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "forced personal-details read failure",
                            .recoverable = true
                        });
                    }
                    return Platform::WorkspaceStorePersonalDetailsPort(
                        *activeStore).read();
                };
                actions.saveDetails = [this](
                    const Application::PersonalDetailsSaveRequest& request
                    ) {
                    if (saveFailure || !activeStore)
                    {
                        return Application::PersonalDetailsSaveResult::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "forced personal-details save failure",
                            .recoverable = true
                        });
                    }
                    Platform::WorkspaceStorePersonalDetailsPort port(*activeStore);
                    return Application::WorkspacePersonalDetailsSaveUseCase(port)
                        .execute(request);
                };
                actions.loadCampuses = [](const Ui::PageContext&) {
                    const auto campusId = Domain::CampusId::fromString(
                        "fixture-campus");
                    return std::vector<Application::MyInfoCampusMetadata>{
                        {*campusId, "Fixture Campus"}
                    };
                };
                actions.chooseSignatureImage = [this] {
                    return Domain::Result<std::optional<QByteArray>>::success(
                        chosenImage.isEmpty()
                            ? std::optional<QByteArray>{}
                            : std::optional<QByteArray>{chosenImage}
                        );
                };
                actions.workspaceIdentity = [this] { return activeIdentity; };
                actions.reportWarning = [this](const QString&, const QString&) {
                    ++warningCount;
                };
                std::unique_ptr<Ui::Page> page =
                    std::make_unique<Ui::MyWorkspacePage>(context, std::move(actions));
                return std::move(page);
            },
            .resourcePolicy = Ui::PageResourcePolicy::PageScoped,
            .initialDataPolicy = Ui::PageInitialDataPolicy::None
        };
        if (!host->registerPage(std::move(descriptor))
            || !host->activatePage(QStringLiteral("my_workspace")))
        {
            return false;
        }
        return page() != nullptr;
    }

    Ui::MyWorkspacePage* page() const
    {
        return host
            ? dynamic_cast<Ui::MyWorkspacePage*>(
                  host->pageInstance(QStringLiteral("my_workspace")))
            : nullptr;
    }

    Application::PersonalDetailsReadResult readActive() const
    {
        return Platform::WorkspaceStorePersonalDetailsPort(*activeStore).read();
    }
};

} // namespace

class NextMyWorkspacePageTests final : public QObject
{
    Q_OBJECT

private slots:
    void completeBundleIsLoadedAndNormalSaveAllowsEmptyName();
    void zoomNotAvailableKeepsCredentialsAndAvailableBlankValuesNormalize();
    void deletedCampusIsCorrectedAndPersistedWhenDetailsLoad();
    void failedReadPreservesDraftAndBlocksSaveUntilReload();
    void failedSaveKeepsDraftAndRetries();
    void automaticSaveFailureShowsStatusWithoutPersistentSaveControls();
    void typedAndImageSignaturesPersistAndCanBeRemoved();
    void typedSignaturePreviewRendersTheSelectedTextAndFont();
    void signaturePreviewIsReleasedOnSuspendAndReloadedOnActivation();
    void workspaceIdentityReplacementDiscardsOldDraftWithoutSavingIt();
    void automaticModeDebouncesSave();
    void activePageChangesAdapterSavesAndDiscardsTheDetailsDraft();
};

void NextMyWorkspacePageTests::completeBundleIsLoadedAndNormalSaveAllowsEmptyName()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    Ui::MyWorkspacePage* const page = fixture.page();
    auto* const name = page->findChild<QLineEdit*>(
        QStringLiteral("nextMyWorkspaceName"));
    auto* const footer = page->findChild<QWidget*>(
        QStringLiteral("nextMyWorkspaceStatusFooter"));
    QVERIFY(name);
    QVERIFY(footer);
    QVERIFY(footer->isHidden());
    QVERIFY(!page->findChild<QPushButton*>(
        QStringLiteral("nextMyWorkspaceSaveDetails")));
    QCOMPARE(name->text(), QStringLiteral("Fixture Teacher"));
    QCOMPARE(page->findChild<QComboBox*>(QStringLiteral("nextMyWorkspaceCampus"))
                 ->currentText(), QStringLiteral("Fixture Campus"));

    name->clear();
    QVERIFY(page->hasUnsavedChanges());
    QVERIFY(footer->isHidden());
    QVERIFY(page->saveChanges());
    QVERIFY(!page->hasUnsavedChanges());

    const auto stored = fixture.readActive();
    QVERIFY(stored);
    QVERIFY(stored.value().name.empty());
    QCOMPARE(stored.value().campus.value(), std::string("Fixture Campus"));
    QCOMPARE(stored.value().zoomLoginId, std::string("fixture.zoom@example.test"));
    QCOMPARE(stored.value().zoomPassword, std::string("fixture-only"));
    QVERIFY(!stored.value().zoomNotAvailable);

    Platform::WorkspaceStorePersonalDetailsPort port(fixture.firstStore);
    const auto setupRejected = Application::PersonalDetailsSaveUseCase(port).execute({});
    QVERIFY(!setupRejected);
    QCOMPARE(setupRejected.error().code, Domain::ErrorCode::Validation);
}

void NextMyWorkspacePageTests::
zoomNotAvailableKeepsCredentialsAndAvailableBlankValuesNormalize()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    Ui::MyWorkspacePage* const page = fixture.page();
    auto* const login = page->findChild<QLineEdit*>(
        QStringLiteral("nextMyWorkspaceZoomLoginId"));
    auto* const password = page->findChild<QLineEdit*>(
        QStringLiteral("nextMyWorkspaceZoomPassword"));
    auto* const notAvailable = page->findChild<QCheckBox*>(
        QStringLiteral("nextMyWorkspaceZoomNotAvailable"));
    QVERIFY(login && password && notAvailable);

    notAvailable->setChecked(true);
    QVERIFY(!login->isEnabled());
    QVERIFY(!password->isEnabled());
    QVERIFY(page->saveChanges());
    auto stored = fixture.readActive();
    QVERIFY(stored);
    QVERIFY(stored.value().zoomNotAvailable);
    QCOMPARE(stored.value().zoomLoginId,
             std::string("fixture.zoom@example.test"));
    QCOMPARE(stored.value().zoomPassword, std::string("fixture-only"));

    notAvailable->setChecked(false);
    QVERIFY(login->isEnabled());
    QVERIFY(password->isEnabled());
    login->clear();
    password->clear();
    QVERIFY(page->saveChanges());
    stored = fixture.readActive();
    QVERIFY(stored);
    QVERIFY(!stored.value().zoomNotAvailable);
    QCOMPARE(stored.value().zoomLoginId, std::string("N/A"));
    QCOMPARE(stored.value().zoomPassword, std::string("N/A"));
    QCOMPARE(login->text(), QStringLiteral("N/A"));
    QCOMPARE(password->text(), QStringLiteral("N/A"));
}

void NextMyWorkspacePageTests::
deletedCampusIsCorrectedAndPersistedWhenDetailsLoad()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    SettingsRepository* const settings = fixture.firstStore.settingsRepository();
    QVERIFY(settings);
    QVERIFY(settings->saveSetting(
        QStringLiteral("myInfo/campus"), QStringLiteral("Deleted Campus")));

    Ui::MyWorkspacePage* const page = fixture.page();
    page->reloadPersonalDetails();
    auto* const campus = page->findChild<QComboBox*>(
        QStringLiteral("nextMyWorkspaceCampus"));
    QVERIFY(campus);
    QCOMPARE(campus->currentText(), QStringLiteral("Fixture Campus"));
    QCOMPARE(fixture.readActive().value().campus.value(),
             std::string("Fixture Campus"));
}

void NextMyWorkspacePageTests::failedReadPreservesDraftAndBlocksSaveUntilReload()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    Ui::MyWorkspacePage* const page = fixture.page();
    auto* const name = page->findChild<QLineEdit*>(
        QStringLiteral("nextMyWorkspaceName"));
    auto* const footer = page->findChild<QWidget*>(
        QStringLiteral("nextMyWorkspaceStatusFooter"));
    auto* const reload = page->findChild<QPushButton*>(
        QStringLiteral("nextMyWorkspaceReloadDetails"));
    QVERIFY(name);
    QVERIFY(footer && reload);
    QVERIFY(!page->findChild<QPushButton*>(
        QStringLiteral("nextMyWorkspaceSaveDetails")));
    name->setText(QStringLiteral("Unsaved Draft"));
    QVERIFY(footer->isHidden());
    fixture.readFailure = true;
    page->reloadPersonalDetails();

    QCOMPARE(name->text(), QStringLiteral("Unsaved Draft"));
    QVERIFY(page->hasUnsavedChanges());
    QVERIFY(!footer->isHidden());
    QVERIFY(!reload->isHidden());
    const auto blocked = page->saveChanges();
    QVERIFY(!blocked);
    QCOMPARE(fixture.readActive().value().name, std::string("Fixture Teacher"));

    fixture.readFailure = false;
    page->reloadPersonalDetails();
    QCOMPARE(name->text(), QStringLiteral("Fixture Teacher"));
    QVERIFY(!page->hasUnsavedChanges());
    QVERIFY(footer->isHidden());
    QVERIFY(reload->isHidden());
    QCOMPARE(fixture.warningCount, 1);
}

void NextMyWorkspacePageTests::failedSaveKeepsDraftAndRetries()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    Ui::MyWorkspacePage* const page = fixture.page();
    auto* const name = page->findChild<QLineEdit*>(
        QStringLiteral("nextMyWorkspaceName"));
    auto* const footer = page->findChild<QWidget*>(
        QStringLiteral("nextMyWorkspaceStatusFooter"));
    QVERIFY(name);
    QVERIFY(footer);
    QVERIFY(executeSql(
        fixture.firstStore,
        QStringLiteral(R"(
            CREATE TRIGGER fail_my_workspace_save
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'myInfo/typedSignatureText'
            BEGIN
                SELECT RAISE(ABORT, 'forced typed signature failure');
            END
        )")
        ));

    name->setText(QStringLiteral("Retry Name"));
    const auto failed = page->saveChanges();
    QVERIFY(!failed);
    QVERIFY(page->hasUnsavedChanges());
    QVERIFY(footer->isHidden());
    QCOMPARE(name->text(), QStringLiteral("Retry Name"));
    QCOMPARE(fixture.readActive().value().name, std::string("Fixture Teacher"));
    QCOMPARE(fixture.warningCount, 1);

    QVERIFY(executeSql(fixture.firstStore,
                       QStringLiteral("DROP TRIGGER fail_my_workspace_save")));
    QVERIFY(page->saveChanges());
    QVERIFY(!page->hasUnsavedChanges());
    QCOMPARE(fixture.readActive().value().name, std::string("Retry Name"));
}

void NextMyWorkspacePageTests::
automaticSaveFailureShowsStatusWithoutPersistentSaveControls()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    Ui::MyWorkspacePage* const page = fixture.page();
    auto* const name = page->findChild<QLineEdit*>(
        QStringLiteral("nextMyWorkspaceName"));
    auto* const footer = page->findChild<QWidget*>(
        QStringLiteral("nextMyWorkspaceStatusFooter"));
    auto* const status = page->findChild<QLabel*>(
        QStringLiteral("nextMyWorkspaceSaveStatus"));
    QVERIFY(name && footer && status);
    QVERIFY(!page->findChild<QPushButton*>(
        QStringLiteral("nextMyWorkspaceSaveDetails")));
    QVERIFY(executeSql(
        fixture.firstStore,
        QStringLiteral(R"(
            CREATE TRIGGER fail_my_workspace_autosave
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'myInfo/typedSignatureText'
            BEGIN
                SELECT RAISE(ABORT, 'forced autosave failure');
            END
        )")
        ));

    page->setSaveMode(Application::SaveMode::Automatic);
    QVERIFY(footer->isHidden());
    name->setText(QStringLiteral("Autosave retry"));
    QVERIFY(page->hasUnsavedChanges());
    QVERIFY(footer->isHidden());
    QTRY_VERIFY_WITH_TIMEOUT(!footer->isHidden(), 2000);
    QVERIFY(status->text().contains(QStringLiteral("could not be saved"),
                                    Qt::CaseInsensitive));
    QCOMPARE(fixture.readActive().value().name, std::string("Fixture Teacher"));

    QVERIFY(executeSql(fixture.firstStore,
                       QStringLiteral("DROP TRIGGER fail_my_workspace_autosave")));
    QVERIFY(page->saveChanges());
    QVERIFY(!page->hasUnsavedChanges());
    QVERIFY(footer->isHidden());
    QCOMPARE(fixture.readActive().value().name, std::string("Autosave retry"));
}

void NextMyWorkspacePageTests::typedAndImageSignaturesPersistAndCanBeRemoved()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    Ui::MyWorkspacePage* const page = fixture.page();
    auto* const choose = page->findChild<QPushButton*>(
        QStringLiteral("nextMyWorkspaceChooseSignatureImage"));
    auto* const remove = page->findChild<QPushButton*>(
        QStringLiteral("nextMyWorkspaceRemoveSignatureImage"));
    auto* const typeMode = page->findChild<QPushButton*>(
        QStringLiteral("nextMyWorkspaceSignatureTypeMode"));
    auto* const typedText = page->findChild<QLineEdit*>(
        QStringLiteral("nextMyWorkspaceTypedSignature"));
    auto* const font = page->findChild<QPushButton*>(
        QStringLiteral("nextMyWorkspaceFontButton2"));
    QVERIFY(choose);
    QVERIFY(remove);
    QVERIFY(!page->findChild<QPushButton*>(
        QStringLiteral("nextMyWorkspaceSaveDetails")));
    QVERIFY(typeMode);
    QVERIFY(typedText);
    QVERIFY(font);

    QImage selectedImage(24, 12, QImage::Format_ARGB32_Premultiplied);
    selectedImage.fill(Qt::white);
    selectedImage.setPixelColor(0, 0, Qt::black);
    QBuffer imageBuffer(&fixture.chosenImage);
    QVERIFY(imageBuffer.open(QIODevice::WriteOnly));
    QVERIFY(selectedImage.save(&imageBuffer, "PNG"));
    imageBuffer.close();
    QTest::mouseClick(choose, Qt::LeftButton);
    QVERIFY(page->hasUnsavedChanges());
    QVERIFY(page->saveChanges());
    auto stored = fixture.readActive();
    QVERIFY(stored);
    QCOMPARE(stored.value().signatureMode, Application::PersonalSignatureMode::Image);
    QVERIFY(!stored.value().signatureImage.empty());
    const QByteArray storedImage(
        stored.value().signatureImage.value().data(),
        static_cast<qsizetype>(stored.value().signatureImage.value().size()));
    QVERIFY(storedImage.startsWith(QByteArray::fromHex("89504e470d0a1a0a")));
    const QImage decodedImage = QImage::fromData(storedImage, "PNG");
    QVERIFY(!decodedImage.isNull());
    QCOMPARE(decodedImage.pixelColor(23, 11).alpha(), 0);
    QCOMPARE(decodedImage.pixelColor(0, 0).alpha(), 255);
    QCOMPARE(choose->text(), QStringLiteral("Replace Signature..."));

    QImage replacementImage(24, 12, QImage::Format_ARGB32_Premultiplied);
    replacementImage.fill(QColor(30, 100, 160));
    fixture.chosenImage.clear();
    QBuffer replacementBuffer(&fixture.chosenImage);
    QVERIFY(replacementBuffer.open(QIODevice::WriteOnly));
    QVERIFY(replacementImage.save(&replacementBuffer, "PNG"));
    replacementBuffer.close();
    QTest::mouseClick(choose, Qt::LeftButton);
    QVERIFY(page->saveChanges());
    stored = fixture.readActive();
    QVERIFY(stored);
    const QByteArray replacedImage(
        stored.value().signatureImage.value().data(),
        static_cast<qsizetype>(stored.value().signatureImage.value().size()));
    QVERIFY(replacedImage != storedImage);
    QVERIFY(!QImage::fromData(replacedImage, "PNG").isNull());

    QTest::mouseClick(remove, Qt::LeftButton);
    QVERIFY(page->hasUnsavedChanges());
    QVERIFY(page->saveChanges());
    QVERIFY(fixture.readActive().value().signatureImage.empty());

    QTest::mouseClick(typeMode, Qt::LeftButton);
    typedText->setText(QString::fromUtf8("Ms. Kim / 🧭"));
    QTest::mouseClick(font, Qt::LeftButton);
    QVERIFY(page->hasUnsavedChanges());
    QVERIFY(page->saveChanges());
    stored = fixture.readActive();
    QVERIFY(stored);
    QCOMPARE(stored.value().signatureMode, Application::PersonalSignatureMode::Type);
    QCOMPARE(stored.value().typedSignatureText, utf8(QString::fromUtf8("Ms. Kim / 🧭")));
    QCOMPARE(stored.value().typedSignatureFont,
             Application::PersonalSignatureFontChoice::GreatVibes);
    QVERIFY(!stored.value().signatureImage.empty());
    const QByteArray typedImage(
        stored.value().signatureImage.value().data(),
        static_cast<qsizetype>(stored.value().signatureImage.value().size()));
    QVERIFY(typedImage.startsWith(QByteArray::fromHex("89504e470d0a1a0a")));

    SettingsRepository* const settings = fixture.firstStore.settingsRepository();
    QVERIFY(settings);
    const auto storedReportSignature = settings->loadSetting(
        QStringLiteral("myInfo/signatureImage"));
    QVERIFY(storedReportSignature);
    const QByteArray reportImageBytes = QByteArray::fromBase64(
        storedReportSignature->toString().toLatin1());
    QVERIFY(reportImageBytes.startsWith(
        QByteArray::fromHex("89504e470d0a1a0a")));
    QVERIFY(!QImage::fromData(reportImageBytes, "PNG").isNull());
}

void NextMyWorkspacePageTests::
signaturePreviewIsReleasedOnSuspendAndReloadedOnActivation()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    Ui::MyWorkspacePage* const page = fixture.page();
    auto* const preview = page->findChild<QLabel*>(
        QStringLiteral("nextMyWorkspaceSignaturePreview"));
    auto* const choose = page->findChild<QPushButton*>(
        QStringLiteral("nextMyWorkspaceChooseSignatureImage"));
    QVERIFY(preview && choose);
    QVERIFY(!page->findChild<QPushButton*>(
        QStringLiteral("nextMyWorkspaceSaveDetails")));

    page->resize(1024, 768);
    page->show();
    QCoreApplication::processEvents();

    QImage selectedImage(24, 12, QImage::Format_ARGB32_Premultiplied);
    selectedImage.fill(QColor(30, 100, 160));
    QBuffer imageBuffer(&fixture.chosenImage);
    QVERIFY(imageBuffer.open(QIODevice::WriteOnly));
    QVERIFY(selectedImage.save(&imageBuffer, "PNG"));
    imageBuffer.close();
    QTest::mouseClick(choose, Qt::LeftButton);
    QVERIFY(page->saveChanges());
    QVERIFY(!page->hasUnsavedChanges());
    QVERIFY(!preview->pixmap().isNull());

    page->onSuspending();
    QVERIFY(preview->pixmap().isNull());
    QVERIFY(page->onActivating());
    QVERIFY(!preview->pixmap().isNull());
    QVERIFY(!fixture.readActive().value().signatureImage.empty());
}

void NextMyWorkspacePageTests::
typedSignaturePreviewRendersTheSelectedTextAndFont()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    Ui::MyWorkspacePage* const page = fixture.page();
    auto* const preview = page->findChild<QLabel*>(
        QStringLiteral("nextMyWorkspaceSignaturePreview"));
    auto* const typeMode = page->findChild<QPushButton*>(
        QStringLiteral("nextMyWorkspaceSignatureTypeMode"));
    auto* const typedText = page->findChild<QLineEdit*>(
        QStringLiteral("nextMyWorkspaceTypedSignature"));
    auto* const dancingScript = page->findChild<QPushButton*>(
        QStringLiteral("nextMyWorkspaceFontButton1"));
    QVERIFY(preview && typeMode && typedText && dancingScript);

    fixture.host->resize(1024, 768);
    fixture.host->show();
    QCoreApplication::processEvents();
    typedText->setText(QStringLiteral("Alex Morgan"));
    QTest::mouseClick(typeMode, Qt::LeftButton);
    QTest::mouseClick(dancingScript, Qt::LeftButton);
    QCoreApplication::processEvents();

    const QPixmap displayed = preview->pixmap();
    QVERIFY(!displayed.isNull());
    const QImage rendered = displayed.toImage();
    QVERIFY(!rendered.isNull());
    qsizetype visibleSignaturePixels = 0;
    for (int y = 0; y < rendered.height(); ++y)
    {
        for (int x = 0; x < rendered.width(); ++x)
        {
            const QRgb pixel = rendered.pixel(x, y);
            if (qAlpha(pixel) > 32 && qGray(pixel) < 240)
            {
                ++visibleSignaturePixels;
            }
        }
    }
    QVERIFY(visibleSignaturePixels > 100);
    QVERIFY(dancingScript->isChecked());
}

void NextMyWorkspacePageTests::workspaceIdentityReplacementDiscardsOldDraftWithoutSavingIt()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    Platform::WorkspaceStorePersonalDetailsPort secondPort(fixture.secondStore);
    auto secondDetails = secondPort.read();
    QVERIFY(secondDetails);
    secondDetails.value().name = "Second Workspace Teacher";
    QVERIFY(secondPort.save(secondDetails.value()));

    Ui::MyWorkspacePage* const page = fixture.page();
    auto* const name = page->findChild<QLineEdit*>(
        QStringLiteral("nextMyWorkspaceName"));
    QVERIFY(name);
    name->setText(QStringLiteral("First Workspace Draft"));
    QVERIFY(page->hasUnsavedChanges());

    fixture.activeStore = &fixture.secondStore;
    fixture.activeIdentity = QStringLiteral("second-workspace");
    page->refreshForWorkspaceIdentity();
    QCOMPARE(name->text(), QStringLiteral("Second Workspace Teacher"));
    QVERIFY(!page->hasUnsavedChanges());
    QCOMPARE(Platform::WorkspaceStorePersonalDetailsPort(fixture.firstStore)
                 .read().value().name,
             std::string("Fixture Teacher"));
    QCOMPARE(fixture.readActive().value().name,
             std::string("Second Workspace Teacher"));
}

void NextMyWorkspacePageTests::automaticModeDebouncesSave()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    Ui::MyWorkspacePage* const page = fixture.page();
    auto* const name = page->findChild<QLineEdit*>(
        QStringLiteral("nextMyWorkspaceName"));
    auto* const footer = page->findChild<QWidget*>(
        QStringLiteral("nextMyWorkspaceStatusFooter"));
    QVERIFY(name);
    QVERIFY(footer);
    QVERIFY(!page->findChild<QPushButton*>(
        QStringLiteral("nextMyWorkspaceSaveDetails")));
    page->setSaveMode(Application::SaveMode::Automatic);
    QVERIFY(footer->isHidden());
    name->setText(QStringLiteral("Autosaved Teacher"));
    QVERIFY(page->hasUnsavedChanges());
    QVERIFY(footer->isHidden());
    QTRY_VERIFY_WITH_TIMEOUT(!page->hasUnsavedChanges(), 2000);
    QVERIFY(footer->isHidden());
    QCOMPARE(fixture.readActive().value().name, std::string("Autosaved Teacher"));
}

void NextMyWorkspacePageTests::
activePageChangesAdapterSavesAndDiscardsTheDetailsDraft()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    Ui::MyWorkspacePage* const page = fixture.page();
    auto* const name = page->findChild<QLineEdit*>(
        QStringLiteral("nextMyWorkspaceName"));
    QVERIFY(name);
    Ui::PageHostActivePageChangesAdapter activePage(*fixture.host);

    name->setText(QStringLiteral("Discard this draft"));
    QVERIFY(activePage.hasUnsavedChanges());
    QVERIFY(activePage.discardChanges());
    QVERIFY(!activePage.hasUnsavedChanges());
    QCOMPARE(name->text(), QStringLiteral("Fixture Teacher"));
    QCOMPARE(fixture.readActive().value().name, std::string("Fixture Teacher"));

    name->setText(QStringLiteral("Save through page adapter"));
    QVERIFY(activePage.hasUnsavedChanges());
    QVERIFY(activePage.saveChanges());
    QVERIFY(!activePage.hasUnsavedChanges());
    QCOMPARE(fixture.readActive().value().name,
             std::string("Save through page adapter"));
}

QTEST_MAIN(NextMyWorkspacePageTests)
#include "my_workspace_page_tests.moc"
