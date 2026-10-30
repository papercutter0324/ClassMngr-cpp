#include "core/resource_paths.h"
#include "data/database/workspace_store.h"
#include "features/my_info/data/signature_image_processor.h"
#include "features/my_info/data/typed_signature_renderer.h"
#include "features/calendar/ui/calendar_event_model.h"
#include "next/application/personal_details_save_use_case.h"
#include "next/application/my_workspace_schedule_read.h"
#include "next/ui/calendar_event_dialog.h"
#include "next/platform/workspace_store_personal_details_port.h"
#include "next/platform/workspace_store_my_workspace_schedule_read_port.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/my_workspace_page.h"
#include "next/ui/page_host.h"
#include "next/ui/page_host_active_page_changes_adapter.h"
#include "preferences_test_ports.h"

#include <QBuffer>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QQuickItem>
#include <QQuickWidget>
#include <QPalette>
#include <QScrollArea>
#include <QTabBar>
#include <QSqlQuery>
#include <QScopeGuard>
#include <QTableView>
#include <QTimer>
#include <QTemporaryDir>
#include <QTranslator>
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

bool addWorkspaceReadFixture(
    WorkspaceStore& store,
    const QString& grade,
    const QString& level,
    const QString& teacherName,
    const QString& room,
    const QString& eventPrefix,
    const int eventCount
    )
{
    QSqlQuery teacher(store.database());
    teacher.prepare(QStringLiteral(R"(
        INSERT INTO teachers (
            id, teacher_kr, teacher_en, preferred_name, room_number
        ) VALUES (1, ?, ?, ?, ?)
    )"));
    teacher.addBindValue(teacherName);
    teacher.addBindValue(teacherName);
    teacher.addBindValue(teacherName);
    teacher.addBindValue(room);
    if (!teacher.exec())
    {
        return false;
    }

    QSqlQuery classroom(store.database());
    classroom.prepare(QStringLiteral(
        "INSERT INTO classes (id, name) VALUES (101, ?)"));
    classroom.addBindValue(level);
    if (!classroom.exec())
    {
        return false;
    }

    QSqlQuery info(store.database());
    info.prepare(QStringLiteral(R"(
        INSERT INTO class_info (
            class_id, teacher_id, class_grade, class_level
        ) VALUES (101, 1, ?, ?)
    )"));
    info.addBindValue(grade);
    info.addBindValue(level);
    if (!info.exec())
    {
        return false;
    }

    QSqlQuery time(store.database());
    time.prepare(QStringLiteral(R"(
        INSERT INTO class_times (class_id, day, start_time, end_time)
        VALUES (101, 'Monday', '09:00', '10:00')
    )"));
    if (!time.exec())
    {
        return false;
    }

    QSqlQuery event(store.database());
    event.prepare(QStringLiteral(R"(
        INSERT INTO calendar_events (
            title, event_type, time_status, all_day, start_date, start_time,
            end_date, end_time
        ) VALUES (?, 'Meeting', 'Timed', 0, ?, '09:00', ?, '10:00')
    )"));
    const QDate today = QDate::currentDate();
    for (int index = 0; index < eventCount; ++index)
    {
        const QString date = today.addDays(index).toString(Qt::ISODate);
        event.bindValue(0, eventPrefix + QStringLiteral(" %1").arg(index, 2, 10, QLatin1Char('0')));
        event.bindValue(1, date);
        event.bindValue(2, date);
        if (!event.exec())
        {
            return false;
        }
    }
    QSqlQuery holiday(store.database());
    holiday.prepare(QStringLiteral(R"(
        INSERT INTO calendar_events (
            title, event_type, time_status, all_day, start_date, end_date
        ) VALUES (?, 'Holiday', 'Timed', 1, ?, ?)
    )"));
    const QString holidayDate = today.toString(Qt::ISODate);
    holiday.addBindValue(eventPrefix + QStringLiteral(" Holiday"));
    holiday.addBindValue(holidayDate);
    holiday.addBindValue(holidayDate);
    return holiday.exec();
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

class ActivationFailurePage final : public Ui::Page
{
public:
    explicit ActivationFailurePage(const Ui::PageContext& context)
        : Ui::Page(context)
    {
    }

    [[nodiscard]] Ui::PageOperationResult<void> onActivating() override
    {
        return std::unexpected(Ui::PageOperationError{
            QStringLiteral("injected activation failure")});
    }
};

struct PageFixture final
{
    QTemporaryDir directory;
    WorkspaceStore firstStore;
    WorkspaceStore secondStore;
    WorkspaceStore* activeStore = nullptr;
    QString activeIdentity = QStringLiteral("first");
    bool readFailure = false;
    bool saveFailure = false;
    bool scheduleReadFailure = false;
    int personalDetailsReadCount = 0;
    int scheduleReadCount = 0;
    QByteArray chosenImage;
    int warningCount = 0;
    Test::MemoryPreferences preferences;
    std::unique_ptr<Resources::ResourceCatalog> catalog;
    std::unique_ptr<Resources::ResourceLoader> loader;
    std::unique_ptr<Ui::PageHost> host;
    Ui::CalendarPage* standaloneCalendar = nullptr;

    Ui::CalendarPageActions calendarActions(const Ui::PageContext&) const
    {
        Ui::CalendarPageActions actions;
        actions.activeWorkspacePath = [this] {
            return activeStore
                ? activeStore->database().databaseName()
                : QString{};
        };
        actions.readCampusDirectory = [] {
            return std::vector<Application::CalendarPageCampusMetadata>{};
        };
        return actions;
    }

    bool initialize(const bool activateDetails = true)
    {
        if (!directory.isValid()
            || !firstStore.open(databasePath(directory, QStringLiteral("first")))
            || !applyFixture(firstStore)
            || !secondStore.open(databasePath(directory, QStringLiteral("second")))
            || !applyFixture(secondStore)
            || !addWorkspaceReadFixture(
                firstStore,
                QStringLiteral("Grade 4"),
                QStringLiteral("First Schedule"),
                QStringLiteral("First Teacher"),
                QStringLiteral("Room 101"),
                QStringLiteral("First Event"),
                12
                )
            || !addWorkspaceReadFixture(
                secondStore,
                QStringLiteral("Grade 5"),
                QStringLiteral("Second Schedule"),
                QStringLiteral("Second Teacher"),
                QStringLiteral("Room 202"),
                QStringLiteral("Second Event"),
                2
                ))
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
                    ++personalDetailsReadCount;
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
                actions.readSchedule = [this] {
                    ++scheduleReadCount;
                    if (scheduleReadFailure || !activeStore)
                    {
                        return Application::MyWorkspaceScheduleReadResult::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "forced workspace schedule read failure",
                            .recoverable = true
                        });
                    }
                    Platform::WorkspaceStoreMyWorkspaceScheduleReadPort port(
                        *activeStore);
                    return Application::MyWorkspaceScheduleReadUseCase::execute(port);
                };
                actions.createCalendarActions = [this](
                    const Ui::PageContext& calendarContext) {
                    return calendarActions(calendarContext);
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
            || !host->registerPage({
                .id = *Ui::PageId::fromString(QStringLiteral("calendar")),
                .label = QStringLiteral("Calendar"),
                .factory = [this](const Ui::PageContext& context)
                    -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
                    auto page = std::make_unique<Ui::CalendarPage>(
                        context, calendarActions(context));
                    standaloneCalendar = page.get();
                    return std::unique_ptr<Ui::Page>(std::move(page));
                }
            })
            || !host->registerPage({
                .id = *Ui::PageId::fromString(QStringLiteral("other")),
                .label = QStringLiteral("Other"),
                .factory = [](const Ui::PageContext& context)
                    -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
                    return std::unique_ptr<Ui::Page>(
                        std::make_unique<Ui::Page>(context));
                }
            })
            || !host->registerPage({
                .id = *Ui::PageId::fromString(QStringLiteral("activation_failure")),
                .label = QStringLiteral("Activation Failure"),
                .factory = [](const Ui::PageContext& context)
                    -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
                    return std::unique_ptr<Ui::Page>(
                        std::make_unique<ActivationFailurePage>(context));
                }
            })
            || !host->activatePage(QStringLiteral("my_workspace")))
        {
            return false;
        }
        if (activateDetails)
        {
            Ui::MyWorkspacePage* const activePage = page();
            auto* const tabs = activePage
                ? activePage->findChild<QTabBar*>(
                      QStringLiteral("nextMyWorkspaceTabBar"))
                : nullptr;
            if (!tabs)
            {
                return false;
            }
            tabs->setCurrentIndex(0);
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
    void workspaceScheduleAndCalendarContentAreLazyAndReleased();
    void scheduleWeekdayUsesLegacyCatalogWithoutChangingCanonicalSource();
    void scheduleRefreshUsesTheReplacedActiveStoreAndCloseReleasesRows();
    void scheduleReadErrorsExposeRetryAndEmptyStates();
    void scheduleSummaryReloadsAfterSuspendAndThemeLanguageChanges();
    void embeddedCalendarControlsFiltersAndDayActivationWork();
    void embeddedCalendarMatchesLegacyViewportAndLightCanvasInDarkTheme();
    void embeddedCalendarReleasesAndRestoresAcrossTabAndPageLifecycle();
    void standaloneAndEmbeddedCalendarTransitionsStayExclusiveAndRollbackSafely();
    void calendarWorkspaceReplacementAndCloseClearBothRoutes();
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

void NextMyWorkspacePageTests::
workspaceScheduleAndCalendarContentAreLazyAndReleased()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize(false));
    Ui::MyWorkspacePage* const page = fixture.page();
    auto* const tabs = page->findChild<QTabBar*>(
        QStringLiteral("nextMyWorkspaceTabBar"));
    auto* const scheduleTable = page->findChild<QTableView*>(
        QStringLiteral("nextMyWorkspaceScheduleTable"));
    auto* const calendar = page->findChild<Ui::CalendarPage*>(
        QStringLiteral("nextMyWorkspaceEmbeddedCalendar"));
    QVERIFY(tabs && scheduleTable && calendar);
    QCOMPARE(tabs->count(), 3);
    QCOMPARE(tabs->currentIndex(), 1);
    QCOMPARE(fixture.personalDetailsReadCount, 0);
    QCOMPARE(fixture.scheduleReadCount, 1);

    QAbstractItemModel* const scheduleModel = scheduleTable->model();
    QVERIFY(scheduleModel);
    QCOMPARE(scheduleModel->rowCount(), 1);
    QCOMPARE(scheduleModel->data(scheduleModel->index(0, 0)).toString(),
             QStringLiteral("Monday"));
    QVERIFY(scheduleModel->data(scheduleModel->index(0, 2)).toString()
                .contains(QStringLiteral("First Teacher")));
    QCOMPARE(scheduleModel->data(scheduleModel->index(0, 3)).toString(),
             QStringLiteral("Room 101"));
    QVERIFY(!calendar->calendarView());
    QVERIFY(!calendar->eventModel());
    QVERIFY(!calendar->eventCache());

    tabs->setCurrentIndex(2);
    QCOMPARE(fixture.scheduleReadCount, 1);
    QCOMPARE(fixture.personalDetailsReadCount, 0);
    QCOMPARE(scheduleModel->rowCount(), 0);
    QVERIFY(!calendar->calendarView());
    QCOMPARE(calendar->runtimeMetrics().liveHeavySessionCount, 0);

    fixture.host->show();
    QTRY_VERIFY_WITH_TIMEOUT(calendar->calendarView(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(
        !calendar->runtimeMetrics().cacheLoading, 5000);
    QVERIFY(calendar->eventModel());
    QVERIFY(calendar->eventCache());
    QVERIFY(calendar->upcomingList());
    QVERIFY(calendar->upcomingList()->count() <= 10);

    tabs->setCurrentIndex(1);
    QCOMPARE(fixture.scheduleReadCount, 2);
    QCOMPARE(calendar->runtimeMetrics().calendarViewCount, 0);
    QVERIFY(!calendar->eventModel());
    QVERIFY(!calendar->eventCache());
    QCOMPARE(calendar->runtimeMetrics().liveHeavySessionCount, 0);
    QCOMPARE(scheduleModel->rowCount(), 1);

    tabs->setCurrentIndex(2);
    QTRY_VERIFY_WITH_TIMEOUT(calendar->calendarView(), 5000);
    QCOMPARE(calendar->runtimeMetrics().calendarViewConstructionCount, 2);
    tabs->setCurrentIndex(0);
    QCOMPARE(fixture.personalDetailsReadCount, 1);
    QVERIFY(page->findChild<QLineEdit*>(QStringLiteral("nextMyWorkspaceName")));
}

void NextMyWorkspacePageTests::
scheduleWeekdayUsesLegacyCatalogWithoutChangingCanonicalSource()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize(false));
    Ui::MyWorkspacePage* const page = fixture.page();
    auto* const scheduleTable = page->findChild<QTableView*>(
        QStringLiteral("nextMyWorkspaceScheduleTable"));
    QVERIFY(scheduleTable && scheduleTable->model());
    QCOMPARE(scheduleTable->model()->data(
                 scheduleTable->model()->index(0, 0)).toString(),
             QStringLiteral("Monday"));

    QSqlQuery storedDay(fixture.firstStore.database());
    QVERIFY(storedDay.exec(QStringLiteral(
        "SELECT day FROM class_times WHERE class_id = 101")));
    QVERIFY(storedDay.next());
    QCOMPARE(storedDay.value(0).toString(), QStringLiteral("Monday"));

    const QString translationPath = QDir(QStringLiteral(
        CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY)).filePath(
            QStringLiteral("ClassMngr_ko_KR.qm"));
    QTranslator koreanTranslator;
    QVERIFY(koreanTranslator.load(translationPath));
    QVERIFY(QCoreApplication::installTranslator(&koreanTranslator));
    const auto removeTranslator = qScopeGuard([&koreanTranslator] {
        QCoreApplication::removeTranslator(&koreanTranslator);
    });
    Q_UNUSED(removeTranslator);

    QEvent languageChange(QEvent::LanguageChange);
    QCoreApplication::sendEvent(page, &languageChange);
    QCOMPARE(scheduleTable->model()->data(
                 scheduleTable->model()->index(0, 0)).toString(),
             QStringLiteral("월요일"));

    QCoreApplication::removeTranslator(&koreanTranslator);
    QEvent englishChange(QEvent::LanguageChange);
    QCoreApplication::sendEvent(page, &englishChange);
    QCOMPARE(scheduleTable->model()->data(
                 scheduleTable->model()->index(0, 0)).toString(),
             QStringLiteral("Monday"));
}

void NextMyWorkspacePageTests::
scheduleRefreshUsesTheReplacedActiveStoreAndCloseReleasesRows()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize(false));
    Ui::MyWorkspacePage* const page = fixture.page();
    auto* const tabs = page->findChild<QTabBar*>(
        QStringLiteral("nextMyWorkspaceTabBar"));
    auto* const scheduleTable = page->findChild<QTableView*>(
        QStringLiteral("nextMyWorkspaceScheduleTable"));
    auto* const scheduleStatus = page->findChild<QLabel*>(
        QStringLiteral("nextMyWorkspaceScheduleStatus"));
    QVERIFY(tabs && scheduleTable && scheduleStatus);

    QAbstractItemModel* const scheduleModel = scheduleTable->model();
    QVERIFY(scheduleModel);
    QVERIFY(scheduleModel->data(scheduleModel->index(0, 2)).toString()
                .contains(QStringLiteral("First Schedule")));

    fixture.activeStore = &fixture.secondStore;
    fixture.activeIdentity = QStringLiteral("second-workspace");
    page->refreshForWorkspaceIdentity();
    QCOMPARE(fixture.scheduleReadCount, 2);
    QCOMPARE(scheduleModel->rowCount(), 1);
    const QString currentSchedule =
        scheduleModel->data(scheduleModel->index(0, 2)).toString();
    QVERIFY(currentSchedule.contains(QStringLiteral("Second Schedule")));
    QVERIFY(!currentSchedule.contains(QStringLiteral("First Schedule")));

    page->clearForClosedWorkspace();
    QCOMPARE(scheduleModel->rowCount(), 0);
    QVERIFY(scheduleStatus->text().contains(QStringLiteral("No Teacher Profile")));
}

void NextMyWorkspacePageTests::scheduleReadErrorsExposeRetryAndEmptyStates()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize(false));
    Ui::MyWorkspacePage* const page = fixture.page();
    auto* const tabs = page->findChild<QTabBar*>(
        QStringLiteral("nextMyWorkspaceTabBar"));
    auto* const scheduleTable = page->findChild<QTableView*>(
        QStringLiteral("nextMyWorkspaceScheduleTable"));
    auto* const scheduleStatus = page->findChild<QLabel*>(
        QStringLiteral("nextMyWorkspaceScheduleStatus"));
    auto* const scheduleRetry = page->findChild<QPushButton*>(
        QStringLiteral("nextMyWorkspaceScheduleRetry"));
    QVERIFY(tabs && scheduleTable && scheduleStatus && scheduleRetry);

    fixture.scheduleReadFailure = true;
    tabs->setCurrentIndex(0);
    tabs->setCurrentIndex(1);
    QVERIFY(scheduleStatus->text().contains(QStringLiteral("could not be loaded")));
    QVERIFY(!scheduleRetry->isHidden());
    fixture.scheduleReadFailure = false;
    scheduleRetry->click();
    QCOMPARE(scheduleTable->model()->rowCount(), 1);
    QVERIFY(scheduleRetry->isHidden());

    QVERIFY(executeSql(fixture.firstStore, QStringLiteral("DELETE FROM class_times")));
    QVERIFY(executeSql(fixture.firstStore, QStringLiteral("DELETE FROM calendar_events")));
    tabs->setCurrentIndex(1);
    tabs->setCurrentIndex(0);
    tabs->setCurrentIndex(1);
    QCOMPARE(scheduleTable->model()->rowCount(), 0);
    QVERIFY(scheduleStatus->text().contains(QStringLiteral("no schedule entries")));
}

void NextMyWorkspacePageTests::
scheduleSummaryReloadsAfterSuspendAndThemeLanguageChanges()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize(false));
    Ui::MyWorkspacePage* const page = fixture.page();
    auto* const tabs = page->findChild<QTabBar*>(
        QStringLiteral("nextMyWorkspaceTabBar"));
    auto* const scheduleTable = page->findChild<QTableView*>(
        QStringLiteral("nextMyWorkspaceScheduleTable"));
    QVERIFY(tabs && scheduleTable);
    QCOMPARE(fixture.scheduleReadCount, 1);

    QEvent paletteChange(QEvent::PaletteChange);
    QCoreApplication::sendEvent(page, &paletteChange);
    QCOMPARE(fixture.scheduleReadCount, 2);
    QCOMPARE(scheduleTable->model()->rowCount(), 1);

    QEvent languageChange(QEvent::LanguageChange);
    QCoreApplication::sendEvent(page, &languageChange);
    QCOMPARE(fixture.scheduleReadCount, 3);
    QCOMPARE(scheduleTable->model()->rowCount(), 1);

    page->onSuspending();
    QCOMPARE(scheduleTable->model()->rowCount(), 0);
    QVERIFY(page->onActivating());
    QCOMPARE(fixture.scheduleReadCount, 4);
    QCOMPARE(scheduleTable->model()->rowCount(), 1);
}

void NextMyWorkspacePageTests::embeddedCalendarControlsFiltersAndDayActivationWork()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize(false));
    Ui::MyWorkspacePage* const page = fixture.page();
    auto* const tabs = page->findChild<QTabBar*>(
        QStringLiteral("nextMyWorkspaceTabBar"));
    auto* const calendar = page->findChild<Ui::CalendarPage*>(
        QStringLiteral("nextMyWorkspaceEmbeddedCalendar"));
    QVERIFY(tabs && calendar);
    tabs->setCurrentIndex(2);
    fixture.host->resize(1280, 920);
    fixture.host->show();
    QTRY_VERIFY_WITH_TIMEOUT(calendar->calendarView(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(calendar->calendarView()->rootObject(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!calendar->runtimeMetrics().cacheLoading, 5000);

    QVERIFY(!calendar->findChild<QPushButton*>(QStringLiteral("calendarNewEvent")));
    QVERIFY(!calendar->findChild<QPushButton*>(QStringLiteral("calendarPreferences")));
    auto* const scope = calendar->findChild<QComboBox*>(
        QStringLiteral("calendarUpcomingScope"));
    QListWidget* const upcoming = calendar->upcomingList();
    QVERIFY(scope && upcoming);
    QCOMPARE(scope->count(), 3);
    QCOMPARE(scope->itemText(0), QStringLiteral("This month"));
    QCOMPARE(scope->itemText(1), QStringLiteral("Next 30 days"));
    QCOMPARE(scope->itemText(2), QStringLiteral("Next 10 events"));
    QCOMPARE(calendar->runtimeMetrics().upcomingRangeDayCount,
             QDate::currentDate().daysInMonth());

    scope->setCurrentIndex(1);
    QCOMPARE(calendar->runtimeMetrics().upcomingRangeDayCount, 30);
    scope->setCurrentIndex(2);
    QCOMPARE(calendar->runtimeMetrics().upcomingRangeDayCount, 366);
    QTRY_VERIFY_WITH_TIMEOUT(!calendar->runtimeMetrics().cacheLoading, 5000);
    QVERIFY(upcoming->count() <= 10);

    auto* const holidayFilter = calendar->findChild<QPushButton*>(
        QStringLiteral("calendarEventTypeFilter1"));
    QVERIFY(holidayFilter && holidayFilter->isChecked());
    const auto containsUpcoming = [upcoming](const QString& title) {
        for (int index = 0; index < upcoming->count(); ++index)
        {
            if (upcoming->item(index)->text().contains(title))
            {
                return true;
            }
        }
        return false;
    };
    QTRY_VERIFY_WITH_TIMEOUT(
        containsUpcoming(QStringLiteral("First Event Holiday")), 5000);
    const QDate holidayDate = QDate::currentDate();
    const auto gridContainsHoliday = [calendar, holidayDate] {
        const QVariantList events = calendar->eventModel()->eventsForDate(
            holidayDate.year(), holidayDate.month(), holidayDate.day());
        for (const QVariant& event : events)
        {
            if (event.toMap().value(QStringLiteral("eventType")).toString()
                == QStringLiteral("Holiday"))
            {
                return true;
            }
        }
        return false;
    };
    QTRY_VERIFY_WITH_TIMEOUT(gridContainsHoliday(), 5000);
    holidayFilter->click();
    QTRY_VERIFY_WITH_TIMEOUT(
        !containsUpcoming(QStringLiteral("First Event Holiday")), 5000);
    QVERIFY(gridContainsHoliday());

    bool draftOpened = false;
    QDate draftDate;
    const QDate selectedDate = QDate::currentDate().addDays(3);
    QTimer::singleShot(0, calendar, [&draftOpened, &draftDate] {
        auto* const dialog = qobject_cast<Ui::CalendarEventDialog*>(
            QApplication::activeModalWidget());
        auto* const startDate = dialog
            ? dialog->findChild<QDateEdit*>(
                  QStringLiteral("calendarEventStartDate"))
            : nullptr;
        auto* const buttons = dialog
            ? dialog->findChild<QDialogButtonBox*>(
                  QStringLiteral("calendarEventButtons"))
            : nullptr;
        if (startDate && buttons && buttons->button(QDialogButtonBox::Cancel))
        {
            draftOpened = true;
            draftDate = startDate->date();
            buttons->button(QDialogButtonBox::Cancel)->click();
        }
    });
    QVERIFY(QMetaObject::invokeMethod(
        calendar,
        "handleCalendarDayActivated",
        Qt::DirectConnection,
        Q_ARG(int, selectedDate.year()),
        Q_ARG(int, selectedDate.month()),
        Q_ARG(int, selectedDate.day())));
    QVERIFY(draftOpened);
    QCOMPARE(draftDate, selectedDate);
}

void NextMyWorkspacePageTests::
embeddedCalendarMatchesLegacyViewportAndLightCanvasInDarkTheme()
{
    const QString previousStyle = QApplication::style()->objectName();
    const auto restoreStyle = qScopeGuard([previousStyle] {
        QApplication::setStyle(previousStyle);
    });
    QApplication::setStyle(QStringLiteral("Fusion"));

    PageFixture fixture;
    QVERIFY(fixture.initialize(false));
    Ui::MyWorkspacePage* const page = fixture.page();
    auto* const tabs = page->findChild<QTabBar*>(
        QStringLiteral("nextMyWorkspaceTabBar"));
    auto* const viewport = page->findChild<QScrollArea*>(
        QStringLiteral("nextMyWorkspaceCalendarScrollArea"));
    auto* const calendar = page->findChild<Ui::CalendarPage*>(
        QStringLiteral("nextMyWorkspaceEmbeddedCalendar"));
    QVERIFY(tabs && viewport && calendar);

    const QPalette previousPalette = QApplication::palette();
    const auto restorePalette = qScopeGuard([previousPalette] {
        QApplication::setPalette(previousPalette);
    });
    QPalette darkPalette = previousPalette;
    darkPalette.setColor(QPalette::Window, QColor(QStringLiteral("#202326")));
    darkPalette.setColor(QPalette::Base, QColor(QStringLiteral("#292c30")));
    darkPalette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#30343a")));
    darkPalette.setColor(QPalette::Text, QColor(QStringLiteral("#f1f3f4")));
    darkPalette.setColor(QPalette::Mid, QColor(QStringLiteral("#4b5056")));
    QApplication::setPalette(darkPalette);

    fixture.host->resize(1024, 768);
    fixture.host->show();
    tabs->setCurrentIndex(2);
    QTRY_VERIFY_WITH_TIMEOUT(calendar->calendarView(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(calendar->calendarView()->rootObject(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(viewport->mapTo(page, QPoint{}), QPoint(44, 185), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(viewport->size(), QSize(936, 495), 5000);

    QObject* const root = calendar->calendarView()->rootObject();
    QCOMPARE(root->property("calendarBackground").value<QColor>(),
             QColor(QStringLiteral("#f5f3ee")));
    QCOMPARE(root->property("headerBackground").value<QColor>(),
             QColor(QStringLiteral("#e2e1dc")));
    QCOMPARE(root->property("cellBackground").value<QColor>(),
             QColor(QStringLiteral("#f5f3ee")));
    QCOMPARE(root->property("weekendBackground").value<QColor>(),
             QColor(QStringLiteral("#e8edf1")));
    QCOMPARE(root->property("gridLineColor").value<QColor>(),
             QColor(QStringLiteral("#cbc9c2")));
}

void NextMyWorkspacePageTests::
embeddedCalendarReleasesAndRestoresAcrossTabAndPageLifecycle()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize(false));
    Ui::MyWorkspacePage* const page = fixture.page();
    auto* const tabs = page->findChild<QTabBar*>(
        QStringLiteral("nextMyWorkspaceTabBar"));
    auto* const calendar = page->findChild<Ui::CalendarPage*>(
        QStringLiteral("nextMyWorkspaceEmbeddedCalendar"));
    QVERIFY(tabs && calendar);
    tabs->setCurrentIndex(2);
    fixture.host->resize(1280, 920);
    fixture.host->show();
    QTRY_VERIFY_WITH_TIMEOUT(calendar->calendarView(), 5000);
    QCOMPARE(calendar->runtimeMetrics().liveHeavySessionCount, 1);

    auto* const scope = calendar->findChild<QComboBox*>(
        QStringLiteral("calendarUpcomingScope"));
    auto* const filter = calendar->findChild<QPushButton*>(
        QStringLiteral("calendarEventTypeFilter1"));
    QVERIFY(scope && filter);
    scope->setCurrentIndex(2);
    filter->click();
    QVERIFY(!filter->isChecked());
    const int constructions = calendar->runtimeMetrics()
        .calendarViewConstructionCount;

    tabs->setCurrentIndex(1);
    QCOMPARE(calendar->runtimeMetrics().calendarViewCount, 0);
    QVERIFY(!calendar->eventCache());
    QCOMPARE(calendar->runtimeMetrics().liveHeavySessionCount, 0);
    tabs->setCurrentIndex(2);
    QTRY_VERIFY_WITH_TIMEOUT(calendar->calendarView(), 5000);
    QCOMPARE(calendar->runtimeMetrics().calendarViewConstructionCount,
             constructions + 1);
    QCOMPARE(calendar->findChild<QComboBox*>(
                 QStringLiteral("calendarUpcomingScope"))->currentIndex(), 2);
    QVERIFY(!calendar->findChild<QPushButton*>(
        QStringLiteral("calendarEventTypeFilter1"))->isChecked());
    QCOMPARE(calendar->runtimeMetrics().liveHeavySessionCount, 1);

    QVERIFY(fixture.host->activatePage(QStringLiteral("other")));
    QCOMPARE(calendar->runtimeMetrics().calendarViewCount, 0);
    QCOMPARE(calendar->runtimeMetrics().liveHeavySessionCount, 0);
    QVERIFY(fixture.host->activatePage(QStringLiteral("my_workspace")));
    QTRY_VERIFY_WITH_TIMEOUT(calendar->calendarView(), 5000);
    QCOMPARE(calendar->runtimeMetrics().calendarViewConstructionCount,
             constructions + 2);
    QCOMPARE(calendar->findChild<QComboBox*>(
                 QStringLiteral("calendarUpcomingScope"))->currentIndex(), 2);
    QVERIFY(!calendar->findChild<QPushButton*>(
        QStringLiteral("calendarEventTypeFilter1"))->isChecked());
    QCOMPARE(calendar->runtimeMetrics().liveHeavySessionCount, 1);
    QVERIFY(calendar->runtimeMetrics().peakLiveHeavySessionCount <= 1);
}

void NextMyWorkspacePageTests::
standaloneAndEmbeddedCalendarTransitionsStayExclusiveAndRollbackSafely()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize(false));
    Ui::MyWorkspacePage* const page = fixture.page();
    auto* const tabs = page->findChild<QTabBar*>(
        QStringLiteral("nextMyWorkspaceTabBar"));
    auto* const embedded = page->findChild<Ui::CalendarPage*>(
        QStringLiteral("nextMyWorkspaceEmbeddedCalendar"));
    QVERIFY(tabs && embedded);
    tabs->setCurrentIndex(2);
    fixture.host->resize(1280, 920);
    fixture.host->show();
    QTRY_VERIFY_WITH_TIMEOUT(embedded->calendarView(), 5000);
    QQuickWidget* const embeddedView = embedded->calendarView();

    const auto activationFailure = fixture.host->activatePage(
        QStringLiteral("activation_failure"));
    QVERIFY(!activationFailure);
    QCOMPARE(fixture.host->activePageKey(),
             std::optional<QString>(QStringLiteral("my_workspace")));
    QCOMPARE(embedded->calendarView(), embeddedView);
    QCOMPARE(embedded->runtimeMetrics().liveHeavySessionCount, 1);

    QVERIFY(fixture.host->activatePage(QStringLiteral("calendar")));
    QVERIFY(!embedded->calendarView());
    QVERIFY(fixture.standaloneCalendar);
    QTRY_VERIFY_WITH_TIMEOUT(fixture.standaloneCalendar->calendarView(), 5000);
    QCOMPARE(fixture.standaloneCalendar->runtimeMetrics().liveHeavySessionCount, 1);
    QVERIFY(!embedded->eventModel());
    QVERIFY(!embedded->eventCache());

    QVERIFY(fixture.host->activatePage(QStringLiteral("my_workspace")));
    QTRY_VERIFY_WITH_TIMEOUT(embedded->calendarView(), 5000);
    QVERIFY(!fixture.standaloneCalendar->calendarView());
    QCOMPARE(embedded->runtimeMetrics().liveHeavySessionCount, 1);
    QVERIFY(embedded->runtimeMetrics().peakLiveHeavySessionCount <= 1);
}

void NextMyWorkspacePageTests::calendarWorkspaceReplacementAndCloseClearBothRoutes()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize(false));
    Ui::MyWorkspacePage* const page = fixture.page();
    auto* const tabs = page->findChild<QTabBar*>(
        QStringLiteral("nextMyWorkspaceTabBar"));
    auto* const embedded = page->findChild<Ui::CalendarPage*>(
        QStringLiteral("nextMyWorkspaceEmbeddedCalendar"));
    QVERIFY(tabs && embedded);
    tabs->setCurrentIndex(2);
    fixture.host->resize(1280, 920);
    fixture.host->show();
    QTRY_VERIFY_WITH_TIMEOUT(embedded->calendarView(), 5000);
    const auto containsUpcoming = [](const Ui::CalendarPage* calendar,
                                     const QString& title) {
        const QListWidget* const upcoming = calendar->upcomingList();
        if (!upcoming)
        {
            return false;
        }
        for (int index = 0; index < upcoming->count(); ++index)
        {
            if (upcoming->item(index)->text().contains(title))
            {
                return true;
            }
        }
        return false;
    };
    QTRY_VERIFY_WITH_TIMEOUT(
        containsUpcoming(embedded, QStringLiteral("First Event 00")), 5000);

    fixture.activeStore = &fixture.secondStore;
    fixture.activeIdentity = QStringLiteral("second-workspace");
    page->refreshForWorkspaceIdentity();
    QTRY_VERIFY_WITH_TIMEOUT(
        containsUpcoming(embedded, QStringLiteral("Second Event 00")), 5000);
    QVERIFY(!containsUpcoming(embedded, QStringLiteral("First Event 00")));

    fixture.activeStore = nullptr;
    fixture.activeIdentity.clear();
    page->clearForClosedWorkspace();
    QTRY_COMPARE_WITH_TIMEOUT(embedded->runtimeMetrics().cacheEventCount, 0, 5000);
    QCOMPARE(embedded->upcomingList()->count(), 0);

    QVERIFY(fixture.host->activatePage(QStringLiteral("calendar")));
    QVERIFY(fixture.standaloneCalendar);
    QTRY_VERIFY_WITH_TIMEOUT(fixture.standaloneCalendar->calendarView(), 5000);
    QCOMPARE(fixture.standaloneCalendar->runtimeMetrics().cacheEventCount, 0);

    fixture.activeStore = &fixture.firstStore;
    fixture.activeIdentity = QStringLiteral("first");
    fixture.standaloneCalendar->refreshForWorkspaceIdentity();
    QTRY_VERIFY_WITH_TIMEOUT(
        containsUpcoming(fixture.standaloneCalendar, QStringLiteral("First Event 00")),
        5000);
    fixture.activeStore = &fixture.secondStore;
    fixture.activeIdentity = QStringLiteral("second-workspace");
    fixture.standaloneCalendar->refreshForWorkspaceIdentity();
    QTRY_VERIFY_WITH_TIMEOUT(
        containsUpcoming(fixture.standaloneCalendar, QStringLiteral("Second Event 00")),
        5000);
    QVERIFY(!containsUpcoming(
        fixture.standaloneCalendar, QStringLiteral("First Event 00")));

    fixture.activeStore = nullptr;
    fixture.activeIdentity.clear();
    fixture.standaloneCalendar->clearForClosedWorkspace();
    QTRY_COMPARE_WITH_TIMEOUT(
        fixture.standaloneCalendar->runtimeMetrics().cacheEventCount, 0, 5000);
    QCOMPARE(fixture.standaloneCalendar->upcomingList()->count(), 0);
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
