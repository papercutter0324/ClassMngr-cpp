#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "data/database/database_session.h"
#include "next/platform/application_services_workspace_port.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <string>

using namespace ClassMngr::Next::Platform;

namespace
{

std::string utf8(const QString& value)
{
    return value.toUtf8().toStdString();
}

std::string normalizedPath(const QString& value)
{
    return utf8(QFileInfo(value).absoluteFilePath());
}

void writeInvalidWorkspace(
    const QString& path,
    const QByteArray& contents
    )
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(contents), contents.size());
    file.close();
    QVERIFY(!file.isOpen());
}

bool createAndCloseWorkspace(
    ApplicationServicesWorkspacePort& port,
    const std::string& path
    )
{
    const auto created = port.createOrOpenDatabase(path);
    if (!created)
    {
        return false;
    }

    return static_cast<bool>(port.closeDatabase(created.value()));
}

class CurrentDirectoryRestore final
{
public:
    CurrentDirectoryRestore()
        : m_directory(QDir::currentPath())
    {
    }

    ~CurrentDirectoryRestore()
    {
        QDir::setCurrent(m_directory);
    }

private:
    QString m_directory;
};

} // namespace

class NextPlatformApplicationServicesWorkspacePortTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void openAcceptsUtf8PathAndNormalizesHandle();
    void existingOnlyServiceOpenDoesNotCreateMissingTargetOrParent();
    void missingWorkspaceOpenIsNotFoundAndPreservesActiveSession();
    void unicodeWhitespaceAndMalformedPathsAreRejectedBeforeServiceEffects();
    void openPreservesNormalUnicodePathBytes();
    void existingOnlyOpenAcceptsSpecialCharacterPath();
    void existingOnlyUriTreatsQuestionMarkAsPathData();
    void existingOnlyOpenCreatesSchemaMigrationBackupAtFilesystemPath();
    void createOrOpenCreatesANewWorkspace();
    void existingInvalidFileIsNotSilentlyReplaced();
    void closeClosesTheOpenWorkspace();
    void voidSaveSucceedsOpenAndFailsClosed();
    void saveAsPreservesIdentityAndReopensNormalizedDestination();
    void exportReturnsNormalizedDestinationWithoutChangingCurrentPath();
    void openPreservesLegacyErrorMessage();
    void portsUseIndependentServicesWithoutSessionSnapshots();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesWorkspacePortTests::initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesWorkspacePortTests::openAcceptsUtf8PathAndNormalizesHandle()
{
    ApplicationServices services;
    ApplicationServicesWorkspacePort port(services);
    const QString requested = m_directory.filePath(
        QStringLiteral("nested/수업 프로필.tps")
        );
    const std::string expected = normalizedPath(requested);

    QVERIFY(createAndCloseWorkspace(port, utf8(requested)));

    const auto result = port.openDatabase(utf8(requested));

    QVERIFY(result);
    QCOMPARE(result.value().workspaceId(), expected);
    QCOMPARE(result.value().location(), expected);
    QCOMPARE(port.currentDatabasePath(), expected);
    QVERIFY(services.hasOpenDatabase());
    QVERIFY(QFile::exists(QString::fromUtf8(expected.c_str())));

    services.closeDatabase();
}

void NextPlatformApplicationServicesWorkspacePortTests::
existingOnlyServiceOpenDoesNotCreateMissingTargetOrParent()
{
    ApplicationServices services;
    const QString activePath = m_directory.filePath(
        QStringLiteral("existing-only-active.tps")
        );
    QVERIFY(services.openDatabase(activePath));
    const QString currentPathBefore = services.currentDatabasePath();
    const QString connectionNameBefore =
        services.databaseSession()->database().connectionName();

    const QString missingPath = m_directory.filePath(
        QStringLiteral("existing-only-missing-parent/workspace.tps")
        );
    const QString missingParent = QFileInfo(missingPath).absolutePath();
    QVERIFY(!QFileInfo::exists(missingParent));
    QVERIFY(!QFileInfo::exists(missingPath));

    const Status opened = services.openExistingDatabase(missingPath);

    QVERIFY(!opened);
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), currentPathBefore);
    QCOMPARE(
        services.databaseSession()->database().connectionName(),
        connectionNameBefore
        );
    QVERIFY(!QFileInfo::exists(missingPath));
    QVERIFY(!QFileInfo::exists(missingParent));

    services.closeDatabase();
}

void NextPlatformApplicationServicesWorkspacePortTests::
missingWorkspaceOpenIsNotFoundAndPreservesActiveSession()
{
    ApplicationServices services;
    ApplicationServicesWorkspacePort port(services);
    const QString activePath = m_directory.filePath(
        QStringLiteral("open-existing/active.tps")
        );
    QVERIFY(createAndCloseWorkspace(port, utf8(activePath)));

    const auto active = port.openDatabase(utf8(activePath));
    QVERIFY(active);
    const QString currentPathBefore = services.currentDatabasePath();
    const QString connectionNameBefore =
        services.databaseSession()->database().connectionName();
    DatabaseSession* const sessionBefore = services.databaseSession();

    const QString missingPath = m_directory.filePath(
        QStringLiteral("missing-parent/workspace.tps")
        );
    const QString missingParent = QFileInfo(missingPath).absolutePath();
    QVERIFY(!QFileInfo::exists(missingParent));
    QVERIFY(!QFileInfo::exists(missingPath));

    const auto missing = port.openDatabase(utf8(missingPath));

    QVERIFY(!missing);
    QCOMPARE(missing.error().code, LegacyWorkspaceError::Code::NotFound);
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.databaseSession(), sessionBefore);
    QCOMPARE(services.currentDatabasePath(), currentPathBefore);
    QCOMPARE(
        services.databaseSession()->database().connectionName(),
        connectionNameBefore
        );
    QVERIFY(!QFileInfo::exists(missingPath));
    QVERIFY(!QFileInfo::exists(missingParent));

    services.closeDatabase();
}

void NextPlatformApplicationServicesWorkspacePortTests::
existingOnlyOpenAcceptsSpecialCharacterPath()
{
    ApplicationServices services;
    ApplicationServicesWorkspacePort port(services);
#if defined(Q_OS_WIN)
    // Win32 reserves '?' in file-name components; cover # and % in an actual
    // file here, while the URI construction still percent-encodes '?'.
    const QString path = m_directory.filePath(
        QString::fromUtf8(
            "existing-\xED\x95\x9C\xEA\xB8\x80 space # and 100% profile.tps"
            )
        );
#else
    const QString path = m_directory.filePath(
        QString::fromUtf8(
            "existing-\xED\x95\x9C\xEA\xB8\x80 ? # and 100% profile.tps"
            )
        );
#endif
    const std::string pathUtf8 = utf8(path);
    QVERIFY(createAndCloseWorkspace(port, pathUtf8));

    const auto opened = port.openDatabase(pathUtf8);

    QVERIFY(opened);
    QCOMPARE(opened.value().location(), normalizedPath(path));
    QCOMPARE(port.currentDatabasePath(), normalizedPath(path));
    QVERIFY(services.hasOpenDatabase());
    services.closeDatabase();
}

void NextPlatformApplicationServicesWorkspacePortTests::
existingOnlyUriTreatsQuestionMarkAsPathData()
{
    ApplicationServices services;
    const QString existingPath = m_directory.filePath(
        QStringLiteral("uri-question-mark-prefix.tps")
        );
    QVERIFY(services.openDatabase(existingPath));
    const QString currentPathBefore = services.currentDatabasePath();

    // On Windows '?' cannot be part of a normal file name. This missing path
    // still verifies that URI serialization encodes it as path data: an
    // unescaped '?' would turn cache=shared into a SQLite URI option and open
    // the existing prefix file instead.
    const QString questionMarkPath =
        existingPath + QStringLiteral("?cache=shared");
    const Status opened = services.openExistingDatabase(questionMarkPath);

    QVERIFY(!opened);
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), currentPathBefore);
    QVERIFY(QFileInfo::exists(existingPath));
    QVERIFY(!QFileInfo::exists(questionMarkPath));

    services.closeDatabase();
}

void NextPlatformApplicationServicesWorkspacePortTests::
existingOnlyOpenCreatesSchemaMigrationBackupAtFilesystemPath()
{
    ApplicationServices services;
    ApplicationServicesWorkspacePort port(services);
    const QString databasePath = m_directory.filePath(
        QStringLiteral("existing-only-schema-v5/profile.tps")
        );
    const QString backupPath = databasePath
        + QStringLiteral(".pre-schema-v6-backup");
    QVERIFY(createAndCloseWorkspace(port, utf8(databasePath)));
    QVERIFY(!QFileInfo::exists(backupPath));

    const QString legacyConnectionName =
        QStringLiteral("workspace-schema-v5-observer");
    {
        QSqlDatabase legacyDatabase = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            legacyConnectionName
            );
        legacyDatabase.setDatabaseName(databasePath);
        QVERIFY(legacyDatabase.open());

        QSqlQuery versionQuery(legacyDatabase);
        QVERIFY(versionQuery.exec(QStringLiteral("PRAGMA user_version = 5")));
        legacyDatabase.close();
    }
    QSqlDatabase::removeDatabase(legacyConnectionName);

    const auto opened = port.openDatabase(utf8(databasePath));

    QVERIFY(opened);
    QVERIFY(QFileInfo::exists(backupPath));
    QCOMPARE(port.currentDatabasePath(), normalizedPath(databasePath));
    services.closeDatabase();
}

void NextPlatformApplicationServicesWorkspacePortTests::
unicodeWhitespaceAndMalformedPathsAreRejectedBeforeServiceEffects()
{
    QTemporaryDir isolatedWorkingDirectory;
    QVERIFY(isolatedWorkingDirectory.isValid());
    CurrentDirectoryRestore restoreCurrentDirectory;
    QVERIFY(QDir::setCurrent(isolatedWorkingDirectory.path()));

    ApplicationServices services;
    ApplicationServicesWorkspacePort port(services);
    const std::string noBreakSpace("\xC2\xA0");
    const std::string ideographicSpace("\xE3\x80\x80");

    for (const std::string& whitespace : {noBreakSpace, ideographicSpace})
    {
        const auto result = port.openDatabase(whitespace);

        QVERIFY(!result);
        QCOMPARE(
            result.error().message,
            std::string(
                "Opening the workspace path must not be empty or whitespace-only."
                )
            );
        QVERIFY(!services.hasOpenDatabase());
        QVERIFY(services.currentDatabasePath().isEmpty());
        QVERIFY(QDir::current().entryList(
            QDir::Files | QDir::NoDotAndDotDot
            ).isEmpty());
    }

    const std::string malformedUtf8{
        static_cast<char>(0xF0),
        static_cast<char>(0x28),
        static_cast<char>(0x8C),
        static_cast<char>(0x28)
    };
    const auto malformedResult = port.openDatabase(malformedUtf8);

    QVERIFY(!malformedResult);
    QCOMPARE(
        malformedResult.error().message,
        std::string("Opening the workspace path is not valid UTF-8.")
        );
    QVERIFY(!services.hasOpenDatabase());
    QVERIFY(services.currentDatabasePath().isEmpty());
    QVERIFY(QDir::current().entryList(
        QDir::Files | QDir::NoDotAndDotDot
        ).isEmpty());
}

void NextPlatformApplicationServicesWorkspacePortTests::
openPreservesNormalUnicodePathBytes()
{
    ApplicationServices services;
    ApplicationServicesWorkspacePort port(services);
    const QString requested = m_directory.filePath(
        QString::fromUtf8("unicode-\xED\x95\x9C\xEA\xB8\x80.tps")
        );
    const std::string requestedUtf8 = utf8(requested);
    const std::string expected = normalizedPath(requested);

    QVERIFY(createAndCloseWorkspace(port, requestedUtf8));

    const auto result = port.openDatabase(requestedUtf8);

    QVERIFY(result);
    QCOMPARE(result.value().workspaceId(), expected);
    QCOMPARE(result.value().location(), expected);
    QCOMPARE(port.currentDatabasePath(), expected);
    QCOMPARE(utf8(services.currentDatabasePath()), expected);
    QVERIFY(services.hasOpenDatabase());
    QVERIFY(QFile::exists(QString::fromUtf8(expected.c_str())));

    services.closeDatabase();
}

void NextPlatformApplicationServicesWorkspacePortTests::createOrOpenCreatesANewWorkspace()
{
    ApplicationServices services;
    ApplicationServicesWorkspacePort port(services);
    const QString requested = m_directory.filePath(
        QStringLiteral("created/new-profile.tps")
        );

    QVERIFY(!QFile::exists(requested));
    const auto result = port.createOrOpenDatabase(utf8(requested));

    QVERIFY(result);
    QVERIFY(QFile::exists(requested));
    QCOMPARE(result.value().location(), normalizedPath(requested));
    QCOMPARE(port.currentDatabasePath(), normalizedPath(requested));
    QVERIFY(services.hasOpenDatabase());

    services.closeDatabase();
}

void NextPlatformApplicationServicesWorkspacePortTests::existingInvalidFileIsNotSilentlyReplaced()
{
    ApplicationServices services;
    ApplicationServicesWorkspacePort port(services);
    const QString path = m_directory.filePath(
        QStringLiteral("existing-invalid.tps")
        );
    const QByteArray contents("this is not a Teacher Profile database\n");
    writeInvalidWorkspace(path, contents);

    const auto result = port.createOrOpenDatabase(utf8(path));

    QVERIFY(!result);
    QVERIFY(!services.hasOpenDatabase());
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), contents);
}

void NextPlatformApplicationServicesWorkspacePortTests::closeClosesTheOpenWorkspace()
{
    ApplicationServices services;
    ApplicationServicesWorkspacePort port(services);
    const std::string path = utf8(
        m_directory.filePath(QStringLiteral("close.tps"))
        );
    QVERIFY(createAndCloseWorkspace(port, path));
    const auto opened = port.openDatabase(path);
    QVERIFY(opened);

    const auto closed = port.closeDatabase(opened.value());

    QVERIFY(closed);
    QVERIFY(!services.hasOpenDatabase());
    QVERIFY(!port.hasOpenDatabase());
    QVERIFY(port.currentDatabasePath().empty());
}

void NextPlatformApplicationServicesWorkspacePortTests::voidSaveSucceedsOpenAndFailsClosed()
{
    ApplicationServices services;
    ApplicationServicesWorkspacePort port(services);
    const QString databasePath =
        m_directory.filePath(QStringLiteral("save.tps"));
    QVERIFY(createAndCloseWorkspace(port, utf8(databasePath)));
    const auto opened = port.openDatabase(
        utf8(databasePath)
        );
    QVERIFY(opened);

    QSqlDatabase activeDatabase = services.databaseSession()->database();
    QVERIFY(activeDatabase.transaction());
    QVERIFY(services.settingsService()->save(
        QStringLiteral("workspace/save-commit"),
        QStringLiteral("committed")
        ));
    const auto saved = port.saveDatabase(opened.value());
    QVERIFY(saved);

    const QString observerConnection =
        QStringLiteral("workspace-save-observer");
    {
        QSqlDatabase observer = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            observerConnection
            );
        observer.setDatabaseName(databasePath);
        QVERIFY(observer.open());
        QSqlQuery query(observer);
        QVERIFY(query.exec(
            QStringLiteral(
                "SELECT value FROM app_settings WHERE key='workspace/save-commit'"
                )
            ));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("committed"));
        observer.close();
    }
    QSqlDatabase::removeDatabase(observerConnection);
    activeDatabase = QSqlDatabase();

    services.closeDatabase();
    const auto closedSave = port.saveDatabase(opened.value());

    QVERIFY(!closedSave);
    QCOMPARE(
        closedSave.error().message,
        std::string(
            "Saving the workspace reported success without an open workspace."
            )
        );
}

void NextPlatformApplicationServicesWorkspacePortTests::saveAsPreservesIdentityAndReopensNormalizedDestination()
{
    ApplicationServices services;
    ApplicationServicesWorkspacePort port(services);
    const QString sourcePath = m_directory.filePath(
        QStringLiteral("save-as/source.tps")
        );
    const QString destinationPath = m_directory.filePath(
        QStringLiteral("save-as/保存先.tps")
        );
    QVERIFY(createAndCloseWorkspace(port, utf8(sourcePath)));
    const auto opened = port.openDatabase(utf8(sourcePath));
    QVERIFY(opened);
    const std::string originalIdentity = opened.value().workspaceId();
    const std::string expectedDestination = normalizedPath(destinationPath);
    QVERIFY(services.settingsService()->save(
        QStringLiteral("workspace/save-as-content"),
        QStringLiteral("copied")
        ));

    const auto saved = port.saveDatabaseAs(
        opened.value(),
        utf8(destinationPath)
        );

    QVERIFY(saved);
    QCOMPARE(saved.value().workspaceId(), originalIdentity);
    QCOMPARE(saved.value().location(), expectedDestination);
    QCOMPARE(port.currentDatabasePath(), expectedDestination);
    QCOMPARE(utf8(services.currentDatabasePath()), expectedDestination);
    QVERIFY(services.hasOpenDatabase());
    QVERIFY(QFile::exists(destinationPath));
    const auto copiedValue = services.settingsService()->load(
        QStringLiteral("workspace/save-as-content")
        );
    QVERIFY(copiedValue);
    QCOMPARE(copiedValue->toString(), QStringLiteral("copied"));
}

void NextPlatformApplicationServicesWorkspacePortTests::exportReturnsNormalizedDestinationWithoutChangingCurrentPath()
{
    ApplicationServices services;
    ApplicationServicesWorkspacePort port(services);
    const QString sourcePath = m_directory.filePath(
        QStringLiteral("export/source.tps")
        );
    const QString destinationPath = m_directory.filePath(
        QStringLiteral("export/내보내기.tps")
        );
    QVERIFY(createAndCloseWorkspace(port, utf8(sourcePath)));
    const auto opened = port.openDatabase(utf8(sourcePath));
    QVERIFY(opened);
    const std::string currentPath = port.currentDatabasePath();
    const std::string expectedDestination = normalizedPath(destinationPath);
    QVERIFY(services.settingsService()->save(
        QStringLiteral("workspace/export-content"),
        QStringLiteral("exported")
        ));

    const auto exported = port.exportDatabaseAs(
        opened.value(),
        utf8(destinationPath)
        );

    QVERIFY(exported);
    QCOMPARE(exported.value(), expectedDestination);
    QCOMPARE(port.currentDatabasePath(), currentPath);
    QCOMPARE(utf8(services.currentDatabasePath()), currentPath);
    QVERIFY(QFile::exists(destinationPath));

    ApplicationServices exportedServices;
    QVERIFY(exportedServices.openDatabase(destinationPath));
    const auto exportedValue = exportedServices.settingsService()->load(
        QStringLiteral("workspace/export-content")
        );
    QVERIFY(exportedValue);
    QCOMPARE(exportedValue->toString(), QStringLiteral("exported"));
    QCOMPARE(port.currentDatabasePath(), currentPath);
}

void NextPlatformApplicationServicesWorkspacePortTests::openPreservesLegacyErrorMessage()
{
    const QString path = m_directory.filePath(
        QStringLiteral("legacy-open-error.tps")
        );
    writeInvalidWorkspace(path, QByteArray("not a database"));

    ApplicationServices legacyServices;
    const Status legacyStatus = legacyServices.openDatabase(path);
    QVERIFY(!legacyStatus);

    ApplicationServices services;
    ApplicationServicesWorkspacePort port(services);
    const auto result = port.openDatabase(utf8(path));

    QVERIFY(!result);
    QCOMPARE(result.error().message, utf8(legacyStatus.error()));
}

void NextPlatformApplicationServicesWorkspacePortTests::portsUseIndependentServicesWithoutSessionSnapshots()
{
    ApplicationServices firstServices;
    ApplicationServices secondServices;
    ApplicationServicesWorkspacePort firstPort(firstServices);
    ApplicationServicesWorkspacePort secondPort(secondServices);
    const QString firstPath = m_directory.filePath(
        QStringLiteral("independent/first.tps")
        );
    const QString secondPath = m_directory.filePath(
        QStringLiteral("independent/second.tps")
        );

    QVERIFY(createAndCloseWorkspace(firstPort, utf8(firstPath)));
    QVERIFY(createAndCloseWorkspace(secondPort, utf8(secondPath)));
    const auto first = firstPort.openDatabase(utf8(firstPath));
    const auto second = secondPort.openDatabase(utf8(secondPath));

    QVERIFY(first);
    QVERIFY(second);
    QCOMPARE(firstPort.currentDatabasePath(), normalizedPath(firstPath));
    QCOMPARE(secondPort.currentDatabasePath(), normalizedPath(secondPath));

    firstServices.closeDatabase();
    QVERIFY(!firstPort.hasOpenDatabase());
    QVERIFY(firstPort.currentDatabasePath().empty());
    QVERIFY(secondPort.hasOpenDatabase());
    QCOMPARE(secondPort.currentDatabasePath(), normalizedPath(secondPath));
}

QTEST_MAIN(NextPlatformApplicationServicesWorkspacePortTests)

#include "next_platform_application_services_workspace_port_tests.moc"
