#include "app/mainwindow.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/settings_repository.h"
#include "domain/models/class_info.h"
#include "features/classes/ui/classes_page.h"
#include "features/schedule/ui/schedule_page.h"
#include "next/application/class_visibility_preferences.h"
#include "next/application/class_day_filter_reset_policy.h"
#include "next/application/class_selection_reset_policy.h"
#include "next/application/evaluation_default_policy_preferences.h"
#include "next/application/middle_school_analytics_preferences.h"
#include "next/platform/settings_manager_excel_import_timeout_port.h"
#include "next/platform/application_services_class_visibility_preferences_port.h"
#include "next/platform/application_services_class_day_filter_reset_policy_port.h"
#include "next/platform/application_services_class_selection_reset_policy_port.h"
#include "next/platform/application_services_evaluation_default_policy_port.h"
#include "next/platform/application_services_middle_school_analytics_preferences_port.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/navigation_tab_widget.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDialog>
#include <QMessageBox>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QSqlQuery>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include <memory>
#include <utility>

namespace
{

constexpr auto PreferenceKey =
    "classes_navigation_visibility_scope";

bool executeSql(
    DatabaseSession* session,
    const QString& statement
    )
{
    if (!session || !session->isOpen())
    {
        return false;
    }

    QSqlQuery query(session->database());
    return query.exec(statement);
}

QString persistedScope(SettingsRepository* repository)
{
    if (!repository)
    {
        return {};
    }

    const auto loaded = repository->loadSetting(
        QString::fromLatin1(PreferenceKey)
        );
    return loaded && loaded->isValid()
        ? loaded->toString()
        : QString();
}

int auditRowCount(DatabaseSession* session)
{
    if (!session || !session->isOpen())
    {
        return -1;
    }

    QSqlQuery query(session->database());
    if (!query.exec(QStringLiteral(
            "SELECT COUNT(*) FROM f659_class_visibility_save_audit"
            ))
        || !query.next())
    {
        return -1;
    }

    return query.value(0).toInt();
}

int middleSchoolAuditRowCount(DatabaseSession* session)
{
    if (!session || !session->isOpen())
    {
        return -1;
    }

    QSqlQuery query(session->database());
    if (!query.exec(QStringLiteral(
            "SELECT COUNT(*) FROM f663_middle_school_preference_save_audit"
            ))
        || !query.next())
    {
        return -1;
    }

    return query.value(0).toInt();
}

int evaluationDefaultPolicyAuditRowCount(DatabaseSession* session)
{
    if (!session || !session->isOpen())
    {
        return -1;
    }

    QSqlQuery query(session->database());
    if (!query.exec(QStringLiteral(
            "SELECT COUNT(*) FROM f664_evaluation_default_policy_save_audit"
            ))
        || !query.next())
    {
        return -1;
    }

    return query.value(0).toInt();
}

int dayFilterResetPolicyAuditRowCount(DatabaseSession* session)
{
    if (!session || !session->isOpen())
    {
        return -1;
    }

    QSqlQuery query(session->database());
    if (!query.exec(QStringLiteral(
            "SELECT COUNT(*) FROM f666_day_filter_reset_policy_save_audit"
            ))
        || !query.next())
    {
        return -1;
    }

    return query.value(0).toInt();
}

int classSelectionResetPolicyAuditRowCount(DatabaseSession* session)
{
    if (!session || !session->isOpen())
    {
        return -1;
    }

    QSqlQuery query(session->database());
    if (!query.exec(QStringLiteral(
            "SELECT COUNT(*) FROM f667_class_selection_reset_policy_save_audit"
            ))
        || !query.next())
    {
        return -1;
    }

    return query.value(0).toInt();
}

} // namespace

class MainWindowClassVisibilityPreferencesFailureTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void noDatabaseScheduleDisplayPreferenceFailureRestoresDefaultsWithoutRefreshingSchedule();
    void preferenceFailureRestoresSelectionAndSuccessfulRetryRefreshesOnce();
    void middleSchoolPreferenceFailureRestoresSelectionAndSuccessfulRetryRefreshesOnce();
    void middleSchoolPreferenceReadFailurePreservesKnownVisibleState();
    void evaluationDefaultPolicyReadFailureShowsWarningAndDisablesControls();
    void evaluationDefaultPolicyFailureRestoresSelectionAndSuccessfulRetryDoesNotRefresh();
    void evaluationDefaultPolicyReadFailureAfterSaveFailureRestoresLastKnownSelection();
    void dayFilterResetPolicyFailureRestoresSelectionAndRetryPersistsOnce();
    void classSelectionResetPolicyFailureRestoresSelectionAndRetryPersistsOnce();
    void excelImportTimeoutFailureRestoresSelectionAndAllowsRetry();

private:
    std::unique_ptr<QTemporaryDir> m_settingsDirectory;
    std::unique_ptr<QTemporaryDir> m_databaseDirectory;
};

void MainWindowClassVisibilityPreferencesFailureTests::initTestCase()
{
    const QString isolatedTempRoot =
        QDir::cleanPath(qEnvironmentVariable("TMP"));
    QVERIFY(!isolatedTempRoot.isEmpty());
    QCOMPARE(
        QDir::cleanPath(qEnvironmentVariable("TEMP")),
        isolatedTempRoot
        );
    QCOMPARE(
        QDir::cleanPath(qEnvironmentVariable("TMPDIR")),
        isolatedTempRoot
        );
    QCOMPARE(
        QDir::cleanPath(
            QStandardPaths::writableLocation(
                QStandardPaths::TempLocation
                )
            ),
        isolatedTempRoot
        );
    QVERIFY(QDir().mkpath(isolatedTempRoot));

    m_settingsDirectory = std::make_unique<QTemporaryDir>();
    QVERIFY(m_settingsDirectory->isValid());
    m_databaseDirectory = std::make_unique<QTemporaryDir>();
    QVERIFY(m_databaseDirectory->isValid());
    QVERIFY(qputenv(
        "CLASSMNGR_SETTINGS_ROOT",
        m_settingsDirectory->path().toUtf8()
        ));
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void MainWindowClassVisibilityPreferencesFailureTests::
noDatabaseScheduleDisplayPreferenceFailureRestoresDefaultsWithoutRefreshingSchedule()
{
    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );

    QVERIFY(window.services());
    DatabaseSession* const session = window.services()->databaseSession();
    QVERIFY(session);
    QVERIFY(!session->isOpen());

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    pages->showPage(PageType::Schedule);
    SchedulePage* const schedulePage = pages->schedulePage();
    QVERIFY(schedulePage);
    QVERIFY(!schedulePage->needsRefresh());
    pages->showPage(PageType::Classes);
    QVERIFY(!pages->isCurrentPage(PageType::Schedule));
    QVERIFY(!schedulePage->needsRefresh());

    QAction* const preferencesAction = window.findChild<QAction*>(
        QStringLiteral("preferencesAction")
        );
    QVERIFY(preferencesAction);

    bool preferencesDialogObserved = false;
    bool warningObserved = false;
    int warningCount = 0;
    QString warningTitle;
    QString warningText;
    QString scriptFailure;
    bool allDefaultsRestored = false;
    bool scheduleRemainedFresh = false;

    QTimer::singleShot(
        0,
        &window,
        [&]()
        {
            QDialog* const dialog = window.findChild<QDialog*>(
                QStringLiteral("preferencesDialog")
                );
            const auto fail = [&](const QString& reason)
            {
                scriptFailure = reason;
                if (dialog)
                {
                    dialog->reject();
                }
            };
            if (!dialog)
            {
                fail(QStringLiteral("Preferences dialog was not opened."));
                return;
            }

            auto* const tabs = dialog->findChild<QTabWidget*>(
                QStringLiteral("preferencesTabs")
                );
            QWidget* const scheduleTab = dialog->findChild<QWidget*>(
                QStringLiteral("preferencesScheduleTab")
                );
            auto* const showEnglishNames = dialog->findChild<QCheckBox*>(
                QStringLiteral("preferencesScheduleShowEnglishNames")
                );
            auto* const use24HourTime = dialog->findChild<QCheckBox*>(
                QStringLiteral("preferencesScheduleUse24HourTime")
                );
            auto* const showWeekends = dialog->findChild<QCheckBox*>(
                QStringLiteral("preferencesScheduleShowWeekends")
                );
            auto* const showAllHours = dialog->findChild<QCheckBox*>(
                QStringLiteral("preferencesScheduleShowAllIntensiveHours")
                );
            auto* const testingAffectsM1 = dialog->findChild<QCheckBox*>(
                QStringLiteral("preferencesScheduleTestingAffectsM1")
                );
            if (!tabs || !scheduleTab || !showEnglishNames || !use24HourTime
                || !showWeekends || !showAllHours || !testingAffectsM1
                || tabs->indexOf(scheduleTab) < 0)
            {
                fail(QStringLiteral(
                    "Schedule display preference controls were missing."
                    ));
                return;
            }

            preferencesDialogObserved = true;
            tabs->setCurrentWidget(scheduleTab);
            if (showEnglishNames->isChecked() || use24HourTime->isChecked()
                || showWeekends->isChecked() || showAllHours->isChecked()
                || testingAffectsM1->isChecked())
            {
                fail(QStringLiteral(
                    "No-database schedule preference defaults were not false."
                    ));
                return;
            }

            for (QCheckBox* checkBox : {
                     showEnglishNames,
                     use24HourTime,
                     showWeekends,
                     showAllHours,
                     testingAffectsM1
                 })
            {
                const QSignalBlocker blocker(checkBox);
                checkBox->setChecked(true);
            }

            QTimer::singleShot(
                0,
                &window,
                [&]()
                {
                    QWidget* const activeModal =
                        QApplication::activeModalWidget();
                    auto* const warning = qobject_cast<QMessageBox*>(
                        activeModal
                        );
                    if (!warning)
                    {
                        if (activeModal)
                        {
                            activeModal->close();
                        }
                        scriptFailure = QStringLiteral(
                            "Unavailable schedule preferences did not show a warning."
                            );
                        return;
                    }

                    ++warningCount;
                    warningObserved = true;
                    warningTitle = warning->windowTitle();
                    warningText = warning->text();
                    warning->accept();
                }
                );

            use24HourTime->click();
            if (!scriptFailure.isEmpty())
            {
                dialog->reject();
                return;
            }

            allDefaultsRestored =
                !showEnglishNames->isChecked()
                && !use24HourTime->isChecked()
                && !showWeekends->isChecked()
                && !showAllHours->isChecked()
                && !testingAffectsM1->isChecked();
            scheduleRemainedFresh = !schedulePage->needsRefresh();
            if (!warningObserved
                || warningCount != 1
                || warningTitle != QStringLiteral("Unable to Save Preference")
                || !warningText.contains(QStringLiteral("open database"))
                || !allDefaultsRestored
                || !scheduleRemainedFresh)
            {
                fail(QStringLiteral(
                    "Unavailable schedule preference save did not warn, restore all defaults, and preserve schedule freshness."
                    ));
                return;
            }

            dialog->accept();
        }
        );

    preferencesAction->trigger();

    QVERIFY2(scriptFailure.isEmpty(), qPrintable(scriptFailure));
    QVERIFY(preferencesDialogObserved);
    QVERIFY(warningObserved);
    QCOMPARE(warningCount, 1);
    QCOMPARE(warningTitle, QStringLiteral("Unable to Save Preference"));
    QVERIFY(warningText.contains(QStringLiteral("open database")));
    QVERIFY(allDefaultsRestored);
    QVERIFY(scheduleRemainedFresh);
}

void MainWindowClassVisibilityPreferencesFailureTests::
preferenceFailureRestoresSelectionAndSuccessfulRetryRefreshesOnce()
{
    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );

    const QString databasePath = m_databaseDirectory->filePath(
        QStringLiteral("class-visibility-preferences.tps")
        );
    const Status opened = window.services()->openDatabase(databasePath);
    QVERIFY2(opened.has_value(), "Could not open the test database.");
    window.applyDatabaseLoadedState();

    ClassMngr::Next::Platform::
        ApplicationServicesClassVisibilityPreferencesPort preferences(
            *window.services()
            );
    QVERIFY(preferences.save(
        ClassMngr::Next::Application::ClassVisibilityScope::ActiveSchedule
        ));

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    pages->showPage(PageType::Classes);
    ClassesPage* const classesPage = pages->classesPage();
    QVERIFY(classesPage);
    const int initialClassQueryCount =
        classesPage->runtimeMetrics().classQueryCount;
    QVERIFY(initialClassQueryCount > 0);
    QVERIFY(!classesPage->needsRefresh());

    DatabaseSession* const session = window.services()->databaseSession();
    QVERIFY(session);
    QVERIFY(executeSql(
        session,
        QStringLiteral(
            "CREATE TABLE f659_class_visibility_save_audit (value TEXT NOT NULL)"
            )
        ));
    QVERIFY(executeSql(
        session,
        QStringLiteral(R"(
            CREATE TRIGGER fail_class_visibility_preference_write
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'classes_navigation_visibility_scope'
            BEGIN
                SELECT RAISE(ABORT, 'forced class visibility write failure');
            END
        )")
        ));
    QVERIFY(executeSql(
        session,
        QStringLiteral(R"(
            CREATE TRIGGER audit_class_visibility_preference_update
            AFTER UPDATE ON app_settings
            WHEN NEW.key = 'classes_navigation_visibility_scope'
            BEGIN
                INSERT INTO f659_class_visibility_save_audit(value)
                VALUES (NEW.value);
            END
        )")
        ));

    QAction* const preferencesAction = window.findChild<QAction*>(
        QStringLiteral("preferencesAction")
        );
    QVERIFY(preferencesAction);

    bool preferencesDialogObserved = false;
    bool failedSelectionRestored = false;
    bool warningObserved = false;
    int warningCount = 0;
    QString warningTitle;
    QString warningText;
    QString scriptFailure;
    int classQueriesAfterFailure = -1;
    int classQueriesAfterRetry = -1;
    QString scopeAfterFailure;
    QString scopeAfterRetry;
    int auditsAfterFailure = -1;
    int auditsAfterRetry = -1;

    QTimer::singleShot(
        0,
        &window,
        [&]()
        {
            QDialog* const dialog = window.findChild<QDialog*>(
                QStringLiteral("preferencesDialog")
                );
            const auto fail = [&](const QString& reason)
            {
                scriptFailure = reason;
                if (dialog)
                {
                    dialog->reject();
                }
            };
            if (!dialog)
            {
                fail(QStringLiteral("Preferences dialog was not opened."));
                return;
            }

            auto* const tabs = dialog->findChild<QTabWidget*>(
                QStringLiteral("preferencesTabs")
                );
            auto* const allClasses = dialog->findChild<QRadioButton*>(
                QStringLiteral("preferencesNavigationAllClasses")
                );
            auto* const activeSchedule = dialog->findChild<QRadioButton*>(
                QStringLiteral("preferencesNavigationActiveSchedule")
                );
            if (!tabs || !allClasses || !activeSchedule)
            {
                fail(QStringLiteral(
                    "Preferences visibility controls were missing."
                    ));
                return;
            }

            QWidget* const navigationTab = dialog->findChild<QWidget*>(
                QStringLiteral("preferencesNavigationTab")
                );
            if (!navigationTab || tabs->indexOf(navigationTab) < 0)
            {
                fail(QStringLiteral(
                    "Navigation preferences tab was missing."
                    ));
                return;
            }

            preferencesDialogObserved = true;
            tabs->setCurrentWidget(navigationTab);
            if (allClasses->isChecked() || !activeSchedule->isChecked())
            {
                fail(QStringLiteral(
                    "Initial radio selection did not match storage."
                    ));
                return;
            }

            QTimer::singleShot(
                0,
                &window,
                [&]()
                {
                    QWidget* const activeModal =
                        QApplication::activeModalWidget();
                    auto* const warning = qobject_cast<QMessageBox*>(
                        activeModal
                        );
                    if (!warning)
                    {
                        if (activeModal)
                        {
                            activeModal->close();
                        }
                        scriptFailure = QStringLiteral(
                            "Preference write failure did not surface a warning dialog."
                            );
                        return;
                    }

                    ++warningCount;
                    warningObserved = true;
                    warningTitle = warning->windowTitle();
                    warningText = warning->text();
                    warning->accept();
                }
                );

            allClasses->click();
            if (!scriptFailure.isEmpty())
            {
                dialog->reject();
                return;
            }

            failedSelectionRestored =
                !allClasses->isChecked() && activeSchedule->isChecked();
            classQueriesAfterFailure =
                classesPage->runtimeMetrics().classQueryCount;
            scopeAfterFailure = persistedScope(
                session->settingsRepository()
                );
            auditsAfterFailure = auditRowCount(session);
            if (!warningObserved
                || warningCount != 1
                || warningText.isEmpty()
                || !warningText.contains(
                    QStringLiteral("forced class visibility write failure")
                    )
                || !failedSelectionRestored
                || classQueriesAfterFailure != initialClassQueryCount
                || scopeAfterFailure != QStringLiteral("active_schedule")
                || auditsAfterFailure != 0)
            {
                fail(QStringLiteral(
                    "Failed preference write did not preserve persisted and displayed state."
                    ));
                return;
            }

            if (!executeSql(
                    session,
                    QStringLiteral(
                        "DROP TRIGGER fail_class_visibility_preference_write"
                        )
                    ))
            {
                fail(QStringLiteral("Could not remove the test failure trigger."));
                return;
            }

            allClasses->click();
            classQueriesAfterRetry =
                classesPage->runtimeMetrics().classQueryCount;
            scopeAfterRetry = persistedScope(session->settingsRepository());
            auditsAfterRetry = auditRowCount(session);
            if (!allClasses->isChecked()
                || activeSchedule->isChecked()
                || classQueriesAfterRetry != initialClassQueryCount + 1
                || scopeAfterRetry != QStringLiteral("all_classes")
                || auditsAfterRetry != 1)
            {
                fail(QStringLiteral(
                    "Successful preference retry did not save and refresh exactly once."
                    ));
                return;
            }

            dialog->accept();
        }
        );

    preferencesAction->trigger();

    QVERIFY2(scriptFailure.isEmpty(), qPrintable(scriptFailure));
    QVERIFY(preferencesDialogObserved);
    QVERIFY(warningObserved);
    QCOMPARE(warningCount, 1);
    QCOMPARE(
        warningTitle,
        QStringLiteral("Unable to Save Preference")
        );
    QVERIFY(warningText.contains(
        QStringLiteral("forced class visibility write failure")
        ));
    QVERIFY(failedSelectionRestored);
    QCOMPARE(classQueriesAfterFailure, initialClassQueryCount);
    QCOMPARE(scopeAfterFailure, QStringLiteral("active_schedule"));
    QCOMPARE(auditsAfterFailure, 0);
    QCOMPARE(classQueriesAfterRetry, initialClassQueryCount + 1);
    QCOMPARE(scopeAfterRetry, QStringLiteral("all_classes"));
    QCOMPARE(auditsAfterRetry, 1);
}

void MainWindowClassVisibilityPreferencesFailureTests::
middleSchoolPreferenceFailureRestoresSelectionAndSuccessfulRetryRefreshesOnce()
{
    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );

    const QString databasePath = m_databaseDirectory->filePath(
        QStringLiteral("middle-school-analytics-preferences.tps")
        );
    const Status opened = window.services()->openDatabase(databasePath);
    QVERIFY2(opened.has_value(), "Could not open the test database.");
    window.applyDatabaseLoadedState();

    ClassMngr::Next::Platform::
        ApplicationServicesMiddleSchoolAnalyticsPreferencesPort preferences(
            *window.services()
            );
    const auto initialSave = preferences.save(false);
    QVERIFY(initialSave);

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    pages->showPage(PageType::Classes);
    ClassesPage* const classesPage = pages->classesPage();
    QVERIFY(classesPage);
    const int initialClassQueryCount =
        classesPage->runtimeMetrics().classQueryCount;
    QVERIFY(initialClassQueryCount > 0);
    QVERIFY(!classesPage->needsRefresh());

    DatabaseSession* const session = window.services()->databaseSession();
    QVERIFY(session);
    QVERIFY(executeSql(
        session,
        QStringLiteral(R"(
            CREATE TABLE f663_middle_school_preference_save_audit (
                value TEXT NOT NULL
            )
            )")
        ));
    QVERIFY(executeSql(
        session,
        QStringLiteral(R"(
            CREATE TRIGGER fail_middle_school_preference_write
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'classes_navigation_show_middle_school_analytics_and_evaluations'
            BEGIN
                SELECT RAISE(ABORT, 'forced middle-school preference write failure');
            END
            )")
        ));
    QVERIFY(executeSql(
        session,
        QStringLiteral(R"(
            CREATE TRIGGER audit_middle_school_preference_update
            AFTER UPDATE ON app_settings
            WHEN NEW.key = 'classes_navigation_show_middle_school_analytics_and_evaluations'
            BEGIN
                INSERT INTO f663_middle_school_preference_save_audit(value)
                VALUES (NEW.value);
            END
            )")
        ));

    QAction* const preferencesAction = window.findChild<QAction*>(
        QStringLiteral("preferencesAction")
        );
    QVERIFY(preferencesAction);

    bool preferencesDialogObserved = false;
    bool failedSelectionRestored = false;
    bool warningObserved = false;
    int warningCount = 0;
    QString warningTitle;
    QString warningText;
    QString scriptFailure;
    int classQueriesAfterFailure = -1;
    int classQueriesAfterRetry = -1;
    bool preferenceAfterFailure = true;
    bool preferenceAfterRetry = false;
    int auditsAfterFailure = -1;
    int auditsAfterRetry = -1;

    QTimer::singleShot(
        0,
        &window,
        [&]()
        {
            QDialog* const dialog = window.findChild<QDialog*>(
                QStringLiteral("preferencesDialog")
                );
            const auto fail = [&](const QString& reason)
            {
                scriptFailure = reason;
                if (dialog)
                {
                    dialog->reject();
                }
            };
            if (!dialog)
            {
                fail(QStringLiteral("Preferences dialog was not opened."));
                return;
            }

            auto* const tabs = dialog->findChild<QTabWidget*>(
                QStringLiteral("preferencesTabs")
                );
            auto* const showAnalytics = dialog->findChild<QCheckBox*>(
                QStringLiteral(
                    "preferencesNavigationShowMiddleSchoolAnalyticsAndEvaluations"
                    )
                );
            QWidget* const navigationTab = dialog->findChild<QWidget*>(
                QStringLiteral("preferencesNavigationTab")
                );
            if (!tabs || !showAnalytics || !navigationTab
                || tabs->indexOf(navigationTab) < 0)
            {
                fail(QStringLiteral(
                    "Middle-school analytics preference controls were missing."
                    ));
                return;
            }

            preferencesDialogObserved = true;
            tabs->setCurrentWidget(navigationTab);
            if (showAnalytics->isChecked())
            {
                fail(QStringLiteral(
                    "Initial middle-school preference did not match storage."
                    ));
                return;
            }

            QTimer::singleShot(
                0,
                &window,
                [&]()
                {
                    QWidget* const activeModal =
                        QApplication::activeModalWidget();
                    auto* const warning = qobject_cast<QMessageBox*>(
                        activeModal
                        );
                    if (!warning)
                    {
                        if (activeModal)
                        {
                            activeModal->close();
                        }
                        scriptFailure = QStringLiteral(
                            "Preference write failure did not surface a warning dialog."
                            );
                        return;
                    }

                    ++warningCount;
                    warningObserved = true;
                    warningTitle = warning->windowTitle();
                    warningText = warning->text();
                    warning->accept();
                }
                );

            showAnalytics->click();
            if (!scriptFailure.isEmpty())
            {
                dialog->reject();
                return;
            }

            failedSelectionRestored = !showAnalytics->isChecked();
            classQueriesAfterFailure =
                classesPage->runtimeMetrics().classQueryCount;
            const auto persistedAfterFailure =
                session->settingsRepository()->loadSetting(
                    QStringLiteral(
                        "classes_navigation_show_middle_school_analytics_and_evaluations"
                        )
                    );
            preferenceAfterFailure = persistedAfterFailure
                && persistedAfterFailure->isValid()
                && persistedAfterFailure->toBool();
            auditsAfterFailure = middleSchoolAuditRowCount(session);
            if (!warningObserved
                || warningCount != 1
                || warningTitle != QStringLiteral("Unable to Save Preference")
                || !warningText.contains(QStringLiteral(
                    "forced middle-school preference write failure"
                    ))
                || !failedSelectionRestored
                || classQueriesAfterFailure != initialClassQueryCount
                || preferenceAfterFailure
                || auditsAfterFailure != 0)
            {
                fail(QStringLiteral(
                    "Failed middle-school preference write did not preserve persisted and displayed state."
                    ));
                return;
            }

            if (!executeSql(
                    session,
                    QStringLiteral(
                        "DROP TRIGGER fail_middle_school_preference_write"
                        )
                    ))
            {
                fail(QStringLiteral(
                    "Could not remove the middle-school preference failure trigger."
                    ));
                return;
            }

            showAnalytics->click();
            classQueriesAfterRetry =
                classesPage->runtimeMetrics().classQueryCount;
            const auto persistedAfterRetry =
                session->settingsRepository()->loadSetting(
                    QStringLiteral(
                        "classes_navigation_show_middle_school_analytics_and_evaluations"
                        )
                    );
            preferenceAfterRetry = persistedAfterRetry
                && persistedAfterRetry->isValid()
                && persistedAfterRetry->toBool();
            auditsAfterRetry = middleSchoolAuditRowCount(session);
            if (!showAnalytics->isChecked()
                || classQueriesAfterRetry != initialClassQueryCount + 1
                || !preferenceAfterRetry
                || auditsAfterRetry != 1)
            {
                fail(QStringLiteral(
                    "Successful middle-school preference retry did not save and refresh exactly once."
                    ));
                return;
            }

            dialog->accept();
        }
        );

    preferencesAction->trigger();

    QVERIFY2(scriptFailure.isEmpty(), qPrintable(scriptFailure));
    QVERIFY(preferencesDialogObserved);
    QVERIFY(warningObserved);
    QCOMPARE(warningCount, 1);
    QCOMPARE(warningTitle, QStringLiteral("Unable to Save Preference"));
    QVERIFY(warningText.contains(QStringLiteral(
        "forced middle-school preference write failure"
        )));
    QVERIFY(failedSelectionRestored);
    QCOMPARE(classQueriesAfterFailure, initialClassQueryCount);
    QVERIFY(!preferenceAfterFailure);
    QCOMPARE(auditsAfterFailure, 0);
    QCOMPARE(classQueriesAfterRetry, initialClassQueryCount + 1);
    QVERIFY(preferenceAfterRetry);
    QCOMPARE(auditsAfterRetry, 1);
}

void MainWindowClassVisibilityPreferencesFailureTests::
middleSchoolPreferenceReadFailurePreservesKnownVisibleState()
{
    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );

    const QString databasePath = m_databaseDirectory->filePath(
        QStringLiteral("middle-school-analytics-read-failure.tps")
        );
    const Status opened = window.services()->openDatabase(databasePath);
    QVERIFY2(opened.has_value(), "Could not open the test database.");
    window.applyDatabaseLoadedState();

    auto* const services = window.services();
    QVERIFY(services);
    const auto createdClass = services->classService()->create(
        QStringLiteral("Middle School Read Failure")
        );
    QVERIFY(createdClass.has_value());
    DatabaseSession* const session = services->databaseSession();
    QVERIFY(session);
    ClassInfo classInfo;
    classInfo.classId = createdClass.value();
    classInfo.classGrade = QStringLiteral("M1");
    QVERIFY(session->classInfoRepository()->saveClassInfo(classInfo));

    ClassMngr::Next::Platform::
        ApplicationServicesMiddleSchoolAnalyticsPreferencesPort preferences(
            *services
            );
    QVERIFY(preferences.save(true));

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    pages->showPage(PageType::Classes);
    ClassesPage* const classesPage = pages->classesPage();
    QVERIFY(classesPage);
    QVERIFY(classesPage->openClass(createdClass.value()));
    QVERIFY(classesPage->showsMiddleSchoolAnalyticsAndEvaluations());

    auto* const sectionTabs = classesPage->findChild<NavigationTabWidget*>(
        QStringLiteral("classesSectionTabs")
        );
    QVERIFY(sectionTabs);
    QCOMPARE(sectionTabs->count(), 6);
    QCOMPARE(sectionTabs->tabText(2), QStringLiteral("Analytics"));
    QCOMPARE(sectionTabs->tabText(3), QStringLiteral("Evaluations"));

    QAction* const preferencesAction = window.findChild<QAction*>(
        QStringLiteral("preferencesAction")
        );
    QVERIFY(preferencesAction);
    QVERIFY(executeSql(session, QStringLiteral("DROP TABLE app_settings")));

    classesPage->refreshNavigationPreferences();
    QVERIFY(classesPage->showsMiddleSchoolAnalyticsAndEvaluations());
    QCOMPARE(sectionTabs->count(), 6);
    QCOMPARE(sectionTabs->tabText(2), QStringLiteral("Analytics"));
    QCOMPARE(sectionTabs->tabText(3), QStringLiteral("Evaluations"));

    bool dialogObserved = false;
    bool preferenceControlPreserved = false;
    QString scriptFailure;
    const auto inspectPreferencesDialog = [&]()
    {
        QDialog* const dialog = window.findChild<QDialog*>(
            QStringLiteral("preferencesDialog")
            );
        if (!dialog)
        {
            scriptFailure = QStringLiteral(
                "Preferences dialog was not opened after a preference read failure."
                );
            return;
        }

        auto* const showAnalytics = dialog->findChild<QCheckBox*>(
            QStringLiteral(
                "preferencesNavigationShowMiddleSchoolAnalyticsAndEvaluations"
                )
            );
        if (!showAnalytics)
        {
            scriptFailure = QStringLiteral(
                "Middle-school analytics preference control was missing."
                );
            dialog->reject();
            return;
        }

        dialogObserved = true;
        preferenceControlPreserved = showAnalytics->isChecked();
        dialog->reject();
    };
    QTimer::singleShot(
        0,
        &window,
        [&]()
        {
            auto* const warning = qobject_cast<QMessageBox*>(
                QApplication::activeModalWidget()
                );
            if (warning
                && warning->windowTitle()
                    == QStringLiteral("Unable to Load Preference"))
            {
                warning->accept();
                QTimer::singleShot(
                    0,
                    &window,
                    inspectPreferencesDialog
                    );
                return;
            }
            inspectPreferencesDialog();
        }
        );

    preferencesAction->trigger();

    QVERIFY2(scriptFailure.isEmpty(), qPrintable(scriptFailure));
    QVERIFY(dialogObserved);
    QVERIFY(preferenceControlPreserved);
    QVERIFY(classesPage->showsMiddleSchoolAnalyticsAndEvaluations());
    QCOMPARE(sectionTabs->count(), 6);
    QCOMPARE(sectionTabs->tabText(2), QStringLiteral("Analytics"));
    QCOMPARE(sectionTabs->tabText(3), QStringLiteral("Evaluations"));
}

void MainWindowClassVisibilityPreferencesFailureTests::
evaluationDefaultPolicyReadFailureShowsWarningAndDisablesControls()
{
    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );

    const QString databasePath = m_databaseDirectory->filePath(
        QStringLiteral("evaluation-default-policy-initial-read-failure.tps")
        );
    QVERIFY2(
        window.services()->openDatabase(databasePath).has_value(),
        "Could not open the test database."
        );
    window.applyDatabaseLoadedState();

    ClassMngr::Next::Platform::ApplicationServicesEvaluationDefaultPolicyPort
        preferences(*window.services());
    const auto initialSave = preferences.save(
        ClassMngr::Next::Application::EvaluationDefaultPolicy::
            CurrentOrPreviousTerm
        );
    QVERIFY(initialSave);

    DatabaseSession* const session = window.services()->databaseSession();
    QVERIFY(session);
    QVERIFY(executeSql(session, QStringLiteral("DROP TABLE app_settings")));

    QAction* const preferencesAction = window.findChild<QAction*>(
        QStringLiteral("preferencesAction")
        );
    QVERIFY(preferencesAction);

    bool warningObserved = false;
    QString warningTitle;
    QString warningText;
    bool controlsDisabled = false;
    bool disabledSelectionPreserved = false;
    QString scriptFailure;

    QTimer::singleShot(
        0,
        &window,
        [&]()
        {
            auto* const warning = qobject_cast<QMessageBox*>(
                QApplication::activeModalWidget()
                );
            if (!warning)
            {
                scriptFailure = QStringLiteral(
                    "Evaluation default policy read failure did not show a warning."
                    );
                return;
            }

            warningObserved = true;
            warningTitle = warning->windowTitle();
            warningText = warning->text();
            warning->accept();

            QTimer::singleShot(
                0,
                &window,
                [&]()
                {
                    QDialog* const dialog = window.findChild<QDialog*>(
                        QStringLiteral("preferencesDialog")
                        );
                    auto* const followTermSchedule =
                        dialog ? dialog->findChild<QRadioButton*>(
                            QStringLiteral(
                                "preferencesNavigationEvaluationDefaultTermSchedule"
                                )
                            ) : nullptr;
                    auto* const defaultAnalyticsToAll =
                        dialog ? dialog->findChild<QRadioButton*>(
                            QStringLiteral(
                                "preferencesNavigationEvaluationDefaultAll"
                                )
                            ) : nullptr;
                    if (!dialog || !followTermSchedule
                        || !defaultAnalyticsToAll)
                    {
                        scriptFailure = QStringLiteral(
                            "Evaluation default policy controls were missing after the load warning."
                            );
                        if (dialog)
                        {
                            dialog->reject();
                        }
                        return;
                    }

                    controlsDisabled = !followTermSchedule->isEnabled()
                        && !defaultAnalyticsToAll->isEnabled();
                    const bool initiallyAll =
                        defaultAnalyticsToAll->isChecked()
                        && !followTermSchedule->isChecked();
                    followTermSchedule->click();
                    disabledSelectionPreserved = initiallyAll
                        && defaultAnalyticsToAll->isChecked()
                        && !followTermSchedule->isChecked();
                    dialog->accept();
                }
                );
        }
        );

    preferencesAction->trigger();

    QVERIFY2(scriptFailure.isEmpty(), qPrintable(scriptFailure));
    QVERIFY(warningObserved);
    QCOMPARE(warningTitle, QStringLiteral("Unable to Load Preference"));
    QVERIFY(!warningText.trimmed().isEmpty());
    QVERIFY(controlsDisabled);
    QVERIFY(disabledSelectionPreserved);
}

void MainWindowClassVisibilityPreferencesFailureTests::
evaluationDefaultPolicyFailureRestoresSelectionAndSuccessfulRetryDoesNotRefresh()
{
    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );

    const QString databasePath = m_databaseDirectory->filePath(
        QStringLiteral("evaluation-default-policy-preferences.tps")
        );
    const Status opened = window.services()->openDatabase(databasePath);
    QVERIFY2(opened.has_value(), "Could not open the test database.");
    window.applyDatabaseLoadedState();

    ClassMngr::Next::Platform::
        ApplicationServicesEvaluationDefaultPolicyPort preferences(
            *window.services()
            );
    const auto initialSave = preferences.save(
        ClassMngr::Next::Application::EvaluationDefaultPolicy::
            CurrentOrPreviousTerm
        );
    QVERIFY(initialSave);

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    pages->showPage(PageType::Classes);
    ClassesPage* const classesPage = pages->classesPage();
    QVERIFY(classesPage);
    const int initialClassQueryCount =
        classesPage->runtimeMetrics().classQueryCount;
    QVERIFY(initialClassQueryCount > 0);
    QVERIFY(!classesPage->needsRefresh());

    DatabaseSession* const session = window.services()->databaseSession();
    QVERIFY(session);
    QVERIFY(executeSql(
        session,
        QStringLiteral(R"(
            CREATE TABLE f664_evaluation_default_policy_save_audit (
                value TEXT NOT NULL
            )
            )")
        ));
    QVERIFY(executeSql(
        session,
        QStringLiteral(R"(
            CREATE TRIGGER fail_evaluation_default_policy_preference_write
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'classes_navigation_evaluation_default_policy'
            BEGIN
                SELECT RAISE(ABORT, 'forced evaluation policy write failure');
            END
            )")
        ));
    QVERIFY(executeSql(
        session,
        QStringLiteral(R"(
            CREATE TRIGGER audit_evaluation_default_policy_preference_update
            AFTER UPDATE ON app_settings
            WHEN NEW.key = 'classes_navigation_evaluation_default_policy'
            BEGIN
                INSERT INTO f664_evaluation_default_policy_save_audit(value)
                VALUES (NEW.value);
            END
            )")
        ));

    QAction* const preferencesAction = window.findChild<QAction*>(
        QStringLiteral("preferencesAction")
        );
    QVERIFY(preferencesAction);

    bool preferencesDialogObserved = false;
    bool failedSelectionRestored = false;
    bool warningObserved = false;
    int warningCount = 0;
    QString warningTitle;
    QString warningText;
    QString scriptFailure;
    int classQueriesAfterFailure = -1;
    int classQueriesAfterRetry = -1;
    QString policyAfterFailure;
    QString policyAfterRetry;
    int auditsAfterFailure = -1;
    int auditsAfterRetry = -1;
    bool retrySelectionConsistent = false;

    QTimer::singleShot(
        0,
        &window,
        [&]()
        {
            QDialog* const dialog = window.findChild<QDialog*>(
                QStringLiteral("preferencesDialog")
                );
            const auto fail = [&](const QString& reason)
            {
                scriptFailure = reason;
                if (dialog)
                {
                    dialog->reject();
                }
            };
            if (!dialog)
            {
                fail(QStringLiteral("Preferences dialog was not opened."));
                return;
            }

            auto* const tabs = dialog->findChild<QTabWidget*>(
                QStringLiteral("preferencesTabs")
                );
            auto* const followTermSchedule = dialog->findChild<QRadioButton*>(
                QStringLiteral(
                    "preferencesNavigationEvaluationDefaultTermSchedule"
                    )
                );
            auto* const defaultAnalyticsToAll = dialog->findChild<QRadioButton*>(
                QStringLiteral("preferencesNavigationEvaluationDefaultAll")
                );
            QWidget* const navigationTab = dialog->findChild<QWidget*>(
                QStringLiteral("preferencesNavigationTab")
                );
            if (!tabs || !followTermSchedule || !defaultAnalyticsToAll
                || !navigationTab || tabs->indexOf(navigationTab) < 0)
            {
                fail(QStringLiteral(
                    "Evaluation default policy controls were missing."
                    ));
                return;
            }

            preferencesDialogObserved = true;
            tabs->setCurrentWidget(navigationTab);
            if (!followTermSchedule->isChecked()
                || defaultAnalyticsToAll->isChecked())
            {
                fail(QStringLiteral(
                    "Initial evaluation default policy radios did not match storage."
                    ));
                return;
            }

            QTimer::singleShot(
                0,
                &window,
                [&]()
                {
                    QWidget* const activeModal =
                        QApplication::activeModalWidget();
                    auto* const warning = qobject_cast<QMessageBox*>(
                        activeModal
                        );
                    if (!warning)
                    {
                        if (activeModal)
                        {
                            activeModal->close();
                        }
                        scriptFailure = QStringLiteral(
                            "Evaluation default policy failure did not surface a warning dialog."
                            );
                        return;
                    }

                    ++warningCount;
                    warningObserved = true;
                    warningTitle = warning->windowTitle();
                    warningText = warning->text();
                    warning->accept();
                }
                );

            defaultAnalyticsToAll->click();
            if (!scriptFailure.isEmpty())
            {
                dialog->reject();
                return;
            }

            failedSelectionRestored =
                followTermSchedule->isChecked()
                && !defaultAnalyticsToAll->isChecked();
            classQueriesAfterFailure =
                classesPage->runtimeMetrics().classQueryCount;
            const auto persistedAfterFailure =
                session->settingsRepository()->loadSetting(
                    QStringLiteral(
                        "classes_navigation_evaluation_default_policy"
                        )
                    );
            policyAfterFailure = persistedAfterFailure
                && persistedAfterFailure->isValid()
                ? persistedAfterFailure->toString()
                : QString();
            auditsAfterFailure =
                evaluationDefaultPolicyAuditRowCount(session);
            if (!warningObserved
                || warningCount != 1
                || warningTitle != QStringLiteral("Unable to Save Preference")
                || !warningText.contains(QStringLiteral(
                    "forced evaluation policy write failure"
                    ))
                || !failedSelectionRestored
                || classQueriesAfterFailure != initialClassQueryCount
                || policyAfterFailure != QStringLiteral("current_or_previous_term")
                || auditsAfterFailure != 0)
            {
                fail(QStringLiteral(
                    "Failed evaluation default policy write did not preserve persisted and displayed state."
                    ));
                return;
            }

            if (!executeSql(
                    session,
                    QStringLiteral(
                        "DROP TRIGGER fail_evaluation_default_policy_preference_write"
                        )
                    ))
            {
                fail(QStringLiteral(
                    "Could not remove the evaluation default policy failure trigger."
                    ));
                return;
            }

            defaultAnalyticsToAll->click();
            retrySelectionConsistent =
                !followTermSchedule->isChecked()
                && defaultAnalyticsToAll->isChecked();
            classQueriesAfterRetry =
                classesPage->runtimeMetrics().classQueryCount;
            const auto persistedAfterRetry =
                session->settingsRepository()->loadSetting(
                    QStringLiteral(
                        "classes_navigation_evaluation_default_policy"
                        )
                    );
            policyAfterRetry = persistedAfterRetry
                && persistedAfterRetry->isValid()
                ? persistedAfterRetry->toString()
                : QString();
            auditsAfterRetry =
                evaluationDefaultPolicyAuditRowCount(session);
            if (!retrySelectionConsistent
                || classQueriesAfterRetry != initialClassQueryCount
                || policyAfterRetry != QStringLiteral("all")
                || auditsAfterRetry != 1)
            {
                fail(QStringLiteral(
                    "Successful evaluation default policy retry did not persist exactly once without refreshing Classes."
                    ));
                return;
            }

            dialog->accept();
        }
        );

    preferencesAction->trigger();

    QVERIFY2(scriptFailure.isEmpty(), qPrintable(scriptFailure));
    QVERIFY(preferencesDialogObserved);
    QVERIFY(warningObserved);
    QCOMPARE(warningCount, 1);
    QCOMPARE(warningTitle, QStringLiteral("Unable to Save Preference"));
    QVERIFY(warningText.contains(QStringLiteral(
        "forced evaluation policy write failure"
        )));
    QVERIFY(failedSelectionRestored);
    QCOMPARE(classQueriesAfterFailure, initialClassQueryCount);
    QCOMPARE(policyAfterFailure, QStringLiteral("current_or_previous_term"));
    QCOMPARE(auditsAfterFailure, 0);
    QVERIFY(retrySelectionConsistent);
    QCOMPARE(classQueriesAfterRetry, initialClassQueryCount);
    QCOMPARE(policyAfterRetry, QStringLiteral("all"));
    QCOMPARE(auditsAfterRetry, 1);
}

void MainWindowClassVisibilityPreferencesFailureTests::
evaluationDefaultPolicyReadFailureAfterSaveFailureRestoresLastKnownSelection()
{
    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );

    const QString databasePath = m_databaseDirectory->filePath(
        QStringLiteral("evaluation-default-policy-save-read-failure.tps")
        );
    QVERIFY2(
        window.services()->openDatabase(databasePath).has_value(),
        "Could not open the test database."
        );
    window.applyDatabaseLoadedState();

    ClassMngr::Next::Platform::ApplicationServicesEvaluationDefaultPolicyPort
        preferences(*window.services());
    QVERIFY(preferences.save(
        ClassMngr::Next::Application::EvaluationDefaultPolicy::
            CurrentOrPreviousTerm
        ));

    DatabaseSession* const session = window.services()->databaseSession();
    QVERIFY(session);
    QVERIFY(executeSql(
        session,
        QStringLiteral(R"(
            CREATE TRIGGER fail_evaluation_default_policy_write_before_read
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'classes_navigation_evaluation_default_policy'
            BEGIN
                SELECT RAISE(ABORT, 'forced evaluation policy write failure');
            END
            )")
        ));

    QAction* const preferencesAction = window.findChild<QAction*>(
        QStringLiteral("preferencesAction")
        );
    QVERIFY(preferencesAction);

    bool preferencesDialogObserved = false;
    bool warningObserved = false;
    QString warningTitle;
    QString warningText;
    bool lastKnownSelectionRestored = false;
    QString scriptFailure;

    QTimer::singleShot(
        0,
        &window,
        [&]()
        {
            QDialog* const dialog = window.findChild<QDialog*>(
                QStringLiteral("preferencesDialog")
                );
            auto* const followTermSchedule = dialog
                ? dialog->findChild<QRadioButton*>(QStringLiteral(
                    "preferencesNavigationEvaluationDefaultTermSchedule"
                    ))
                : nullptr;
            auto* const defaultAnalyticsToAll = dialog
                ? dialog->findChild<QRadioButton*>(QStringLiteral(
                    "preferencesNavigationEvaluationDefaultAll"
                    ))
                : nullptr;
            if (!dialog || !followTermSchedule || !defaultAnalyticsToAll)
            {
                scriptFailure = QStringLiteral(
                    "Evaluation default policy controls were missing."
                    );
                if (dialog)
                {
                    dialog->reject();
                }
                return;
            }

            preferencesDialogObserved = true;
            if (!followTermSchedule->isChecked()
                || defaultAnalyticsToAll->isChecked())
            {
                scriptFailure = QStringLiteral(
                    "Initial evaluation default policy radios did not match storage."
                    );
                dialog->reject();
                return;
            }

            QTimer::singleShot(
                0,
                &window,
                [&]()
                {
                    auto* const warning = qobject_cast<QMessageBox*>(
                        QApplication::activeModalWidget()
                        );
                    if (!warning)
                    {
                        scriptFailure = QStringLiteral(
                            "Evaluation default policy save failure did not show a warning."
                            );
                        return;
                    }

                    warningObserved = true;
                    warningTitle = warning->windowTitle();
                    warningText = warning->text();
                    if (!executeSql(
                            session,
                            QStringLiteral("DROP TABLE app_settings")
                            ))
                    {
                        scriptFailure = QStringLiteral(
                            "Could not inject the evaluation policy read failure."
                            );
                    }
                    warning->accept();
                }
                );

            defaultAnalyticsToAll->click();
            const auto loadAfterFailure = preferences.load();
            const bool technicalReadFailure = !loadAfterFailure
                && loadAfterFailure.error().code
                    == ClassMngr::Next::Domain::ErrorCode::Technical;
            lastKnownSelectionRestored = followTermSchedule->isChecked()
                && !defaultAnalyticsToAll->isChecked();
            if (loadAfterFailure || !technicalReadFailure
                || !lastKnownSelectionRestored)
            {
                scriptFailure = QStringLiteral(
                    "A failed save followed by a failed read did not restore the last known evaluation default policy."
                    );
            }
            dialog->accept();
        }
        );

    preferencesAction->trigger();

    QVERIFY2(scriptFailure.isEmpty(), qPrintable(scriptFailure));
    QVERIFY(preferencesDialogObserved);
    QVERIFY(warningObserved);
    QCOMPARE(warningTitle, QStringLiteral("Unable to Save Preference"));
    QVERIFY(warningText.contains(QStringLiteral(
        "forced evaluation policy write failure"
        )));
    QVERIFY(lastKnownSelectionRestored);
}

void MainWindowClassVisibilityPreferencesFailureTests::
dayFilterResetPolicyFailureRestoresSelectionAndRetryPersistsOnce()
{
    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );

    const QString databasePath = m_databaseDirectory->filePath(
        QStringLiteral("day-filter-reset-policy-preferences.tps")
        );
    const Status opened = window.services()->openDatabase(databasePath);
    QVERIFY2(opened.has_value(), "Could not open the test database.");
    window.applyDatabaseLoadedState();

    ClassMngr::Next::Platform::
        ApplicationServicesClassDayFilterResetPolicyPort policyPort(
            *window.services()
            );
    const auto initialSave = policyPort.save(
        ClassMngr::Next::Application::
            ClassDayFilterResetPolicy::OnApplicationClose
        );
    QVERIFY(initialSave);

    DatabaseSession* const session = window.services()->databaseSession();
    QVERIFY(session);
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);
    const QString unrelatedKey =
        QStringLiteral("classes_navigation_unrelated_preference");
    QVERIFY(repository->saveSetting(
        unrelatedKey,
        QStringLiteral("preserved")
        ));

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    pages->showPage(PageType::Classes);
    ClassesPage* const classesPage = pages->classesPage();
    QVERIFY(classesPage);
    const int initialClassQueryCount =
        classesPage->runtimeMetrics().classQueryCount;
    QVERIFY(initialClassQueryCount > 0);

    QVERIFY(executeSql(
        session,
        QStringLiteral(R"(
            CREATE TABLE f666_day_filter_reset_policy_save_audit (
                value TEXT NOT NULL
            )
            )")
        ));
    QVERIFY(executeSql(
        session,
        QStringLiteral(R"(
            CREATE TRIGGER fail_day_filter_reset_policy_preference_write
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'classes_navigation_day_filter_reset_policy'
                AND NEW.value = 'on_page_leave'
            BEGIN
                SELECT RAISE(ABORT, 'forced day-filter reset policy write failure');
            END
            )")
        ));
    QVERIFY(executeSql(
        session,
        QStringLiteral(R"(
            CREATE TRIGGER audit_day_filter_reset_policy_preference_update
            AFTER UPDATE ON app_settings
            WHEN NEW.key = 'classes_navigation_day_filter_reset_policy'
            BEGIN
                INSERT INTO f666_day_filter_reset_policy_save_audit(value)
                VALUES (NEW.value);
            END
            )")
        ));

    QAction* const preferencesAction = window.findChild<QAction*>(
        QStringLiteral("preferencesAction")
        );
    QVERIFY(preferencesAction);

    bool preferencesDialogObserved = false;
    bool failedSelectionRestored = false;
    bool classSelectionUntouched = false;
    bool warningObserved = false;
    int warningCount = 0;
    QString warningTitle;
    QString warningText;
    QString scriptFailure;
    QString policyAfterFailure;
    QString policyAfterRetry;
    QString unrelatedAfterFailure;
    int classQueriesAfterFailure = -1;
    int classQueriesAfterRetry = -1;
    int auditsAfterFailure = -1;
    int auditsAfterRetry = -1;

    QTimer::singleShot(
        0,
        &window,
        [&]()
        {
            QDialog* const dialog = window.findChild<QDialog*>(
                QStringLiteral("preferencesDialog")
                );
            const auto fail = [&](const QString& reason)
            {
                scriptFailure = reason;
                if (dialog)
                {
                    dialog->reject();
                }
            };
            if (!dialog)
            {
                fail(QStringLiteral("Preferences dialog was not opened."));
                return;
            }

            auto* const tabs = dialog->findChild<QTabWidget*>(
                QStringLiteral("preferencesTabs")
                );
            auto* const dayFilterOnClose = dialog->findChild<QRadioButton*>(
                QStringLiteral("preferencesNavigationDayFiltersResetOnClose")
                );
            auto* const dayFilterOnLeave = dialog->findChild<QRadioButton*>(
                QStringLiteral("preferencesNavigationDayFiltersResetOnLeave")
                );
            auto* const classSelectionOnClose =
                dialog->findChild<QRadioButton*>(
                    QStringLiteral(
                        "preferencesNavigationClassSelectionResetOnClose"
                        )
                    );
            auto* const classSelectionOnLeave =
                dialog->findChild<QRadioButton*>(
                    QStringLiteral(
                        "preferencesNavigationClassSelectionResetOnLeave"
                        )
                    );
            QWidget* const navigationTab = dialog->findChild<QWidget*>(
                QStringLiteral("preferencesNavigationTab")
                );
            if (!tabs || !dayFilterOnClose || !dayFilterOnLeave
                || !classSelectionOnClose || !classSelectionOnLeave
                || !navigationTab || tabs->indexOf(navigationTab) < 0)
            {
                fail(QStringLiteral(
                    "Day-filter reset policy controls were missing."
                    ));
                return;
            }

            preferencesDialogObserved = true;
            tabs->setCurrentWidget(navigationTab);
            if (!dayFilterOnClose->isChecked()
                || dayFilterOnLeave->isChecked()
                || !classSelectionOnClose->isChecked()
                || classSelectionOnLeave->isChecked())
            {
                fail(QStringLiteral(
                    "Initial reset policy radios did not match persisted defaults."
                    ));
                return;
            }

            QTimer::singleShot(
                0,
                &window,
                [&]()
                {
                    QWidget* const activeModal =
                        QApplication::activeModalWidget();
                    auto* const warning = qobject_cast<QMessageBox*>(
                        activeModal
                        );
                    if (!warning)
                    {
                        if (activeModal)
                        {
                            activeModal->close();
                        }
                        scriptFailure = QStringLiteral(
                            "Day-filter policy failure did not surface a warning dialog."
                            );
                        return;
                    }

                    ++warningCount;
                    warningObserved = true;
                    warningTitle = warning->windowTitle();
                    warningText = warning->text();
                    warning->accept();
                }
                );

            dayFilterOnLeave->click();
            if (!scriptFailure.isEmpty())
            {
                dialog->reject();
                return;
            }

            failedSelectionRestored =
                dayFilterOnClose->isChecked()
                && !dayFilterOnLeave->isChecked();
            classSelectionUntouched =
                classSelectionOnClose->isChecked()
                && !classSelectionOnLeave->isChecked();
            classQueriesAfterFailure =
                classesPage->runtimeMetrics().classQueryCount;
            const auto persistedAfterFailure =
                repository->loadSetting(QStringLiteral(
                    "classes_navigation_day_filter_reset_policy"
                    ));
            policyAfterFailure = persistedAfterFailure
                && persistedAfterFailure->isValid()
                ? persistedAfterFailure->toString()
                : QString();
            const auto unrelated = repository->loadSetting(unrelatedKey);
            unrelatedAfterFailure = unrelated && unrelated->isValid()
                ? unrelated->toString()
                : QString();
            auditsAfterFailure = dayFilterResetPolicyAuditRowCount(session);
            if (!warningObserved
                || warningCount != 1
                || warningTitle != QStringLiteral("Unable to Save Preference")
                || !warningText.contains(QStringLiteral(
                    "forced day-filter reset policy write failure"
                    ))
                || !failedSelectionRestored
                || !classSelectionUntouched
                || classQueriesAfterFailure != initialClassQueryCount
                || policyAfterFailure != QStringLiteral("on_application_close")
                || unrelatedAfterFailure != QStringLiteral("preserved")
                || auditsAfterFailure != 0)
            {
                fail(QStringLiteral(
                    "Failed day-filter policy write did not restore and preserve state."
                    ));
                return;
            }

            if (!executeSql(
                    session,
                    QStringLiteral(
                        "DROP TRIGGER fail_day_filter_reset_policy_preference_write"
                        )
                    ))
            {
                fail(QStringLiteral(
                    "Could not remove the day-filter policy failure trigger."
                    ));
                return;
            }

            dayFilterOnLeave->click();
            classQueriesAfterRetry =
                classesPage->runtimeMetrics().classQueryCount;
            const auto persistedAfterRetry =
                repository->loadSetting(QStringLiteral(
                    "classes_navigation_day_filter_reset_policy"
                    ));
            policyAfterRetry = persistedAfterRetry
                && persistedAfterRetry->isValid()
                ? persistedAfterRetry->toString()
                : QString();
            auditsAfterRetry = dayFilterResetPolicyAuditRowCount(session);
            if (dayFilterOnClose->isChecked()
                || !dayFilterOnLeave->isChecked()
                || !classSelectionOnClose->isChecked()
                || classSelectionOnLeave->isChecked()
                || classQueriesAfterRetry != initialClassQueryCount
                || policyAfterRetry != QStringLiteral("on_page_leave")
                || auditsAfterRetry != 1)
            {
                fail(QStringLiteral(
                    "Day-filter policy retry did not persist exactly once."
                    ));
                return;
            }

            dialog->accept();
        }
        );

    preferencesAction->trigger();

    QVERIFY2(scriptFailure.isEmpty(), qPrintable(scriptFailure));
    QVERIFY(preferencesDialogObserved);
    QVERIFY(warningObserved);
    QCOMPARE(warningCount, 1);
    QCOMPARE(warningTitle, QStringLiteral("Unable to Save Preference"));
    QVERIFY(warningText.contains(QStringLiteral(
        "forced day-filter reset policy write failure"
        )));
    QVERIFY(failedSelectionRestored);
    QVERIFY(classSelectionUntouched);
    QCOMPARE(classQueriesAfterFailure, initialClassQueryCount);
    QCOMPARE(policyAfterFailure, QStringLiteral("on_application_close"));
    QCOMPARE(unrelatedAfterFailure, QStringLiteral("preserved"));
    QCOMPARE(auditsAfterFailure, 0);
    QCOMPARE(classQueriesAfterRetry, initialClassQueryCount);
    QCOMPARE(policyAfterRetry, QStringLiteral("on_page_leave"));
    QCOMPARE(auditsAfterRetry, 1);
}

void MainWindowClassVisibilityPreferencesFailureTests::
classSelectionResetPolicyFailureRestoresSelectionAndRetryPersistsOnce()
{
    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );

    const QString databasePath = m_databaseDirectory->filePath(
        QStringLiteral("class-selection-reset-policy-preferences.tps")
        );
    const Status opened = window.services()->openDatabase(databasePath);
    QVERIFY2(opened.has_value(), "Could not open the test database.");
    window.applyDatabaseLoadedState();

    ClassMngr::Next::Platform::
        ApplicationServicesClassSelectionResetPolicyPort policyPort(
            *window.services()
            );
    const auto initialSave = policyPort.save(
        ClassMngr::Next::Application::
            ClassSelectionResetPolicy::OnApplicationClose
        );
    QVERIFY(initialSave);

    DatabaseSession* const session = window.services()->databaseSession();
    QVERIFY(session);
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);
    const QString unrelatedKey =
        QStringLiteral("classes_navigation_unrelated_preference");
    QVERIFY(repository->saveSetting(
        unrelatedKey,
        QStringLiteral("preserved")
        ));

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    pages->showPage(PageType::Classes);
    ClassesPage* const classesPage = pages->classesPage();
    QVERIFY(classesPage);
    const int initialClassQueryCount =
        classesPage->runtimeMetrics().classQueryCount;
    QVERIFY(initialClassQueryCount > 0);

    QVERIFY(executeSql(
        session,
        QStringLiteral(R"(
            CREATE TABLE f667_class_selection_reset_policy_save_audit (
                value TEXT NOT NULL
            )
            )")
        ));
    QVERIFY(executeSql(
        session,
        QStringLiteral(R"(
            CREATE TRIGGER fail_class_selection_reset_policy_preference_write
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'classes_navigation_class_selection_reset_policy'
                AND NEW.value = 'on_page_leave'
            BEGIN
                SELECT RAISE(ABORT, 'forced class-selection reset policy write failure');
            END
            )")
        ));
    QVERIFY(executeSql(
        session,
        QStringLiteral(R"(
            CREATE TRIGGER audit_class_selection_reset_policy_preference_update
            AFTER UPDATE ON app_settings
            WHEN NEW.key = 'classes_navigation_class_selection_reset_policy'
            BEGIN
                INSERT INTO f667_class_selection_reset_policy_save_audit(value)
                VALUES (NEW.value);
            END
            )")
        ));

    QAction* const preferencesAction = window.findChild<QAction*>(
        QStringLiteral("preferencesAction")
        );
    QVERIFY(preferencesAction);

    bool preferencesDialogObserved = false;
    bool failedSelectionRestored = false;
    bool dayFiltersUntouched = false;
    bool warningObserved = false;
    int warningCount = 0;
    QString warningTitle;
    QString warningText;
    QString scriptFailure;
    QString policyAfterFailure;
    QString policyAfterRetry;
    QString unrelatedAfterFailure;
    int classQueriesAfterFailure = -1;
    int classQueriesAfterRetry = -1;
    int auditsAfterFailure = -1;
    int auditsAfterRetry = -1;

    QTimer::singleShot(
        0,
        &window,
        [&]()
        {
            QDialog* const dialog = window.findChild<QDialog*>(
                QStringLiteral("preferencesDialog")
                );
            const auto fail = [&](const QString& reason)
            {
                scriptFailure = reason;
                if (dialog)
                {
                    dialog->reject();
                }
            };
            if (!dialog)
            {
                fail(QStringLiteral("Preferences dialog was not opened."));
                return;
            }

            auto* const tabs = dialog->findChild<QTabWidget*>(
                QStringLiteral("preferencesTabs")
                );
            auto* const dayFilterOnClose = dialog->findChild<QRadioButton*>(
                QStringLiteral("preferencesNavigationDayFiltersResetOnClose")
                );
            auto* const dayFilterOnLeave = dialog->findChild<QRadioButton*>(
                QStringLiteral("preferencesNavigationDayFiltersResetOnLeave")
                );
            auto* const classSelectionOnClose =
                dialog->findChild<QRadioButton*>(
                    QStringLiteral(
                        "preferencesNavigationClassSelectionResetOnClose"
                        )
                    );
            auto* const classSelectionOnLeave =
                dialog->findChild<QRadioButton*>(
                    QStringLiteral(
                        "preferencesNavigationClassSelectionResetOnLeave"
                        )
                    );
            QWidget* const navigationTab = dialog->findChild<QWidget*>(
                QStringLiteral("preferencesNavigationTab")
                );
            if (!tabs || !dayFilterOnClose || !dayFilterOnLeave
                || !classSelectionOnClose || !classSelectionOnLeave
                || !navigationTab || tabs->indexOf(navigationTab) < 0)
            {
                fail(QStringLiteral(
                    "Class-selection reset policy controls were missing."
                    ));
                return;
            }

            preferencesDialogObserved = true;
            tabs->setCurrentWidget(navigationTab);
            if (!dayFilterOnClose->isChecked()
                || dayFilterOnLeave->isChecked()
                || !classSelectionOnClose->isChecked()
                || classSelectionOnLeave->isChecked())
            {
                fail(QStringLiteral(
                    "Initial reset policy radios did not match persisted defaults."
                    ));
                return;
            }

            QTimer::singleShot(
                0,
                &window,
                [&]()
                {
                    QWidget* const activeModal =
                        QApplication::activeModalWidget();
                    auto* const warning = qobject_cast<QMessageBox*>(
                        activeModal
                        );
                    if (!warning)
                    {
                        if (activeModal)
                        {
                            activeModal->close();
                        }
                        scriptFailure = QStringLiteral(
                            "Class-selection policy failure did not surface a warning dialog."
                            );
                        return;
                    }

                    ++warningCount;
                    warningObserved = true;
                    warningTitle = warning->windowTitle();
                    warningText = warning->text();
                    warning->accept();
                }
                );

            classSelectionOnLeave->click();
            if (!scriptFailure.isEmpty())
            {
                dialog->reject();
                return;
            }

            failedSelectionRestored =
                classSelectionOnClose->isChecked()
                && !classSelectionOnLeave->isChecked();
            dayFiltersUntouched =
                dayFilterOnClose->isChecked()
                && !dayFilterOnLeave->isChecked();
            classQueriesAfterFailure =
                classesPage->runtimeMetrics().classQueryCount;
            const auto persistedAfterFailure =
                repository->loadSetting(QStringLiteral(
                    "classes_navigation_class_selection_reset_policy"
                    ));
            policyAfterFailure = persistedAfterFailure
                && persistedAfterFailure->isValid()
                ? persistedAfterFailure->toString()
                : QString();
            const auto unrelated = repository->loadSetting(unrelatedKey);
            unrelatedAfterFailure = unrelated && unrelated->isValid()
                ? unrelated->toString()
                : QString();
            auditsAfterFailure = classSelectionResetPolicyAuditRowCount(session);
            if (!warningObserved
                || warningCount != 1
                || warningTitle != QStringLiteral("Unable to Save Preference")
                || !warningText.contains(QStringLiteral(
                    "forced class-selection reset policy write failure"
                    ))
                || !failedSelectionRestored
                || !dayFiltersUntouched
                || classQueriesAfterFailure != initialClassQueryCount
                || policyAfterFailure != QStringLiteral("on_application_close")
                || unrelatedAfterFailure != QStringLiteral("preserved")
                || auditsAfterFailure != 0)
            {
                fail(QStringLiteral(
                    "Failed class-selection policy write did not restore and preserve state."
                    ));
                return;
            }

            if (!executeSql(
                    session,
                    QStringLiteral(
                        "DROP TRIGGER fail_class_selection_reset_policy_preference_write"
                        )
                    ))
            {
                fail(QStringLiteral(
                    "Could not remove the class-selection policy failure trigger."
                    ));
                return;
            }

            classSelectionOnLeave->click();
            classQueriesAfterRetry =
                classesPage->runtimeMetrics().classQueryCount;
            const auto persistedAfterRetry =
                repository->loadSetting(QStringLiteral(
                    "classes_navigation_class_selection_reset_policy"
                    ));
            policyAfterRetry = persistedAfterRetry
                && persistedAfterRetry->isValid()
                ? persistedAfterRetry->toString()
                : QString();
            auditsAfterRetry = classSelectionResetPolicyAuditRowCount(session);
            if (classSelectionOnClose->isChecked()
                || !classSelectionOnLeave->isChecked()
                || !dayFilterOnClose->isChecked()
                || dayFilterOnLeave->isChecked()
                || classQueriesAfterRetry != initialClassQueryCount
                || policyAfterRetry != QStringLiteral("on_page_leave")
                || auditsAfterRetry != 1)
            {
                fail(QStringLiteral(
                    "Class-selection policy retry did not persist exactly once."
                    ));
                return;
            }

            dialog->accept();
        }
        );

    preferencesAction->trigger();

    QVERIFY2(scriptFailure.isEmpty(), qPrintable(scriptFailure));
    QVERIFY(preferencesDialogObserved);
    QVERIFY(warningObserved);
    QCOMPARE(warningCount, 1);
    QCOMPARE(warningTitle, QStringLiteral("Unable to Save Preference"));
    QVERIFY(warningText.contains(QStringLiteral(
        "forced class-selection reset policy write failure"
        )));
    QVERIFY(failedSelectionRestored);
    QVERIFY(dayFiltersUntouched);
    QCOMPARE(classQueriesAfterFailure, initialClassQueryCount);
    QCOMPARE(policyAfterFailure, QStringLiteral("on_application_close"));
    QCOMPARE(unrelatedAfterFailure, QStringLiteral("preserved"));
    QCOMPARE(auditsAfterFailure, 0);
    QCOMPARE(classQueriesAfterRetry, initialClassQueryCount);
    QCOMPARE(policyAfterRetry, QStringLiteral("on_page_leave"));
    QCOMPARE(auditsAfterRetry, 1);
}

void MainWindowClassVisibilityPreferencesFailureTests::
excelImportTimeoutFailureRestoresSelectionAndAllowsRetry()
{
    using ClassMngr::Next::Platform::
        SettingsManagerExcelImportTimeoutPort;

    SettingsManager& settings = SettingsManager::instance();
    const QString key = QString::fromUtf8(
        SettingsManager::Keys::EXCEL_IMPORT_TIMEOUT_SECONDS
        );
    settings.setExcelImportTimeoutSeconds(60);
    settings.sync();

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    const auto makeFaultablePort =
        [&injectAccessError, &synchronizationAttempts]()
        {
            return std::make_shared<SettingsManagerExcelImportTimeoutPort>(
                [&injectAccessError, &synchronizationAttempts](
                    QSettings& settings
                    )
                {
                    ++synchronizationAttempts;
                    if (injectAccessError)
                    {
                        return QSettings::AccessError;
                    }

                    settings.sync();
                    return settings.status();
                }
                );
        };

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.excelImportTimeoutPreferencesPort = makeFaultablePort();

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );

    QAction* const preferencesAction = window.findChild<QAction*>(
        QStringLiteral("preferencesAction")
        );
    QVERIFY(preferencesAction);

    bool preferencesDialogObserved = false;
    bool failedSelectionRestored = false;
    bool retrySelectionUpdated = false;
    bool warningObserved = false;
    int warningCount = 0;
    QString warningTitle;
    QString warningText;
    QString scriptFailure;
    int selectionAfterFailure = -1;
    int selectionAfterRetry = -1;
    int storedAfterFailure = -1;
    int storedAfterRetry = -1;
    int effectiveAfterFailure = -1;
    int effectiveAfterRetry = -1;
    int attemptsAfterFailure = -1;
    int attemptsAfterRetry = -1;

    QTimer::singleShot(
        0,
        &window,
        [&]()
        {
            QDialog* const dialog = window.findChild<QDialog*>(
                QStringLiteral("preferencesDialog")
                );
            const auto fail = [&](const QString& reason)
            {
                scriptFailure = reason;
                if (dialog)
                {
                    dialog->reject();
                }
            };
            if (!dialog)
            {
                fail(QStringLiteral("Preferences dialog was not opened."));
                return;
            }

            auto* const timeout = dialog->findChild<QComboBox*>(
                QStringLiteral("preferencesExcelImportTimeout")
                );
            if (!timeout)
            {
                fail(QStringLiteral("Excel import timeout control was missing."));
                return;
            }

            const int priorIndex = timeout->findData(60);
            const int candidateIndex = timeout->findData(300);
            if (priorIndex < 0 || candidateIndex < 0
                || timeout->currentIndex() != priorIndex)
            {
                fail(QStringLiteral(
                    "Initial timeout selection did not match storage."
                    ));
                return;
            }
            preferencesDialogObserved = true;

            QTimer::singleShot(
                0,
                &window,
                [&]()
                {
                    QWidget* const activeModal =
                        QApplication::activeModalWidget();
                    auto* const warning = qobject_cast<QMessageBox*>(
                        activeModal
                        );
                    if (!warning)
                    {
                        if (activeModal)
                        {
                            activeModal->close();
                        }
                        scriptFailure = QStringLiteral(
                            "Timeout preference write failure did not surface a warning dialog."
                            );
                        return;
                    }

                    ++warningCount;
                    warningObserved = true;
                    warningTitle = warning->windowTitle();
                    warningText = warning->text();
                    warning->accept();
                }
                );

            timeout->setCurrentIndex(candidateIndex);
            if (!scriptFailure.isEmpty())
            {
                dialog->reject();
                return;
            }

            selectionAfterFailure = timeout->currentData().toInt();
            storedAfterFailure = settings.get(key).toInt();
            effectiveAfterFailure = settings.excelImportTimeoutSeconds();
            attemptsAfterFailure = synchronizationAttempts;
            failedSelectionRestored =
                timeout->currentIndex() == priorIndex;
            if (!warningObserved
                || warningCount != 1
                || warningTitle != QStringLiteral("Unable to Save Preference")
                || !warningText.contains(QStringLiteral("AccessError"))
                || !failedSelectionRestored
                || selectionAfterFailure != 60
                || storedAfterFailure != 60
                || effectiveAfterFailure != 60
                || attemptsAfterFailure != 1)
            {
                fail(QStringLiteral(
                    "Failed timeout write did not preserve persisted and displayed state."
                    ));
                return;
            }

            injectAccessError = false;
            timeout->setCurrentIndex(candidateIndex);
            selectionAfterRetry = timeout->currentData().toInt();
            storedAfterRetry = settings.get(key).toInt();
            effectiveAfterRetry = settings.excelImportTimeoutSeconds();
            attemptsAfterRetry = synchronizationAttempts;
            retrySelectionUpdated =
                timeout->currentIndex() == candidateIndex;
            if (!retrySelectionUpdated
                || selectionAfterRetry != 300
                || storedAfterRetry != 300
                || effectiveAfterRetry != 300
                || attemptsAfterRetry != 2
                || warningCount != 1)
            {
                fail(QStringLiteral(
                    "Successful timeout retry did not persist and display the selected value."
                    ));
                return;
            }

            dialog->accept();
        }
        );

    preferencesAction->trigger();

    QVERIFY2(scriptFailure.isEmpty(), qPrintable(scriptFailure));
    QVERIFY(preferencesDialogObserved);
    QVERIFY(warningObserved);
    QCOMPARE(warningCount, 1);
    QCOMPARE(warningTitle, QStringLiteral("Unable to Save Preference"));
    QVERIFY(warningText.contains(QStringLiteral("AccessError")));
    QVERIFY(failedSelectionRestored);
    QCOMPARE(selectionAfterFailure, 60);
    QCOMPARE(storedAfterFailure, 60);
    QCOMPARE(effectiveAfterFailure, 60);
    QCOMPARE(attemptsAfterFailure, 1);
    QVERIFY(retrySelectionUpdated);
    QCOMPARE(selectionAfterRetry, 300);
    QCOMPARE(storedAfterRetry, 300);
    QCOMPARE(effectiveAfterRetry, 300);
    QCOMPARE(attemptsAfterRetry, 2);
}

QTEST_MAIN(MainWindowClassVisibilityPreferencesFailureTests)

#include "mainwindow_class_visibility_preferences_failure_tests.moc"
