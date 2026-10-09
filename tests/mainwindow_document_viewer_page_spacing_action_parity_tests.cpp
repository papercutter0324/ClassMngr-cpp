#include "app/mainwindow.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/pages/pdf_viewer_page.h"
#include "ui/shared/state/option_state_keys.h"

#include <QAction>
#include <QPdfView>
#include <QTemporaryDir>
#include <QtTest>

#include <utility>

namespace
{
class DocumentPageSpacingRestorer final
{
public:
    explicit DocumentPageSpacingRestorer(MainWindow& window)
        : m_window(window)
    {
    }

    ~DocumentPageSpacingRestorer()
    {
        auto* const state = m_window.actions().documentPageSpacingState;
        if (!state)
        {
            return;
        }

        QAction* const smallAction = state->action(
            ::DocumentPageSpacing::Small
            );
        if (smallAction)
        {
            smallAction->trigger();
        }
    }

    DocumentPageSpacingRestorer(
        const DocumentPageSpacingRestorer&
        ) = delete;
    DocumentPageSpacingRestorer& operator=(
        const DocumentPageSpacingRestorer&
        ) = delete;

private:
    MainWindow& m_window;
};
}

class MainWindowDocumentViewerPageSpacingActionParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void largeAndSmallActionsUpdateViewerAndPersist();

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
largeAndSmallActionsUpdateViewerAndPersist()
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
    DocumentPageSpacingRestorer spacingRestorer(window);

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

    QAction* const smallAction = state->action(
        ::DocumentPageSpacing::Small
        );
    QAction* const largeAction = state->action(
        ::DocumentPageSpacing::Large
        );
    QVERIFY(smallAction);
    QVERIFY(largeAction);
    QVERIFY(smallAction->isChecked());
    QVERIFY(!largeAction->isChecked());

    const QString spacingKey = QString::fromUtf8(
        OptionKeys::DocumentPageSpacing
        );

    QVERIFY(largeAction->isEnabled());
    largeAction->trigger();

    QCOMPARE(state->current(), ::DocumentPageSpacing::Large);
    QVERIFY(largeAction->isChecked());
    QVERIFY(!smallAction->isChecked());
    SettingsManager::instance().sync();
    QCOMPARE(SettingsManager::instance().get(spacingKey).toInt(), 3);
    QCOMPARE(view->pageSpacing(), 32);

    QVERIFY(smallAction->isEnabled());
    smallAction->trigger();

    QCOMPARE(state->current(), ::DocumentPageSpacing::Small);
    QVERIFY(smallAction->isChecked());
    QVERIFY(!largeAction->isChecked());
    SettingsManager::instance().sync();
    QCOMPARE(SettingsManager::instance().get(spacingKey).toInt(), 1);
    QCOMPARE(view->pageSpacing(), 8);
}

QTEST_MAIN(MainWindowDocumentViewerPageSpacingActionParityTests)

#include "mainwindow_document_viewer_page_spacing_action_parity_tests.moc"
