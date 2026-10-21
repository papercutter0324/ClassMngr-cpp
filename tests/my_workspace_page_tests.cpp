#include "app/services/feature_services.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "features/my_info/ui/personal_details_page.h"
#include "fakes/fake_user_prompt_service.h"
#include "features/calendar/ui/calendar_page.h"
#include "features/schedule/ui/schedule_page.h"
#include "features/schedule/ui/schedule_widget.h"
#include "next/application/schedule_display_mode_preferences.h"
#include "next/platform/application_services_schedule_display_mode_preferences_port.h"
#include "next/platform/application_services_schedule_display_preferences_port.h"
#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "features/my_info/data/signature_image_processor.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/pages/autosave_coordinator.h"
#include "ui/shared/widgets/navigation_tab_widget.h"

#include <QApplication>
#include <QBuffer>
#include <QCoreApplication>
#include <QCheckBox>
#include <QImage>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QPointer>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSqlQuery>
#include <QEvent>
#include <QTemporaryDir>
#include <QTableWidget>
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

class ScopedUserPromptService final
{
public:
    explicit ScopedUserPromptService(IUserPromptService& service)
    {
        DialogServices::setUserPromptServiceForTesting(&service);
    }

    ~ScopedUserPromptService()
    {
        DialogServices::setUserPromptServiceForTesting(nullptr);
    }

    ScopedUserPromptService(const ScopedUserPromptService&) = delete;
    ScopedUserPromptService& operator=(const ScopedUserPromptService&) = delete;
};

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

bool installFailingSignaturePreferenceReadView(
    ApplicationServices& services
    )
{
    return executeSql(
               services,
               QStringLiteral(
                   "ALTER TABLE app_settings "
                   "RENAME TO app_settings_preserved"
                   )
               )
        && executeSql(
            services,
            QStringLiteral(R"(
                CREATE VIEW app_settings AS
                SELECT
                    key,
                    CASE
                        WHEN key IN (
                            'myInfo/signatureMode',
                            'myInfo/typedSignatureFont',
                            'myInfo/typedSignatureText'
                        )
                        THEN abs(-9223372036854775807 - 1)
                        ELSE value
                    END AS value
                FROM app_settings_preserved
            )")
            );
}

bool installFailingSignatureImageReadView(
    ApplicationServices& services
    )
{
    return executeSql(
               services,
               QStringLiteral(
                   "ALTER TABLE app_settings "
                   "RENAME TO app_settings_preserved"
                   )
               )
        && executeSql(
            services,
            QStringLiteral(R"(
                CREATE VIEW app_settings AS
                SELECT
                    key,
                    CASE
                        WHEN key = 'myInfo/signatureImage'
                        THEN abs(-9223372036854775807 - 1)
                        ELSE value
                    END AS value
                FROM app_settings_preserved
            )")
            );
}

bool installFailingCurrentCampusReadView(
    ApplicationServices& services
    )
{
    return executeSql(
               services,
               QStringLiteral(
                   "ALTER TABLE app_settings "
                   "RENAME TO app_settings_preserved"
                   )
               )
        && executeSql(
            services,
            QStringLiteral(R"(
                CREATE VIEW app_settings AS
                SELECT
                    key,
                    CASE
                        WHEN key = 'myInfo/campus'
                        THEN abs(-9223372036854775807 - 1)
                        ELSE value
                    END AS value
                FROM app_settings_preserved
            )")
            );
}

bool installFailingZoomUnavailableReadView(
    ApplicationServices& services
    )
{
    return executeSql(
               services,
               QStringLiteral(
                   "ALTER TABLE app_settings "
                   "RENAME TO app_settings_preserved"
                   )
               )
        && executeSql(
            services,
            QStringLiteral(R"(
                CREATE VIEW app_settings AS
                SELECT
                    key,
                    CASE
                        WHEN key = 'myInfo/zoomNotAvailable'
                        THEN abs(-9223372036854775807 - 1)
                        ELSE value
                    END AS value
                FROM app_settings_preserved
            )")
            );
}

bool restoreSettingsTable(ApplicationServices& services)
{
    return executeSql(services, QStringLiteral("DROP VIEW app_settings"))
        && executeSql(
            services,
            QStringLiteral(
                "ALTER TABLE app_settings_preserved RENAME TO app_settings"
                )
            );
}

void refreshPersonalDetails(MyWorkspacePage& page)
{
    page.show();
    page.openTab(WorkspaceTab::Details);
    QApplication::processEvents();
    page.personalDetailsPage()->refresh();
    QApplication::processEvents();
}

QStringList workspaceScheduleFacts(ApplicationServices& services)
{
    DataService* const dataService = services.dataService();
    if (!dataService || !dataService->databaseSession())
    {
        return {};
    }

    QSqlQuery query(dataService->databaseSession()->database());
    if (!query.exec(QStringLiteral(R"(
        SELECT 'intensive|' || day || '|' || start_time || '|' || end_time
        FROM class_intensive_times
        WHERE class_id = 6042
        UNION ALL
        SELECT 'regular|' || day || '|' || start_time || '|' || end_time
        FROM class_times
        WHERE class_id = 6042
        ORDER BY 1
    )")))
    {
        return {};
    }

    QStringList facts;
    while (query.next())
    {
        facts.append(query.value(0).toString());
    }
    return facts;
}

int workspaceScheduleModeWriteCount(ApplicationServices& services)
{
    DataService* const dataService = services.dataService();
    if (!dataService || !dataService->databaseSession())
    {
        return -1;
    }

    QSqlQuery query(dataService->databaseSession()->database());
    if (!query.exec(QStringLiteral(
            "SELECT count FROM workspace_schedule_mode_write_count"
            ))
        || !query.next())
    {
        return -1;
    }

    return query.value(0).toInt();
}

int campusCorrectionWriteAttempts(ApplicationServices& services)
{
    DataService* const dataService = services.dataService();
    if (!dataService || !dataService->databaseSession())
    {
        return -1;
    }

    QSqlQuery query(dataService->databaseSession()->database());
    if (
        !query.exec(QStringLiteral(
            "SELECT attempts FROM campus_correction_write_attempts"
            ))
        || !query.next()
        )
    {
        return -1;
    }

    return query.value(0).toInt();
}

bool installFailingCampusCorrectionTrigger(ApplicationServices& services)
{
    return executeSql(
        services,
        QStringLiteral(R"(
            CREATE TRIGGER fail_current_campus_correction
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'myInfo/campus'
            BEGIN
                UPDATE campus_correction_write_attempts
                SET attempts = attempts + 1;
                SELECT RAISE(FAIL, 'forced current campus correction failure');
            END
        )")
        );
}

QList<QPointer<QWidget>> workspaceScheduleCellWidgets(
    QTableWidget* table
    )
{
    QList<QPointer<QWidget>> widgets;
    if (!table)
    {
        return widgets;
    }

    for (int row = 0; row < table->rowCount(); ++row)
    {
        for (int column = 0; column < table->columnCount(); ++column)
        {
            if (QWidget* const widget = table->cellWidget(row, column))
            {
                widgets.append(widget);
            }
        }
    }
    return widgets;
}

void processDeferredWidgetDeletes()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
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
    void absentDetailsLifecycleDoesNotConstructPage();
    void lazyDetailsLoadsSettingsAndRetainsManualDraft_data();
    void lazyDetailsLoadsSettingsAndRetainsManualDraft();
    void releasesWorkspaceScheduleResourcesAndRestoresOnReentry();
    void defersCalendarConstructionWhileOtherTabsAreOpen();
    void initializesCalendarWithoutChangingTheActiveWorkspaceTab();
    void isAvailableAsTheDefaultTopLevelPage();
    void receivesTheTopLevelDatabaseState();
    void storedSignatureImageIsPreparedForPreview();
    void missingCorruptAndUnavailableSignatureImagesStayEmpty();
    void storedCampusPrefillsAndCorrectsThroughTypedPort();
    void failedCampusCorrectionRetriesAndSuppressesRepeatedWarnings();
    void failedMatchedCampusIdCorrectionWarnsAndRecovers();
    void settingsReadFailurePreservesDisplayedCampusChoice();
    void currentCampusReadFailurePreservesChoiceAndBlocksSaveUntilReload();
    void storedDisplayNamePrefillsWithUtf8AndWhitespace();
    void displayNameReadFailurePreservesDraftAndBlocksSaveUntilReload();
    void missingAndUnavailableDisplayNamePrefillEmpty();
    void storedZoomCredentialsPrefillAndRespectUnavailableState();
    void missingZoomValuesUseNaFallbackAndDisableFields();
    void legacyZoomValuesMigrateDuringPersonalDetailsLoad();
    void zoomPreferenceReadFailurePreservesControlsAndBlocksSaveUntilReload();
    void storedSignaturePreferencesPopulateModeTextAndFont();
    void signatureFontChoicesMapToPersistedValuesThroughMyWorkspace();
    void signaturePreferenceReadFailurePreservesControlsAndBlocksSaveUntilReload();
    void signatureImageReadFailurePreservesImageAndBlocksSaveUntilReload();
    void unavailableSettingsLeavePersonalDetailsFieldsUnchangedOnLoad();
    void unavailablePersonalDetailsSaveDoesNotNormalizeOrClearDirtyValues();
    void aggregateSavePersistsAllPersonalDetailsKeys();
    void aggregateSaveFailureRollsBackAndPreservesUnrelatedSettings();
    void aggregateSaveNoninteractiveFailureShowsStatusAndAllowsRetry();
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
    QVERIFY(!page.personalDetailsPage());
    QVERIFY(page.findChildren<PersonalDetailsPage*>().isEmpty());
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
    QVERIFY(!page.personalDetailsPage());
    SchedulePage* const schedule = page.schedulePage();

    page.openTab(WorkspaceTab::Details);
    PersonalDetailsPage* const details = page.personalDetailsPage();
    QVERIFY(details);
    page.openTab(WorkspaceTab::Schedule);
    page.openTab(WorkspaceTab::Details);

    QCOMPARE(page.personalDetailsPage(), details);
    QCOMPARE(page.schedulePage(), schedule);
}

void MyWorkspacePageTests::absentDetailsLifecycleDoesNotConstructPage()
{
    MyWorkspacePage page(nullptr);
    page.setSaveMode(SaveMode::Manual);
    page.setDatabaseOpen(true);
    page.markStale();
    page.refresh();
    page.retranslateUi();
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(page.saveChanges());
    page.saveData();
    page.discardChanges();
    page.clearDatabaseState();
    page.setDatabaseOpen(false);
    QVERIFY(!page.personalDetailsPage());
    QVERIFY(page.findChildren<PersonalDetailsPage*>().isEmpty());
    QCOMPARE(page.currentTab(), WorkspaceTab::Schedule);
}

void MyWorkspacePageTests::lazyDetailsLoadsSettingsAndRetainsManualDraft_data()
{
    QTest::addColumn<bool>("selectTabDirectly");
    QTest::newRow("openTab") << false;
    QTest::newRow("tabSelection") << true;
}

void MyWorkspacePageTests::lazyDetailsLoadsSettingsAndRetainsManualDraft()
{
    QFETCH(bool, selectTabDirectly);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(openDatabase(services, directory));
    const QString storedName = QStringLiteral("Stored before Details exists");
    QVERIFY(services.dataService()->saveSetting(QString::fromUtf8(DisplayNameKey), storedName));

    MyWorkspacePage page(&services);
    page.setDatabaseOpen(true);
    page.setSaveMode(SaveMode::Manual);
    page.show();
    QApplication::processEvents();
    QVERIFY(!page.personalDetailsPage());
    QVERIFY(page.findChildren<PersonalDetailsPage*>().isEmpty());
    auto* tabs = page.findChild<NavigationTabWidget*>(QStringLiteral("myWorkspaceTabs"));
    QVERIFY(tabs);
    SchedulePage* const schedule = page.schedulePage();
    QSignalSpy tabChanges(tabs, &NavigationTabWidget::currentChanged);
    QVERIFY(tabChanges.isValid());
    if (selectTabDirectly)
        tabs->setCurrentIndex(0);
    else
        page.openTab(WorkspaceTab::Details);
    PersonalDetailsPage* const details = page.personalDetailsPage();
    QVERIFY(details);
    QCOMPARE(tabChanges.size(), 1);
    QCOMPARE(page.findChildren<PersonalDetailsPage*>().size(), 1);
    QCOMPARE(tabs->widget(0), details);
    QCOMPARE(tabs->widget(1), schedule);
    QCOMPARE(tabs->count(), 3);
    QCOMPARE(tabs->tabText(0), QStringLiteral("My Details"));
    QCOMPARE(tabs->tabText(1), QStringLiteral("My Schedule"));
    QCOMPARE(tabs->tabText(2), QStringLiteral("Calendar"));
    QVERIFY(!details->needsRefresh());
    QLineEdit* const name = personalNameEditor(page);
    QVERIFY(name);
    QVERIFY(name->isVisibleTo(details));
    QCOMPARE(name->text(), storedName);
    auto* autosave = details->findChild<AutosaveCoordinator*>();
    QVERIFY(autosave);
    QCOMPARE(autosave->saveMode(), SaveMode::Manual);
    QSignalSpy saves(autosave, &AutosaveCoordinator::saveRequested);
    QVERIFY(saves.isValid());
    const QString draft = QStringLiteral("Unsaved Manual draft");
    name->setText(draft);
    QVERIFY(page.hasUnsavedChanges());
    page.openTab(WorkspaceTab::Schedule);
    QVERIFY(!page.calendarPage());
    page.openTab(WorkspaceTab::Calendar);
    QVERIFY(page.calendarPage());
    page.markStale();
    page.openTab(WorkspaceTab::Details);
    QApplication::processEvents();
    QCOMPARE(page.personalDetailsPage(), details);
    QCOMPARE(page.findChildren<PersonalDetailsPage*>().size(), 1);
    QCOMPARE(name->text(), draft);
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(saves.size(), 0);
    const auto stored = services.dataService()->loadSetting(QString::fromUtf8(DisplayNameKey));
    QVERIFY(stored);
    QCOMPARE(stored.value().toString(), storedName);
    QCOMPARE(tabs->count(), 3);
    QCOMPARE(tabs->widget(0), details);
    QCOMPARE(tabs->widget(1), schedule);
    QCOMPARE(tabs->widget(2), page.calendarPage());
    QCOMPARE(tabs->tabText(0), QStringLiteral("My Details"));
    QCOMPARE(tabs->tabText(1), QStringLiteral("My Schedule"));
    QCOMPARE(tabs->tabText(2), QStringLiteral("Calendar"));
}

void MyWorkspacePageTests
    ::releasesWorkspaceScheduleResourcesAndRestoresOnReentry()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(openDatabase(services, directory));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "INSERT INTO classes (id, name) "
            "VALUES (6042, 'Workspace Lifecycle E4')"
            )
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(R"(
            INSERT INTO class_info (
                class_id, teacher_id, class_grade, class_level,
                class_color, font_color
            ) VALUES (
                6042, NULL, 'E4', 'Blue', '#DDEBFF', '#172B4D'
            )
        )")
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "INSERT INTO class_times "
            "(class_id, day, start_time, end_time) "
            "VALUES (6042, 'Monday', '4:00 PM', '4:50 PM')"
            )
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "INSERT INTO class_intensive_times "
            "(class_id, day, start_time, end_time) "
            "VALUES (6042, 'Tuesday', '9:00 AM', '9:50 AM')"
            )
        ));

    ClassMngr::Next::Platform::
        ApplicationServicesScheduleDisplayPreferencesPort preferencesPort(
            services
            );
    const ClassMngr::Next::Application::ScheduleDisplayPreferences
        expectedPreferences{
        .use24HourTime = true,
        .showEnglishNames = true,
        .showWeekends = true,
        .showAllIntensiveHours = true,
        .testingAffectsM1 = true
    };
    QVERIFY(preferencesPort.save(expectedPreferences).hasValue());

    ClassMngr::Next::Platform::
        ApplicationServicesScheduleDisplayModePreferencesPort modePort(
            services
            );
    const auto initialModeSave = modePort.save(
        ClassMngr::Next::Application::ScheduleDisplayMode::Intensive
        );
    QVERIFY(initialModeSave);

    const QStringList savedScheduleFacts =
        workspaceScheduleFacts(services);
    QCOMPARE(savedScheduleFacts.size(), 2);
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "CREATE TABLE workspace_schedule_mode_write_count "
            "(count INTEGER NOT NULL)"
            )
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "INSERT INTO workspace_schedule_mode_write_count (count) "
            "VALUES (0)"
            )
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(R"(
            CREATE TRIGGER workspace_schedule_mode_write_count_trigger
            AFTER UPDATE ON app_settings
            WHEN OLD.key = 'schedule_display_mode'
            BEGIN
                UPDATE workspace_schedule_mode_write_count
                SET count = count + 1;
            END
        )")
        ));

    MyWorkspacePage page(&services);
    MyWorkspacePage* const workspacePageIdentity = &page;
    page.setDatabaseOpen(true);
    page.resize(1100, 800);
    page.show();
    page.activate();
    QApplication::processEvents();
    QCOMPARE(workspaceScheduleModeWriteCount(services), 0);

    SchedulePage* const schedulePage = page.schedulePage();
    QVERIFY(schedulePage);
    ScheduleWidget* const scheduleWidget =
        schedulePage->findChild<ScheduleWidget*>();
    QVERIFY(scheduleWidget);
    QTableWidget* const table = scheduleWidget->findChild<QTableWidget*>(
        QStringLiteral("scheduleTable")
        );
    QVERIFY(table);

    const ScheduleWidgetRuntimeMetrics populatedMetrics =
        scheduleWidget->runtimeMetrics();
    QVERIFY(populatedMetrics.modelRowCount > 0);
    QVERIFY(populatedMetrics.modelCellCount > 0);
    QVERIFY(populatedMetrics.modelEntryCount > 0);
    QVERIFY(populatedMetrics.tableRowCount > 0);
    QVERIFY(populatedMetrics.tableItemCount > 0);
    QCOMPARE(populatedMetrics.tableCellWidgetCount, 0);
    QCOMPARE(populatedMetrics.visibleClassCount, 1);
    const int persistentColumnCount = table->columnCount();
    const QList<QPointer<QWidget>> renderedCellWidgets =
        workspaceScheduleCellWidgets(table);
    QVERIFY(renderedCellWidgets.isEmpty());

    const auto loadedPreferences = preferencesPort.load();
    QVERIFY(loadedPreferences.hasValue());
    QCOMPARE(loadedPreferences.value(), expectedPreferences);
    const auto initialLoadedMode = modePort.load();
    QVERIFY(initialLoadedMode);
    QCOMPARE(
        initialLoadedMode.value(),
        ClassMngr::Next::Application::ScheduleDisplayMode::Intensive
        );
    QCOMPARE(workspaceScheduleModeWriteCount(services), 0);

    page.openTab(WorkspaceTab::Details);
    processDeferredWidgetDeletes();

    const ScheduleWidgetRuntimeMetrics releasedMetrics =
        scheduleWidget->runtimeMetrics();
    QCOMPARE(releasedMetrics.modelRowCount, 0);
    QCOMPARE(releasedMetrics.modelCellCount, 0);
    QCOMPARE(releasedMetrics.modelEntryCount, 0);
    QCOMPARE(releasedMetrics.tableRowCount, 0);
    QCOMPARE(releasedMetrics.tableItemCount, 0);
    QCOMPARE(releasedMetrics.tableCellWidgetCount, 0);
    QCOMPARE(releasedMetrics.visibleClassCount, 0);
    QCOMPARE(table->columnCount(), persistentColumnCount);
    QVERIFY(workspaceScheduleCellWidgets(table).isEmpty());
    QCOMPARE(page.schedulePage(), schedulePage);
    QCOMPARE(&page, workspacePageIdentity);
    QCOMPARE(
        schedulePage->findChild<ScheduleWidget*>(),
        scheduleWidget
        );
    QCOMPARE(
        scheduleWidget->findChild<QTableWidget*>(
            QStringLiteral("scheduleTable")
            ),
        table
        );
    QCOMPARE(workspaceScheduleFacts(services), savedScheduleFacts);
    QCOMPARE(workspaceScheduleModeWriteCount(services), 0);
    const auto preferencesAfterLeave = preferencesPort.load();
    QVERIFY(preferencesAfterLeave.hasValue());
    QCOMPARE(preferencesAfterLeave.value(), expectedPreferences);
    const auto loadedModeAfterLeave = modePort.load();
    QVERIFY(loadedModeAfterLeave);
    QCOMPARE(
        loadedModeAfterLeave.value(),
        ClassMngr::Next::Application::ScheduleDisplayMode::Intensive
        );

    page.openTab(WorkspaceTab::Schedule);
    QApplication::processEvents();

    const ScheduleWidgetRuntimeMetrics restoredMetrics =
        scheduleWidget->runtimeMetrics();
    QCOMPARE(restoredMetrics.modelRowCount, populatedMetrics.modelRowCount);
    QCOMPARE(restoredMetrics.modelCellCount, populatedMetrics.modelCellCount);
    QCOMPARE(restoredMetrics.modelEntryCount, populatedMetrics.modelEntryCount);
    QCOMPARE(restoredMetrics.tableRowCount, populatedMetrics.tableRowCount);
    QCOMPARE(restoredMetrics.tableItemCount, populatedMetrics.tableItemCount);
    QCOMPARE(
        restoredMetrics.tableCellWidgetCount,
        populatedMetrics.tableCellWidgetCount
        );
    QCOMPARE(
        restoredMetrics.visibleClassCount,
        populatedMetrics.visibleClassCount
        );
    const ScheduleDisplayState restoredDisplayState =
        scheduleWidget->displayState();
    QVERIFY(restoredDisplayState.use24HourTime);
    QVERIFY(restoredDisplayState.showKoreanTeacherEnglishNames);
    QVERIFY(restoredDisplayState.showAllHours);
    QVERIFY(restoredDisplayState.showWeekends);
    QVERIFY(restoredDisplayState.testingAffectsM1);
    QCOMPARE(
        restoredDisplayState.displayMode,
        ScheduleDisplayMode::Intensive
        );
    QCOMPARE(workspaceScheduleFacts(services), savedScheduleFacts);
    QCOMPARE(workspaceScheduleModeWriteCount(services), 0);

    const QList<QPointer<QWidget>> renderedCellWidgetsBeforeDeactivate =
        workspaceScheduleCellWidgets(table);
    QVERIFY(renderedCellWidgetsBeforeDeactivate.isEmpty());
    page.deactivate();
    processDeferredWidgetDeletes();
    const ScheduleWidgetRuntimeMetrics deactivatedMetrics =
        scheduleWidget->runtimeMetrics();
    QCOMPARE(deactivatedMetrics.modelRowCount, 0);
    QCOMPARE(deactivatedMetrics.modelCellCount, 0);
    QCOMPARE(deactivatedMetrics.modelEntryCount, 0);
    QCOMPARE(deactivatedMetrics.tableRowCount, 0);
    QCOMPARE(deactivatedMetrics.tableItemCount, 0);
    QCOMPARE(deactivatedMetrics.tableCellWidgetCount, 0);
    QCOMPARE(deactivatedMetrics.visibleClassCount, 0);
    QCOMPARE(table->columnCount(), persistentColumnCount);
    QVERIFY(workspaceScheduleCellWidgets(table).isEmpty());
    QCOMPARE(workspaceScheduleModeWriteCount(services), 0);
    page.activate();
    QApplication::processEvents();
    QCOMPARE(&page, workspacePageIdentity);
    QCOMPARE(page.schedulePage(), schedulePage);
    QCOMPARE(
        schedulePage->findChild<ScheduleWidget*>(),
        scheduleWidget
        );
    QCOMPARE(
        scheduleWidget->findChild<QTableWidget*>(
            QStringLiteral("scheduleTable")
            ),
        table
        );
    QCOMPARE(
        scheduleWidget->runtimeMetrics().modelRowCount,
        populatedMetrics.modelRowCount
        );
    QCOMPARE(
        scheduleWidget->runtimeMetrics().tableCellWidgetCount,
        populatedMetrics.tableCellWidgetCount
        );
    QCOMPARE(workspaceScheduleFacts(services), savedScheduleFacts);
    const auto preferencesAfterReentry = preferencesPort.load();
    QVERIFY(preferencesAfterReentry.hasValue());
    QCOMPARE(preferencesAfterReentry.value(), expectedPreferences);
    const auto loadedModeAfterReentry = modePort.load();
    QVERIFY(loadedModeAfterReentry);
    QCOMPARE(
        loadedModeAfterReentry.value(),
        ClassMngr::Next::Application::ScheduleDisplayMode::Intensive
        );
    QCOMPARE(workspaceScheduleModeWriteCount(services), 0);

    int displayModeCallbackCount = 0;
    connect(
        schedulePage,
        &SchedulePage::displayModeChanged,
        &page,
        [&displayModeCallbackCount](ScheduleDisplayMode)
        {
            ++displayModeCallbackCount;
        }
        );
    QPushButton* const regularModeButton =
        scheduleWidget->findChild<QPushButton*>(
            QStringLiteral("scheduleRegularModeButton")
            );
    QVERIFY(regularModeButton);
    regularModeButton->click();
    QCOMPARE(displayModeCallbackCount, 1);
    const auto loadedModeAfterSelection = modePort.load();
    QVERIFY(loadedModeAfterSelection);
    QCOMPARE(
        loadedModeAfterSelection.value(),
        ClassMngr::Next::Application::ScheduleDisplayMode::Regular
        );
    QCOMPARE(workspaceScheduleModeWriteCount(services), 1);
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

void MyWorkspacePageTests::
failedCampusCorrectionRetriesAndSuppressesRepeatedWarnings()
{
    FakeUserPromptService prompts;
    ScopedUserPromptService promptScope(prompts);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(openDatabase(services, directory));
    QVERIFY(services.dataService());
    const QString unresolvedCampus = QStringLiteral("Closed Campus");
    QVERIFY(services.dataService()->saveSetting(
        QString::fromUtf8(CurrentCampusKey),
        unresolvedCampus
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(R"(
            CREATE TABLE campus_correction_write_attempts (
                attempts INTEGER NOT NULL
            )
        )")
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "INSERT INTO campus_correction_write_attempts (attempts) "
            "VALUES (0)"
            )
        ));
    QVERIFY(installFailingCampusCorrectionTrigger(services));

    MyWorkspacePage page(&services);
    refreshPersonalDetails(page);

    PersonalDetailsPage* const details = page.personalDetailsPage();
    QVERIFY(details);
    auto* const campus = details->findChild<QComboBox*>();
    QVERIFY(campus);
    QVERIFY(campus->count() > 0);
    const QString fallbackCampus = campus->itemText(0);
    QVERIFY(!fallbackCampus.isEmpty());
    QCOMPARE(campus->currentIndex(), 0);
    QCOMPARE(campus->currentText(), fallbackCampus);

    const auto storedAfterFirstFailure =
        services.dataService()->loadSetting(
            QString::fromUtf8(CurrentCampusKey)
            );
    QVERIFY(storedAfterFirstFailure);
    QCOMPARE(storedAfterFirstFailure->toString(), unresolvedCampus);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        prompts.messages.constFirst().severity,
        PromptSeverity::Warning
        );
    QVERIFY(prompts.messages.constFirst().details.contains(
        QStringLiteral("forced current campus correction failure")
        ));

    const int attemptsAfterInitialLoad =
        campusCorrectionWriteAttempts(services);
    QVERIFY(attemptsAfterInitialLoad >= 1);

    details->refresh();
    QCOMPARE(
        campusCorrectionWriteAttempts(services),
        attemptsAfterInitialLoad + 1
        );
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(campus->currentText(), fallbackCampus);
    const auto storedAfterRepeatedFailure =
        services.dataService()->loadSetting(
            QString::fromUtf8(CurrentCampusKey)
            );
    QVERIFY(storedAfterRepeatedFailure);
    QCOMPARE(storedAfterRepeatedFailure->toString(), unresolvedCampus);

    details->refresh();
    QCOMPARE(
        campusCorrectionWriteAttempts(services),
        attemptsAfterInitialLoad + 2
        );
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(campus->currentText(), fallbackCampus);
    const auto storedAfterThirdFailure =
        services.dataService()->loadSetting(
            QString::fromUtf8(CurrentCampusKey)
            );
    QVERIFY(storedAfterThirdFailure);
    QCOMPARE(storedAfterThirdFailure->toString(), unresolvedCampus);

    page.clearDatabaseState();
    details->refresh();
    QCOMPARE(
        campusCorrectionWriteAttempts(services),
        attemptsAfterInitialLoad + 3
        );
    QCOMPARE(campus->currentText(), fallbackCampus);
    QCOMPARE(prompts.messages.size(), 2);
    QCOMPARE(
        prompts.messages.constLast().severity,
        PromptSeverity::Warning
        );
    QVERIFY(prompts.messages.constLast().details.contains(
        QStringLiteral("forced current campus correction failure")
        ));
    const auto storedAfterStateClearFailure =
        services.dataService()->loadSetting(
            QString::fromUtf8(CurrentCampusKey)
            );
    QVERIFY(storedAfterStateClearFailure);
    QCOMPARE(storedAfterStateClearFailure->toString(), unresolvedCampus);

    QVERIFY(executeSql(
        services,
        QStringLiteral("DROP TRIGGER fail_current_campus_correction")
        ));
    details->refresh();

    const auto storedAfterRecovery =
        services.dataService()->loadSetting(
            QString::fromUtf8(CurrentCampusKey)
            );
    QVERIFY(storedAfterRecovery);
    QCOMPARE(storedAfterRecovery->toString(), fallbackCampus);
    QCOMPARE(campus->currentText(), fallbackCampus);
    QCOMPARE(
        campus->currentData().toString(),
        campus->itemData(0).toString()
        );
    QCOMPARE(prompts.messages.size(), 2);

    QVERIFY(services.dataService()->saveSetting(
        QString::fromUtf8(CurrentCampusKey),
        unresolvedCampus
        ));
    QVERIFY(installFailingCampusCorrectionTrigger(services));
    const int attemptsBeforeLaterFailure =
        campusCorrectionWriteAttempts(services);
    details->refresh();
    QCOMPARE(
        campusCorrectionWriteAttempts(services),
        attemptsBeforeLaterFailure + 1
        );
    QCOMPARE(campus->currentText(), fallbackCampus);
    QCOMPARE(prompts.messages.size(), 3);
    QCOMPARE(
        prompts.messages.constLast().severity,
        PromptSeverity::Warning
        );
    QVERIFY(prompts.messages.constLast().details.contains(
        QStringLiteral("forced current campus correction failure")
        ));
    const auto storedAfterLaterFailure =
        services.dataService()->loadSetting(
            QString::fromUtf8(CurrentCampusKey)
            );
    QVERIFY(storedAfterLaterFailure);
    QCOMPARE(storedAfterLaterFailure->toString(), unresolvedCampus);
}

void MyWorkspacePageTests::failedMatchedCampusIdCorrectionWarnsAndRecovers()
{
    FakeUserPromptService prompts;
    ScopedUserPromptService promptScope(prompts);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(openDatabase(services, directory));
    QVERIFY(services.dataService());
    QVERIFY(services.dataService()->saveSetting(
        QString::fromUtf8(CurrentCampusKey),
        QStringLiteral("j")
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(R"(
            CREATE TABLE campus_correction_write_attempts (
                attempts INTEGER NOT NULL
            )
        )")
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "INSERT INTO campus_correction_write_attempts (attempts) "
            "VALUES (0)"
            )
        ));
    QVERIFY(installFailingCampusCorrectionTrigger(services));

    MyWorkspacePage page(&services);
    refreshPersonalDetails(page);

    PersonalDetailsPage* const details = page.personalDetailsPage();
    QVERIFY(details);
    auto* const campus = details->findChild<QComboBox*>();
    QVERIFY(campus);
    QCOMPARE(campus->currentData().toString(), QStringLiteral("j"));
    QCOMPARE(campus->currentText(), QStringLiteral("Jeongja"));
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        prompts.messages.constFirst().severity,
        PromptSeverity::Warning
        );
    QVERIFY(prompts.messages.constFirst().details.contains(
        QStringLiteral("forced current campus correction failure")
        ));
    const auto storedAfterFailure = services.dataService()->loadSetting(
        QString::fromUtf8(CurrentCampusKey)
        );
    QVERIFY(storedAfterFailure);
    QCOMPARE(storedAfterFailure->toString(), QStringLiteral("j"));

    const int attemptsAfterInitialLoad =
        campusCorrectionWriteAttempts(services);
    QVERIFY(attemptsAfterInitialLoad >= 1);
    details->refresh();
    QCOMPARE(
        campusCorrectionWriteAttempts(services),
        attemptsAfterInitialLoad + 1
        );
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(campus->currentText(), QStringLiteral("Jeongja"));
    const auto storedAfterRetry = services.dataService()->loadSetting(
        QString::fromUtf8(CurrentCampusKey)
        );
    QVERIFY(storedAfterRetry);
    QCOMPARE(storedAfterRetry->toString(), QStringLiteral("j"));

    QVERIFY(executeSql(
        services,
        QStringLiteral("DROP TRIGGER fail_current_campus_correction")
        ));
    details->refresh();
    const auto storedAfterRecovery = services.dataService()->loadSetting(
        QString::fromUtf8(CurrentCampusKey)
        );
    QVERIFY(storedAfterRecovery);
    QCOMPARE(storedAfterRecovery->toString(), QStringLiteral("Jeongja"));
    QCOMPARE(campus->currentData().toString(), QStringLiteral("j"));
    QCOMPARE(campus->currentText(), QStringLiteral("Jeongja"));
    QCOMPARE(prompts.messages.size(), 1);
}

void MyWorkspacePageTests::settingsReadFailurePreservesDisplayedCampusChoice()
{
    FakeUserPromptService prompts;
    ScopedUserPromptService promptScope(prompts);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(openDatabase(services, directory));
    QVERIFY(services.dataService());
    QVERIFY(services.dataService()->saveSetting(
        QString::fromUtf8(CurrentCampusKey),
        QStringLiteral("j")
        ));

    MyWorkspacePage page(&services);
    refreshPersonalDetails(page);

    PersonalDetailsPage* const details = page.personalDetailsPage();
    QVERIFY(details);
    auto* const campus = details->findChild<QComboBox*>();
    QVERIFY(campus);
    QVERIFY(campus->count() > 1);

    const int displayedIndex = campus->count() - 1;
    QVERIFY(displayedIndex > 0);
    const QString displayedCampus = campus->itemText(displayedIndex);
    QVERIFY(services.dataService()->saveSetting(
        QString::fromUtf8(CurrentCampusKey),
        displayedCampus
        ));
    details->discardChanges();
    QCOMPARE(campus->currentIndex(), displayedIndex);
    QCOMPARE(campus->currentText(), displayedCampus);

    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "ALTER TABLE app_settings RENAME TO app_settings_preserved"
            )
        ));

    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression(QStringLiteral("Failed to load setting.*"))
        );
    details->refresh();

    QCOMPARE(campus->currentIndex(), displayedIndex);
    QCOMPARE(campus->currentText(), displayedCampus);
    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& warning = prompts.messages.constFirst();
    QCOMPARE(warning.severity, PromptSeverity::Warning);
    QCOMPARE(
        warning.title,
        QStringLiteral("Personal Name Could Not Be Loaded")
        );
    QVERIFY(warning.message.contains(QStringLiteral("form has been preserved")));
    QVERIFY(warning.details.contains(
        QStringLiteral("Loading application setting")
        ));

    QSqlQuery query(services.databaseSession()->database());
    QVERIFY(query.exec(QStringLiteral(
        "SELECT value FROM app_settings_preserved WHERE key='myInfo/campus'"
        )));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), displayedCampus);
}

void MyWorkspacePageTests::
currentCampusReadFailurePreservesChoiceAndBlocksSaveUntilReload()
{
    FakeUserPromptService prompts;
    ScopedUserPromptService promptScope(prompts);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(openDatabase(services, directory));
    QVERIFY(services.dataService());

    const QByteArray source = sourcePng();
    QVERIFY(!source.isEmpty());
    const QString encodedImage = QString::fromLatin1(source.toBase64());
    const QString storedName = QStringLiteral("Stored Name");
    const QString storedSignatureText = QStringLiteral("Stored Signature");
    const QVariantMap initialValues = {
        {QString::fromUtf8(DisplayNameKey), storedName},
        {QString::fromUtf8(CurrentCampusKey), QStringLiteral("Jeongja")},
        {QString::fromUtf8(ZoomLoginIdKey), QStringLiteral("stored login")},
        {QString::fromUtf8(ZoomPasswordKey), QStringLiteral("stored password")},
        {QString::fromUtf8(ZoomNotAvailableKey), false},
        {QString::fromUtf8(SignatureImageKey), encodedImage},
        {QString::fromUtf8(SignatureModeKey), 1},
        {QString::fromUtf8(TypedSignatureTextKey), storedSignatureText},
        {QString::fromUtf8(TypedSignatureFontKey), 2},
        {QString::fromUtf8(UnrelatedKey), QStringLiteral("preserved")}
    };
    QVERIFY(services.dataService()->saveSettings(initialValues));

    MyWorkspacePage page(&services);
    refreshPersonalDetails(page);

    PersonalDetailsPage* const details = page.personalDetailsPage();
    QVERIFY(details);
    QLineEdit* const name = personalNameEditor(page);
    QComboBox* const campus = details->findChild<QComboBox*>();
    QVERIFY(name);
    QVERIFY(campus);
    QCOMPARE(campus->currentText(), QStringLiteral("Jeongja"));

    const int campusAIndex = campus->currentIndex();
    QVERIFY(campusAIndex >= 0);
    QVERIFY(campus->count() > 1);
    const int campusBIndex = campusAIndex == 0 ? 1 : 0;
    const QString campusB = campus->itemText(campusBIndex);
    QVERIFY(!campusB.isEmpty());
    QVERIFY(campusB != campus->currentText());
    QVERIFY(services.dataService()->saveSetting(
        QString::fromUtf8(CurrentCampusKey),
        campusB
        ));

    QVERIFY(installFailingCurrentCampusReadView(services));
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression(
            QStringLiteral("Failed to load setting.*myInfo/campus.*")
            )
        );
    details->discardChanges();

    QCOMPARE(campus->currentText(), QStringLiteral("Jeongja"));
    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& readWarning = prompts.messages.constFirst();
    QCOMPARE(readWarning.severity, PromptSeverity::Warning);
    QCOMPARE(
        readWarning.title,
        QStringLiteral("Campus Preference Could Not Be Loaded")
        );
    QCOMPARE(
        readWarning.message,
        QStringLiteral(
            "The current campus could not be read. Your displayed campus "
            "selection has been preserved. Refresh this page to try again."
            )
        );

    QVERIFY(restoreSettingsTable(services));

    const auto campusBeforeSave = services.dataService()->loadSetting(
        QString::fromUtf8(CurrentCampusKey)
        );
    QVERIFY(campusBeforeSave);
    QCOMPARE(campusBeforeSave->toString(), campusB);

    name->setText(QStringLiteral("Unsaved Draft Name"));
    QCOMPARE(name->text(), QStringLiteral("Unsaved Draft Name"));
    QVERIFY(details->hasUnsavedChanges());
    QVERIFY(!details->saveChanges());

    QCOMPARE(campus->currentText(), QStringLiteral("Jeongja"));
    QCOMPARE(name->text(), QStringLiteral("Unsaved Draft Name"));
    QVERIFY(details->hasUnsavedChanges());
    QCOMPARE(prompts.messages.size(), 2);
    const PromptRequest& saveWarning = prompts.messages.constLast();
    QCOMPARE(saveWarning.severity, PromptSeverity::Warning);
    QCOMPARE(
        saveWarning.title,
        QStringLiteral("My Details Could Not Be Saved")
        );
    QCOMPARE(
        saveWarning.details,
        QStringLiteral(
            "The current campus could not be read. Your displayed campus "
            "selection has been preserved. Refresh this page to try again."
            )
        );

    const auto savedName = services.dataService()->loadSetting(
        QString::fromUtf8(DisplayNameKey)
        );
    QVERIFY(savedName);
    QCOMPARE(savedName->toString(), storedName);
    const auto savedCampus = services.dataService()->loadSetting(
        QString::fromUtf8(CurrentCampusKey)
        );
    QVERIFY(savedCampus);
    QCOMPARE(savedCampus->toString(), campusB);
    const auto savedZoomLogin = services.dataService()->loadSetting(
        QString::fromUtf8(ZoomLoginIdKey)
        );
    QVERIFY(savedZoomLogin);
    QCOMPARE(savedZoomLogin->toString(), QStringLiteral("stored login"));
    const auto savedZoomPassword = services.dataService()->loadSetting(
        QString::fromUtf8(ZoomPasswordKey)
        );
    QVERIFY(savedZoomPassword);
    QCOMPARE(savedZoomPassword->toString(), QStringLiteral("stored password"));
    const auto savedZoomUnavailable = services.dataService()->loadSetting(
        QString::fromUtf8(ZoomNotAvailableKey)
        );
    QVERIFY(savedZoomUnavailable);
    QCOMPARE(savedZoomUnavailable->toBool(), false);
    const auto savedImage = services.dataService()->loadSetting(
        QString::fromUtf8(SignatureImageKey)
        );
    QVERIFY(savedImage);
    QCOMPARE(savedImage->toString(), encodedImage);
    const auto savedSignatureMode = services.dataService()->loadSetting(
        QString::fromUtf8(SignatureModeKey)
        );
    QVERIFY(savedSignatureMode);
    QCOMPARE(savedSignatureMode->toInt(), 1);
    const auto savedSignatureText = services.dataService()->loadSetting(
        QString::fromUtf8(TypedSignatureTextKey)
        );
    QVERIFY(savedSignatureText);
    QCOMPARE(savedSignatureText->toString(), storedSignatureText);
    const auto savedSignatureFont = services.dataService()->loadSetting(
        QString::fromUtf8(TypedSignatureFontKey)
        );
    QVERIFY(savedSignatureFont);
    QCOMPARE(savedSignatureFont->toInt(), 2);
    const auto savedUnrelated = services.dataService()->loadSetting(
        QString::fromUtf8(UnrelatedKey)
        );
    QVERIFY(savedUnrelated);
    QCOMPARE(savedUnrelated->toString(), QStringLiteral("preserved"));

    details->discardChanges();
    QCOMPARE(name->text(), storedName);
    QCOMPARE(campus->currentText(), campusB);
    QVERIFY(!details->hasUnsavedChanges());
    QCOMPARE(prompts.messages.size(), 2);

    name->setText(QStringLiteral("Recovered Name"));
    QVERIFY(details->hasUnsavedChanges());
    QVERIFY(details->saveChanges());
    QCOMPARE(prompts.messages.size(), 2);
    const auto recoveredName = services.dataService()->loadSetting(
        QString::fromUtf8(DisplayNameKey)
        );
    QVERIFY(recoveredName);
    QCOMPARE(recoveredName->toString(), QStringLiteral("Recovered Name"));
    const auto recoveredCampus = services.dataService()->loadSetting(
        QString::fromUtf8(CurrentCampusKey)
        );
    QVERIFY(recoveredCampus);
    QCOMPARE(recoveredCampus->toString(), campusB);
    QVERIFY(!details->hasUnsavedChanges());
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

void MyWorkspacePageTests::
displayNameReadFailurePreservesDraftAndBlocksSaveUntilReload()
{
    FakeUserPromptService prompts;
    ScopedUserPromptService promptScope(prompts);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(openDatabase(services, directory));
    QVERIFY(services.dataService());

    const QString storedName = QStringLiteral("Stored personal name");
    QVERIFY(services.dataService()->saveSetting(
        QString::fromUtf8(DisplayNameKey),
        storedName
        ));

    MyWorkspacePage page(&services);
    page.setDatabaseOpen(true);
    page.setSaveMode(SaveMode::Manual);

    page.openTab(WorkspaceTab::Details);
    PersonalDetailsPage* const details = page.personalDetailsPage();
    QVERIFY(details);
    QLineEdit* const name = personalNameEditor(page);
    QVERIFY(name);
    QCOMPARE(name->text(), storedName);
    QVERIFY(!details->hasUnsavedChanges());

    const QString draftName = QStringLiteral("Unsaved personal name draft");
    name->setText(draftName);
    QVERIFY(details->hasUnsavedChanges());

    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "ALTER TABLE app_settings RENAME TO app_settings_preserved"
            )
        ));

    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression(QStringLiteral("Failed to load setting.*"))
        );
    // Force the reload even though the draft is dirty; refresh skips dirty
    // forms by design.
    details->discardChanges();

    QCOMPARE(name->text(), draftName);
    QVERIFY(details->hasUnsavedChanges());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        prompts.messages.constFirst().title,
        QStringLiteral("Personal Name Could Not Be Loaded")
        );
    QVERIFY(prompts.messages.constFirst().message.contains(
        QStringLiteral("form has been preserved")
        ));

    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "ALTER TABLE app_settings_preserved RENAME TO app_settings"
            )
        ));

    QVERIFY(!details->saveChanges());
    QCOMPARE(name->text(), draftName);
    QVERIFY(details->hasUnsavedChanges());
    QCOMPARE(prompts.messages.size(), 2);
    QCOMPARE(
        prompts.messages.constLast().title,
        QStringLiteral("My Details Could Not Be Saved")
        );
    QVERIFY(prompts.messages.constLast().details.contains(
        QStringLiteral("stored personal name is unknown")
        ));
    const auto stillStored = services.dataService()->loadSetting(
        QString::fromUtf8(DisplayNameKey)
        );
    QVERIFY(stillStored);
    QCOMPARE(stillStored->toString(), storedName);

    details->discardChanges();
    QCOMPARE(name->text(), storedName);
    QVERIFY(!details->hasUnsavedChanges());

    const QString recoveredName = QStringLiteral("Recovered personal name");
    name->setText(recoveredName);
    QVERIFY(details->saveChanges());
    QCOMPARE(prompts.messages.size(), 2);
    const auto recovered = services.dataService()->loadSetting(
        QString::fromUtf8(DisplayNameKey)
        );
    QVERIFY(recovered);
    QCOMPARE(recovered->toString(), recoveredName);
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
            QStringLiteral("  teacher / 김 선생님 📚  ")
            )
        );
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(ZoomPasswordKey),
            QStringLiteral("  secret / 비밀번호 🔒  ")
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
    QCOMPARE(login->text(), QStringLiteral("  teacher / 김 선생님 📚  "));
    QCOMPARE(password->text(), QStringLiteral("  secret / 비밀번호 🔒  "));
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
    QCOMPARE(login->text(), QStringLiteral("  teacher / 김 선생님 📚  "));
    QCOMPARE(password->text(), QStringLiteral("  secret / 비밀번호 🔒  "));
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

void MyWorkspacePageTests::
zoomPreferenceReadFailurePreservesControlsAndBlocksSaveUntilReload()
{
    FakeUserPromptService prompts;
    ScopedUserPromptService promptScope(prompts);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(openDatabase(services, directory));
    QVERIFY(services.dataService());

    const QVariantMap initialValues = {
        {QString::fromUtf8(DisplayNameKey), QStringLiteral("Stored Name")},
        {QString::fromUtf8(CurrentCampusKey), QStringLiteral("Jeongja")},
        {QString::fromUtf8(ZoomLoginIdKey), QStringLiteral("stored-login")},
        {QString::fromUtf8(ZoomPasswordKey), QStringLiteral("stored-pass")},
        {QString::fromUtf8(ZoomNotAvailableKey), false},
        {QString::fromUtf8(UnrelatedKey), QStringLiteral("preserved")}
    };
    QVERIFY(services.dataService()->saveSettings(initialValues));

    MyWorkspacePage page(&services);
    refreshPersonalDetails(page);

    PersonalDetailsPage* const details = page.personalDetailsPage();
    auto* const name = personalNameEditor(page);
    auto* const loginId = personalZoomLoginEditor(page);
    auto* const password = personalZoomPasswordEditor(page);
    auto* const unavailable = personalZoomUnavailableCheck(page);
    QVERIFY(details);
    QVERIFY(name);
    QVERIFY(loginId);
    QVERIFY(password);
    QVERIFY(unavailable);
    QCOMPARE(loginId->text(), QStringLiteral("stored-login"));
    QCOMPARE(password->text(), QStringLiteral("stored-pass"));
    QVERIFY(!unavailable->isChecked());

    loginId->setText(QStringLiteral("visible draft login"));
    password->setText(QStringLiteral("visible draft password"));
    QVERIFY(installFailingZoomUnavailableReadView(services));
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression(
            QStringLiteral(
                "Failed to load setting.*myInfo/zoomNotAvailable.*"
                )
            )
        );
    details->discardChanges();

    QCOMPARE(loginId->text(), QStringLiteral("visible draft login"));
    QCOMPARE(password->text(), QStringLiteral("visible draft password"));
    QVERIFY(!unavailable->isChecked());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        prompts.messages.constFirst().title,
        QStringLiteral("Zoom Preferences Could Not Be Loaded")
        );
    QVERIFY(prompts.messages.constFirst().details.contains(
        QStringLiteral("myInfo/zoomNotAvailable")
        ));

    name->setText(QStringLiteral("Should Not Be Saved"));
    QVERIFY(!details->saveChanges());
    QCOMPARE(prompts.messages.size(), 2);
    QVERIFY(prompts.messages.constLast().details.contains(
        QStringLiteral("Zoom preferences are unknown")
        ));

    QVERIFY(restoreSettingsTable(services));
    for (auto setting = initialValues.cbegin();
         setting != initialValues.cend();
         ++setting)
    {
        QCOMPARE(
            services.dataService()->loadSetting(setting.key()).value(),
            setting.value()
            );
    }

    details->discardChanges();
    QCOMPARE(loginId->text(), QStringLiteral("stored-login"));
    QCOMPARE(password->text(), QStringLiteral("stored-pass"));
    QVERIFY(!unavailable->isChecked());
    name->setText(QStringLiteral("Recovered Name"));
    QVERIFY(details->saveChanges());
    QCOMPARE(prompts.messages.size(), 2);
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(DisplayNameKey))
            ->toString(),
        QStringLiteral("Recovered Name")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(ZoomLoginIdKey))
            ->toString(),
        QStringLiteral("stored-login")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(ZoomPasswordKey))
            ->toString(),
        QStringLiteral("stored-pass")
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

void MyWorkspacePageTests::
signatureFontChoicesMapToPersistedValuesThroughMyWorkspace()
{
    for (int storedValue = 0; storedValue < 4; ++storedValue)
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        ApplicationServices services;
        QVERIFY(openDatabase(services, directory));
        QVERIFY(services.dataService());
        QVERIFY(
            services.dataService()->saveSetting(
                QString::fromUtf8(TypedSignatureFontKey),
                storedValue
                )
            );

        MyWorkspacePage page(&services);
        refreshPersonalDetails(page);
        PersonalDetailsPage* const details = page.personalDetailsPage();
        QVERIFY(details);
        const QList<QPushButton*> fontButtons =
            details->findChildren<QPushButton*>(
                QStringLiteral("typedSignatureFontButton")
                );
        QCOMPARE(fontButtons.size(), 4);
        for (int index = 0; index < fontButtons.size(); ++index)
        {
            QCOMPARE(fontButtons.at(index)->isChecked(), index == storedValue);
        }

        const int selectedValue = (storedValue + 1) % 4;
        fontButtons.at(selectedValue)->click();
        QVERIFY(fontButtons.at(selectedValue)->isChecked());
        QVERIFY(details->saveChanges());

        const auto savedFont = services.dataService()->loadSetting(
            QString::fromUtf8(TypedSignatureFontKey)
            );
        QVERIFY(savedFont);
        QCOMPARE(savedFont->toInt(), selectedValue);
    }
}

void MyWorkspacePageTests::
signaturePreferenceReadFailurePreservesControlsAndBlocksSaveUntilReload()
{
    FakeUserPromptService prompts;
    ScopedUserPromptService promptScope(prompts);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(openDatabase(services, directory));
    QVERIFY(services.dataService());

    const QString storedName = QStringLiteral("Stored Name");
    const QString storedText = QString::fromUtf8(
        "  \xEA\xB9\x80\xEC\x84\xA0\xEC\x83\x9D\xEB\x8B\x98 / "
        "\xF0\x9F\xA7\xAD  "
        );
    const QVariantMap initialValues = {
        {QString::fromUtf8(DisplayNameKey), storedName},
        {QString::fromUtf8(CurrentCampusKey), QStringLiteral("Jeongja")},
        {QString::fromUtf8(SignatureModeKey), 1},
        {QString::fromUtf8(TypedSignatureTextKey), storedText},
        {QString::fromUtf8(TypedSignatureFontKey), 2},
        {QString::fromUtf8(UnrelatedKey), QStringLiteral("preserved")}
    };
    QVERIFY(services.dataService()->saveSettings(initialValues));

    MyWorkspacePage page(&services);
    refreshPersonalDetails(page);

    PersonalDetailsPage* const details = page.personalDetailsPage();
    QVERIFY(details);
    auto* const name = personalNameEditor(page);
    auto* const imageMode = signatureModeButton(
        page,
        QStringLiteral("signatureImageModeButton")
        );
    auto* const typeMode = signatureModeButton(
        page,
        QStringLiteral("signatureTypeModeButton")
        );
    auto* const typedText = details->findChild<QLineEdit*>(
        QStringLiteral("typedSignatureEdit")
        );
    const QList<QPushButton*> fontButtons =
        details->findChildren<QPushButton*>(
            QStringLiteral("typedSignatureFontButton")
            );
    QVERIFY(name);
    QVERIFY(imageMode);
    QVERIFY(typeMode);
    QVERIFY(typedText);
    QCOMPARE(fontButtons.size(), 4);
    QVERIFY(typeMode->isChecked());
    QCOMPARE(typedText->text(), storedText);
    QVERIFY(fontButtons.at(2)->isChecked());

    imageMode->click();
    typedText->setText(QStringLiteral("current signature draft"));
    fontButtons.at(3)->click();
    QVERIFY(imageMode->isChecked());
    QCOMPARE(typedText->text(), QStringLiteral("current signature draft"));
    QVERIFY(fontButtons.at(3)->isChecked());

    QVERIFY(installFailingSignaturePreferenceReadView(services));
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression(
            QStringLiteral("Failed to load setting.*myInfo/signatureMode.*")
            )
        );
    details->discardChanges();

    QVERIFY(imageMode->isChecked());
    QVERIFY(!typeMode->isChecked());
    QCOMPARE(typedText->text(), QStringLiteral("current signature draft"));
    QVERIFY(fontButtons.at(3)->isChecked());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        prompts.messages.constFirst().title,
        QStringLiteral("Signature Preferences Could Not Be Loaded")
        );

    name->setText(QStringLiteral("Should Not Be Saved"));
    QVERIFY(!details->saveChanges());
    QCOMPARE(prompts.messages.size(), 2);
    QVERIFY(prompts.messages.constLast().details.contains(
        QStringLiteral("signature preferences are unknown")
        ));

    QVERIFY(restoreSettingsTable(services));
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(DisplayNameKey))
            ->toString(),
        storedName
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
        storedText
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

    details->discardChanges();
    QCOMPARE(typedText->text(), storedText);
    QVERIFY(typeMode->isChecked());
    QVERIFY(fontButtons.at(2)->isChecked());
    name->setText(QStringLiteral("Recovered Name"));
    QVERIFY(details->saveChanges());
    QCOMPARE(prompts.messages.size(), 2);
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(DisplayNameKey))
            ->toString(),
        QStringLiteral("Recovered Name")
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
        storedText
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(TypedSignatureFontKey))
            ->toInt(),
        2
        );
}

void MyWorkspacePageTests::
signatureImageReadFailurePreservesImageAndBlocksSaveUntilReload()
{
    FakeUserPromptService prompts;
    ScopedUserPromptService promptScope(prompts);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(openDatabase(services, directory));
    QVERIFY(services.dataService());

    const QString storedName = QStringLiteral("Stored Name");
    const QByteArray source = sourcePng();
    QVERIFY(!source.isEmpty());
    const QString encodedImage = QString::fromLatin1(source.toBase64());
    const QVariantMap initialValues = {
        {QString::fromUtf8(DisplayNameKey), storedName},
        {QString::fromUtf8(CurrentCampusKey), QStringLiteral("Jeongja")},
        {QString::fromUtf8(ZoomLoginIdKey), QStringLiteral("N/A")},
        {QString::fromUtf8(ZoomPasswordKey), QStringLiteral("N/A")},
        {QString::fromUtf8(ZoomNotAvailableKey), true},
        {QString::fromUtf8(SignatureImageKey), encodedImage},
        {QString::fromUtf8(SignatureModeKey), 0},
        {QString::fromUtf8(TypedSignatureTextKey), QStringLiteral("stored")},
        {QString::fromUtf8(TypedSignatureFontKey), 1},
        {QString::fromUtf8(UnrelatedKey), QStringLiteral("preserved")}
    };
    QVERIFY(services.dataService()->saveSettings(initialValues));

    MyWorkspacePage page(&services);
    refreshPersonalDetails(page);

    PersonalDetailsPage* const details = page.personalDetailsPage();
    QVERIFY(details);
    QLabel* const preview = signaturePreview(page);
    QLineEdit* const name = personalNameEditor(page);
    QVERIFY(preview);
    QVERIFY(name);
    QVERIFY(preview->pixmap().isNull() == false);

    QVERIFY(installFailingSignatureImageReadView(services));
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression(
            QStringLiteral("Failed to load setting.*myInfo/signatureImage.*")
            )
        );
    details->discardChanges();

    QVERIFY(!preview->pixmap().isNull());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        prompts.messages.constFirst().title,
        QStringLiteral("Signature Image Could Not Be Loaded")
        );
    QVERIFY(prompts.messages.constFirst().message.contains(
        QStringLiteral("current signature image has been preserved")
        ));

    name->setText(QStringLiteral("Should Not Be Saved"));
    QVERIFY(!details->saveChanges());
    QCOMPARE(prompts.messages.size(), 2);
    QVERIFY(prompts.messages.constLast().details.contains(
        QStringLiteral("stored signature image is unknown")
        ));

    QSqlQuery preservedSettings(services.databaseSession()->database());
    QVERIFY(preservedSettings.exec(QStringLiteral(
        "SELECT value FROM app_settings_preserved WHERE key='myInfo/name'"
        )));
    QVERIFY(preservedSettings.next());
    QCOMPARE(preservedSettings.value(0).toString(), storedName);
    QVERIFY(preservedSettings.exec(QStringLiteral(
        "SELECT value FROM app_settings_preserved "
        "WHERE key='myInfo/signatureImage'"
        )));
    QVERIFY(preservedSettings.next());
    QCOMPARE(preservedSettings.value(0).toString(), encodedImage);

    QVERIFY(restoreSettingsTable(services));
    details->discardChanges();
    name->setText(QStringLiteral("Recovered Name"));
    QVERIFY(details->saveChanges());
    QCOMPARE(prompts.messages.size(), 2);
    const QByteArray expectedImage = SignatureImage::prepareForEmbedding(source);
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(SignatureImageKey))
            ->toString()
            .toLatin1(),
        expectedImage.toBase64()
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(DisplayNameKey))
            ->toString(),
        QStringLiteral("Recovered Name")
        );
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
    page.openTab(WorkspaceTab::Details);
    QVERIFY(page.personalDetailsPage());
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
    FakeUserPromptService prompts;
    ScopedUserPromptService promptScope(prompts);

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

    QVERIFY(!page.saveChanges());

    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
    QCOMPARE(
        prompts.messages.constFirst().details,
        QStringLiteral(
            "The current campus could not be read. Your displayed campus "
            "selection has been preserved. Refresh this page to try again."
            )
        );

    QCOMPARE(login->text(), QStringLiteral("  "));
    QCOMPARE(password->text(), QStringLiteral(" \t "));
    QVERIFY(page.personalDetailsPage()->hasUnsavedChanges());

    page.personalDetailsPage()->saveData();
    QLabel* const saveStatus = page.personalDetailsPage()->findChild<QLabel*>(
        QStringLiteral("personalDetailsSaveStatus")
        );
    QVERIFY(saveStatus);
    QVERIFY(saveStatus->isVisible());
    QCOMPARE(
        saveStatus->text(),
        QStringLiteral(
            "Your personal details could not be saved. Your changes are "
            "still here."
            )
        );
    auto* autosave = page.personalDetailsPage()
        ->findChild<AutosaveCoordinator*>();
    QVERIFY(autosave);
    QSignalSpy autosaveRequests(
        autosave,
        &AutosaveCoordinator::saveRequested
        );
    QVERIFY(autosaveRequests.isValid());
    autosave->requestSave(false);
    QCOMPARE(autosaveRequests.size(), 1);
    QCOMPARE(autosaveRequests.constFirst().at(0).toBool(), false);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(login->text(), QStringLiteral("  "));
    QCOMPARE(password->text(), QStringLiteral(" \t "));
    QVERIFY(page.personalDetailsPage()->hasUnsavedChanges());
}

void MyWorkspacePageTests::
aggregateSaveFailureRollsBackAndPreservesUnrelatedSettings()
{
    FakeUserPromptService prompts;
    ScopedUserPromptService promptScope(prompts);

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
    page.setSaveMode(SaveMode::Manual);
    auto* name = personalNameEditor(page);
    auto* login = personalZoomLoginEditor(page);
    auto* password = personalZoomPasswordEditor(page);
    auto* unavailable = personalZoomUnavailableCheck(page);
    QVERIFY(name);
    QVERIFY(login);
    QVERIFY(password);
    QVERIFY(unavailable);
    name->setText(QStringLiteral("new name"));
    login->clear();
    password->setText(QStringLiteral(" \t "));
    unavailable->setChecked(false);

    QVERIFY(page.personalDetailsPage()->hasUnsavedChanges());
    QVERIFY(!page.saveChanges());

    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
    QVERIFY(prompts.messages.constFirst().details.contains(
        QStringLiteral("forced personal details page save failure")
        ));
    QVERIFY(prompts.messages.constFirst().message.contains(
        QStringLiteral("changes are still here")
        ));
    QCOMPARE(name->text(), QStringLiteral("new name"));
    QCOMPARE(login->text(), QString());
    QCOMPARE(password->text(), QStringLiteral(" \t "));
    QVERIFY(!unavailable->isChecked());
    QVERIFY(page.personalDetailsPage()->hasUnsavedChanges());

    page.personalDetailsPage()->saveData();
    auto* autosave = page.personalDetailsPage()
        ->findChild<AutosaveCoordinator*>();
    QVERIFY(autosave);
    QSignalSpy autosaveRequests(
        autosave,
        &AutosaveCoordinator::saveRequested
        );
    QVERIFY(autosaveRequests.isValid());
    autosave->requestSave(false);
    QCOMPARE(autosaveRequests.size(), 1);
    QCOMPARE(autosaveRequests.constFirst().at(0).toBool(), false);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(login->text(), QString());
    QCOMPARE(password->text(), QStringLiteral(" \t "));
    QVERIFY(page.personalDetailsPage()->hasUnsavedChanges());

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

    QVERIFY(executeSql(
        services,
        QStringLiteral("DROP TRIGGER fail_personal_details_page_save")
        ));
    QVERIFY(page.saveChanges());
    QCOMPARE(prompts.messages.size(), 1);
    QVERIFY(!page.personalDetailsPage()->hasUnsavedChanges());
    QCOMPARE(name->text(), QStringLiteral("new name"));
    QCOMPARE(login->text(), QStringLiteral("N/A"));
    QCOMPARE(password->text(), QStringLiteral("N/A"));
    QVERIFY(!unavailable->isChecked());

    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(DisplayNameKey))
            ->toString(),
        QStringLiteral("new name")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(ZoomLoginIdKey))
            ->toString(),
        QStringLiteral("N/A")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(ZoomPasswordKey))
            ->toString(),
        QStringLiteral("N/A")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(ZoomNotAvailableKey))
            ->toBool(),
        false
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(UnrelatedKey))
            ->toString(),
        QStringLiteral("preserved")
        );
}

void MyWorkspacePageTests::
aggregateSaveNoninteractiveFailureShowsStatusAndAllowsRetry()
{
    FakeUserPromptService prompts;
    ScopedUserPromptService promptScope(prompts);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(openDatabase(services, directory));
    QVERIFY(services.dataService());

    for (const auto& setting : {
             std::pair{QString::fromUtf8(DisplayNameKey),
                       QVariant(QStringLiteral("old name"))},
             std::pair{QString::fromUtf8(CurrentCampusKey),
                       QVariant(QStringLiteral("Jeongja"))},
             std::pair{QString::fromUtf8(ZoomLoginIdKey),
                       QVariant(QStringLiteral("old login"))},
             std::pair{QString::fromUtf8(ZoomPasswordKey),
                       QVariant(QStringLiteral("old password"))},
             std::pair{QString::fromUtf8(ZoomNotAvailableKey), QVariant(true)},
             std::pair{QString::fromUtf8(SignatureImageKey),
                       QVariant(QStringLiteral("old-image"))},
             std::pair{QString::fromUtf8(SignatureModeKey), QVariant(0)},
             std::pair{QString::fromUtf8(TypedSignatureTextKey),
                       QVariant(QStringLiteral("old text"))},
             std::pair{QString::fromUtf8(TypedSignatureFontKey), QVariant(1)},
             std::pair{QString::fromUtf8(UnrelatedKey),
                       QVariant(QStringLiteral("preserved"))}
         })
    {
        QVERIFY(services.dataService()->saveSetting(
            setting.first,
            setting.second
            ));
    }
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
    page.setSaveMode(SaveMode::Manual);

    PersonalDetailsPage* const details = page.personalDetailsPage();
    QVERIFY(details);
    QLineEdit* const name = personalNameEditor(page);
    QLineEdit* const login = personalZoomLoginEditor(page);
    QLineEdit* const password = personalZoomPasswordEditor(page);
    QCheckBox* const unavailable = personalZoomUnavailableCheck(page);
    QLabel* const status = details->findChild<QLabel*>(
        QStringLiteral("personalDetailsSaveStatus")
        );
    QVERIFY(name);
    QVERIFY(login);
    QVERIFY(password);
    QVERIFY(unavailable);
    QVERIFY(status);
    QVERIFY(status->isHidden());
    QVERIFY(status->text().isEmpty());
    AutosaveCoordinator* const autosave =
        details->findChild<AutosaveCoordinator*>();
    QVERIFY(autosave);
    QSignalSpy saveRequests(
        autosave,
        &AutosaveCoordinator::saveRequested
        );
    QVERIFY(saveRequests.isValid());

    name->setText(QStringLiteral("new name"));
    login->clear();
    password->setText(QStringLiteral(" \t "));
    unavailable->setChecked(false);
    QVERIFY(details->hasUnsavedChanges());

    autosave->requestSave(false);
    QCOMPARE(saveRequests.size(), 1);
    QCOMPARE(saveRequests.constFirst().at(0).toBool(), false);

    QVERIFY(status->isVisible());
    QCOMPARE(
        status->text(),
        QStringLiteral(
            "Your personal details could not be saved. Your changes are "
            "still here."
            )
        );
    QVERIFY(prompts.messages.isEmpty());
    QCOMPARE(name->text(), QStringLiteral("new name"));
    QCOMPARE(login->text(), QString());
    QCOMPARE(password->text(), QStringLiteral(" \t "));
    QVERIFY(!unavailable->isChecked());
    QVERIFY(details->hasUnsavedChanges());

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

    QVERIFY(executeSql(
        services,
        QStringLiteral("DROP TRIGGER fail_personal_details_page_save")
        ));
    autosave->requestSave(false);
    QCOMPARE(saveRequests.size(), 2);
    QCOMPARE(saveRequests.constLast().at(0).toBool(), false);

    QVERIFY(status->isHidden());
    QVERIFY(status->text().isEmpty());
    QVERIFY(!details->hasUnsavedChanges());
    QVERIFY(prompts.messages.isEmpty());
    QCOMPARE(name->text(), QStringLiteral("new name"));
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(DisplayNameKey))
            ->toString(),
        QStringLiteral("new name")
        );
}

QTEST_MAIN(MyWorkspacePageTests)

#include "my_workspace_page_tests.moc"
