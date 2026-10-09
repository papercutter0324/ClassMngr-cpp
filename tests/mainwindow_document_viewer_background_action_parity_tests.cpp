#include "app/mainwindow.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/pages/pdf_viewer_page.h"
#include "ui/shared/state/option_state_keys.h"

#include <QAbstractScrollArea>
#include <QAction>
#include <QApplication>
#include <QPalette>
#include <QTemporaryDir>
#include <QWidget>
#include <QtTest>

#include <utility>

namespace
{
class DocumentViewerBackgroundRestorer final
{
public:
    explicit DocumentViewerBackgroundRestorer(MainWindow& window)
        : m_window(window)
    {
    }

    ~DocumentViewerBackgroundRestorer()
    {
        auto* const state = m_window.actions().documentViewerBackgroundState;
        if (state)
        {
            QAction* const defaultAction = state->action(
                ::DocumentViewerBackground::Default
                );
            if (
                defaultAction
                && state->current() != ::DocumentViewerBackground::Default
                )
            {
                defaultAction->trigger();
            }
        }

        SettingsManager::instance().sync();
    }

    DocumentViewerBackgroundRestorer(
        const DocumentViewerBackgroundRestorer&
        ) = delete;
    DocumentViewerBackgroundRestorer& operator=(
        const DocumentViewerBackgroundRestorer&
        ) = delete;

private:
    MainWindow& m_window;
};
}

class MainWindowDocumentViewerBackgroundActionParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void whiteAndBlackActionsUpdateViewerAndDefaultRestoresIt();

private:
    QTemporaryDir m_settingsDirectory;
};

void MainWindowDocumentViewerBackgroundActionParityTests::initTestCase()
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

void MainWindowDocumentViewerBackgroundActionParityTests::
whiteAndBlackActionsUpdateViewerAndDefaultRestoresIt()
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
    DocumentViewerBackgroundRestorer backgroundRestorer(window);

    PageManager* const pageManager = window.pageManager();
    QVERIFY(pageManager);
    PdfViewerPage* const viewerPage = pageManager->ensurePdfViewerPage();
    QVERIFY(viewerPage);

    QWidget* const view = viewerPage->findChild<QWidget*>(
        QStringLiteral("pdfViewerView")
        );
    QVERIFY(view);

    auto* const scrollArea = qobject_cast<QAbstractScrollArea*>(view);
    QVERIFY(scrollArea);
    QWidget* const viewport = scrollArea->viewport();
    QVERIFY(viewport);

    auto* const state = window.actions().documentViewerBackgroundState;
    QVERIFY(state);
    QCOMPARE(state->current(), ::DocumentViewerBackground::Default);

    QAction* const defaultAction = state->action(
        ::DocumentViewerBackground::Default
        );
    QAction* const whiteAction = state->action(
        ::DocumentViewerBackground::White
        );
    QAction* const blackAction = state->action(
        ::DocumentViewerBackground::Black
        );
    QVERIFY(defaultAction);
    QVERIFY(whiteAction);
    QVERIFY(blackAction);
    QVERIFY(defaultAction->isChecked());
    QVERIFY(!whiteAction->isChecked());
    QVERIFY(!blackAction->isChecked());

    const QString backgroundKey = QString::fromUtf8(
        OptionKeys::DocumentViewerBackground
        );

    QVERIFY(whiteAction->isEnabled());
    whiteAction->trigger();

    QCOMPARE(state->current(), ::DocumentViewerBackground::White);
    QVERIFY(whiteAction->isChecked());
    QVERIFY(!defaultAction->isChecked());
    QVERIFY(!blackAction->isChecked());
    SettingsManager::instance().sync();
    QCOMPARE(SettingsManager::instance().get(backgroundKey).toInt(), 1);
    QCOMPARE(
        view->property("pdfViewerBackground").toString(),
        QStringLiteral("white")
        );
    QCOMPARE(
        viewport->property("pdfViewerBackground").toString(),
        QStringLiteral("white")
        );
    QCOMPARE(
        view->palette().color(QPalette::Dark),
        QColor(Qt::white)
        );

    QVERIFY(blackAction->isEnabled());
    blackAction->trigger();

    QCOMPARE(state->current(), ::DocumentViewerBackground::Black);
    QVERIFY(blackAction->isChecked());
    QVERIFY(!defaultAction->isChecked());
    QVERIFY(!whiteAction->isChecked());
    SettingsManager::instance().sync();
    QCOMPARE(SettingsManager::instance().get(backgroundKey).toInt(), 2);
    QCOMPARE(
        view->property("pdfViewerBackground").toString(),
        QStringLiteral("black")
        );
    QCOMPARE(
        viewport->property("pdfViewerBackground").toString(),
        QStringLiteral("black")
        );
    QCOMPARE(
        view->palette().color(QPalette::Dark),
        QColor(Qt::black)
        );

    QVERIFY(defaultAction->isEnabled());
    defaultAction->trigger();

    QCOMPARE(state->current(), ::DocumentViewerBackground::Default);
    QVERIFY(defaultAction->isChecked());
    QVERIFY(!whiteAction->isChecked());
    QVERIFY(!blackAction->isChecked());
    SettingsManager::instance().sync();
    QCOMPARE(SettingsManager::instance().get(backgroundKey).toInt(), 0);
    QCOMPARE(
        view->property("pdfViewerBackground").toString(),
        QStringLiteral("default")
        );
    QCOMPARE(
        viewport->property("pdfViewerBackground").toString(),
        QStringLiteral("default")
        );
    QCOMPARE(view->palette(), QApplication::palette(view));
}

QTEST_MAIN(MainWindowDocumentViewerBackgroundActionParityTests)

#include "mainwindow_document_viewer_background_action_parity_tests.moc"
