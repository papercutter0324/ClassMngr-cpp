#include "app/mainwindow.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "next/domain/operation_result.h"
#include "next/platform/settings_manager_document_viewer_background_preferences_port.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/pages/pdf_viewer_page.h"
#include "ui/shared/state/option_state_keys.h"

#include <QAbstractScrollArea>
#include <QAction>
#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QTemporaryDir>
#include <QWidget>
#include <QtTest>

#include <memory>
#include <string>
#include <utility>

namespace
{
class DocumentViewerBackgroundRestorer final
{
public:
    DocumentViewerBackgroundRestorer(
        MainWindow& window,
        bool& injectAccessError
        )
        : m_window(window)
        , m_injectAccessError(injectAccessError)
    {
    }

    ~DocumentViewerBackgroundRestorer()
    {
        m_injectAccessError = false;
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
    bool& m_injectAccessError;
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

    SettingsManager& settings = SettingsManager::instance();
    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    bool injectAccessError = false;
    int synchronizationAttempts = 0;
    const auto backgroundPreferences =
        std::make_shared<
            ClassMngr::Next::Platform::
                SettingsManagerDocumentViewerBackgroundPreferencesPort
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
    startupOptions.documentViewerBackgroundPreferencesPort =
        backgroundPreferences;
    startupOptions.documentViewerBackgroundPreferenceErrorHandler =
        [&diagnosticCount, &lastDiagnostic](
            const ClassMngr::Next::Domain::OperationError& error
            )
        {
            ++diagnosticCount;
            lastDiagnostic = error;
        };

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );
    DocumentViewerBackgroundRestorer backgroundRestorer(
        window,
        injectAccessError
        );
    QCOMPARE(synchronizationAttempts, 0);
    QCOMPARE(diagnosticCount, 0);

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
    QVERIFY(!settings.get(backgroundKey).isValid());

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
    QCOMPARE(synchronizationAttempts, 1);

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
    QCOMPARE(synchronizationAttempts, 2);

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
    QCOMPARE(synchronizationAttempts, 3);
    QCOMPARE(diagnosticCount, 0);

    const QPalette defaultPalette = view->palette();
    injectAccessError = true;
    whiteAction->trigger();

    QCOMPARE(synchronizationAttempts, 4);
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
    QCOMPARE(state->current(), ::DocumentViewerBackground::Default);
    QVERIFY(defaultAction->isChecked());
    QVERIFY(!whiteAction->isChecked());
    QVERIFY(!blackAction->isChecked());
    QCOMPARE(settings.get(backgroundKey), QVariant(0));
    QCOMPARE(
        view->property("pdfViewerBackground").toString(),
        QStringLiteral("default")
        );
    QCOMPARE(
        viewport->property("pdfViewerBackground").toString(),
        QStringLiteral("default")
        );
    QCOMPARE(view->palette(), defaultPalette);

    injectAccessError = false;
    whiteAction->trigger();
    QCOMPARE(synchronizationAttempts, 5);
    QCOMPARE(diagnosticCount, 1);
    QCOMPARE(state->current(), ::DocumentViewerBackground::White);
    QVERIFY(whiteAction->isChecked());
    QVERIFY(!defaultAction->isChecked());
    QVERIFY(!blackAction->isChecked());
    QCOMPARE(settings.get(backgroundKey), QVariant(1));
    QCOMPARE(
        view->property("pdfViewerBackground").toString(),
        QStringLiteral("white")
        );
    QCOMPARE(
        viewport->property("pdfViewerBackground").toString(),
        QStringLiteral("white")
        );
    QCOMPARE(view->palette().color(QPalette::Dark), QColor(Qt::white));

    const QPalette whitePalette = view->palette();
    injectAccessError = true;
    state->set(::DocumentViewerBackground::Black);
    QCOMPARE(synchronizationAttempts, 6);
    QCOMPARE(diagnosticCount, 2);
    QCOMPARE(
        lastDiagnostic.code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );
    QVERIFY(lastDiagnostic.recoverable);
    QCOMPARE(state->current(), ::DocumentViewerBackground::White);
    QVERIFY(whiteAction->isChecked());
    QVERIFY(!defaultAction->isChecked());
    QVERIFY(!blackAction->isChecked());
    QCOMPARE(settings.get(backgroundKey), QVariant(1));
    QCOMPARE(
        view->property("pdfViewerBackground").toString(),
        QStringLiteral("white")
        );
    QCOMPARE(
        viewport->property("pdfViewerBackground").toString(),
        QStringLiteral("white")
        );
    QCOMPARE(view->palette(), whitePalette);

    injectAccessError = false;
    state->set(::DocumentViewerBackground::Black);
    QCOMPARE(synchronizationAttempts, 7);
    QCOMPARE(diagnosticCount, 2);
    QCOMPARE(state->current(), ::DocumentViewerBackground::Black);
    QVERIFY(blackAction->isChecked());
    QVERIFY(!defaultAction->isChecked());
    QVERIFY(!whiteAction->isChecked());
    QCOMPARE(settings.get(backgroundKey), QVariant(2));
    QCOMPARE(
        view->property("pdfViewerBackground").toString(),
        QStringLiteral("black")
        );
    QCOMPARE(
        viewport->property("pdfViewerBackground").toString(),
        QStringLiteral("black")
        );
    QCOMPARE(view->palette().color(QPalette::Dark), QColor(Qt::black));
}

QTEST_MAIN(MainWindowDocumentViewerBackgroundActionParityTests)

#include "mainwindow_document_viewer_background_action_parity_tests.moc"
