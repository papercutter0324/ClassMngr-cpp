#include "app/controllers/file_controller.h"
#include "core/application_services.h"
#include "core/settingsmanager.h"
#include "fakes/fake_file_dialog_service.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/file_dialog_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
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
}

class FileControllerWorkspaceLifecycleTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void nullServicesAreSafe();
    void normalCreateUsesCoordinatorAndUpdatesRecent();
    void normalCreateReplacesExistingTarget();
    void createDoesNotPrepareAfterCloseFailure();
    void initialSetupBackupIsRemovedOnFinish();
    void initialSetupBackupIsRestoredOnCancel();
    void createErrorUsesStructuredUtf8Message();
    void successfulStartupLoadPersistsNormalizedPath();
    void missingStartupPathUsesCapturedWarning();
    void invalidStartupDatabaseUsesLegacyErrorText();
    void startupLoadUsesCoordinatorAndLegacyFallback();

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

QTEST_MAIN(FileControllerWorkspaceLifecycleTests)

#include "file_controller_workspace_lifecycle_tests.moc"
