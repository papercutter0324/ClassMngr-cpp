#include "app/mainwindow.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "core/theme_service.h"
#include "data/database/database_schema_manager.h"
#include "data/database/workspace_store.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "features/my_info/ui/personal_details_page.h"
#include "next/bootstrap/application_bootstrap.h"
#include "next/bootstrap/startup_diagnostics_sink.h"
#include "next/bootstrap/startup_update_dispatcher.h"
#include "next/platform/settings_manager_language_preferences_port.h"
#include "next/platform/settings_manager_theme_preferences_port.h"
#include "next/resources/resource_root_resolver.h"
#include "next/ui/application_shell.h"
#include "next/ui/page.h"
#include "next/ui/page_host.h"
#include "next/ui/my_workspace_page.h"
#include "ui/shared/dialogs/file_dialog_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/navigation_tab_widget.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDir>
#include <QDialog>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenuBar>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>
#include <QPixmap>
#include <QTranslator>
#include <QWidget>

#include <memory>
#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr;
using namespace ClassMngr::Next;
namespace NextUi = ClassMngr::Next::Ui;

namespace
{

constexpr QSize Viewport{1024, 720};
constexpr int ExpectedSchemaVersion = 6;
constexpr int ExpectedPairCount = 18;

struct Fixture final
{
    QString directory;
    QString profilePath;
    QString databasePath;
    QString recentSecondPath;
    QString recentThirdPath;
    QString replacementRecoveryPath;
    int schemaVersion = 0;
};

struct CapturedImage final
{
    QString fileName;
    qint64 fileSize = 0;
    QString sha256;
    QSize size;
};

struct CapturePair final
{
    QString state;
    QString locale;
    QString theme;
    CapturedImage legacy;
    CapturedImage next;
};

struct SupplementalCapture final
{
    QString state;
    QString locale;
    QString theme;
    CapturedImage image;
};

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

class MemoryDirectoryPreferences final
    : public Application::FileDialogDirectoryPreferencesPort
{
public:
    [[nodiscard]] Application::DirectoryPreferencePath readDirectory(
        Application::FileDialogPurpose
        ) const override
    {
        return Application::DirectoryPreferencePath(m_directory);
    }

    [[nodiscard]] Domain::Result<void> writeDirectory(
        Application::FileDialogPurpose,
        const Application::DirectoryPreferencePath& path
        ) const override
    {
        m_directory = path.value();
        return Domain::Result<void>::success();
    }

private:
    mutable std::string m_directory;
};

enum class CaptureWorkflow
{
    NewReplacement,
    Open,
    DirtyClose,
    SaveAs,
    Export,
    RecoverySelection,
    SchemaV6DatabaseOpen
};

class ModalCaptureRouter final : public QObject
{
public:
    ModalCaptureRouter(
        QWidget& shell,
        QString state,
        QString outputPath,
        QString targetPath,
        CaptureWorkflow workflow,
        bool isNext,
        QString auxiliaryPromptPath = {}
        )
        : m_shell(&shell),
          m_state(std::move(state)),
          m_outputPath(std::move(outputPath)),
          m_targetPath(std::move(targetPath)),
          m_workflow(workflow),
          m_isNext(isNext),
          m_auxiliaryPromptPath(std::move(auxiliaryPromptPath))
    {
    }

    [[nodiscard]] bool captured() const noexcept { return m_captured; }
    [[nodiscard]] const QString& error() const noexcept { return m_error; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() != QEvent::Show || m_scheduled || m_done)
        {
            return QObject::eventFilter(watched, event);
        }

        auto* const dialog = qobject_cast<QDialog*>(watched);
        if (!dialog || !dialog->isWindow())
        {
            return QObject::eventFilter(watched, event);
        }

        if (auto* const fileDialog = qobject_cast<QFileDialog*>(dialog))
        {
            m_scheduled = true;
            QTimer::singleShot(0, dialog, [this, fileDialog] {
                prepareAndCaptureFileDialog(*fileDialog);
            });
            return QObject::eventFilter(watched, event);
        }

        if (m_workflow == CaptureWorkflow::NewReplacement && m_isNext
            && !m_acceptedNewConfirmation)
        {
            auto* const messageBox = qobject_cast<QMessageBox*>(dialog);
            if (messageBox)
            {
                m_acceptedNewConfirmation = true;
                m_scheduled = true;
                QTimer::singleShot(0, messageBox, [this, messageBox] {
                    if (!m_auxiliaryPromptPath.isEmpty()
                        && !captureComposite(*messageBox, m_auxiliaryPromptPath))
                    {
                        m_done = true;
                        messageBox->reject();
                        return;
                    }
                    m_scheduled = false;
                    for (QAbstractButton* const button : messageBox->buttons())
                    {
                        const QMessageBox::ButtonRole role =
                            messageBox->buttonRole(button);
                        if (role == QMessageBox::AcceptRole
                            || role == QMessageBox::YesRole
                            || role == QMessageBox::ApplyRole)
                        {
                            button->click();
                            return;
                        }
                    }
                    messageBox->accept();
                });
                return QObject::eventFilter(watched, event);
            }
        }

        const bool promptState = m_workflow == CaptureWorkflow::DirtyClose
            || m_workflow == CaptureWorkflow::RecoverySelection;
        if (promptState)
        {
            m_scheduled = true;
            QTimer::singleShot(0, dialog, [this, dialog] {
                QApplication::processEvents(QEventLoop::AllEvents, 50);
                if (!captureComposite(*dialog))
                {
                    m_error = QStringLiteral("Could not retain the visible prompt for %1.")
                        .arg(m_state);
                }
                m_done = true;
                m_captured = m_error.isEmpty();
                dialog->reject();
            });
        }

        return QObject::eventFilter(watched, event);
    }

private:
    void prepareAndCaptureFileDialog(QFileDialog& dialog)
    {
        QApplication::processEvents(QEventLoop::AllEvents, 50);
        const QFileInfo target(m_targetPath);
        if (!target.absolutePath().isEmpty())
        {
            dialog.setDirectory(target.absolutePath());
        }
        if (m_workflow == CaptureWorkflow::SchemaV6DatabaseOpen)
        {
            for (const QString& filter : dialog.nameFilters())
            {
                if (filter.contains(QStringLiteral("*.db"), Qt::CaseInsensitive))
                {
                    dialog.selectNameFilter(filter);
                    QApplication::processEvents(QEventLoop::AllEvents, 50);
                    break;
                }
            }
        }
        const QString targetFileName = target.fileName();
        dialog.selectFile(targetFileName);
        QLineEdit* fileNameEdit = dialog.findChild<QLineEdit*>(
            QStringLiteral("fileNameEdit"));
        if (!fileNameEdit)
        {
            for (QLineEdit* const candidate : dialog.findChildren<QLineEdit*>())
            {
                if (candidate->isVisible() && candidate->isEnabled())
                {
                    fileNameEdit = candidate;
                    break;
                }
            }
        }
        if (fileNameEdit && fileNameEdit->text() != targetFileName)
        {
            fileNameEdit->setText(targetFileName);
        }
        QApplication::processEvents(QEventLoop::AllEvents, 50);
        const QStringList selections = dialog.selectedFiles();
        const QString selectedPath = selections.isEmpty()
            ? QString{} : QDir::cleanPath(
                QFileInfo(selections.constFirst()).absoluteFilePath());
        const QString expectedPath = QDir::cleanPath(target.absoluteFilePath());
        if (!fileNameEdit || selectedPath != expectedPath)
        {
            m_error = QStringLiteral(
                "Could not select the synthetic path for %1 (selected '%2', expected '%3').")
                .arg(m_state, selectedPath, expectedPath);
            m_done = true;
            dialog.reject();
            return;
        }
        if (!captureComposite(dialog, m_outputPath))
        {
            m_error = QStringLiteral("Could not retain the visible file dialog for %1.")
                .arg(m_state);
        }
        m_done = true;
        m_captured = m_error.isEmpty();
        dialog.reject();
    }

    [[nodiscard]] bool captureComposite(
        QWidget& overlay,
        const QString& outputPath = {}
        )
    {
        if (!m_shell || m_shell->size() != Viewport)
        {
            m_error = QStringLiteral("The production shell did not render at %1x%2.")
                .arg(Viewport.width()).arg(Viewport.height());
            return false;
        }
        const QPixmap shellPixmap = m_shell->grab();
        const QPixmap overlayPixmap = overlay.grab();
        if (shellPixmap.size() != Viewport || overlayPixmap.isNull())
        {
            m_error = QStringLiteral("A shell or visible modal image was empty for %1.")
                .arg(m_state);
            return false;
        }

        QImage image(Viewport, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.drawPixmap(0, 0, shellPixmap);
        const QPoint shellOrigin = m_shell->mapToGlobal(QPoint(0, 0));
        const QPoint overlayOrigin = overlay.mapToGlobal(QPoint(0, 0));
        painter.drawPixmap(overlayOrigin - shellOrigin, overlayPixmap);
        painter.end();
        const QString destination = outputPath.isEmpty()
            ? m_outputPath : outputPath;
        if (!image.save(destination, "PNG"))
        {
            m_error = QStringLiteral("Could not write visual capture: %1")
                .arg(destination);
            return false;
        }
        return true;
    }

    QPointer<QWidget> m_shell;
    QString m_state;
    QString m_outputPath;
    QString m_targetPath;
    CaptureWorkflow m_workflow;
    bool m_isNext = false;
    bool m_scheduled = false;
    bool m_done = false;
    bool m_captured = false;
    bool m_acceptedNewConfirmation = false;
    QString m_auxiliaryPromptPath;
    QString m_error;
};

[[nodiscard]] QString sha256(const QString& path)
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

[[nodiscard]] bool writeFile(
    const QString& path,
    const QByteArray& contents,
    QString* error
    )
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || file.write(contents) != contents.size())
    {
        if (error) *error = QStringLiteral("Could not write fixture file: %1")
            .arg(path);
        return false;
    }
    return true;
}

[[nodiscard]] bool createFixture(
    const QString& directory,
    Fixture* fixture,
    QString* error
    )
{
    fixture->directory = directory;
    fixture->profilePath = QDir(directory).filePath(QStringLiteral("profile.tps"));
    fixture->databasePath = QDir(directory).filePath(QStringLiteral("legacy-v6.db"));
    fixture->recentSecondPath = QDir(directory).filePath(
        QStringLiteral("recent-archive.tps"));
    fixture->recentThirdPath = QDir(directory).filePath(
        QStringLiteral("recent-backup.tps"));
    fixture->replacementRecoveryPath = QDir(directory).filePath(
        QStringLiteral(".profile.tps.workspace-create-12345678-1234-1234-1234-123456789012.recovery"));

    ApplicationServices seedServices;
    if (!seedServices.openDatabase(fixture->profilePath))
    {
        if (error) *error = QStringLiteral("Legacy ApplicationServices could not create the synthetic profile.");
        return false;
    }

    bool fixtureSeeded = false;
    QString seedError;
    {
        QSqlDatabase database = seedServices.workspaceStore()->database();
        QSqlQuery teacher(database);
        teacher.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO teachers (id, teacher_kr, teacher_en, "
            "preferred_romanization, preferred_name, room_number, birthday, "
            "phone_number, wifi_name, wifi_password, internet_type, zoom_id, "
            "zoom_password, projection_type, notes) "
            "VALUES (1, ?, ?, ?, ?, '', '', '', '', '', 'WiFi', '', '', 'Any', '')"));
        teacher.addBindValue(QString::fromUtf8("김다은"));
        teacher.addBindValue(QStringLiteral("Dana Kim"));
        teacher.addBindValue(QStringLiteral("Kim Da-eun"));
        teacher.addBindValue(QStringLiteral("Dana"));
        if (!teacher.exec())
        {
            seedError = QStringLiteral("Could not seed synthetic teacher: %1")
                .arg(teacher.lastError().text());
        }
        else
        {
            QSqlQuery personalName(database);
            personalName.prepare(QStringLiteral(
                "INSERT OR REPLACE INTO app_settings (key, value) VALUES (?, ?)"));
            personalName.addBindValue(QStringLiteral("myInfo/name"));
            personalName.addBindValue(QStringLiteral("Synthetic Profile"));
            if (!personalName.exec())
            {
                seedError = QStringLiteral(
                    "Could not seed synthetic profile details: %1")
                    .arg(personalName.lastError().text());
            }
            else
            {
                const auto version = DatabaseSchemaManager::schemaVersion(database);
                if (!version || *version != ExpectedSchemaVersion)
                {
                    seedError = QStringLiteral(
                        "Synthetic profile schema was not v6.");
                }
                else
                {
                    fixture->schemaVersion = *version;
                    fixtureSeeded = true;
                }
            }
        }
    }
    seedServices.closeDatabase();
    if (!fixtureSeeded)
    {
        if (error) *error = seedError;
        return false;
    }

    if (!QFile::copy(fixture->profilePath, fixture->databasePath)
        || !QFile::copy(fixture->profilePath, fixture->recentSecondPath)
        || !QFile::copy(fixture->profilePath, fixture->recentThirdPath)
        || !QFile::copy(fixture->profilePath, fixture->replacementRecoveryPath))
    {
        if (error) *error = QStringLiteral("Could not copy disposable synthetic profile variants.");
        return false;
    }
    return true;
}

[[nodiscard]] bool configurePreferences(
    const Fixture& fixture,
    const QString& locale,
    const QString& theme,
    QString* error
    )
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
    SettingsManager::instance().setRecentFiles({
        fixture.recentSecondPath,
        fixture.recentThirdPath
    });
    SettingsManager::instance().setLastDatabaseDirectory(fixture.directory);
    if (!themeResult || !languageResult)
    {
        if (error) *error = QStringLiteral("Could not isolate the synthetic locale/theme preferences.");
        return false;
    }
    SettingsManager::instance().sync();
    return true;
}

[[nodiscard]] Bootstrap::ApplicationBootstrap::ResourcePackagePathsResolver
resourceResolver()
{
    return []() -> Resources::ResourceResult<Resources::ResourcePackagePaths> {
        return Resources::ResourcePackagePaths{
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT),
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG)
        };
    };
}

[[nodiscard]] bool configureLegacy(
    QApplication& application,
    const Fixture& fixture,
    const QString& locale,
    const QString& theme,
    std::unique_ptr<LanguageService>* languageService,
    std::unique_ptr<MainWindow>* window,
    MemoryDirectoryPreferences& directoryPreferences,
    std::unique_ptr<QtFileDialogService>* fileDialogService,
    QString* error
    )
{
    if (!configurePreferences(fixture, locale, theme, error))
    {
        return false;
    }
    QLocale::setDefault(QLocale(locale));

    const QString translationsRoot = QStringLiteral(
        CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY);
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
        if (error) *error = QStringLiteral("Legacy could not install its production translation catalog.");
        return false;
    }

    auto startupTheme = std::make_unique<ThemeService>();
    startupTheme->setTheme(theme == QStringLiteral("dark")
        ? Theme::Dark : Theme::Light);
    MainWindowStartupOptions options;
    options.loadMostRecentDatabase = false;
    options.initialDatabasePath = fixture.profilePath;
    options.startupThemeService = std::move(startupTheme);
    window->reset(new MainWindow(false, languageService->get(), nullptr,
                                 std::move(options)));
    (*window)->resize(Viewport);
    (*window)->show();
    QApplication::processEvents();
    if ((*window)->size() != Viewport || !(*window)->services()
        || !(*window)->services()->hasOpenDatabase())
    {
        if (error) *error = QStringLiteral("Legacy MainWindow did not open the synthetic profile.");
        return false;
    }
    (*window)->pageManager()->showPage(PageType::MyWorkspace);
    if (MyWorkspacePage* const workspace =
            (*window)->pageManager()->myWorkspacePage())
    {
        workspace->openTab(WorkspaceTab::Details);
    }
    else
    {
        if (error) *error = QStringLiteral("Legacy My Workspace page was unavailable.");
        return false;
    }
    (*window)->pageManager()->setSaveMode(SaveMode::Manual);
    QApplication::processEvents();
    *fileDialogService = std::make_unique<QtFileDialogService>(
        directoryPreferences, FileDialogBackend::Qt);
    DialogServices::setFileDialogServiceForTesting(fileDialogService->get());
    return true;
}

[[nodiscard]] bool configureNext(
    QApplication& application,
    const Fixture& fixture,
    const QString& locale,
    const QString& theme,
    std::unique_ptr<Bootstrap::ApplicationBootstrap>* bootstrap,
    QString* error
    )
{
    if (!configurePreferences(fixture, locale, theme, error))
    {
        return false;
    }
    QLocale::setDefault(QLocale(locale));
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams workspace;
    workspace.commandLineArguments = QStringList{
        QStringLiteral("ClassMngr"), fixture.profilePath
    };
    workspace.recentAutoOpenEnabled = false;
    workspace.initialSetupRecoveryJournalPath = QDir(fixture.directory).filePath(
        QStringLiteral("startup-recovery.json"));
    bootstrap->reset(new Bootstrap::ApplicationBootstrap(
        application,
        {},
        [] { return std::make_unique<QuietStartupDiagnosticsSink>(); },
        resourceResolver(),
        QStringList{locale},
        std::move(workspace),
        [] { return std::make_unique<QuietStartupUpdateDispatcher>(); }
        ));
    const Application::StartupResult started = (*bootstrap)->start();
    if (!started.succeeded() || !(*bootstrap)->shell())
    {
        if (error) *error = QStringLiteral("Next ApplicationBootstrap could not open the synthetic profile.");
        return false;
    }
    (*bootstrap)->shell()->resize(Viewport);
    (*bootstrap)->shell()->show();
    const auto myWorkspaceId = NextUi::PageId::fromString(QStringLiteral("my_workspace"));
    if (!myWorkspaceId
        || !(*bootstrap)->shell()->navigateTo(*myWorkspaceId, NextUi::NavigationCause::Selection))
    {
        if (error) *error = QStringLiteral("Next shell could not display My Workspace.");
        return false;
    }
    QApplication::processEvents();
    if ((*bootstrap)->shell()->size() != Viewport)
    {
        if (error) *error = QStringLiteral("Next ApplicationShell did not render at the selected viewport.");
        return false;
    }
    return true;
}

[[nodiscard]] QAction* nextAction(QWidget* shell, const QString& objectName)
{
    return shell ? shell->findChild<QAction*>(objectName) : nullptr;
}

[[nodiscard]] QMenu* legacyFileMenu(MainWindow& window)
{
    const QList<QAction*> actions = window.menuBar()->actions();
    return actions.isEmpty() ? nullptr : actions.constFirst()->menu();
}

[[nodiscard]] bool captureActionModal(
    QApplication& application,
    QWidget& shell,
    QAction& action,
    const QString& state,
    const QString& outputPath,
    const QString& targetPath,
    CaptureWorkflow workflow,
    bool isNext,
    QString* error,
    QString auxiliaryPromptPath = {}
    )
{
    ModalCaptureRouter router(
        shell, state, outputPath, targetPath, workflow, isNext,
        std::move(auxiliaryPromptPath));
    application.installEventFilter(&router);
    action.trigger();
    QApplication::processEvents(QEventLoop::AllEvents, 100);
    application.removeEventFilter(&router);
    if (!router.captured())
    {
        if (error) *error = router.error().isEmpty()
            ? QStringLiteral("The %1 action did not expose its expected visible UI.")
                .arg(state)
            : router.error();
        return false;
    }
    return true;
}

[[nodiscard]] QImage captureMenuComposite(
    QWidget& shell,
    QMenu& fileMenu,
    QMenu& recentMenu,
    QString* error
    )
{
    fileMenu.ensurePolished();
    recentMenu.ensurePolished();
    const QMenuBar* const menuBar = shell.findChild<QMenuBar*>();
    const QPoint fileAnchor = shell.mapToGlobal(QPoint(
        16, menuBar ? menuBar->height() : 0));
    fileMenu.popup(fileAnchor);
    QApplication::processEvents(QEventLoop::AllEvents, 50);
    const QRect recentActionRect = fileMenu.actionGeometry(recentMenu.menuAction());
    if (recentActionRect.isEmpty())
    {
        if (error) *error = QStringLiteral("The production File menu has no Recent Files entry.");
        fileMenu.close();
        return {};
    }
    const QPoint recentAnchor = fileMenu.mapToGlobal(recentActionRect.topRight());
    recentMenu.popup(recentAnchor);
    QApplication::processEvents(QEventLoop::AllEvents, 50);
    const QPixmap shellPixmap = shell.grab();
    const QPixmap filePixmap = fileMenu.grab();
    const QPixmap recentPixmap = recentMenu.grab();
    if (shellPixmap.size() != Viewport || filePixmap.isNull()
        || recentPixmap.isNull())
    {
        if (error) *error = QStringLiteral("Could not render production Recent Files menus.");
        fileMenu.close();
        recentMenu.close();
        return {};
    }

    QImage image(Viewport, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.drawPixmap(0, 0, shellPixmap);
    const QPoint shellOrigin = shell.mapToGlobal(QPoint(0, 0));
    painter.drawPixmap(fileMenu.mapToGlobal(QPoint(0, 0)) - shellOrigin, filePixmap);
    painter.drawPixmap(recentMenu.mapToGlobal(QPoint(0, 0)) - shellOrigin, recentPixmap);
    painter.end();
    fileMenu.close();
    recentMenu.close();
    QApplication::processEvents();
    return image;
}

[[nodiscard]] bool saveMenuCapture(
    QWidget& shell,
    QMenu& fileMenu,
    QMenu& recentMenu,
    const QString& path,
    QString* error
    )
{
    const QImage image = captureMenuComposite(shell, fileMenu, recentMenu, error);
    if (image.isNull() || !image.save(path, "PNG"))
    {
        if (error && error->isEmpty())
        {
            *error = QStringLiteral("Could not write production menu capture: %1")
                .arg(path);
        }
        return false;
    }
    return true;
}

[[nodiscard]] bool captureLegacyPair(
    QApplication& application,
    const Fixture& fixture,
    const QString& locale,
    const QString& theme,
    const QString& outputDirectory,
    QVector<CapturePair>* pairs,
    QString* error
    )
{
    std::unique_ptr<LanguageService> language;
    std::unique_ptr<MainWindow> window;
    MemoryDirectoryPreferences directoryPreferences;
    std::unique_ptr<QtFileDialogService> fileDialogs;
    if (!configureLegacy(application, fixture, locale, theme, &language,
            &window, directoryPreferences, &fileDialogs, error))
    {
        return false;
    }
    QMenu* const fileMenu = legacyFileMenu(*window);
    QMenu* const recentMenu = window->actions().recentFilesMenu;
    if (!fileMenu || !recentMenu)
    {
        if (error) *error = QStringLiteral("Legacy MainWindow did not expose its production File menus.");
        return false;
    }

    const auto output = [&](const QString& state) {
        return QDir(outputDirectory).filePath(
            QStringLiteral("legacy-%1-%2-%3.png").arg(state, locale, theme));
    };
    const auto captureAction = [&](const QString& state,
                                   QAction* action,
                                   const QString& target,
                                   CaptureWorkflow workflow) {
        if (!action)
        {
            if (error) *error = QStringLiteral("Legacy file action was unavailable for %1.")
                .arg(state);
            return false;
        }
        return captureActionModal(application, *window, *action, state,
            output(state), target, workflow, false, error);
    };

    if (!captureAction(QStringLiteral("new-replacement"), window->actions().newFile,
            fixture.profilePath, CaptureWorkflow::NewReplacement)
        || !captureAction(QStringLiteral("open-profile"), window->actions().openFile,
            fixture.profilePath, CaptureWorkflow::Open)
        || !captureAction(QStringLiteral("save-as"), window->actions().saveAsFile,
            QDir(fixture.directory).filePath(QStringLiteral("profile-copy.tps")),
            CaptureWorkflow::SaveAs)
        || !captureAction(QStringLiteral("export"), window->actions().exportAsFile,
            QDir(fixture.directory).filePath(QStringLiteral("profile-export.tps")),
            CaptureWorkflow::Export))
    {
        DialogServices::setFileDialogServiceForTesting(nullptr);
        return false;
    }

    if (!saveMenuCapture(*window, *fileMenu, *recentMenu,
            output(QStringLiteral("recent-populated")), error))
    {
        DialogServices::setFileDialogServiceForTesting(nullptr);
        return false;
    }
    QAction* const clearAction = recentMenu->actions().isEmpty()
        ? nullptr : recentMenu->actions().constLast();
    if (!clearAction || clearAction->isSeparator())
    {
        if (error) *error = QStringLiteral("Legacy Recent Files menu did not expose its Clear action.");
        DialogServices::setFileDialogServiceForTesting(nullptr);
        return false;
    }
    clearAction->trigger();
    QApplication::processEvents();
    if (!saveMenuCapture(*window, *fileMenu, *recentMenu,
            output(QStringLiteral("recent-cleared")), error))
    {
        DialogServices::setFileDialogServiceForTesting(nullptr);
        return false;
    }

    if (!captureAction(QStringLiteral("recovery-selection"),
            window->actions().recoverInterruptedProfile,
            {}, CaptureWorkflow::RecoverySelection))
    {
        DialogServices::setFileDialogServiceForTesting(nullptr);
        return false;
    }
    if (!captureAction(QStringLiteral("schema-v6-db-open"),
            window->actions().openFile, fixture.databasePath,
            CaptureWorkflow::SchemaV6DatabaseOpen))
    {
        DialogServices::setFileDialogServiceForTesting(nullptr);
        return false;
    }

    if (PersonalDetailsPage* details = window->pageManager()->myWorkspacePage()
            ? window->pageManager()->myWorkspacePage()->personalDetailsPage()
            : nullptr)
    {
        const QList<QLineEdit*> editors = details->findChildren<QLineEdit*>();
        if (editors.isEmpty())
        {
            if (error) *error = QStringLiteral("Legacy My Workspace did not expose the synthetic name editor.");
            DialogServices::setFileDialogServiceForTesting(nullptr);
            return false;
        }
        editors.constFirst()->setText(QStringLiteral("Unsaved synthetic draft"));
        QApplication::processEvents();
        if (!details->hasUnsavedChanges())
        {
            if (error) *error = QStringLiteral("Legacy synthetic My Workspace edit did not become dirty.");
            DialogServices::setFileDialogServiceForTesting(nullptr);
            return false;
        }
    }
    else
    {
        if (error) *error = QStringLiteral("Legacy My Workspace details were unavailable for dirty-close capture.");
        DialogServices::setFileDialogServiceForTesting(nullptr);
        return false;
    }
    if (!captureAction(QStringLiteral("dirty-close-resolution"),
            window->actions().closeFile, {}, CaptureWorkflow::DirtyClose))
    {
        DialogServices::setFileDialogServiceForTesting(nullptr);
        return false;
    }

    const QStringList states{
        QStringLiteral("new-replacement"),
        QStringLiteral("open-profile"),
        QStringLiteral("dirty-close-resolution"),
        QStringLiteral("save-as"),
        QStringLiteral("export"),
        QStringLiteral("recent-populated"),
        QStringLiteral("recent-cleared"),
        QStringLiteral("recovery-selection"),
        QStringLiteral("schema-v6-db-open")
    };
    for (const QString& state : states)
    {
        const QString path = output(state);
        const QImage image(path);
        const QFileInfo info(path);
        if (image.size() != Viewport || !info.exists() || info.size() <= 0)
        {
            if (error) *error = QStringLiteral("Legacy capture is missing or has the wrong size: %1")
                .arg(path);
            DialogServices::setFileDialogServiceForTesting(nullptr);
            return false;
        }
        pairs->append(CapturePair{
            .state = state,
            .locale = locale,
            .theme = theme,
            .legacy = CapturedImage{
                .fileName = QFileInfo(path).fileName(),
                .fileSize = info.size(),
                .sha256 = sha256(path),
                .size = image.size()
            }
        });
    }

    DialogServices::setFileDialogServiceForTesting(nullptr);
    window.reset();
    language.reset();
    return true;
}

[[nodiscard]] bool captureNextPair(
    QApplication& application,
    const Fixture& fixture,
    const QString& locale,
    const QString& theme,
    const QString& outputDirectory,
    QVector<CapturePair>* pairs,
    QVector<SupplementalCapture>* supplementalCaptures,
    QString* error
    )
{
    std::unique_ptr<Bootstrap::ApplicationBootstrap> bootstrap;
    if (!configureNext(application, fixture, locale, theme, &bootstrap, error))
    {
        return false;
    }
    NextUi::ApplicationShell* const shell = bootstrap->shell();
    QMenu* const fileMenu = shell->findChild<QMenu*>(
        QStringLiteral("classMngrNextFileMenu"));
    QMenu* const recentMenu = shell->recentWorkspaceMenu();
    if (!fileMenu || !recentMenu)
    {
        if (error) *error = QStringLiteral("Next ApplicationShell did not expose its production File menus.");
        return false;
    }
    const auto output = [&](const QString& state) {
        return QDir(outputDirectory).filePath(
            QStringLiteral("next-%1-%2-%3.png").arg(state, locale, theme));
    };
    const auto captureAction = [&](const QString& state,
                                   const QString& actionName,
                                   const QString& target,
                                   CaptureWorkflow workflow,
                                   const QString& auxiliaryPromptPath = {}) {
        QAction* const action = nextAction(shell, actionName);
        if (!action)
        {
            if (error) *error = QStringLiteral("Next file action was unavailable for %1.")
                .arg(state);
            return false;
        }
        return captureActionModal(application, *shell, *action, state,
            output(state), target, workflow, true, error,
            auxiliaryPromptPath);
    };

    const QString newConfirmationPath = QDir(outputDirectory).filePath(
        QStringLiteral("next-new-profile-confirmation-%1-%2.png")
            .arg(locale, theme));
    if (!captureAction(QStringLiteral("new-replacement"),
            QStringLiteral("classMngrNextNewTeacherProfile"),
            fixture.profilePath, CaptureWorkflow::NewReplacement,
            newConfirmationPath)
        || !captureAction(QStringLiteral("open-profile"),
            QStringLiteral("classMngrNextOpenTeacherProfile"),
            fixture.profilePath, CaptureWorkflow::Open)
        || !captureAction(QStringLiteral("save-as"),
            QStringLiteral("classMngrNextSaveWorkspaceAs"),
            QDir(fixture.directory).filePath(QStringLiteral("profile-copy.tps")),
            CaptureWorkflow::SaveAs)
        || !captureAction(QStringLiteral("export"),
            QStringLiteral("classMngrNextExportWorkspace"),
            QDir(fixture.directory).filePath(QStringLiteral("profile-export.tps")),
            CaptureWorkflow::Export))
    {
        return false;
    }
    {
        const QImage image(newConfirmationPath);
        const QFileInfo info(newConfirmationPath);
        if (image.size() != Viewport || !info.exists() || info.size() <= 0)
        {
            if (error) *error = QStringLiteral("Next New Teacher Profile confirmation was not retained.");
            return false;
        }
        supplementalCaptures->append(SupplementalCapture{
            .state = QStringLiteral("new-profile-confirmation"),
            .locale = locale,
            .theme = theme,
            .image = CapturedImage{
                .fileName = info.fileName(),
                .fileSize = info.size(),
                .sha256 = sha256(newConfirmationPath),
                .size = image.size()
            }
        });
    }

    if (!saveMenuCapture(*shell, *fileMenu, *recentMenu,
            output(QStringLiteral("recent-populated")), error))
    {
        return false;
    }
    QAction* const clearAction = recentMenu->actions().isEmpty()
        ? nullptr : recentMenu->actions().constLast();
    if (!clearAction || clearAction->isSeparator())
    {
        if (error) *error = QStringLiteral("Next Recent Files menu did not expose its Clear action.");
        return false;
    }
    clearAction->trigger();
    QApplication::processEvents();
    if (!saveMenuCapture(*shell, *fileMenu, *recentMenu,
            output(QStringLiteral("recent-cleared")), error))
    {
        return false;
    }

    if (!captureAction(QStringLiteral("recovery-selection"),
            QStringLiteral("classMngrNextRecoverInterruptedTeacherProfile"),
            {}, CaptureWorkflow::RecoverySelection)
        || !captureAction(QStringLiteral("schema-v6-db-open"),
            QStringLiteral("classMngrNextOpenTeacherProfile"),
            fixture.databasePath, CaptureWorkflow::SchemaV6DatabaseOpen))
    {
        return false;
    }

    auto* const myWorkspace = dynamic_cast<NextUi::MyWorkspacePage*>(
        shell->pageHost().pageInstance(QStringLiteral("my_workspace")));
    QLineEdit* const nameEdit = myWorkspace
        ? myWorkspace->findChild<QLineEdit*>(QStringLiteral("nextMyWorkspaceName"))
        : nullptr;
    if (!myWorkspace || !nameEdit)
    {
        if (error) *error = QStringLiteral("Next My Workspace name editor was unavailable for dirty-close capture.");
        return false;
    }
    nameEdit->setText(QStringLiteral("Unsaved synthetic draft"));
    QApplication::processEvents();
    if (!myWorkspace->hasUnsavedChanges())
    {
        if (error) *error = QStringLiteral("Next synthetic My Workspace edit did not become dirty.");
        return false;
    }
    if (!captureAction(QStringLiteral("dirty-close-resolution"),
            QStringLiteral("classMngrNextCloseWorkspace"), {},
            CaptureWorkflow::DirtyClose))
    {
        return false;
    }

    const QStringList states{
        QStringLiteral("new-replacement"),
        QStringLiteral("open-profile"),
        QStringLiteral("dirty-close-resolution"),
        QStringLiteral("save-as"),
        QStringLiteral("export"),
        QStringLiteral("recent-populated"),
        QStringLiteral("recent-cleared"),
        QStringLiteral("recovery-selection"),
        QStringLiteral("schema-v6-db-open")
    };
    for (const QString& state : states)
    {
        const QString path = output(state);
        const QImage image(path);
        const QFileInfo info(path);
        if (image.size() != Viewport || !info.exists() || info.size() <= 0)
        {
            if (error) *error = QStringLiteral("Next capture is missing or has the wrong size: %1")
                .arg(path);
            return false;
        }
        const auto pair = std::find_if(pairs->begin(), pairs->end(),
            [&](const CapturePair& candidate) {
                return candidate.state == state && candidate.locale == locale
                    && candidate.theme == theme;
            });
        if (pair == pairs->end())
        {
            if (error) *error = QStringLiteral("Could not match Next image to its Legacy pair: %1")
                .arg(state);
            return false;
        }
        pair->next = CapturedImage{
            .fileName = QFileInfo(path).fileName(),
            .fileSize = info.size(),
            .sha256 = sha256(path),
            .size = image.size()
        };
    }
    static_cast<void>(bootstrap->shutdown());
    bootstrap.reset();
    return true;
}

[[nodiscard]] QJsonObject imageJson(const CapturedImage& image)
{
    return {
        {QStringLiteral("file"), image.fileName},
        {QStringLiteral("width"), image.size.width()},
        {QStringLiteral("height"), image.size.height()},
        {QStringLiteral("bytes"), static_cast<double>(image.fileSize)},
        {QStringLiteral("sha256"), image.sha256}
    };
}

[[nodiscard]] QJsonObject fixtureFileJson(const QString& path)
{
    const QFileInfo info(path);
    return {
        {QStringLiteral("file"), info.fileName()},
        {QStringLiteral("bytes"), static_cast<double>(info.size())},
        {QStringLiteral("sha256"), sha256(path)}
    };
}

[[nodiscard]] bool writeManifest(
    const QString& outputDirectory,
    const Fixture& fixture,
    const QVector<CapturePair>& pairs,
    const QVector<SupplementalCapture>& supplementalCaptures,
    QString* error
    )
{
    QJsonArray records;
    for (const CapturePair& pair : pairs)
    {
        records.append(QJsonObject{
            {QStringLiteral("state"), pair.state},
            {QStringLiteral("locale"), pair.locale},
            {QStringLiteral("theme"), pair.theme},
            {QStringLiteral("legacy"), imageJson(pair.legacy)},
            {QStringLiteral("next"), imageJson(pair.next)}
        });
    }
    QJsonArray supplementalRecords;
    for (const SupplementalCapture& capture : supplementalCaptures)
    {
        supplementalRecords.append(QJsonObject{
            {QStringLiteral("state"), capture.state},
            {QStringLiteral("locale"), capture.locale},
            {QStringLiteral("theme"), capture.theme},
            {QStringLiteral("image"), imageJson(capture.image)}
        });
    }
    const QJsonObject manifest{
        {QStringLiteral("captureVersion"), 1},
        {QStringLiteral("profilePathName"), QStringLiteral("profile.tps")},
        {QStringLiteral("schemaV6InputFileName"), QStringLiteral("legacy-v6.db")},
        {QStringLiteral("schemaVersion"), fixture.schemaVersion},
        {QStringLiteral("pairCount"), pairs.size()},
        {QStringLiteral("imageCount"), pairs.size() * 2},
        {QStringLiteral("supplementalImageCount"), supplementalCaptures.size()},
        {QStringLiteral("viewport"), QJsonObject{
            {QStringLiteral("width"), Viewport.width()},
            {QStringLiteral("height"), Viewport.height()}}},
        {QStringLiteral("platform"), QStringLiteral("macOS; Qt offscreen; Fusion style")},
        {QStringLiteral("fixtureProvenance"), QStringLiteral(
            "Created in a QTemporaryDir by Legacy ApplicationServices::openDatabase, seeded with one synthetic teacher and synthetic My Workspace name, then copied to disposable .db/recent/recovery variants.")},
        {QStringLiteral("fixtureFiles"), QJsonArray{
            fixtureFileJson(fixture.profilePath),
            fixtureFileJson(fixture.databasePath),
            fixtureFileJson(fixture.recentSecondPath),
            fixtureFileJson(fixture.recentThirdPath),
            fixtureFileJson(fixture.replacementRecoveryPath)
        }},
        {QStringLiteral("captures"), records},
        {QStringLiteral("supplementalCaptures"), supplementalRecords}
    };
    QFile file(QDir(outputDirectory).filePath(QStringLiteral("capture-manifest.json")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || file.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented)) < 0)
    {
        if (error) *error = QStringLiteral("Could not write capture manifest.");
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ClassMngr Profile File Workflow Visual Capture"));
    QApplication::setStyle(QStringLiteral("Fusion"));
    QApplication::setQuitOnLastWindowClosed(false);
    if (argc != 2)
    {
        qCritical("Usage: ClassMngrProfileFileWorkflowVisualCapture OUTPUT_DIR");
        return 2;
    }

    const QString outputDirectory = QString::fromLocal8Bit(argv[1]);
    if (!QDir().mkpath(outputDirectory))
    {
        qCritical("Could not create capture output directory");
        return 2;
    }
    QTemporaryDir settingsDirectory;
    QTemporaryDir profileDirectory(QStringLiteral("/private/tmp/cmnf-XXXXXX"));
    if (!settingsDirectory.isValid() || !profileDirectory.isValid())
    {
        qCritical("Could not create disposable settings or profile directories");
        return 1;
    }
    qputenv("CLASSMNGR_SETTINGS_ROOT", settingsDirectory.path().toUtf8());

    Fixture fixture;
    QString error;
    if (!createFixture(profileDirectory.path(), &fixture, &error))
    {
        qCritical().noquote() << error;
        return 1;
    }

    QVector<CapturePair> pairs;
    QVector<SupplementalCapture> supplementalCaptures;
    for (const auto& appearance : {
             std::pair{QStringLiteral("en_US"), QStringLiteral("light")},
             std::pair{QStringLiteral("ko_KR"), QStringLiteral("dark")}})
    {
        const QString locale = appearance.first;
        const QString theme = appearance.second;
        if (!captureLegacyPair(application, fixture, locale, theme,
                outputDirectory, &pairs, &error))
        {
            qCritical().noquote() << error;
            return 1;
        }
        if (!captureNextPair(application, fixture, locale, theme,
                outputDirectory, &pairs, &supplementalCaptures, &error))
        {
            qCritical().noquote() << error;
            return 1;
        }
    }

    if (pairs.size() != ExpectedPairCount)
    {
        qCritical("Visual capture pair count did not match the planned state matrix");
        return 1;
    }
    for (const CapturePair& pair : pairs)
    {
        if (pair.legacy.sha256.isEmpty() || pair.next.sha256.isEmpty())
        {
            qCritical().noquote() << "A capture is missing its SHA-256 record:" << pair.state;
            return 1;
        }
    }
    if (!writeManifest(
            outputDirectory, fixture, pairs, supplementalCaptures, &error))
    {
        qCritical().noquote() << error;
        return 1;
    }
    qInfo().noquote() << "Captured" << pairs.size() << "Legacy/Next pairs and"
                      << pairs.size() * 2 << "images at" << outputDirectory;
    return 0;
}
