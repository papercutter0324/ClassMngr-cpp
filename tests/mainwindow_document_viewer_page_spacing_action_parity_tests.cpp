#include "app/mainwindow.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "next/domain/operation_result.h"
#include "next/platform/settings_manager_document_page_spacing_preferences_port.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/pages/pdf_viewer_page.h"
#include "ui/shared/state/option_state_keys.h"

#include <QAction>
#include <QPdfView>
#include <QSettings>
#include <QTemporaryDir>
#include <QVariant>
#include <QtTest>

#include <memory>
#include <string>
#include <utility>

namespace
{
class DocumentPageSpacingRestorer final
{
public:
    DocumentPageSpacingRestorer(
        MainWindow& window,
        bool& injectAccessError
        )
        : m_window(window)
        , m_injectAccessError(injectAccessError)
    {
    }

    ~DocumentPageSpacingRestorer()
    {
        m_injectAccessError = false;
        auto* const state = m_window.actions().documentPageSpacingState;
        if (state)
        {
            QAction* const smallAction = state->action(
                ::DocumentPageSpacing::Small
                );
            if (smallAction && state->current() != ::DocumentPageSpacing::Small)
            {
                smallAction->trigger();
            }
        }
        SettingsManager::instance().sync();
    }

    DocumentPageSpacingRestorer(
        const DocumentPageSpacingRestorer&
        ) = delete;
    DocumentPageSpacingRestorer& operator=(
        const DocumentPageSpacingRestorer&
        ) = delete;

private:
    MainWindow& m_window;
    bool& m_injectAccessError;
};
}

class MainWindowDocumentViewerPageSpacingActionParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void allSizesUpdateViewerAndPersist();

private:
    QTemporaryDir m_settingsDirectory;
};

void MainWindowDocumentViewerPageSpacingActionParityTests::initTestCase()
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

void MainWindowDocumentViewerPageSpacingActionParityTests::
allSizesUpdateViewerAndPersist()
{
    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    SettingsManager& settings = SettingsManager::instance();
    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    bool injectAccessError = false;
    int synchronizationAttempts = 0;
    const auto pageSpacingPreferences =
        std::make_shared<
            ClassMngr::Next::Platform::
                SettingsManagerDocumentPageSpacingPreferencesPort
            >(
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
    int diagnosticCount = 0;
    ClassMngr::Next::Domain::OperationError lastDiagnostic;
    startupOptions.documentPageSpacingPreferencesPort =
        pageSpacingPreferences;
    startupOptions.documentPageSpacingPreferenceErrorHandler =
        [&diagnosticCount, &lastDiagnostic](
            const ClassMngr::Next::Domain::OperationError& error
            )
        {
            ++diagnosticCount;
            lastDiagnostic = error;
        };

    MainWindow window(
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );
    DocumentPageSpacingRestorer spacingRestorer(
        window,
        injectAccessError
        );
    QCOMPARE(diagnosticCount, 0);

    PageManager* const pageManager = window.pageManager();
    QVERIFY(pageManager);
    PdfViewerPage* const viewerPage = pageManager->ensurePdfViewerPage();
    QVERIFY(viewerPage);

    QPdfView* const view = viewerPage->findChild<QPdfView*>(
        QStringLiteral("pdfViewerView")
        );
    QVERIFY(view);

    auto* const state = window.actions().documentPageSpacingState;
    QVERIFY(state);
    QCOMPARE(state->current(), ::DocumentPageSpacing::Small);

    QAction* const noneAction = state->action(
        ::DocumentPageSpacing::None
        );
    QAction* const smallAction = state->action(
        ::DocumentPageSpacing::Small
        );
    QAction* const mediumAction = state->action(
        ::DocumentPageSpacing::Medium
        );
    QAction* const largeAction = state->action(
        ::DocumentPageSpacing::Large
        );
    QVERIFY(noneAction);
    QVERIFY(smallAction);
    QVERIFY(mediumAction);
    QVERIFY(largeAction);
    QVERIFY(!noneAction->isChecked());
    QVERIFY(smallAction->isChecked());
    QVERIFY(!mediumAction->isChecked());
    QVERIFY(!largeAction->isChecked());

    const QString spacingKey = QString::fromUtf8(
        OptionKeys::DocumentPageSpacing
        );
    const int startupSynchronizationAttempts = synchronizationAttempts;

    QVERIFY(largeAction->isEnabled());
    largeAction->trigger();

    QCOMPARE(state->current(), ::DocumentPageSpacing::Large);
    QVERIFY(!noneAction->isChecked());
    QVERIFY(!smallAction->isChecked());
    QVERIFY(!mediumAction->isChecked());
    QVERIFY(largeAction->isChecked());
    SettingsManager::instance().sync();
    QCOMPARE(SettingsManager::instance().get(spacingKey).toInt(), 3);
    QCOMPARE(view->pageSpacing(), 32);
    QCOMPARE(synchronizationAttempts, startupSynchronizationAttempts + 1);

    QVERIFY(noneAction->isEnabled());
    noneAction->trigger();

    QCOMPARE(state->current(), ::DocumentPageSpacing::None);
    QVERIFY(noneAction->isChecked());
    QVERIFY(!smallAction->isChecked());
    QVERIFY(!mediumAction->isChecked());
    QVERIFY(!largeAction->isChecked());
    SettingsManager::instance().sync();
    QCOMPARE(SettingsManager::instance().get(spacingKey).toInt(), 0);
    QCOMPARE(view->pageSpacing(), 0);
    QCOMPARE(synchronizationAttempts, startupSynchronizationAttempts + 2);

    QVERIFY(mediumAction->isEnabled());
    mediumAction->trigger();

    QCOMPARE(state->current(), ::DocumentPageSpacing::Medium);
    QVERIFY(!noneAction->isChecked());
    QVERIFY(!smallAction->isChecked());
    QVERIFY(mediumAction->isChecked());
    QVERIFY(!largeAction->isChecked());
    SettingsManager::instance().sync();
    QCOMPARE(SettingsManager::instance().get(spacingKey).toInt(), 2);
    QCOMPARE(view->pageSpacing(), 16);
    QCOMPARE(synchronizationAttempts, startupSynchronizationAttempts + 3);

    QVERIFY(smallAction->isEnabled());
    smallAction->trigger();

    QCOMPARE(state->current(), ::DocumentPageSpacing::Small);
    QVERIFY(!noneAction->isChecked());
    QVERIFY(smallAction->isChecked());
    QVERIFY(!mediumAction->isChecked());
    QVERIFY(!largeAction->isChecked());
    SettingsManager::instance().sync();
    QCOMPARE(SettingsManager::instance().get(spacingKey).toInt(), 1);
    QCOMPARE(view->pageSpacing(), 8);
    QCOMPARE(synchronizationAttempts, startupSynchronizationAttempts + 4);
    QCOMPARE(diagnosticCount, 0);

    injectAccessError = true;
    largeAction->trigger();

    QCOMPARE(synchronizationAttempts, startupSynchronizationAttempts + 5);
    QCOMPARE(diagnosticCount, 1);
    QCOMPARE(
        lastDiagnostic.code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );
    QVERIFY(lastDiagnostic.recoverable);
    QVERIFY(
        lastDiagnostic.message.find("AccessError")
        != std::string::npos
        );
    QCOMPARE(state->current(), ::DocumentPageSpacing::Small);
    QVERIFY(smallAction->isChecked());
    QVERIFY(!noneAction->isChecked());
    QVERIFY(!mediumAction->isChecked());
    QVERIFY(!largeAction->isChecked());
    QCOMPARE(settings.get(spacingKey), QVariant(1));
    QCOMPARE(view->pageSpacing(), 8);

    injectAccessError = false;
    largeAction->trigger();
    QCOMPARE(synchronizationAttempts, startupSynchronizationAttempts + 6);
    QCOMPARE(diagnosticCount, 1);
    QCOMPARE(state->current(), ::DocumentPageSpacing::Large);
    QVERIFY(largeAction->isChecked());
    QVERIFY(!noneAction->isChecked());
    QVERIFY(!smallAction->isChecked());
    QVERIFY(!mediumAction->isChecked());
    QCOMPARE(settings.get(spacingKey), QVariant(3));
    QCOMPARE(view->pageSpacing(), 32);

    injectAccessError = true;
    state->set(::DocumentPageSpacing::Medium);
    QCOMPARE(synchronizationAttempts, startupSynchronizationAttempts + 7);
    QCOMPARE(diagnosticCount, 2);
    QCOMPARE(
        lastDiagnostic.code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );
    QVERIFY(lastDiagnostic.recoverable);
    QCOMPARE(state->current(), ::DocumentPageSpacing::Large);
    QVERIFY(largeAction->isChecked());
    QVERIFY(!noneAction->isChecked());
    QVERIFY(!smallAction->isChecked());
    QVERIFY(!mediumAction->isChecked());
    QCOMPARE(settings.get(spacingKey), QVariant(3));
    QCOMPARE(view->pageSpacing(), 32);

    injectAccessError = false;
    state->set(::DocumentPageSpacing::Medium);
    QCOMPARE(synchronizationAttempts, startupSynchronizationAttempts + 8);
    QCOMPARE(diagnosticCount, 2);
    QCOMPARE(state->current(), ::DocumentPageSpacing::Medium);
    QVERIFY(mediumAction->isChecked());
    QVERIFY(!noneAction->isChecked());
    QVERIFY(!smallAction->isChecked());
    QVERIFY(!largeAction->isChecked());
    QCOMPARE(settings.get(spacingKey), QVariant(2));
    QCOMPARE(view->pageSpacing(), 16);
}

QTEST_MAIN(MainWindowDocumentViewerPageSpacingActionParityTests)

#include "mainwindow_document_viewer_page_spacing_action_parity_tests.moc"
