#include "app/controllers/file_controller.h"
#include "core/application_services.h"
#include "core/settingsmanager.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class FileControllerWorkspaceLifecycleTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void nullServicesAreSafe();
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
    DialogServices::setUserPromptServiceForTesting(nullptr);
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
