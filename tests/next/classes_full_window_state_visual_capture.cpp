#include "app/mainwindow.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "core/theme_service.h"
#include "data/database/database_schema_manager.h"
#include "data/database/workspace_store.h"
#include "features/classes/ui/classes_page.h"
#include "next/bootstrap/application_bootstrap.h"
#include "next/bootstrap/startup_diagnostics_sink.h"
#include "next/bootstrap/startup_update_dispatcher.h"
#include "next/platform/settings_manager_language_preferences_port.h"
#include "next/platform/settings_manager_theme_preferences_port.h"
#include "next/platform/workspace_store_classes_navigator_read_actions.h"
#include "next/resources/resource_root_resolver.h"
#include "next/ui/application_shell.h"
#include "next/ui/classes_navigator_model.h"
#include "next/ui/classes_navigator_page.h"
#include "next/ui/navigation_model.h"
#include "next/ui/page_host.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/widgets/sidebar/sidebar.h"
#include "ui/shared/widgets/navigation_tab_widget.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QRect>
#include <QSqlError>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QWidget>
#include <QtTest>

#include <memory>
#include <optional>
#include <string>
#include <utility>

using namespace ClassMngr::Next;
namespace NextUi = ClassMngr::Next::Ui;

namespace
{

constexpr QSize Viewport{800, 600};
constexpr int SelectedClassId = 25;
constexpr int KeyRole = Qt::UserRole + 4;

class QuietStartupDiagnosticsSink final
    : public Bootstrap::StartupDiagnosticsSink
{
public:
    void record(const Application::StartupEvent&) noexcept override
    {
    }
};

class QuietStartupUpdateDispatcher final
    : public Bootstrap::StartupUpdateDispatcher
{
public:
    void dispatch() override
    {
    }
};

struct Fixture final
{
    QString path;
    QString fileName;
    QString provenance;
    QString source;
    QByteArray sourceSha256;
    QByteArray sha256;
    QString listReadDiagnostic;
    int schemaVersion = 0;
    int classCount = 0;
    bool testingClassesPresent = false;
};

struct ModalImage final
{
    QImage image;
    QString title;
    QString message;
    QString informativeText;
    QString detailedText;
};

QString sha256(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    return QString::fromLatin1(
        QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex()
        );
}

QByteArray readQueryMessage(const QString& databasePath)
{
    const auto result = Platform::readClassesNavigatorListAtPath(databasePath);
    if (result)
    {
        return {};
    }
    return QByteArray::fromStdString(result.error().message);
}

bool readFixtureSchema(
    const QString& path,
    int* schemaVersion,
    int* classCount,
    bool* testingClassesPresent,
    QString* error
    )
{
    WorkspaceStore store;
    if (const auto opened = store.openExisting(path); !opened)
    {
        if (error) *error = QStringLiteral("WorkspaceStore::openExisting failed: %1")
            .arg(opened.error().diagnostic);
        return false;
    }

    QSqlDatabase database = store.database();
    const auto version = DatabaseSchemaManager::schemaVersion(database);
    if (!version)
    {
        if (error) *error = QStringLiteral("Could not read fixture schema version: %1")
            .arg(version.error().diagnostic);
        return false;
    }
    QSqlQuery classCountQuery(store.database());
    if (!classCountQuery.exec(QStringLiteral("SELECT COUNT(*) FROM classes"))
        || !classCountQuery.next())
    {
        if (error) *error = QStringLiteral("Could not count fixture classes: %1")
            .arg(classCountQuery.lastError().text());
        return false;
    }
    QSqlQuery testingTableQuery(store.database());
    testingTableQuery.prepare(QStringLiteral(
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?"));
    testingTableQuery.addBindValue(QStringLiteral("testing_classes"));
    if (!testingTableQuery.exec())
    {
        if (error) *error = QStringLiteral("Could not inspect fixture testing_classes table: %1")
            .arg(testingTableQuery.lastError().text());
        return false;
    }

    *schemaVersion = *version;
    *classCount = classCountQuery.value(0).toInt();
    *testingClassesPresent = testingTableQuery.next();
    return true;
}

bool createFixture(
    const QString& state,
    const QString& pairDirectory,
    Fixture* fixture,
    QString* error
    )
{
    const QString fileName = QStringLiteral("classes-%1.tps").arg(state);
    const QString path = QDir(pairDirectory).filePath(fileName);
    const QString trackedFixture = QStringLiteral(CLASSMNGR_CLASSES_NAVIGATOR_FIXTURE);

    if (state == QStringLiteral("empty"))
    {
        ApplicationServices seedServices;
        if (!seedServices.openDatabase(path))
        {
            if (error) *error = QStringLiteral(
                "Legacy ApplicationServices could not create the empty v6 fixture.");
            return false;
        }
        seedServices.closeDatabase();
        fixture->provenance = QStringLiteral(
            "Created as a new empty v6 .tps by Legacy ApplicationServices::openDatabase; no class rows were inserted.");
    }
    else
    {
        if (!QFile::copy(trackedFixture, path))
        {
            if (error) *error = QStringLiteral("Could not copy tracked fixture: %1")
                .arg(trackedFixture);
            return false;
        }
        fixture->source = QStringLiteral(
            "docs/qt-rewrite/visual-baseline/release/large-classes-visual-states/generated-large-classes-visual.tps");
        fixture->sourceSha256 = sha256(trackedFixture).toLatin1();
        if (fixture->sourceSha256.isEmpty())
        {
            if (error) *error = QStringLiteral("Could not hash the tracked source fixture.");
            return false;
        }
        fixture->provenance = QStringLiteral(
            "Temporary copy of the tracked 96-class visual fixture.");
    }

    if (state == QStringLiteral("list-read-error"))
    {
        WorkspaceStore store;
        if (const auto opened = store.openExisting(path); !opened)
        {
            if (error) *error = QStringLiteral(
                "The populated v6 source fixture did not open before alteration: %1")
                    .arg(opened.error().diagnostic);
            return false;
        }
        QSqlQuery drop(store.database());
        if (!drop.exec(QStringLiteral("DROP TABLE testing_classes")))
        {
            if (error) *error = QStringLiteral(
                "Could not drop testing_classes in the temporary fixture: %1")
                    .arg(drop.lastError().text());
            return false;
        }
        store.close();

        // This re-open is the gating check: v6 is current, so normal open must
        // leave the deliberately removed child table absent.
        int schemaVersion = 0;
        int classCount = 0;
        bool testingClassesPresent = true;
        if (!readFixtureSchema(path, &schemaVersion, &classCount,
                &testingClassesPresent, error))
        {
            return false;
        }
        if (schemaVersion != DatabaseSchemaManager::LatestSchemaVersion
            || classCount != 96 || testingClassesPresent)
        {
            if (error) *error = QStringLiteral(
                "Reopening the altered fixture changed its expected v6 structure (version=%1, classes=%2, testing_classes=%3).")
                    .arg(schemaVersion).arg(classCount)
                    .arg(testingClassesPresent ? QStringLiteral("present")
                                               : QStringLiteral("absent"));
            return false;
        }
        const QByteArray queryError = readQueryMessage(path);
        if (queryError.isEmpty())
        {
            if (error) *error = QStringLiteral(
                "The production Classes list query unexpectedly succeeded on the altered fixture.");
            return false;
        }
        fixture->listReadDiagnostic = QString::fromUtf8(queryError);
        fixture->provenance += QStringLiteral(
            " The temporary copy was altered by dropping testing_classes; WorkspaceStore::openExisting accepted the reopened latest-v6 file without recreating that table, and the production Classes list read returned an error.");
    }

    fixture->path = path;
    fixture->fileName = fileName;
    if (!readFixtureSchema(path, &fixture->schemaVersion, &fixture->classCount,
            &fixture->testingClassesPresent, error))
    {
        return false;
    }
    fixture->sha256 = sha256(path).toLatin1();
    if (fixture->sha256.isEmpty())
    {
        if (error) *error = QStringLiteral("Could not hash the temporary .tps fixture.");
        return false;
    }

    if ((state == QStringLiteral("empty") && fixture->classCount != 0)
        || (state != QStringLiteral("empty") && fixture->classCount != 96))
    {
        if (error) *error = QStringLiteral("Fixture class count did not match state %1: %2")
            .arg(state).arg(fixture->classCount);
        return false;
    }
    if ((state == QStringLiteral("list-read-error") && fixture->testingClassesPresent)
        || (state != QStringLiteral("list-read-error") && !fixture->testingClassesPresent))
    {
        if (error) *error = QStringLiteral(
            "Fixture testing_classes presence did not match state %1.").arg(state);
        return false;
    }
    return true;
}

QPalette themePalette(const QString& theme)
{
    QPalette palette;
    const bool dark = theme == QStringLiteral("dark");
    palette.setColor(QPalette::Window,
        QColor(dark ? QStringLiteral("#202327") : QStringLiteral("#f5f6f7")));
    palette.setColor(QPalette::WindowText,
        QColor(dark ? QStringLiteral("#f5f6f7") : QStringLiteral("#232629")));
    palette.setColor(QPalette::Base,
        QColor(dark ? QStringLiteral("#292c30") : QStringLiteral("#ffffff")));
    palette.setColor(QPalette::Text,
        QColor(dark ? QStringLiteral("#fcfcfc") : QStringLiteral("#232629")));
    palette.setColor(QPalette::Button,
        QColor(dark ? QStringLiteral("#292c30") : QStringLiteral("#fcfcfc")));
    palette.setColor(QPalette::ButtonText,
        QColor(dark ? QStringLiteral("#fcfcfc") : QStringLiteral("#232629")));
    palette.setColor(QPalette::Mid,
        QColor(dark ? QStringLiteral("#4b5056") : QStringLiteral("#b0b5bb")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#3daee9")));
    palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")));
    return palette;
}

bool configurePreferences(const QString& locale, const QString& theme, QString* error)
{
    SettingsManager::instance().clear();
    Platform::SettingsManagerThemePreferencesPort themePreferences;
    Platform::SettingsManagerLanguagePreferencesPort languagePreferences;
    const auto themeResult = themePreferences.write(
        theme == QStringLiteral("dark")
            ? Application::Theme::Dark : Application::Theme::Light);
    const auto languageResult = languagePreferences.write(
        locale == QStringLiteral("ko_KR")
            ? Application::LanguagePreference::Korean
            : Application::LanguagePreference::English);
    if (!themeResult || !languageResult)
    {
        if (error) *error = QStringLiteral("Could not isolate the capture locale/theme preferences.");
        return false;
    }
    SettingsManager::instance().sync();
    return true;
}

QTreeWidgetItem* findTreeItemByKey(QTreeWidget* tree, const QString& key)
{
    if (!tree)
    {
        return nullptr;
    }
    const auto visit = [&key](auto&& self, QTreeWidgetItem* item) -> QTreeWidgetItem* {
        if (!item)
        {
            return nullptr;
        }
        if (item->data(0, KeyRole).toString() == key)
        {
            return item;
        }
        for (int index = 0; index < item->childCount(); ++index)
        {
            if (QTreeWidgetItem* const match = self(self, item->child(index)))
            {
                return match;
            }
        }
        return nullptr;
    };
    for (int index = 0; index < tree->topLevelItemCount(); ++index)
    {
        if (QTreeWidgetItem* const match = visit(visit, tree->topLevelItem(index)))
        {
            return match;
        }
    }
    return nullptr;
}

bool clickTreeRoute(QTreeWidget* tree, const QString& key, QString* error)
{
    QTreeWidgetItem* const item = findTreeItemByKey(tree, key);
    if (!item)
    {
        if (error) *error = QStringLiteral("Legacy sidebar route key was not found: %1")
            .arg(key);
        return false;
    }
    tree->scrollToItem(item);
    const QRect rect = tree->visualItemRect(item);
    if (!rect.isValid() || rect.isEmpty())
    {
        if (error) *error = QStringLiteral("Legacy sidebar route is not visible: %1")
            .arg(key);
        return false;
    }
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());
    QApplication::processEvents();
    return true;
}

bool composeModalImage(
    QWidget* shell,
    QMessageBox* modal,
    ModalImage* capture,
    QString* error
    )
{
    if (!shell || !modal || !capture)
    {
        if (error) *error = QStringLiteral("A full-window capture or modal was unavailable.");
        return false;
    }
    const QPixmap shellPixmap = shell->grab();
    const QPixmap modalPixmap = modal->grab();
    if (shellPixmap.size() != Viewport || modalPixmap.isNull())
    {
        if (error) *error = QStringLiteral(
            "The shell or QMessageBox grab had an unexpected size.");
        return false;
    }

    QImage composite(Viewport, QImage::Format_ARGB32_Premultiplied);
    composite.fill(Qt::transparent);
    QPainter painter(&composite);
    painter.drawPixmap(0, 0, shellPixmap);
    const QPoint shellOrigin = shell->mapToGlobal(QPoint(0, 0));
    const QPoint modalOrigin = modal->mapToGlobal(QPoint(0, 0));
    painter.drawPixmap(modalOrigin - shellOrigin, modalPixmap);
    painter.end();

    capture->image = std::move(composite);
    capture->title = modal->windowTitle();
    capture->message = modal->text();
    capture->informativeText = modal->informativeText();
    capture->detailedText = modal->detailedText();
    return true;
}

class LegacyReadWarningCapturePromptService final : public IUserPromptService
{
public:
    explicit LegacyReadWarningCapturePromptService(ModalImage* capture)
        : m_capture(capture)
    {
    }

    void showMessage(const PromptRequest& request) override
    {
        m_request = request;
        m_hasRequest = true;
    }

    void setShell(QWidget* shell) noexcept
    {
        m_shell = shell;
    }

    [[nodiscard]] bool capturePending()
    {
        if (!m_shell || !m_capture || !m_hasRequest)
        {
            m_error = QStringLiteral("Legacy Classes route did not issue a list-read warning.");
            return false;
        }
        const PromptRequest& request = m_request;
        QMessageBox dialog(request.parent);
        dialog.setObjectName(request.objectName.isEmpty()
                ? QStringLiteral("classmngrUserPrompt") : request.objectName);
        if (!request.automationId.isEmpty())
        {
            dialog.setProperty("classmngrPromptAutomationId", request.automationId);
        }
        dialog.setWindowModality(Qt::WindowModal);
        switch (request.severity)
        {
        case PromptSeverity::Information:
            dialog.setIcon(QMessageBox::Information);
            break;
        case PromptSeverity::Warning:
            dialog.setIcon(QMessageBox::Warning);
            break;
        case PromptSeverity::Error:
            dialog.setIcon(QMessageBox::Critical);
            break;
        }
        dialog.setWindowTitle(request.title);
        static_cast<QWidget&>(dialog).setWindowTitle(request.title);
        dialog.setText(request.message);
        dialog.setTextFormat(Qt::PlainText);
        if (!request.details.isEmpty())
        {
            dialog.setDetailedText(request.details);
        }
        QPushButton* const acknowledge = dialog.addButton(
            request.acceptText.isEmpty()
                ? QCoreApplication::translate("QtUserPromptService", "OK")
                : request.acceptText,
            QMessageBox::AcceptRole);
        acknowledge->setObjectName(QStringLiteral("promptAcceptButton"));
        dialog.setDefaultButton(acknowledge);
        dialog.setEscapeButton(acknowledge);
        dialog.setModal(true);
        dialog.show();
        QApplication::processEvents();
        if (!composeModalImage(m_shell, &dialog, m_capture, &m_error))
        {
            return false;
        }
        m_captured = true;
        dialog.done(QMessageBox::Ok);
        return true;
    }

    void showMessageAsync(const PromptRequest& request) override
    {
        m_fallback.showMessageAsync(request);
    }

    [[nodiscard]] PromptChoice confirm(const PromptRequest& request) override
    {
        return m_fallback.confirm(request);
    }

    [[nodiscard]] UnsavedChangesChoice confirmUnsavedChanges(
        const UnsavedChangesRequest& request) override
    {
        return m_fallback.confirmUnsavedChanges(request);
    }

    [[nodiscard]] QString chooseAction(const ActionPromptRequest& request) override
    {
        return m_fallback.chooseAction(request);
    }

    [[nodiscard]] bool captured() const noexcept { return m_captured; }
    [[nodiscard]] const QString& error() const noexcept { return m_error; }

private:
    QWidget* m_shell = nullptr;
    ModalImage* m_capture = nullptr;
    QtUserPromptService m_fallback;
    PromptRequest m_request;
    bool m_hasRequest = false;
    bool m_captured = false;
    QString m_error;
};

class UserPromptServiceOverride final
{
public:
    explicit UserPromptServiceOverride(IUserPromptService* service)
    {
        DialogServices::setUserPromptServiceForTesting(service);
    }

    ~UserPromptServiceOverride()
    {
        DialogServices::setUserPromptServiceForTesting(nullptr);
    }
};

bool saveImage(
    const QString& path,
    const QImage& image,
    QString* error
    )
{
    if (image.size() != Viewport || !image.save(path, "PNG"))
    {
        if (error) *error = QStringLiteral("Could not save an 800x600 PNG: %1")
            .arg(path);
        return false;
    }
    return true;
}

bool saveWindowImage(QWidget* window, const QString& path, QString* error)
{
    if (!window || window->size() != Viewport)
    {
        if (error) *error = QStringLiteral("Production window is not 800x600.");
        return false;
    }
    return saveImage(path, window->grab().toImage(), error);
}

bool captureLegacyDeleteConfirmation(
    QApplication& application,
    MainWindow& window,
    ModalImage* capture,
    QString* error
    )
{
    bool captured = false;
    QString captureError;
    QTimer::singleShot(0, &application, [&] {
        QWidget* const selector = QApplication::activeModalWidget();
        auto* const records = selector
            ? selector->findChild<QComboBox*>(QStringLiteral("sidebarRecordSelectionCombo"))
            : nullptr;
        auto* const buttons = selector
            ? selector->findChild<QDialogButtonBox*>() : nullptr;
        if (!selector || !records || !buttons)
        {
            captureError = QStringLiteral("Legacy Delete Class selector was not visible.");
            if (selector) selector->close();
            return;
        }
        const int classIndex = records->findData(SelectedClassId);
        QPushButton* const accept = buttons->button(QDialogButtonBox::Ok);
        if (classIndex < 0 || !accept)
        {
            captureError = QStringLiteral("Legacy Delete Class selector did not offer class 25.");
            selector->close();
            return;
        }
        records->setCurrentIndex(classIndex);
        QTimer::singleShot(0, &application, [&] {
            auto* const modal = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (!modal)
            {
                captureError = QStringLiteral("Legacy class deletion did not open a QMessageBox confirmation.");
                if (QWidget* const active = QApplication::activeModalWidget()) active->close();
                return;
            }
            captured = composeModalImage(&window, modal, capture, &captureError);
            modal->done(QMessageBox::Cancel);
        });
        accept->click();
    });
    QAction* const deleteClass = window.actions().deleteClass;
    if (!deleteClass)
    {
        if (error) *error = QStringLiteral("Legacy MainWindow did not expose Delete Class.");
        return false;
    }
    deleteClass->trigger();
    QApplication::processEvents();
    if (!captured)
    {
        if (error) *error = captureError.isEmpty()
            ? QStringLiteral("Legacy delete confirmation capture did not complete.")
            : captureError;
        return false;
    }
    return true;
}

bool setupLegacyWindow(
    QApplication& application,
    const Fixture& fixture,
    const QString& locale,
    const QString& theme,
    std::unique_ptr<LanguageService>* languageService,
    std::unique_ptr<MainWindow>* window,
    ModalImage* classesRouteModal,
    QString* error
    )
{
    if (!configurePreferences(locale, theme, error))
    {
        return false;
    }
    QLocale::setDefault(QLocale(locale));
    const QString translationsRoot = QDir(
        QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT)).absoluteFilePath(
            QStringLiteral("../assets/translations"));
    LanguageService::TranslationOperations translationOperations;
    translationOperations.load = [translationsRoot](
        QTranslator& translator, const QString& resourcePath) {
        const QString catalogName = QFileInfo(resourcePath).fileName()
            + QStringLiteral(".qm");
        return translator.load(QDir(translationsRoot).filePath(catalogName));
    };
    translationOperations.install = [](QTranslator* translator) {
        return qApp && qApp->installTranslator(translator);
    };
    languageService->reset(new LanguageService(
        nullptr, std::move(translationOperations)));
    if (!(*languageService)->setLanguage(
            locale == QStringLiteral("ko_KR") ? Language::Korean : Language::English))
    {
        if (error) *error = QStringLiteral("Legacy could not install its production locale catalog.");
        return false;
    }
    auto startupTheme = std::make_unique<ThemeService>();
    startupTheme->setTheme(theme == QStringLiteral("dark") ? Theme::Dark : Theme::Light);

    MainWindowStartupOptions options;
    options.loadMostRecentDatabase = false;
    options.initialDatabasePath = fixture.path;
    options.startupThemeService = std::move(startupTheme);
    LegacyReadWarningCapturePromptService* routePromptService = nullptr;
    std::unique_ptr<LegacyReadWarningCapturePromptService> routePromptServiceOwner;
    std::unique_ptr<UserPromptServiceOverride> routePromptServiceOverride;
    if (classesRouteModal && !fixture.testingClassesPresent)
    {
        routePromptServiceOwner = std::make_unique<LegacyReadWarningCapturePromptService>(
            classesRouteModal);
        routePromptService = routePromptServiceOwner.get();
        routePromptServiceOverride = std::make_unique<UserPromptServiceOverride>(
            routePromptService);
    }
    window->reset(new MainWindow(
        false, languageService->get(), nullptr, std::move(options)));
    (*window)->resize(Viewport);
    if (routePromptService)
    {
        routePromptService->setShell(window->get());
    }
    (*window)->show();
    QApplication::processEvents();
    if ((*window)->size() != Viewport || !(*window)->services()
        || !(*window)->services()->hasOpenDatabase())
    {
        if (error) *error = QStringLiteral("Legacy MainWindow did not open the fixture at 800x600.");
        return false;
    }
    auto* const tree = (*window)->findChild<QTreeWidget*>(QStringLiteral("sidebarTree"));
    if (!tree)
    {
        if (error) *error = QStringLiteral("Legacy MainWindow sidebar tree was not found.");
        return false;
    }
    if (!clickTreeRoute(tree, QStringLiteral("my_workspace"), error))
    {
        return false;
    }
    const bool openedClassesRoute = clickTreeRoute(
        tree, QStringLiteral("classes"), error);
    if (routePromptService && !routePromptService->capturePending())
    {
        if (error) *error = routePromptService->error();
        return false;
    }
    if (!openedClassesRoute)
    {
        return false;
    }
    if (routePromptService && !routePromptService->captured())
    {
        if (error) *error = routePromptService->error().isEmpty()
            ? QStringLiteral("Legacy Classes route warning capture did not complete.")
            : routePromptService->error();
        return false;
    }
    auto* const classesPage = (*window)->pageManager()->classesPage();
    if (!classesPage
        || (!routePromptService
            && !(*window)->pageManager()->isCurrentPage(PageType::Classes)))
    {
        if (error) *error = QStringLiteral(
            "Legacy MainWindow did not activate its Classes route or retain its captured read warning.");
        return false;
    }
    if (fixture.classCount == 96 && fixture.testingClassesPresent)
    {
        if (!classesPage->selectClassForStartupDiagnostics(SelectedClassId)
            || classesPage->currentClassId() != SelectedClassId)
        {
            if (error) *error = QStringLiteral("Legacy Classes page could not select class 25.");
            return false;
        }
    }
    Q_UNUSED(application);
    return true;
}

bool captureLegacy(
    QApplication& application,
    const Fixture& fixture,
    const QString& state,
    const QString& locale,
    const QString& theme,
    const QString& path,
    ModalImage* modalImage,
    QString* error
    )
{
    std::unique_ptr<LanguageService> languageService;
    std::unique_ptr<MainWindow> window;
    ModalImage routeModal;
    if (!setupLegacyWindow(application, fixture, locale, theme,
            &languageService, &window,
            state == QStringLiteral("list-read-error") ? &routeModal : nullptr,
            error))
    {
        return false;
    }
    auto* const page = window->pageManager()->classesPage();

    if (state == QStringLiteral("list-read-error"))
    {
        *modalImage = std::move(routeModal);
        const QString visibleText = modalImage->message + QLatin1Char('\n')
            + modalImage->informativeText + QLatin1Char('\n')
            + modalImage->detailedText;
        if (!modalImage->title.contains(QStringLiteral("Load Classes"))
            || !visibleText.contains(QStringLiteral("could not be loaded"), Qt::CaseInsensitive))
        {
            if (error) *error = QStringLiteral(
                "Legacy Classes page did not show its expected list-read warning: %1 / %2")
                    .arg(modalImage->title, visibleText);
            return false;
        }
        return saveImage(path, modalImage->image, error);
    }

    if (state == QStringLiteral("delete-confirmation"))
    {
        if (page->currentClassId() != SelectedClassId)
        {
            if (error) *error = QStringLiteral("Legacy page state lost selected class 25.");
            return false;
        }
        if (!captureLegacyDeleteConfirmation(application, *window, modalImage, error))
        {
            return false;
        }
        if (modalImage->message.trimmed().isEmpty())
        {
            if (error) *error = QStringLiteral(
                "Legacy did not show a delete-confirmation message for class 25.");
            return false;
        }
        return saveImage(path, modalImage->image, error);
    }

    if (page->runtimeMetrics().sourceClassCount != 0)
    {
        if (error) *error = QStringLiteral("Legacy empty Classes page has rows.");
        return false;
    }
    return saveWindowImage(window.get(), path, error);
}

Bootstrap::ApplicationBootstrap::ResourcePackagePathsResolver resourceResolver()
{
    return []() -> Resources::ResourceResult<Resources::ResourcePackagePaths> {
        return Resources::ResourcePackagePaths{
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT),
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG)
        };
    };
}

bool stageNextRuntimeTranslations(QString* error)
{
    const QString resourceRoot = QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
    const QString generatedTranslations = QDir(resourceRoot).absoluteFilePath(
        QStringLiteral("../assets/translations"));
    const QString packagedTranslations = QDir(resourceRoot).filePath(
        QStringLiteral("assets/translations"));
    if (!QDir().mkpath(packagedTranslations))
    {
        if (error) *error = QStringLiteral("Could not prepare the temporary Next resource translation directory.");
        return false;
    }
    for (const QString& locale : {QStringLiteral("en_US"), QStringLiteral("ko_KR")})
    {
        const QString fileName = QStringLiteral("ClassMngr_%1.qm").arg(locale);
        const QString source = QDir(generatedTranslations).filePath(fileName);
        const QString destination = QDir(packagedTranslations).filePath(fileName);
        if (QFileInfo::exists(destination) && !QFile::remove(destination))
        {
            if (error) *error = QStringLiteral("Could not replace temporary Next translation: %1")
                .arg(destination);
            return false;
        }
        if (!QFile::copy(source, destination))
        {
            if (error) *error = QStringLiteral("Could not stage generated Next translation: %1")
                .arg(source);
            return false;
        }
    }
    return true;
}

bool configureNextBootstrap(
    QApplication& application,
    const Fixture& fixture,
    const QString& locale,
    const QString& theme,
    std::unique_ptr<Bootstrap::ApplicationBootstrap>* bootstrap,
    QString* error
    )
{
    if (!configurePreferences(locale, theme, error))
    {
        return false;
    }
    if (!stageNextRuntimeTranslations(error))
    {
        return false;
    }
    QLocale::setDefault(QLocale(locale));

    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams workspace;
    workspace.commandLineArguments = QStringList{
        QStringLiteral("ClassMngr"), fixture.path
    };
    workspace.recentAutoOpenEnabled = false;
    workspace.initialSetupRecoveryJournalPath = QDir(QFileInfo(fixture.path).absolutePath())
        .filePath(QStringLiteral("startup-recovery.json"));
    bootstrap->reset(new Bootstrap::ApplicationBootstrap(
        application,
        {},
        [] { return std::make_unique<QuietStartupDiagnosticsSink>(); },
        resourceResolver(),
        QStringList{locale},
        std::move(workspace),
        [] { return std::make_unique<QuietStartupUpdateDispatcher>(); }
        ));
    const auto started = (*bootstrap)->start();
    if (!started.succeeded() || started.status != Application::StartupStatus::StartupReady
        || !(*bootstrap)->shell())
    {
        if (error) *error = QStringLiteral("Production ApplicationBootstrap failed: %1")
            .arg(started.failure
                ? QString::number(static_cast<int>(*started.failure))
                : QStringLiteral("no startup shell"));
        return false;
    }
    return true;
}

bool selectNextClass25(NextUi::ClassesNavigatorPage* page, QString* error)
{
    if (!page)
    {
        if (error) *error = QStringLiteral("Next Classes page was not created by ApplicationBootstrap.");
        return false;
    }
    const auto selectedId = Domain::ClassId::fromString(
        std::to_string(SelectedClassId));
    if (!selectedId)
    {
        if (error) *error = QStringLiteral("Class ID 25 is invalid.");
        return false;
    }
    for (int row = 0; row < page->model()->rowCount(); ++row)
    {
        const QModelIndex index = page->model()->index(row, 0);
        const auto classId = page->model()->classIdAt(index);
        if (classId && *classId == *selectedId)
        {
            page->classList()->selectionModel()->setCurrentIndex(
                index,
                QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            QApplication::processEvents();
            if (page->selectedClassId() == selectedId)
            {
                return true;
            }
            break;
        }
    }
    if (error) *error = QStringLiteral("Next production Classes page could not select fixture class 25.");
    return false;
}

bool captureNextModal(
    QApplication& application,
    NextUi::ApplicationShell* shell,
    const std::function<void()>& invoke,
    ModalImage* capture,
    QString* error
    )
{
    bool captured = false;
    QString captureError;
    QTimer::singleShot(0, &application, [&] {
        auto* const modal = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (!modal)
        {
            captureError = QStringLiteral("Next delete action did not open a QMessageBox.");
            if (QWidget* const active = QApplication::activeModalWidget()) active->close();
            return;
        }
        captured = composeModalImage(shell, modal, capture, &captureError);
        modal->done(QMessageBox::No);
    });
    invoke();
    QApplication::processEvents();
    if (!captured)
    {
        if (error) *error = captureError.isEmpty()
            ? QStringLiteral("Next modal capture did not complete.") : captureError;
        return false;
    }
    return true;
}

bool captureNext(
    QApplication& application,
    const Fixture& fixture,
    const QString& state,
    const QString& locale,
    const QString& theme,
    const QString& path,
    ModalImage* modalImage,
    QString* error
    )
{
    std::unique_ptr<Bootstrap::ApplicationBootstrap> bootstrap;
    if (!configureNextBootstrap(application, fixture, locale, theme, &bootstrap, error))
    {
        return false;
    }
    NextUi::ApplicationShell* const shell = bootstrap->shell();
    shell->resize(Viewport);
    QApplication::processEvents();
    const auto classesId = NextUi::PageId::fromString(QStringLiteral("classes"));
    if (!classesId || !shell->navigateTo(*classesId, NextUi::NavigationCause::Selection))
    {
        if (error) *error = QStringLiteral("Next production navigation could not open Classes.");
        return false;
    }
    auto* const page = dynamic_cast<NextUi::ClassesNavigatorPage*>(
        shell->pageHost().pageInstance(QStringLiteral("classes")));
    if (!page)
    {
        if (error) *error = QStringLiteral("ApplicationBootstrap did not register its Classes page.");
        return false;
    }
    QApplication::processEvents();

    if (state == QStringLiteral("list-read-error"))
    {
        auto* const label = page->findChild<QLabel*>(QStringLiteral("classesNavigatorStatus"));
        auto* const retry = page->findChild<QToolButton*>(QStringLiteral("classesNavigatorRetry"));
        const QString status = label ? label->text() : QString{};
        if (!label || status.trimmed().isEmpty()
            || !retry || !retry->isVisible() || page->model()->rowCount() != 0)
        {
            if (error) *error = QStringLiteral("Next Classes page missed its list-read error state: %1")
                .arg(status);
            return false;
        }
        if (fixture.schemaVersion != DatabaseSchemaManager::LatestSchemaVersion
            || fixture.testingClassesPresent)
        {
            if (error) *error = QStringLiteral("The Next error capture fixture was not the altered latest-v6 file.");
            return false;
        }
        if (!saveWindowImage(shell, path, error))
        {
            return false;
        }
        *modalImage = {};
        modalImage->title = QStringLiteral("inline-status");
        modalImage->message = status;
        return true;
    }

    if (state == QStringLiteral("delete-confirmation"))
    {
        if (page->model()->rowCount() != 96
            || !selectNextClass25(page, error)
            || !page->selectedClassEditor()
            || !page->selectedClassEditor()->isLoaded())
        {
            if (error && error->isEmpty())
            {
                *error = QStringLiteral("Next did not reach the selected class 25 presentation.");
            }
            return false;
        }
        if (!captureNextModal(application, shell, [&] {
                page->deleteClassButton()->click();
            }, modalImage, error))
        {
            return false;
        }
        if (modalImage->message.trimmed().isEmpty())
        {
            if (error) *error = QStringLiteral(
                "Next did not show a delete confirmation for its verified class 25 selection (title=%1, message=%2).")
                    .arg(modalImage->title, modalImage->message);
            return false;
        }
        return saveImage(path, modalImage->image, error);
    }

    auto* const label = page->findChild<QLabel*>(QStringLiteral("classesNavigatorStatus"));
    if (!label || page->model()->rowCount() != 0 || label->text().trimmed().isEmpty())
    {
        if (error) *error = QStringLiteral("Next did not reach the empty Classes state.");
        return false;
    }
    return saveWindowImage(shell, path, error);
}

QJsonObject modalMetadata(const ModalImage& modal)
{
    QString composition = QStringLiteral("Full-window QWidget grab.");
    if (modal.title == QStringLiteral("inline-status"))
    {
        composition = QStringLiteral("Full-window QWidget grab; the Classes read error is shown inline and has no modal window.");
    }
    else if (modal.title == QStringLiteral("Load Classes"))
    {
        composition = QStringLiteral(
            "Full-window QWidget grab with the visible top-level QMessageBox grab composited at its shell-relative location; the Legacy modal was presented from the PromptRequest emitted by production Classes navigation for deterministic offscreen capture.");
    }
    else if (modal.title != QStringLiteral("empty-state"))
    {
        composition = QStringLiteral(
            "Full-window QWidget grab with the visible top-level QMessageBox grab composited at its shell-relative location.");
    }
    return QJsonObject{
        {QStringLiteral("title"), modal.title},
        {QStringLiteral("message"), modal.message},
        {QStringLiteral("informativeText"), modal.informativeText},
        {QStringLiteral("detailedText"), modal.detailedText},
        {QStringLiteral("captureComposition"), composition}
    };
}

QJsonObject captureManifestEntry(
    const QString& state,
    const QString& locale,
    const QString& theme,
    const QString& pairName,
    const Fixture& fixture,
    const QString& legacyPath,
    const QString& nextPath,
    const ModalImage& legacyModal,
    const ModalImage& nextModal
    )
{
    const auto imageRecord = [&pairName, &fixture](
        const QString& path, const ModalImage& modal) {
        QJsonObject record{
            {QStringLiteral("image"), pairName + QLatin1Char('/') + QFileInfo(path).fileName()},
            {QStringLiteral("sha256"), sha256(path)},
            {QStringLiteral("fixtureSha256"), QString::fromLatin1(fixture.sha256)},
            {QStringLiteral("fullWindow"), true},
            {QStringLiteral("modal"), modalMetadata(modal)}
        };
        return record;
    };
    QJsonObject fixtureRecord{
        {QStringLiteral("temporary"), true},
        {QStringLiteral("fileName"), fixture.fileName},
        {QStringLiteral("sha256"), QString::fromLatin1(fixture.sha256)},
        {QStringLiteral("schemaVersion"), fixture.schemaVersion},
        {QStringLiteral("classCount"), fixture.classCount},
        {QStringLiteral("testingClassesTablePresent"), fixture.testingClassesPresent},
        {QStringLiteral("provenance"), fixture.provenance}
    };
    if (!fixture.listReadDiagnostic.isEmpty())
    {
        fixtureRecord.insert(QStringLiteral("productionListReadDiagnostic"),
            fixture.listReadDiagnostic);
    }
    if (!fixture.source.isEmpty())
    {
        fixtureRecord.insert(QStringLiteral("source"), fixture.source);
        fixtureRecord.insert(QStringLiteral("sourceSha256"),
            QString::fromLatin1(fixture.sourceSha256));
    }
    return QJsonObject{
        {QStringLiteral("state"), state},
        {QStringLiteral("locale"), locale},
        {QStringLiteral("theme"), theme},
        {QStringLiteral("viewport"), QJsonObject{
            {QStringLiteral("width"), Viewport.width()},
            {QStringLiteral("height"), Viewport.height()}}},
        {QStringLiteral("fixture"), fixtureRecord},
        {QStringLiteral("selectedClassId"),
            state == QStringLiteral("delete-confirmation") ? SelectedClassId : 0},
        {QStringLiteral("legacy"), imageRecord(legacyPath, legacyModal)},
        {QStringLiteral("next"), imageRecord(nextPath, nextModal)}
    };
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ClassMngr Classes full-window state capture"));
    QApplication::setStyle(QStringLiteral("Fusion"));
    QTemporaryDir settingsDirectory;
    if (!settingsDirectory.isValid())
    {
        qCritical("Could not create a temporary settings directory.");
        return 1;
    }
    qputenv("CLASSMNGR_SETTINGS_ROOT", settingsDirectory.path().toUtf8());

    if (argc != 2)
    {
        qCritical("Usage: ClassMngrNextClassesFullWindowStateVisualCapture OUTPUT_DIR");
        return 2;
    }
    const QString outputRoot = QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath();
    if (!QDir().mkpath(outputRoot))
    {
        qCritical().noquote() << "Could not create full-window state capture directory:" << outputRoot;
        return 2;
    }

    const QList<QPair<QString, QString>> matchedLocalesAndThemes{
        {QStringLiteral("en_US"), QStringLiteral("light")},
        {QStringLiteral("ko_KR"), QStringLiteral("dark")}
    };
    const QStringList states{
        QStringLiteral("empty"),
        QStringLiteral("delete-confirmation"),
        QStringLiteral("list-read-error")
    };

    QJsonArray captures;
    for (const auto& [locale, theme] : matchedLocalesAndThemes)
    {
        QLocale::setDefault(QLocale(locale));
        application.setPalette(themePalette(theme));
        for (const QString& state : states)
        {
            const QString pairName = state + QLatin1Char('/') + locale
                + QLatin1Char('-') + theme;
            const QString pairDirectory = QDir(outputRoot).filePath(pairName);
            if (!QDir().mkpath(pairDirectory))
            {
                qCritical().noquote() << "Could not create state capture pair:" << pairDirectory;
                return 1;
            }
            QTemporaryDir fixtureDirectory;
            if (!fixtureDirectory.isValid())
            {
                qCritical("Could not create a temporary workspace directory.");
                return 1;
            }
            Fixture fixture;
            QString error;
            if (!createFixture(state, fixtureDirectory.path(), &fixture, &error))
            {
                qCritical().noquote() << "Could not prepare" << state << "fixture:" << error;
                return 1;
            }

            const QString legacyPath = QDir(pairDirectory).filePath(QStringLiteral("legacy.png"));
            const QString nextPath = QDir(pairDirectory).filePath(QStringLiteral("next.png"));
            ModalImage legacyModal;
            if (state == QStringLiteral("empty"))
            {
                std::unique_ptr<LanguageService> language;
                std::unique_ptr<MainWindow> legacyWindow;
                if (!setupLegacyWindow(application, fixture, locale, theme,
                        &language, &legacyWindow, nullptr, &error)
                    || !saveWindowImage(legacyWindow.get(), legacyPath, &error))
                {
                    qCritical().noquote() << "Legacy empty capture failed:" << error;
                    return 1;
                }
                legacyWindow.reset();
                language.reset();
            }
            else if (!captureLegacy(application, fixture, state, locale, theme,
                         legacyPath, &legacyModal, &error))
            {
                qCritical().noquote() << "Legacy" << state << "capture failed:" << error;
                return 1;
            }

            ModalImage nextModal;
            if (!captureNext(application, fixture, state, locale, theme,
                    nextPath, &nextModal, &error))
            {
                qCritical().noquote() << "Next" << state << "capture failed:" << error;
                return 1;
            }
            if (state == QStringLiteral("empty"))
            {
                legacyModal.title = QStringLiteral("empty-state");
                legacyModal.message = QStringLiteral("No class rows in the shared v6 fixture.");
                nextModal = legacyModal;
            }
            const QString legacyHash = sha256(legacyPath);
            const QString nextHash = sha256(nextPath);
            if (legacyHash.isEmpty() || nextHash.isEmpty())
            {
                qCritical("Could not hash a matched full-window state capture.");
                return 1;
            }
            captures.append(captureManifestEntry(state, locale, theme, pairName,
                fixture, legacyPath, nextPath, legacyModal, nextModal));
            qInfo().noquote() << "Captured" << state << locale << theme
                << "Legacy/Next pair" << legacyHash << nextHash;
        }
    }

    QJsonObject manifest{
        {QStringLiteral("purpose"), QStringLiteral(
            "Matched empty, delete-confirmation, and Classes list-read-error full-window captures from the production Legacy MainWindow and Next ApplicationBootstrap navigation paths.")},
        {QStringLiteral("states"), QJsonArray{
            QStringLiteral("empty"), QStringLiteral("delete-confirmation"),
            QStringLiteral("list-read-error")}},
        {QStringLiteral("localesAndThemes"), QJsonArray{
            QJsonObject{{QStringLiteral("locale"), QStringLiteral("en_US")},
                {QStringLiteral("theme"), QStringLiteral("light")}},
            QJsonObject{{QStringLiteral("locale"), QStringLiteral("ko_KR")},
                {QStringLiteral("theme"), QStringLiteral("dark")}}}},
        {QStringLiteral("viewport"), QJsonObject{
            {QStringLiteral("width"), Viewport.width()},
            {QStringLiteral("height"), Viewport.height()}}},
        {QStringLiteral("legacyComposition"), QStringLiteral(
            "Production MainWindow opened the temporary .tps and navigated through the real sidebar Classes route.")},
        {QStringLiteral("nextComposition"), QStringLiteral(
            "Production ApplicationBootstrap opened the same temporary .tps and navigated through its production ApplicationShell navigation tree to Classes.")},
        {QStringLiteral("fixturePairing"), QStringLiteral(
            "Each Legacy/Next pair used the same temporary .tps path and fixture SHA-256; fixtures were discarded with QTemporaryDir after both applications closed.")},
        {QStringLiteral("modalCapture"), QStringLiteral(
            "For modal images, the shell and visible top-level QMessageBox were grabbed separately, then composited at the QMessageBox shell-relative location for deterministic Qt offscreen capture.")},
        {QStringLiteral("parityClaim"), false},
        {QStringLiteral("comparisonScope"), QStringLiteral(
            "Semantic state matching only. The Legacy and Next navigation and page presentations differ, so these captures do not claim pixel parity.")},
        {QStringLiteral("captures"), captures}
    };
    const QString manifestPath = QDir(outputRoot).filePath(
        QStringLiteral("matched-full-window-state-captures.json"));
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Text)
        || manifestFile.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented)) < 0)
    {
        qCritical().noquote() << "Could not write the capture manifest:" << manifestPath;
        return 1;
    }
    qInfo().noquote() << "Wrote 12 full-window state images and manifest to" << outputRoot;
    return 0;
}
