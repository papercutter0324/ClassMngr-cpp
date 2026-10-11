#include "app/mainwindow.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "features/classes/ui/classes_page.h"
#include "next/application/class_visibility_preferences.h"
#include "next/platform/application_services_class_visibility_preferences_port.h"
#include "ui/shared/pages/pagemanager.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QDialog>
#include <QMessageBox>
#include <QRadioButton>
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

} // namespace

class MainWindowClassVisibilityPreferencesFailureTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void preferenceFailureRestoresSelectionAndSuccessfulRetryRefreshesOnce();

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

QTEST_MAIN(MainWindowClassVisibilityPreferencesFailureTests)

#include "mainwindow_class_visibility_preferences_failure_tests.moc"
