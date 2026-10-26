#include "app/mainwindow.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "features/classes/ui/testing_classes_page.h"
#include "next/domain/operation_result.h"
#include "next/platform/settings_manager_save_mode_preferences_port.h"
#include "ui/shared/actions/action_registry.h"
#include "ui/shared/pages/autosave_coordinator.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/state/option_state_keys.h"

#include <QAction>
#include <QTemporaryDir>
#include <QTimer>
#include <QWidget>
#include <QSignalSpy>
#include <QtTest>

#include <memory>
#include <string>
#include <utility>

namespace
{
class SaveModeRestorer final
{
public:
    explicit SaveModeRestorer(MainWindow& window)
        : m_window(window)
    {
    }

    ~SaveModeRestorer()
    {
        auto* const state = m_window.actions().saveModeState;
        if (!state)
        {
            return;
        }

        QAction* const automaticAction =
            state->action(::SaveMode::Automatic);
        if (automaticAction)
        {
            automaticAction->trigger();
        }
    }

    SaveModeRestorer(const SaveModeRestorer&) = delete;
    SaveModeRestorer& operator=(const SaveModeRestorer&) = delete;

private:
    MainWindow& m_window;
};
}

class MainWindowSaveModeActionParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void manualAndAutomaticActionsUpdatePageAndPersistPreference();
    void failedActionPreservesEffectiveModeAndRetryAppliesIt();

private:
    QTemporaryDir m_settingsDirectory;
};

void MainWindowSaveModeActionParityTests::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    QVERIFY(
        qputenv(
            "CLASSMNGR_SETTINGS_ROOT",
            m_settingsDirectory.path().toUtf8()
            )
        );

    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void MainWindowSaveModeActionParityTests::
manualAndAutomaticActionsUpdatePageAndPersistPreference()
{
    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;

    MainWindow window(
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );
    SaveModeRestorer saveModeRestorer(window);

    PageManager* const pageManager = window.pageManager();
    QVERIFY(pageManager);
    TestingClassesPage* const page =
        pageManager->ensureTestingClassesPage();
    QVERIFY(page);
    QVERIFY(qobject_cast<TestingClassesPage*>(page));
    QVERIFY(!page->isVisible());
    QVERIFY(!page->hasUnsavedChanges());

    AutosaveCoordinator* const autosave =
        page->findChild<AutosaveCoordinator*>(
            QString(),
            Qt::FindDirectChildrenOnly
            );
    QVERIFY(autosave);
    QVERIFY(!autosave->isDirty());

    auto* const state = window.actions().saveModeState;
    QVERIFY(state);
    QCOMPARE(state->current(), ::SaveMode::Automatic);
    QCOMPARE(autosave->saveMode(), ::SaveMode::Automatic);

    QAction* const automaticAction =
        state->action(::SaveMode::Automatic);
    QAction* const manualAction =
        state->action(::SaveMode::Manual);
    QVERIFY(automaticAction);
    QVERIFY(manualAction);
    QVERIFY(automaticAction->isChecked());
    QVERIFY(!manualAction->isChecked());

    const QString saveModeKey = QString::fromUtf8(OptionKeys::SaveMode);
    SettingsManager::instance().sync();
    QCOMPARE(SettingsManager::instance().get(saveModeKey).toInt(), 0);

    QSignalSpy saveRequestedSpy(
        autosave,
        &AutosaveCoordinator::saveRequested
        );
    QVERIFY(saveRequestedSpy.isValid());

    QVERIFY(manualAction->isEnabled());
    manualAction->trigger();

    QCOMPARE(state->current(), ::SaveMode::Manual);
    QVERIFY(manualAction->isChecked());
    QVERIFY(!automaticAction->isChecked());
    SettingsManager::instance().sync();
    QCOMPARE(SettingsManager::instance().get(saveModeKey).toInt(), 1);
    QCOMPARE(autosave->saveMode(), ::SaveMode::Manual);
    QVERIFY(!page->hasUnsavedChanges());
    QVERIFY(!autosave->isDirty());
    QCOMPARE(saveRequestedSpy.count(), 0);

    QVERIFY(automaticAction->isEnabled());
    automaticAction->trigger();

    QCOMPARE(state->current(), ::SaveMode::Automatic);
    QVERIFY(automaticAction->isChecked());
    QVERIFY(!manualAction->isChecked());
    SettingsManager::instance().sync();
    QCOMPARE(SettingsManager::instance().get(saveModeKey).toInt(), 0);
    QCOMPARE(autosave->saveMode(), ::SaveMode::Automatic);
    QVERIFY(!page->hasUnsavedChanges());
    QVERIFY(!autosave->isDirty());
    QCOMPARE(saveRequestedSpy.count(), 0);
}

void MainWindowSaveModeActionParityTests::
failedActionPreservesEffectiveModeAndRetryAppliesIt()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString saveModeKey = QString::fromUtf8(OptionKeys::SaveMode);
    settings.set(saveModeKey, 0);
    settings.sync();

    bool injectAccessError = false;
    int synchronizationAttempts = 0;
    auto saveModePreferences = std::make_shared<
        ClassMngr::Next::Platform::
            SettingsManagerSaveModePreferencesPort
        >(
            [&injectAccessError, &synchronizationAttempts](QSettings& settings)
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

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.saveModePreferencesPort = saveModePreferences;
    int warningCount = 0;
    ClassMngr::Next::Domain::OperationError warningError;
    startupOptions.saveModePreferenceErrorHandler =
        [&warningCount, &warningError](
            const ClassMngr::Next::Domain::OperationError& error
            )
        {
            ++warningCount;
            warningError = error;
        };

    MainWindow window(
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );
    SaveModeRestorer saveModeRestorer(window);

    PageManager* const pageManager = window.pageManager();
    QVERIFY(pageManager);
    TestingClassesPage* const page =
        pageManager->ensureTestingClassesPage();
    QVERIFY(page);
    QVERIFY(qobject_cast<TestingClassesPage*>(page));

    AutosaveCoordinator* const autosave =
        page->findChild<AutosaveCoordinator*>(
            QString(),
            Qt::FindDirectChildrenOnly
            );
    QVERIFY(autosave);
    QCOMPARE(autosave->saveMode(), ::SaveMode::Automatic);

    auto* const state = window.actions().saveModeState;
    QVERIFY(state);
    QCOMPARE(state->current(), ::SaveMode::Automatic);
    QAction* const automaticAction =
        state->action(::SaveMode::Automatic);
    QAction* const manualAction =
        state->action(::SaveMode::Manual);
    QVERIFY(automaticAction);
    QVERIFY(manualAction);
    QVERIFY(automaticAction->isChecked());
    QVERIFY(!manualAction->isChecked());

    QTimer* const autosaveTimer =
        autosave->findChild<QTimer*>(
            QString(),
            Qt::FindDirectChildrenOnly
            );
    QVERIFY(autosaveTimer);
    autosave->setValid(true);
    autosave->setSaveAvailable(true);
    autosave->setLoading(false);
    autosave->setManualSaveRequired(false);
    autosave->setDebounceInterval(60000);
    autosave->setDirty(true);
    QVERIFY(autosaveTimer->isActive());

    QSignalSpy saveRequestedSpy(
        autosave,
        &AutosaveCoordinator::saveRequested
        );
    QVERIFY(saveRequestedSpy.isValid());

    synchronizationAttempts = 0;
    injectAccessError = true;
    manualAction->trigger();
    injectAccessError = false;

    QCOMPARE(warningCount, 1);
    QCOMPARE(
        warningError.code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );
    QVERIFY(warningError.recoverable);
    QVERIFY(
        warningError.message.find("AccessError") != std::string::npos
        );
    QCOMPARE(synchronizationAttempts, 1);

    QCOMPARE(state->current(), ::SaveMode::Automatic);
    QVERIFY(automaticAction->isChecked());
    QVERIFY(!manualAction->isChecked());
    QCOMPARE(settings.get(saveModeKey).toInt(), 0);
    QSettings persistedSettingsAfterFailure(
        m_settingsDirectory.filePath(
            QStringLiteral("PaperCloud/ClassMngr.ini")
            ),
        QSettings::IniFormat
        );
    persistedSettingsAfterFailure.sync();
    QCOMPARE(persistedSettingsAfterFailure.status(), QSettings::NoError);
    QCOMPARE(
        persistedSettingsAfterFailure.value(saveModeKey).toInt(),
        0
        );
    QCOMPARE(autosave->saveMode(), ::SaveMode::Automatic);
    QVERIFY(autosaveTimer->isActive());
    QCOMPARE(saveRequestedSpy.count(), 0);

    manualAction->trigger();

    QCOMPARE(synchronizationAttempts, 2);
    QCOMPARE(state->current(), ::SaveMode::Manual);
    QVERIFY(manualAction->isChecked());
    QVERIFY(!automaticAction->isChecked());
    QCOMPARE(warningCount, 1);
    settings.sync();
    QCOMPARE(settings.get(saveModeKey).toInt(), 1);
    QSettings persistedSettingsAfterRetry(
        m_settingsDirectory.filePath(
            QStringLiteral("PaperCloud/ClassMngr.ini")
            ),
        QSettings::IniFormat
        );
    persistedSettingsAfterRetry.sync();
    QCOMPARE(persistedSettingsAfterRetry.status(), QSettings::NoError);
    QCOMPARE(persistedSettingsAfterRetry.value(saveModeKey).toInt(), 1);
    QCOMPARE(autosave->saveMode(), ::SaveMode::Manual);
    QVERIFY(!autosaveTimer->isActive());
    QCOMPARE(saveRequestedSpy.count(), 0);

    automaticAction->trigger();
    QCOMPARE(synchronizationAttempts, 3);
    QCOMPARE(warningCount, 1);
    QCOMPARE(state->current(), ::SaveMode::Automatic);
    settings.sync();
    const int persistedSaveMode = settings.get(saveModeKey).toInt();
    QCOMPARE(persistedSaveMode, 0);
    QCOMPARE(autosave->saveMode(), ::SaveMode::Automatic);
    // The synthetic dirty flag is coordinator-only; PageManager recomputes
    // dirty state from the no-database page model after a mode change. That
    // page requires manual save, so Automatic restoration leaves its timer
    // stopped.
    QVERIFY(!autosaveTimer->isActive());
    QCOMPARE(saveRequestedSpy.count(), 0);
}

QTEST_MAIN(MainWindowSaveModeActionParityTests)

#include "mainwindow_save_mode_action_parity_tests.moc"
