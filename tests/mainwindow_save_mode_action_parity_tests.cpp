#include "app/mainwindow.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "features/classes/ui/testing_classes_page.h"
#include "ui/shared/actions/action_registry.h"
#include "ui/shared/pages/autosave_coordinator.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/state/option_state_keys.h"

#include <QAction>
#include <QTemporaryDir>
#include <QWidget>
#include <QSignalSpy>
#include <QtTest>

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
        [](const QString&) {},
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

QTEST_MAIN(MainWindowSaveModeActionParityTests)

#include "mainwindow_save_mode_action_parity_tests.moc"
