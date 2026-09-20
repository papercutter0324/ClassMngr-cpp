#include "app/controllers/file_controller.h"
#include "core/application_services.h"
#include "core/settingsmanager.h"
#include "fakes/fake_file_dialog_service.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/actions/action_registry.h"
#include "ui/shared/dialogs/file_dialog_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMenu>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest/QtTest>

namespace
{
bool writeFile(
    const QString& path,
    const QByteArray& contents
    )
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        return false;
    }

    return file.write(contents) == contents.size();
}

QByteArray readFile(
    const QString& path
    )
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }

    return file.readAll();
}

QStringList initialSetupBackups(
    const QTemporaryDir& workspaceRoot,
    const QString& fileName
    )
{
    return QDir(workspaceRoot.path()).entryList(
        QStringList{
            QStringLiteral(".%1.initial-setup-*.backup")
                .arg(fileName)
        },
        QDir::Files | QDir::Hidden,
        QDir::Name
        );
}

QString fileControllerSource()
{
    QFile source(
        QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
            QStringLiteral("src/app/controllers/file_controller.cpp")
            )
        );
    if (!source.open(QIODevice::ReadOnly))
    {
        return {};
    }

    QString contents = QString::fromUtf8(source.readAll());
    contents.remove(QRegularExpression(QStringLiteral("\\s+")));
    return contents;
}

void connectFileActions(
    FileController& controller,
    ActionRegistry& actions
    )
{
    actions.createActions();
    controller.connectActions(actions);
}
}

class FileControllerWorkspaceLifecycleTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void nullServicesAreSafe();
    void normalCreateUsesCoordinatorAndUpdatesRecent();
    void recentFilesDeduplicateRawAndNormalizedPaths();
    void recentFilesKeepNewestFirstAndCapAtTen();
    void pruningRemovesMissingPathAndClearsLastFile();
    void clearRecentFilesClearsListAndLastFile();
    void recentFilesPreserveUnicodePaths();
    void startupUsesListFirstAndLastFileFallback();
    void normalCreateReplacesExistingTarget();
    void createDoesNotPrepareAfterCloseFailure();
    void initialSetupBackupIsRemovedOnFinish();
    void initialSetupBackupIsRestoredOnCancel();
    void createErrorUsesStructuredUtf8Message();
    void successfulStartupLoadPersistsNormalizedPath();
    void missingStartupPathUsesCapturedWarning();
    void invalidStartupDatabaseUsesLegacyErrorText();
    void startupLoadUsesCoordinatorAndLegacyFallback();
    void autosaveUsesCoordinatorForOpenWorkspace();
    void autosaveWarnsAndPreservesStaleCoordinatorState();
    void autosaveUsesLegacyFallbackForCompatibilityWorkspace();
    void saveAsUsesCoordinatorForOpenWorkspace();
    void saveAsWarnsAndPreservesStaleCoordinatorState();
    void saveAsUsesLegacyFallbackForCompatibilityWorkspace();
    void exportUsesCoordinatorForOpenWorkspaceAndPreservesState();
    void exportWarnsAndPreservesStaleCoordinatorState();
    void exportUsesLegacyFallbackForCompatibilityWorkspace();
    void exportPreservesDialogPolicyAndNoOpenGuard();
    void saveAsMigratesV2AndExportUsesCoordinator();

private:
    QTemporaryDir m_settingsRoot;
};

void FileControllerWorkspaceLifecycleTests::initTestCase()
{
    QVERIFY(m_settingsRoot.isValid());
    qputenv(
        "CLASSMNGR_SETTINGS_ROOT",
        m_settingsRoot.path().toUtf8()
        );
    SettingsManager::instance().clear();
}

void FileControllerWorkspaceLifecycleTests::cleanup()
{
    DialogServices::setFileDialogServiceForTesting(nullptr);
    DialogServices::setUserPromptServiceForTesting(nullptr);
    SettingsManager::instance().clear();
}

void FileControllerWorkspaceLifecycleTests::nullServicesAreSafe()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString missingPath = workspaceRoot.filePath(
        QStringLiteral("missing.tps")
        );

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    FileController controller(nullptr, nullptr);
    controller.loadDatabaseOnStartup(missingPath);

    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        prompts.messages.constFirst().title,
        QStringLiteral("Missing File")
        );
    QCOMPARE(
        prompts.messages.constFirst().message,
        QStringLiteral("File not found:\n%1")
            .arg(QFileInfo(missingPath).absoluteFilePath())
        );
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
}

void FileControllerWorkspaceLifecycleTests::normalCreateUsesCoordinatorAndUpdatesRecent()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(
            workspaceRoot.filePath(QStringLiteral("new-profile"))
            )
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    ApplicationServices services;
    FileController controller(&services, nullptr);

    QVERIFY(controller.createNewDatabaseInteractive());

    const QString expectedPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("new-profile.tps"))
        ).absoluteFilePath();
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), expectedPath);
    QVERIFY(QFileInfo::exists(expectedPath));
    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        QStringList{expectedPath}
        );
    QCOMPARE(SettingsManager::instance().getLastFile(), expectedPath);
    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    QCOMPARE(
        fileDialogs.saveFileRequests.constFirst().title,
        QStringLiteral("New Teacher Profile")
        );
    QCOMPARE(
        fileDialogs.saveFileRequests.constFirst().defaultSuffix,
        QStringLiteral("tps")
        );
}

void FileControllerWorkspaceLifecycleTests::
recentFilesDeduplicateRawAndNormalizedPaths()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString rawPath = workspaceRoot.filePath(
        QStringLiteral("deduplicated-workspace")
        );
    const QString normalizedPath = QFileInfo(
        rawPath + QStringLiteral(".tps")
        ).absoluteFilePath();
    const QString otherPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("other-workspace.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(normalizedPath));
    seedServices.closeDatabase();

    SettingsManager& settings = SettingsManager::instance();
    settings.setRecentFiles({rawPath, normalizedPath, otherPath});
    settings.setLastFile(rawPath);

    ApplicationServices services;
    FileController controller(&services, nullptr);
    controller.loadDatabaseOnStartup(rawPath);

    QCOMPARE(
        settings.getRecentFiles(),
        (QStringList{normalizedPath, otherPath})
        );
    QCOMPARE(settings.getLastFile(), normalizedPath);
}

void FileControllerWorkspaceLifecycleTests::
recentFilesKeepNewestFirstAndCapAtTen()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    ApplicationServices seedServices;
    ApplicationServices services;
    FileController controller(&services, nullptr);
    QStringList expected;
    for (int index = 0; index < 12; ++index)
    {
        const QString rawPath = workspaceRoot.filePath(
            QStringLiteral("workspace-%1").arg(index)
            );
        const QString normalizedPath = QFileInfo(
            rawPath + QStringLiteral(".tps")
            ).absoluteFilePath();
        QVERIFY(seedServices.openDatabase(normalizedPath));
        seedServices.closeDatabase();
        controller.loadDatabaseOnStartup(rawPath);

        expected.prepend(normalizedPath);
        while (expected.size() > 10)
        {
            expected.removeLast();
        }
    }

    QCOMPARE(SettingsManager::instance().getRecentFiles(), expected);
    QCOMPARE(
        SettingsManager::instance().getLastFile(),
        expected.constFirst()
        );
}

void FileControllerWorkspaceLifecycleTests::
pruningRemovesMissingPathAndClearsLastFile()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString rawPath = workspaceRoot.filePath(
        QStringLiteral("missing-workspace")
        );
    const QString normalizedPath = QFileInfo(
        rawPath + QStringLiteral(".tps")
        ).absoluteFilePath();
    const QString otherPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("retained-workspace.tps"))
        ).absoluteFilePath();

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    SettingsManager& settings = SettingsManager::instance();
    settings.setRecentFiles({rawPath, normalizedPath, otherPath});
    settings.setLastFile(rawPath);

    FileController controller(nullptr, nullptr);
    controller.loadDatabaseOnStartup(rawPath);

    QCOMPARE(settings.getRecentFiles(), QStringList{otherPath});
    QVERIFY(settings.getLastFile().isEmpty());
}

void FileControllerWorkspaceLifecycleTests::
clearRecentFilesClearsListAndLastFile()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString directory = QStringLiteral("/keep-this-directory");
    settings.setLastDatabaseDirectory(directory);
    settings.setRecentFiles({QStringLiteral("recent.tps")});
    settings.setLastFile(QStringLiteral("recent.tps"));

    FileController controller(nullptr, nullptr);
    ActionRegistry actions;
    QMenu recentFilesMenu;
    actions.recentFilesMenu = &recentFilesMenu;
    connectFileActions(controller, actions);
    controller.populateRecentMenu();
    QVERIFY(!actions.recentFilesMenu->actions().isEmpty());
    actions.recentFilesMenu->actions().constLast()->trigger();

    QVERIFY(settings.getRecentFiles().isEmpty());
    QVERIFY(settings.getLastFile().isEmpty());
    QCOMPARE(settings.getLastDatabaseDirectory(), directory);
}

void FileControllerWorkspaceLifecycleTests::recentFilesPreserveUnicodePaths()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString rawPath = workspaceRoot.filePath(
        QString::fromUtf8(
            "\xED\x95\x99\xEA\xB5\x90-\xF0\x9F\x93\x9A"
            )
        );
    const QString normalizedPath = QFileInfo(
        rawPath + QStringLiteral(".tps")
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(normalizedPath));
    seedServices.closeDatabase();

    ApplicationServices services;
    FileController controller(&services, nullptr);
    controller.loadDatabaseOnStartup(rawPath);

    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        QStringList{normalizedPath}
        );
    QCOMPARE(SettingsManager::instance().getLastFile(), normalizedPath);
}

void FileControllerWorkspaceLifecycleTests::
startupUsesListFirstAndLastFileFallback()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString firstPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("list-first.tps"))
        ).absoluteFilePath();
    const QString fallbackPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("last-file-fallback.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(firstPath));
    seedServices.closeDatabase();
    QVERIFY(seedServices.openDatabase(fallbackPath));
    seedServices.closeDatabase();

    SettingsManager& settings = SettingsManager::instance();
    settings.setRecentFiles({firstPath});
    settings.setLastFile(fallbackPath);

    {
        ApplicationServices services;
        FileController controller(&services, nullptr);
        controller.loadMostRecentDatabase();
        QVERIFY(services.hasOpenDatabase());
        QCOMPARE(services.currentDatabasePath(), firstPath);
    }

    settings.clearRecentFiles();
    settings.setLastFile(fallbackPath);

    ApplicationServices fallbackServices;
    FileController fallbackController(&fallbackServices, nullptr);
    fallbackController.loadMostRecentDatabase();
    QVERIFY(fallbackServices.hasOpenDatabase());
    QCOMPARE(fallbackServices.currentDatabasePath(), fallbackPath);
}

void FileControllerWorkspaceLifecycleTests::normalCreateReplacesExistingTarget()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString targetPath = workspaceRoot.filePath(
        QStringLiteral("existing-target.tps")
        );
    QVERIFY(writeFile(targetPath, QByteArrayLiteral("obsolete profile")));

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(targetPath)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);

    QVERIFY(controller.createNewDatabaseInteractive());

    const QString expectedPath = QFileInfo(targetPath).absoluteFilePath();
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), expectedPath);
    QVERIFY2(
        readFile(targetPath) != QByteArrayLiteral("obsolete profile"),
        "normal creation must remove an existing target before coordinator create"
        );
    QVERIFY(prompts.messages.isEmpty());
}

void FileControllerWorkspaceLifecycleTests::createDoesNotPrepareAfterCloseFailure()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString existingPath = workspaceRoot.filePath(
        QStringLiteral("currently-open.tps")
        );
    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(existingPath));
    seedServices.closeDatabase();

    FakeFileDialogService fileDialogs;
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);
    controller.loadDatabaseOnStartup(existingPath);
    QVERIFY(services.hasOpenDatabase());

    const QString expectedExistingPath = QFileInfo(existingPath).absoluteFilePath();
    QCOMPARE(SettingsManager::instance().getLastFile(), expectedExistingPath);

    // Leave the coordinator session stale so its close postcondition fails.
    services.closeDatabase();

    const QString replacementPath = workspaceRoot.filePath(
        QStringLiteral("replacement.tps")
        );
    QVERIFY(writeFile(replacementPath, QByteArrayLiteral("must survive")));
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(replacementPath)
        );

    QVERIFY(!controller.createNewDatabaseInteractive());
    QCOMPARE(readFile(replacementPath), QByteArrayLiteral("must survive"));
    QVERIFY(!services.hasOpenDatabase());
    QCOMPARE(SettingsManager::instance().getLastFile(), expectedExistingPath);
    QVERIFY(prompts.messages.isEmpty());

    const QString initialSetupPath = workspaceRoot.filePath(
        QStringLiteral("initial-setup.tps")
        );
    QVERIFY(writeFile(initialSetupPath, QByteArrayLiteral("must also survive")));
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(initialSetupPath)
        );

    QVERIFY(!controller.createInitialSetupDatabaseInteractive());
    QCOMPARE(readFile(initialSetupPath), QByteArrayLiteral("must also survive"));
    QVERIFY(initialSetupBackups(workspaceRoot, QStringLiteral("initial-setup.tps")).isEmpty());
    QVERIFY(prompts.messages.isEmpty());
}

void FileControllerWorkspaceLifecycleTests::initialSetupBackupIsRemovedOnFinish()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString targetPath = workspaceRoot.filePath(
        QStringLiteral("setup-target.tps")
        );
    const QByteArray originalContents = QByteArrayLiteral("original profile");
    QVERIFY(writeFile(targetPath, originalContents));

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(targetPath)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    ApplicationServices services;
    FileController controller(&services, nullptr);

    QVERIFY(controller.createInitialSetupDatabaseInteractive());
    const QString expectedPath = QFileInfo(targetPath).absoluteFilePath();
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), expectedPath);
    QCOMPARE(SettingsManager::instance().getRecentFiles(), QStringList());

    const QStringList backups = initialSetupBackups(
        workspaceRoot,
        QStringLiteral("setup-target.tps")
        );
    QCOMPARE(backups.size(), 1);
    QCOMPARE(
        readFile(workspaceRoot.filePath(backups.constFirst())),
        originalContents
        );

    controller.finishInitialSetup();

    QVERIFY(initialSetupBackups(workspaceRoot, QStringLiteral("setup-target.tps")).isEmpty());
    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        QStringList{expectedPath}
        );
    QCOMPARE(SettingsManager::instance().getLastFile(), expectedPath);
}

void FileControllerWorkspaceLifecycleTests::initialSetupBackupIsRestoredOnCancel()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString targetPath = workspaceRoot.filePath(
        QStringLiteral("cancel-target.tps")
        );
    const QByteArray originalContents = QByteArrayLiteral("restore this profile");
    QVERIFY(writeFile(targetPath, originalContents));

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(targetPath)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    ApplicationServices services;
    FileController controller(&services, nullptr);

    QVERIFY(controller.createInitialSetupDatabaseInteractive());
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(
        initialSetupBackups(workspaceRoot, QStringLiteral("cancel-target.tps")).size(),
        1
        );

    controller.cancelInitialSetup();

    QVERIFY(!services.hasOpenDatabase());
    QCOMPARE(readFile(targetPath), originalContents);
    QVERIFY(initialSetupBackups(workspaceRoot, QStringLiteral("cancel-target.tps")).isEmpty());
    QVERIFY(SettingsManager::instance().getRecentFiles().isEmpty());
    QVERIFY(SettingsManager::instance().getLastFile().isEmpty());
}

void FileControllerWorkspaceLifecycleTests::createErrorUsesStructuredUtf8Message()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString invalidPath = workspaceRoot.filePath(
        QStringLiteral("structured-error")
        ) + QChar::Null + QStringLiteral(".tps");

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(invalidPath)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);

    QVERIFY(!controller.createNewDatabaseInteractive());
    QVERIFY(!services.hasOpenDatabase());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        prompts.messages.constFirst().title,
        QStringLiteral("New Teacher Profile")
        );
    QCOMPARE(
        prompts.messages.constFirst().message,
        QStringLiteral("Opening the workspace returned an invalid path.")
        );
    QVERIFY(SettingsManager::instance().getRecentFiles().isEmpty());
}

void FileControllerWorkspaceLifecycleTests::successfulStartupLoadPersistsNormalizedPath()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());
    SettingsManager::instance().clear();

    const QString requestedPath = workspaceRoot.filePath(
        QStringLiteral("normalized-startup")
        );
    const QString normalizedPath = workspaceRoot.filePath(
        QStringLiteral("normalized-startup.tps")
        );

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(normalizedPath));
    seedServices.closeDatabase();

    ApplicationServices services;
    FileController controller(&services, nullptr);
    controller.loadDatabaseOnStartup(requestedPath);

    QCOMPARE(
        services.currentDatabasePath(),
        QFileInfo(normalizedPath).absoluteFilePath()
        );
    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        QStringList{QFileInfo(normalizedPath).absoluteFilePath()}
        );
    QCOMPARE(
        SettingsManager::instance().getLastFile(),
        QFileInfo(normalizedPath).absoluteFilePath()
        );
}

void FileControllerWorkspaceLifecycleTests::missingStartupPathUsesCapturedWarning()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());
    SettingsManager::instance().clear();

    const QString missingPath = workspaceRoot.filePath(
        QStringLiteral("missing-startup.tps")
        );

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);
    controller.loadDatabaseOnStartup(missingPath);

    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& request = prompts.messages.constFirst();
    QCOMPARE(request.title, QStringLiteral("Missing File"));
    QCOMPARE(
        request.message,
        QStringLiteral("File not found:\n%1")
            .arg(QFileInfo(missingPath).absoluteFilePath())
        );
    QCOMPARE(
        static_cast<int>(request.severity),
        static_cast<int>(PromptSeverity::Warning)
        );
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
}

void FileControllerWorkspaceLifecycleTests::invalidStartupDatabaseUsesLegacyErrorText()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());
    SettingsManager::instance().clear();

    const QString invalidPath = workspaceRoot.filePath(
        QStringLiteral("invalid-startup.tps")
        );
    QFile invalidFile(invalidPath);
    QVERIFY(invalidFile.open(QIODevice::WriteOnly));
    QVERIFY(invalidFile.write("not a SQLite database") > 0);
    invalidFile.close();

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);
    controller.loadDatabaseOnStartup(invalidPath);

    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& request = prompts.messages.constFirst();
    QCOMPARE(request.title, QStringLiteral("Open Teacher Profile"));
    QCOMPARE(
        static_cast<int>(request.severity),
        static_cast<int>(PromptSeverity::Warning)
        );
    QVERIFY2(
        !request.message.trimmed().isEmpty(),
        "the legacy open error text must be preserved"
        );
}

void FileControllerWorkspaceLifecycleTests::startupLoadUsesCoordinatorAndLegacyFallback()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString legacyPath = workspaceRoot.filePath(
        QStringLiteral("created-by-legacy-path.tps")
        );
    const QString firstPath = workspaceRoot.filePath(
        QStringLiteral("first-프로필.tps")
        );
    const QString secondPath = workspaceRoot.filePath(
        QStringLiteral("second-프로필.tps")
        );

    ApplicationServices seedServices;
    for (const QString& path : {legacyPath, firstPath, secondPath})
    {
        QVERIFY(seedServices.openDatabase(path));
        seedServices.closeDatabase();
    }

    ApplicationServices services;
    FileController controller(&services, nullptr);

    // Creation and initial setup still open through the legacy facade in this
    // slice. An already-open legacy service is the compatibility state that
    // the controller must close before entering the coordinator path.
    QVERIFY(services.openDatabase(legacyPath));
    controller.loadDatabaseOnStartup(firstPath);
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(
        services.currentDatabasePath(),
        QFileInfo(firstPath).absoluteFilePath()
        );

    // The next startup load closes the coordinator-owned session before
    // opening the replacement workspace.
    controller.loadDatabaseOnStartup(secondPath);
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(
        services.currentDatabasePath(),
        QFileInfo(secondPath).absoluteFilePath()
        );
}

void FileControllerWorkspaceLifecycleTests::autosaveUsesCoordinatorForOpenWorkspace()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = workspaceRoot.filePath(
        QStringLiteral("coordinator-save.tps")
        );

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(workspacePath));
    seedServices.closeDatabase();

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);
    controller.loadDatabaseOnStartup(workspacePath);
    QVERIFY(services.hasOpenDatabase());
    QVERIFY(prompts.messages.isEmpty());

    controller.autosave();

    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(
        services.currentDatabasePath(),
        QFileInfo(workspacePath).absoluteFilePath()
        );
}

void FileControllerWorkspaceLifecycleTests::autosaveWarnsAndPreservesStaleCoordinatorState()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString firstPath = workspaceRoot.filePath(
        QStringLiteral("coordinator-save-first.tps")
        );
    const QString secondPath = workspaceRoot.filePath(
        QStringLiteral("coordinator-save-second.tps")
        );

    ApplicationServices seedServices;
    for (const QString& path : {firstPath, secondPath})
    {
        QVERIFY(seedServices.openDatabase(path));
        seedServices.closeDatabase();
    }

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);
    controller.loadDatabaseOnStartup(firstPath);
    QVERIFY(services.hasOpenDatabase());

    // Keep the coordinator's session on the first path while the
    // compatibility service is externally moved to a different workspace.
    QVERIFY(services.openDatabase(secondPath));
    QCOMPARE(
        services.currentDatabasePath(),
        QFileInfo(secondPath).absoluteFilePath()
        );

    controller.autosave();

    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest firstWarning = prompts.messages.constFirst();
    QCOMPARE(firstWarning.title, QStringLiteral("Save Teacher Profile"));
    QCOMPARE(
        firstWarning.message,
        QStringLiteral(
            "Saving the workspace received a handle for a different open workspace."
            )
        );
    QCOMPARE(
        static_cast<int>(firstWarning.severity),
        static_cast<int>(PromptSeverity::Warning)
        );
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(
        services.currentDatabasePath(),
        QFileInfo(secondPath).absoluteFilePath()
        );

    // A second failure proves the v2 session and compatibility/UI state were
    // not cleared or replaced after the first structured failure.
    controller.autosave();
    QCOMPARE(prompts.messages.size(), 2);
    QCOMPARE(
        prompts.messages.constLast().message,
        firstWarning.message
        );
}

void FileControllerWorkspaceLifecycleTests::autosaveUsesLegacyFallbackForCompatibilityWorkspace()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = workspaceRoot.filePath(
        QStringLiteral("legacy-save.tps")
        );

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(workspacePath));
    seedServices.closeDatabase();

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    // This compatibility path is open before FileController can create a v2
    // session, so autosave must retain the historical void service call.
    QVERIFY(services.openDatabase(workspacePath));
    FileController controller(&services, nullptr);

    controller.autosave();

    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(
        services.currentDatabasePath(),
        QFileInfo(workspacePath).absoluteFilePath()
        );
}

void FileControllerWorkspaceLifecycleTests::saveAsUsesCoordinatorForOpenWorkspace()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString sourcePath = workspaceRoot.filePath(
        QStringLiteral("coordinator-save-as-source.tps")
        );
    const QString requestedDestination = workspaceRoot.filePath(
        QStringLiteral("coordinator-save-as-destination")
        );
    const QString destinationPath = QFileInfo(
        requestedDestination + QStringLiteral(".tps")
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(sourcePath));
    seedServices.closeDatabase();

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(requestedDestination)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);
    ActionRegistry actions;
    connectFileActions(controller, actions);

    controller.loadDatabaseOnStartup(sourcePath);
    const QString normalizedSourcePath = QFileInfo(sourcePath).absoluteFilePath();
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(SettingsManager::instance().getLastFile(), normalizedSourcePath);
    QVERIFY(actions.saveFile->isEnabled());
    QVERIFY(actions.closeFile->isEnabled());

    actions.saveAsFile->trigger();

    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(QFileInfo::exists(destinationPath));
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), destinationPath);
    const QStringList expectedRecentFiles{
        destinationPath,
        normalizedSourcePath
    };
    QCOMPARE(SettingsManager::instance().getRecentFiles(), expectedRecentFiles);
    QCOMPARE(SettingsManager::instance().getLastFile(), destinationPath);
    QVERIFY(actions.saveFile->isEnabled());
    QVERIFY(actions.closeFile->isEnabled());
    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    QCOMPARE(
        fileDialogs.saveFileRequests.constFirst().title,
        QStringLiteral("Save Teacher Profile")
        );
    QCOMPARE(
        fileDialogs.saveFileRequests.constFirst().defaultSuffix,
        QStringLiteral("tps")
        );

    // The coordinator session must now use the returned save-as location;
    // saving again proves the controller did not need to reopen it through
    // loadDatabase.
    controller.autosave();
    QVERIFY(prompts.messages.isEmpty());
}

void FileControllerWorkspaceLifecycleTests::saveAsWarnsAndPreservesStaleCoordinatorState()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString firstPath = workspaceRoot.filePath(
        QStringLiteral("coordinator-save-as-first.tps")
        );
    const QString secondPath = workspaceRoot.filePath(
        QStringLiteral("coordinator-save-as-second.tps")
        );
    const QString failedDestination = workspaceRoot.filePath(
        QStringLiteral("coordinator-save-as-failed.tps")
        );

    ApplicationServices seedServices;
    for (const QString& path : {firstPath, secondPath})
    {
        QVERIFY(seedServices.openDatabase(path));
        seedServices.closeDatabase();
    }

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(failedDestination)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);
    ActionRegistry actions;
    connectFileActions(controller, actions);

    controller.loadDatabaseOnStartup(firstPath);
    const QString normalizedFirstPath = QFileInfo(firstPath).absoluteFilePath();
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(SettingsManager::instance().getLastFile(), normalizedFirstPath);

    // Leave the coordinator's session on the first path while the
    // compatibility service is externally moved to a different workspace.
    QVERIFY(services.openDatabase(secondPath));
    QCOMPARE(
        services.currentDatabasePath(),
        QFileInfo(secondPath).absoluteFilePath()
        );

    actions.saveAsFile->trigger();

    QVERIFY(!QFileInfo::exists(failedDestination));
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        prompts.messages.constFirst().title,
        QStringLiteral("Save Teacher Profile")
        );
    QCOMPARE(
        prompts.messages.constFirst().message,
        QStringLiteral(
            "Saving the workspace as received a handle for a different open workspace."
            )
        );
    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        QStringList{normalizedFirstPath}
        );
    QCOMPARE(SettingsManager::instance().getLastFile(), normalizedFirstPath);
    QVERIFY(actions.saveFile->isEnabled());
    QVERIFY(actions.closeFile->isEnabled());

    // Re-align the compatibility service and prove the failed coordinator
    // call left its session and controller state available for a later save.
    QVERIFY(services.openDatabase(firstPath));
    const QString recoveredDestination = workspaceRoot.filePath(
        QStringLiteral("coordinator-save-as-recovered.tps")
        );
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(recoveredDestination)
        );

    actions.saveAsFile->trigger();

    QVERIFY(QFileInfo::exists(recoveredDestination));
    QCOMPARE(prompts.messages.size(), 1);
    const QString recoveredPath = QFileInfo(recoveredDestination).absoluteFilePath();
    const QStringList expectedRecentFiles{
        recoveredPath,
        normalizedFirstPath
    };
    QCOMPARE(SettingsManager::instance().getRecentFiles(), expectedRecentFiles);
    QCOMPARE(
        SettingsManager::instance().getLastFile(),
        recoveredPath
        );
}

void FileControllerWorkspaceLifecycleTests::saveAsUsesLegacyFallbackForCompatibilityWorkspace()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString sourcePath = workspaceRoot.filePath(
        QStringLiteral("legacy-save-as-source.tps")
        );
    const QString requestedDestination = workspaceRoot.filePath(
        QStringLiteral("legacy-save-as-destination")
        );
    const QString destinationPath = QFileInfo(
        requestedDestination + QStringLiteral(".tps")
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(sourcePath));
    seedServices.closeDatabase();

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(requestedDestination)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    // This compatibility path is open before FileController can create a v2
    // session, so save-as must retain the legacy call followed by loadDatabase.
    QVERIFY(services.openDatabase(sourcePath));
    FileController controller(&services, nullptr);
    ActionRegistry actions;
    connectFileActions(controller, actions);

    actions.saveAsFile->trigger();

    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(QFileInfo::exists(destinationPath));
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), destinationPath);
    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        QStringList{destinationPath}
        );
    QCOMPARE(SettingsManager::instance().getLastFile(), destinationPath);
    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    QCOMPARE(
        fileDialogs.saveFileRequests.constFirst().title,
        QStringLiteral("Save Teacher Profile")
        );
}

void FileControllerWorkspaceLifecycleTests::exportUsesCoordinatorForOpenWorkspaceAndPreservesState()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString sourcePath = workspaceRoot.filePath(
        QStringLiteral("coordinator-export-source-내보내기.tps")
        );
    const QString firstRequestedDestination = workspaceRoot.filePath(
        QStringLiteral("exports/first-export-내보내기")
        );
    const QString secondRequestedDestination = workspaceRoot.filePath(
        QStringLiteral("exports/second-export-내보내기")
        );
    const QString firstDestination = QFileInfo(
        firstRequestedDestination + QStringLiteral(".tps")
        ).absoluteFilePath();
    const QString secondDestination = QFileInfo(
        secondRequestedDestination + QStringLiteral(".tps")
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(sourcePath));
    seedServices.closeDatabase();

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(firstRequestedDestination)
        );
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(secondRequestedDestination)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);
    ActionRegistry actions;
    connectFileActions(controller, actions);

    controller.loadDatabaseOnStartup(sourcePath);
    const QString normalizedSourcePath = QFileInfo(sourcePath).absoluteFilePath();
    const QString sourceDirectory = QFileInfo(sourcePath).absoluteDir().absolutePath();
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), normalizedSourcePath);
    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        QStringList{normalizedSourcePath}
        );
    QCOMPARE(SettingsManager::instance().getLastFile(), normalizedSourcePath);
    QCOMPARE(
        SettingsManager::instance().getLastDatabaseDirectory(),
        sourceDirectory
        );
    QVERIFY(actions.saveFile->isEnabled());
    QVERIFY(actions.closeFile->isEnabled());

    actions.exportAsFile->trigger();

    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(QFileInfo::exists(firstDestination));
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), normalizedSourcePath);
    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        QStringList{normalizedSourcePath}
        );
    QCOMPARE(SettingsManager::instance().getLastFile(), normalizedSourcePath);
    QCOMPARE(
        SettingsManager::instance().getLastDatabaseDirectory(),
        QFileInfo(firstDestination).absoluteDir().absolutePath()
        );
    QVERIFY(actions.saveFile->isEnabled());
    QVERIFY(actions.closeFile->isEnabled());
    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    const SaveFileRequest firstRequest = fileDialogs.saveFileRequests.constFirst();
    QCOMPARE(firstRequest.title, QStringLiteral("Export Teacher Profile As"));
    QCOMPARE(
        static_cast<int>(firstRequest.purpose),
        static_cast<int>(FileDialogPurpose::TeacherProfile)
        );
    QCOMPARE(firstRequest.initialDirectory, sourceDirectory);
    QCOMPARE(firstRequest.defaultSuffix, QStringLiteral("tps"));
    QCOMPARE(
        firstRequest.nameFilters,
        QStringList{QStringLiteral("ClassMngr Teacher Profile (*.tps)")}
        );

    // The second dialog still follows the active source workspace, proving
    // export did not replace m_currentFile with the returned destination.
    actions.exportAsFile->trigger();

    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(QFileInfo::exists(secondDestination));
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), normalizedSourcePath);
    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        QStringList{normalizedSourcePath}
        );
    QCOMPARE(SettingsManager::instance().getLastFile(), normalizedSourcePath);
    QCOMPARE(fileDialogs.saveFileRequests.size(), 2);
    QCOMPARE(
        fileDialogs.saveFileRequests.at(1).initialDirectory,
        sourceDirectory
        );

    controller.autosave();
    QVERIFY(prompts.messages.isEmpty());
    QCOMPARE(services.currentDatabasePath(), normalizedSourcePath);
}

void FileControllerWorkspaceLifecycleTests::exportWarnsAndPreservesStaleCoordinatorState()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString firstPath = workspaceRoot.filePath(
        QStringLiteral("coordinator-export-first-내보내기.tps")
        );
    const QString secondPath = workspaceRoot.filePath(
        QStringLiteral("coordinator-export-second-내보내기.tps")
        );
    const QString firstFailedDestination = workspaceRoot.filePath(
        QStringLiteral("exports/failed-export-one")
        );
    const QString secondFailedDestination = workspaceRoot.filePath(
        QStringLiteral("exports/failed-export-two")
        );

    ApplicationServices seedServices;
    for (const QString& path : {firstPath, secondPath})
    {
        QVERIFY(seedServices.openDatabase(path));
        seedServices.closeDatabase();
    }

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(firstFailedDestination)
        );
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(secondFailedDestination)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);
    ActionRegistry actions;
    connectFileActions(controller, actions);

    controller.loadDatabaseOnStartup(firstPath);
    const QString normalizedFirstPath = QFileInfo(firstPath).absoluteFilePath();
    const QString normalizedSecondPath = QFileInfo(secondPath).absoluteFilePath();
    const QString firstDirectory = QFileInfo(firstPath).absoluteDir().absolutePath();
    QVERIFY(services.hasOpenDatabase());
    QVERIFY(services.openDatabase(secondPath));
    QCOMPARE(services.currentDatabasePath(), normalizedSecondPath);

    actions.exportAsFile->trigger();

    QVERIFY(!QFileInfo::exists(firstFailedDestination + QStringLiteral(".tps")));
    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest firstWarning = prompts.messages.constFirst();
    QCOMPARE(firstWarning.title, QStringLiteral("Export Teacher Profile"));
    QCOMPARE(
        firstWarning.message,
        QStringLiteral(
            "Exporting the workspace received a handle for a different open workspace."
            )
        );
    QCOMPARE(
        static_cast<int>(firstWarning.severity),
        static_cast<int>(PromptSeverity::Warning)
        );
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), normalizedSecondPath);
    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        QStringList{normalizedFirstPath}
        );
    QCOMPARE(SettingsManager::instance().getLastFile(), normalizedFirstPath);
    QCOMPARE(
        SettingsManager::instance().getLastDatabaseDirectory(),
        firstDirectory
        );
    QVERIFY(actions.saveFile->isEnabled());
    QVERIFY(actions.closeFile->isEnabled());

    // A repeated failure proves the coordinator session and controller state
    // survived the structured export error.
    actions.exportAsFile->trigger();

    QCOMPARE(prompts.messages.size(), 2);
    QCOMPARE(prompts.messages.constLast().title, firstWarning.title);
    QCOMPARE(prompts.messages.constLast().message, firstWarning.message);
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), normalizedSecondPath);
    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        QStringList{normalizedFirstPath}
        );
    QCOMPARE(SettingsManager::instance().getLastFile(), normalizedFirstPath);
    QCOMPARE(
        SettingsManager::instance().getLastDatabaseDirectory(),
        firstDirectory
        );
}

void FileControllerWorkspaceLifecycleTests::exportUsesLegacyFallbackForCompatibilityWorkspace()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString sourcePath = workspaceRoot.filePath(
        QStringLiteral("legacy-export-source-내보내기.tps")
        );
    const QString requestedDestination = workspaceRoot.filePath(
        QStringLiteral("exports/legacy-export-destination-내보내기")
        );
    const QString destinationPath = QFileInfo(
        requestedDestination + QStringLiteral(".tps")
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(sourcePath));
    seedServices.closeDatabase();

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(requestedDestination)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    // This compatibility path is open before FileController can create a v2
    // session, so export must retain the historical service call.
    QVERIFY(services.openDatabase(sourcePath));
    FileController controller(&services, nullptr);
    ActionRegistry actions;
    connectFileActions(controller, actions);

    actions.exportAsFile->trigger();

    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(QFileInfo::exists(destinationPath));
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), QFileInfo(sourcePath).absoluteFilePath());
    QVERIFY(SettingsManager::instance().getRecentFiles().isEmpty());
    QVERIFY(SettingsManager::instance().getLastFile().isEmpty());
    QCOMPARE(
        SettingsManager::instance().getLastDatabaseDirectory(),
        QFileInfo(destinationPath).absoluteDir().absolutePath()
        );
    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    QCOMPARE(
        fileDialogs.saveFileRequests.constFirst().title,
        QStringLiteral("Export Teacher Profile As")
        );
    QCOMPARE(
        static_cast<int>(fileDialogs.saveFileRequests.constFirst().purpose),
        static_cast<int>(FileDialogPurpose::TeacherProfile)
        );
}

void FileControllerWorkspaceLifecycleTests::exportPreservesDialogPolicyAndNoOpenGuard()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString sourcePath = workspaceRoot.filePath(
        QStringLiteral("dialog-policy-source.tps")
        );
    const QString firstDestination = workspaceRoot.filePath(
        QStringLiteral("dialog-policy-export")
        );
    const QString secondDestination = workspaceRoot.filePath(
        QStringLiteral("dialog-policy-no-open")
        );

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(sourcePath));
    seedServices.closeDatabase();

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(firstDestination)
        );
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(secondDestination)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    QVERIFY(services.openDatabase(sourcePath));
    FileController controller(&services, nullptr);
    ActionRegistry actions;
    connectFileActions(controller, actions);

    actions.exportAsFile->trigger();

    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(QFileInfo::exists(firstDestination + QStringLiteral(".tps")));
    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    QCOMPARE(
        fileDialogs.saveFileRequests.constFirst().title,
        QStringLiteral("Export Teacher Profile As")
        );
    QCOMPARE(
        fileDialogs.saveFileRequests.constFirst().defaultSuffix,
        QStringLiteral("tps")
        );

    // The public action keeps the existing no-open guard and must not even
    // consume a queued selection once the compatibility service is closed.
    services.closeDatabase();
    actions.exportAsFile->trigger();

    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    QVERIFY(!QFileInfo::exists(secondDestination + QStringLiteral(".tps")));
    QVERIFY(prompts.messages.isEmpty());
}

void FileControllerWorkspaceLifecycleTests::saveAsMigratesV2AndExportUsesCoordinator()
{
    const QString source = fileControllerSource();
    QVERIFY(!source.isEmpty());
    QVERIFY(source.contains(QStringLiteral("m_workspaceCoordinator->saveWorkspace()")));
    QVERIFY(source.contains(QStringLiteral("m_services->saveDatabase();")));
    QVERIFY(source.contains(QStringLiteral("m_workspaceCoordinator->saveWorkspaceAs(")));
    QVERIFY(source.contains(QStringLiteral("m_services->saveDatabaseAs(normalized);")));
    QVERIFY(source.contains(QStringLiteral("m_workspaceCoordinator->exportWorkspace(")));
    QVERIFY(source.contains(QStringLiteral("m_services->exportDatabaseAs(normalized);")));
    QVERIFY(source.contains(QStringLiteral("domainErrorMessage(exported.error())")));
    QVERIFY(source.contains(QStringLiteral("rememberDatabaseDirectory(returnedDestination)")));
}

QTEST_MAIN(FileControllerWorkspaceLifecycleTests)

#include "file_controller_workspace_lifecycle_tests.moc"
