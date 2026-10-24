#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "data/database/database_file_operations.h"
#include "data/database/database_session.h"
#include "next/platform/application_services_workspace_port.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <QVector>
#include <utility>
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

class SqliteConnection final
{
public:
    SqliteConnection()
        : m_name(
            QStringLiteral("f1441-%1")
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces))
            ),
          m_database(QSqlDatabase::addDatabase(
              QStringLiteral("QSQLITE"),
              m_name
              ))
    {
        m_database.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000"));
    }

    SqliteConnection(const SqliteConnection&) = delete;
    SqliteConnection& operator=(const SqliteConnection&) = delete;

    ~SqliteConnection()
    {
        m_database.close();
        m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_name);
    }

    [[nodiscard]] bool open(const QString& path)
    {
        m_database.setDatabaseName(path);
        return m_database.open();
    }

    [[nodiscard]] QSqlDatabase& database() noexcept
    {
        return m_database;
    }

private:
    QString m_name;
    QSqlDatabase m_database;
};

bool executeSql(
    QSqlDatabase& database,
    const QString& sql,
    QString* errorMessage = nullptr
    )
{
    QSqlQuery query(database);
    if (query.exec(sql))
    {
        return true;
    }

    if (errorMessage)
    {
        *errorMessage = query.lastError().text();
    }
    return false;
}

bool snapshotHasWalAndOpaqueRows(
    const QString& path,
    QString* errorMessage
    )
{
    SqliteConnection connection;
    if (!connection.open(path))
    {
        *errorMessage = connection.database().lastError().text();
        return false;
    }

    QSqlQuery integrityQuery(connection.database());
    if (
        !integrityQuery.exec(QStringLiteral("PRAGMA integrity_check"))
        || !integrityQuery.next()
        || integrityQuery.value(0).toString() != QStringLiteral("ok")
        )
    {
        *errorMessage = integrityQuery.lastError().text();
        return false;
    }
    integrityQuery.finish();

    QSqlQuery rowQuery(connection.database());
    if (!rowQuery.exec(QStringLiteral(
            "SELECT rowid, payload FROM opaque_profile_rows ORDER BY rowid"
            )))
    {
        *errorMessage = rowQuery.lastError().text();
        return false;
    }

    QVector<QPair<qlonglong, QString>> rows;
    while (rowQuery.next())
    {
        rows.append(qMakePair(
            rowQuery.value(0).toLongLong(),
            rowQuery.value(1).toString()
            ));
    }
    rowQuery.finish();

    const QVector<QPair<qlonglong, QString>> expectedRows{
        qMakePair(qlonglong(-17), QStringLiteral("negative rowid")),
        qMakePair(qlonglong(9001), QStringLiteral("opaque rowid"))
    };
    if (rows != expectedRows)
    {
        *errorMessage = QStringLiteral(
            "The snapshot did not preserve opaque rowids and contents."
            );
        return false;
    }

    QSqlQuery markerQuery(connection.database());
    if (
        !markerQuery.exec(QStringLiteral(
            "SELECT marker FROM committed_wal_markers"
            ))
        || !markerQuery.next()
        || markerQuery.value(0).toString()
            != QStringLiteral("committed-after-reader-snapshot")
        )
    {
        *errorMessage = markerQuery.lastError().text();
        return false;
    }
    markerQuery.finish();

    return true;
}

void verifyPopulatedWorkspace(
    ApplicationServices& services,
    const int teacherId,
    const QString& expectedEnglishName,
    const QString& expectedRoomNumber,
    const QString& expectedNotes,
    const QString& expectedSetting
    )
{
    const auto teacher = services.teacherService()->teacher(teacherId);
    QVERIFY(teacher);
    QCOMPARE(teacher->teacherEn, expectedEnglishName);
    QCOMPARE(teacher->roomNumber, expectedRoomNumber);
    QCOMPARE(teacher->notes, expectedNotes);

    const auto setting = services.settingsService()->load(
        QStringLiteral("workspace/file-compatibility")
        );
    QVERIFY(setting);
    QCOMPARE(setting->toString(), expectedSetting);
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
    void olderSchemasAreRejectedWithoutChangingFilesOrActiveSession_data();
    void olderSchemasAreRejectedWithoutChangingFilesOrActiveSession();
    void existingEmptyFilesAreNotInitialized();
    void currentSchemaDbFilesRemainSupported();
    void createOrOpenCreatesANewWorkspace();
    void existingInvalidFileIsNotSilentlyReplaced();
    void closeClosesTheOpenWorkspace();
    void voidSaveSucceedsOpenAndFailsClosed();
    void saveAsPreservesIdentityAndReopensNormalizedDestination();
    void exportReturnsNormalizedDestinationWithoutChangingCurrentPath();
    void populatedDbSaveAsAndExportRoundTripThroughExistingOpen();
    void walSaveAsAndExportPreserveCommittedRowsAndRowids();
    void snapshotFailurePreservesExistingDestinationSentinel();
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
olderSchemasAreRejectedWithoutChangingFilesOrActiveSession_data()
{
    QTest::addColumn<int>("version");
    QTest::addColumn<bool>("createHook");
    for (int version = 0; version < 6; ++version)
    {
        const QByteArray row = QByteArray::number(version);
        QTest::newRow((row + "-open-existing").constData()) << version << false;
        QTest::newRow((row + "-create-or-open-existing").constData()) << version << true;
    }
}

void NextPlatformApplicationServicesWorkspacePortTests::
olderSchemasAreRejectedWithoutChangingFilesOrActiveSession()
{
    QFETCH(int, version);
    QFETCH(bool, createHook);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    ApplicationServicesWorkspacePort port(services);
    const QString activePath = directory.filePath(QStringLiteral("current.tps"));
    QVERIFY(port.createOrOpenDatabase(utf8(activePath)));
    const QString activeConnection = services.databaseSession()->database().connectionName();
    const QString databasePath = directory.filePath(QStringLiteral("older-수업's.db"));
    const QString observerName = QStringLiteral("older-schema-observer");
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), observerName);
        database.setDatabaseName(databasePath);
        QVERIFY(database.open());
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral("CREATE TABLE legacy_data (value TEXT)")));
        QVERIFY(query.exec(QStringLiteral("INSERT INTO legacy_data VALUES ('Keep unchanged')")));
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version = %1").arg(version)));
        database.close();
    }
    QSqlDatabase::removeDatabase(observerName);
    QFile beforeFile(databasePath);
    QVERIFY(beforeFile.open(QIODevice::ReadOnly));
    const QByteArray before = beforeFile.readAll();
    beforeFile.close();
    const QStringList filesBefore = QDir(directory.path()).entryList(QDir::Files);

    const auto opened = createHook
        ? port.createOrOpenDatabase(utf8(databasePath))
        : port.openDatabase(utf8(databasePath));

    QVERIFY(!opened);
    const QString message = QString::fromStdString(opened.error().message);
    QVERIFY(message.contains(QStringLiteral("unsupported schema version %1").arg(version)));
    QVERIFY(message.contains(QStringLiteral("Migration from older Teacher Profiles is not available")));
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(port.currentDatabasePath(), normalizedPath(activePath));
    QCOMPARE(services.databaseSession()->database().connectionName(), activeConnection);
    QFile afterFile(databasePath);
    QVERIFY(afterFile.open(QIODevice::ReadOnly));
    QCOMPARE(afterFile.readAll(), before);
    QCOMPARE(QDir(directory.path()).entryList(QDir::Files), filesBefore);
    services.closeDatabase();
}

void NextPlatformApplicationServicesWorkspacePortTests::existingEmptyFilesAreNotInitialized()
{
    for (const bool createHook : {false, true})
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("empty.tps"));
        writeInvalidWorkspace(path, {});
        ApplicationServices services;
        ApplicationServicesWorkspacePort port(services);

        const auto opened = createHook
            ? port.createOrOpenDatabase(utf8(path))
            : port.openDatabase(utf8(path));

        QVERIFY(!opened);
        QVERIFY(opened.error().message.find("unsupported schema version 0") != std::string::npos);
        QVERIFY(!services.hasOpenDatabase());
        QCOMPARE(QFileInfo(path).size(), qint64(0));
        QCOMPARE(QDir(directory.path()).entryList(QDir::Files), QStringList{QStringLiteral("empty.tps")});
    }
}

void NextPlatformApplicationServicesWorkspacePortTests::currentSchemaDbFilesRemainSupported()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    ApplicationServicesWorkspacePort port(services);
    const QString path = directory.filePath(QStringLiteral("current-schema.db"));
    QVERIFY(createAndCloseWorkspace(port, utf8(path)));
    const auto opened = port.openDatabase(utf8(path));
    QVERIFY(opened);
    QCOMPARE(port.currentDatabasePath(), normalizedPath(path));
    QVERIFY(services.hasOpenDatabase());
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

void NextPlatformApplicationServicesWorkspacePortTests::
populatedDbSaveAsAndExportRoundTripThroughExistingOpen()
{
    ApplicationServices services;
    ApplicationServicesWorkspacePort port(services);
    const QString sourcePath = m_directory.filePath(
        QStringLiteral("populated-db/source-schema-v6.db")
        );
    const QString saveAsPath = m_directory.filePath(
        QStringLiteral("populated-db/save-as/保存先.db")
        );
    const QString exportPath = m_directory.filePath(
        QStringLiteral("populated-db/export/書き出し.db")
        );
    const std::string expectedSource = normalizedPath(sourcePath);
    const std::string expectedSaveAs = normalizedPath(saveAsPath);
    const std::string expectedExport = normalizedPath(exportPath);
    const QString expectedEnglishName =
        QStringLiteral("Fixture Teacher");
    const QString expectedRoomNumber = QStringLiteral("B-12");
    const QString expectedNotes = QStringLiteral("file compatibility fixture");
    const QString expectedSetting = QStringLiteral("schema-v6-db");

    const auto created = port.createOrOpenDatabase(utf8(sourcePath));
    QVERIFY(created);
    QCOMPARE(created.value().location(), expectedSource);
    QCOMPARE(created.value().workspaceId(), expectedSource);

    Teacher teacher;
    teacher.teacherEn = expectedEnglishName;
    teacher.roomNumber = expectedRoomNumber;
    teacher.notes = expectedNotes;
    const auto createdTeacher = services.teacherService()->create(teacher);
    QVERIFY(createdTeacher);
    QVERIFY(services.settingsService()->save(
        QStringLiteral("workspace/file-compatibility"),
        expectedSetting
        ));

    const auto savedAs = port.saveDatabaseAs(
        created.value(),
        utf8(saveAsPath)
        );

    QVERIFY(savedAs);
    QCOMPARE(savedAs.value().workspaceId(), created.value().workspaceId());
    QCOMPARE(savedAs.value().location(), expectedSaveAs);
    QCOMPARE(port.currentDatabasePath(), expectedSaveAs);
    QCOMPARE(utf8(services.currentDatabasePath()), expectedSaveAs);
    QVERIFY(services.hasOpenDatabase());
    QVERIFY(QFile::exists(saveAsPath));
    verifyPopulatedWorkspace(
        services,
        createdTeacher.value(),
        expectedEnglishName,
        expectedRoomNumber,
        expectedNotes,
        expectedSetting
        );

    QVERIFY(port.closeDatabase(savedAs.value()));
    const auto reopenedSaveAs = port.openDatabase(utf8(saveAsPath));
    QVERIFY(reopenedSaveAs);
    QCOMPARE(reopenedSaveAs.value().location(), expectedSaveAs);
    QCOMPARE(port.currentDatabasePath(), expectedSaveAs);
    verifyPopulatedWorkspace(
        services,
        createdTeacher.value(),
        expectedEnglishName,
        expectedRoomNumber,
        expectedNotes,
        expectedSetting
        );

    QVERIFY(port.closeDatabase(reopenedSaveAs.value()));
    const auto reopenedSource = port.openDatabase(utf8(sourcePath));
    QVERIFY(reopenedSource);
    QCOMPARE(reopenedSource.value().location(), expectedSource);
    QCOMPARE(port.currentDatabasePath(), expectedSource);
    verifyPopulatedWorkspace(
        services,
        createdTeacher.value(),
        expectedEnglishName,
        expectedRoomNumber,
        expectedNotes,
        expectedSetting
        );

    DatabaseSession* const sourceSession = services.databaseSession();
    const QString sourceConnection =
        sourceSession->database().connectionName();
    const auto exported = port.exportDatabaseAs(
        reopenedSource.value(),
        utf8(exportPath)
        );

    QVERIFY(exported);
    QCOMPARE(exported.value(), expectedExport);
    QCOMPARE(port.currentDatabasePath(), expectedSource);
    QCOMPARE(utf8(services.currentDatabasePath()), expectedSource);
    QCOMPARE(services.databaseSession(), sourceSession);
    QCOMPARE(
        services.databaseSession()->database().connectionName(),
        sourceConnection
        );
    QVERIFY(services.hasOpenDatabase());
    QVERIFY(QFile::exists(exportPath));
    verifyPopulatedWorkspace(
        services,
        createdTeacher.value(),
        expectedEnglishName,
        expectedRoomNumber,
        expectedNotes,
        expectedSetting
        );

    QVERIFY(port.closeDatabase(reopenedSource.value()));
    const auto reopenedExport = port.openDatabase(utf8(exportPath));
    QVERIFY(reopenedExport);
    QCOMPARE(reopenedExport.value().location(), expectedExport);
    QCOMPARE(port.currentDatabasePath(), expectedExport);
    verifyPopulatedWorkspace(
        services,
        createdTeacher.value(),
        expectedEnglishName,
        expectedRoomNumber,
        expectedNotes,
        expectedSetting
        );

    QVERIFY(port.closeDatabase(reopenedExport.value()));
    const auto sourceAfterExport = port.openDatabase(utf8(sourcePath));
    QVERIFY(sourceAfterExport);
    QCOMPARE(sourceAfterExport.value().location(), expectedSource);
    QCOMPARE(port.currentDatabasePath(), expectedSource);
    verifyPopulatedWorkspace(
        services,
        createdTeacher.value(),
        expectedEnglishName,
        expectedRoomNumber,
        expectedNotes,
        expectedSetting
        );
    QVERIFY(port.closeDatabase(sourceAfterExport.value()));
}

void NextPlatformApplicationServicesWorkspacePortTests::
walSaveAsAndExportPreserveCommittedRowsAndRowids()
{
    const QString sourcePath = m_directory.filePath(
        QStringLiteral("wal-snapshot/source.tps")
        );
    const QString exportPath = m_directory.filePath(
        QStringLiteral("wal-snapshot/export.tps")
        );
    const QString saveAsPath = m_directory.filePath(
        QStringLiteral("wal-snapshot/save-as.tps")
        );
    const std::string expectedSource = normalizedPath(sourcePath);
    const std::string expectedExport = normalizedPath(exportPath);
    const std::string expectedSaveAs = normalizedPath(saveAsPath);

    {
        ApplicationServices setupServices;
        ApplicationServicesWorkspacePort setupPort(setupServices);
        QVERIFY(createAndCloseWorkspace(setupPort, utf8(sourcePath)));
    }

    QString sqlError;
    {
        SqliteConnection setup;
        QVERIFY2(
            setup.open(sourcePath),
            qPrintable(setup.database().lastError().text())
            );

        QSqlQuery journalMode(setup.database());
        QVERIFY2(
            journalMode.exec(QStringLiteral("PRAGMA journal_mode=WAL")),
            qPrintable(journalMode.lastError().text())
            );
        QVERIFY(journalMode.next());
        QCOMPARE(
            journalMode.value(0).toString().toLower(),
            QStringLiteral("wal")
            );
        journalMode.finish();

        QVERIFY2(
            executeSql(
                setup.database(),
                QStringLiteral("PRAGMA wal_autocheckpoint=0"),
                &sqlError
                ),
            qPrintable(sqlError)
            );
        QVERIFY2(
            executeSql(
                setup.database(),
                QStringLiteral(
                    "CREATE TABLE opaque_profile_rows (payload TEXT NOT NULL)"
                    ),
                &sqlError
                ),
            qPrintable(sqlError)
            );
        QVERIFY2(
            executeSql(
                setup.database(),
                QStringLiteral(
                    "INSERT INTO opaque_profile_rows(rowid, payload) VALUES "
                    "(-17, 'negative rowid'), (9001, 'opaque rowid')"
                    ),
                &sqlError
                ),
            qPrintable(sqlError)
            );
        QVERIFY2(
            executeSql(
                setup.database(),
                QStringLiteral(
                    "CREATE INDEX opaque_profile_rows_payload_idx "
                    "ON opaque_profile_rows(payload)"
                    ),
                &sqlError
                ),
            qPrintable(sqlError)
            );
        QVERIFY2(
            executeSql(
                setup.database(),
                QStringLiteral(
                    "CREATE TABLE committed_wal_markers (marker TEXT NOT NULL)"
                    ),
                &sqlError
                ),
            qPrintable(sqlError)
            );
    }

    SqliteConnection oldReader;
    QVERIFY2(
        oldReader.open(sourcePath),
        qPrintable(oldReader.database().lastError().text())
        );
    QVERIFY(oldReader.database().transaction());
    {
        QSqlQuery oldSnapshot(oldReader.database());
        QVERIFY2(
            oldSnapshot.exec(QStringLiteral(
                "SELECT COUNT(*) FROM committed_wal_markers"
                )),
            qPrintable(oldSnapshot.lastError().text())
            );
        QVERIFY(oldSnapshot.next());
        QCOMPARE(oldSnapshot.value(0).toInt(), 0);
        oldSnapshot.finish();
    }

    SqliteConnection writer;
    QVERIFY2(
        writer.open(sourcePath),
        qPrintable(writer.database().lastError().text())
        );
    QVERIFY2(
        executeSql(
            writer.database(),
            QStringLiteral("PRAGMA wal_autocheckpoint=0"),
            &sqlError
            ),
        qPrintable(sqlError)
        );
    QVERIFY(writer.database().transaction());
    QVERIFY2(
        executeSql(
            writer.database(),
            QStringLiteral(
                "INSERT INTO committed_wal_markers(marker) VALUES "
                "('committed-after-reader-snapshot')"
                ),
            &sqlError
            ),
        qPrintable(sqlError)
        );
    QVERIFY(writer.database().commit());

    {
        QSqlQuery oldSnapshot(oldReader.database());
        QVERIFY2(
            oldSnapshot.exec(QStringLiteral(
                "SELECT COUNT(*) FROM committed_wal_markers"
                )),
            qPrintable(oldSnapshot.lastError().text())
            );
        QVERIFY(oldSnapshot.next());
        QCOMPARE(oldSnapshot.value(0).toInt(), 0);
        oldSnapshot.finish();
    }
    const QString walPath = sourcePath + QStringLiteral("-wal");
    QVERIFY(QFileInfo::exists(walPath));
    QVERIFY(QFileInfo(walPath).size() > 32);

    ApplicationServices services;
    ApplicationServicesWorkspacePort port(services);
    const auto opened = port.openDatabase(utf8(sourcePath));
    QVERIFY(opened);
    const std::string originalIdentity = opened.value().workspaceId();
    DatabaseSession* const sourceSession = services.databaseSession();
    const QString sourceConnection =
        sourceSession->database().connectionName();

    const auto exported = port.exportDatabaseAs(
        opened.value(),
        utf8(exportPath)
        );
    QVERIFY(exported);
    QCOMPARE(exported.value(), expectedExport);
    QCOMPARE(port.currentDatabasePath(), expectedSource);
    QCOMPARE(utf8(services.currentDatabasePath()), expectedSource);
    QCOMPARE(services.databaseSession(), sourceSession);
    QCOMPARE(
        services.databaseSession()->database().connectionName(),
        sourceConnection
        );
    QVERIFY(services.hasOpenDatabase());
    QVERIFY(QFileInfo::exists(walPath));
    QString snapshotError;
    QVERIFY2(
        snapshotHasWalAndOpaqueRows(exportPath, &snapshotError),
        qPrintable(snapshotError)
        );

    const auto savedAs = port.saveDatabaseAs(
        opened.value(),
        utf8(saveAsPath)
        );
    QVERIFY(savedAs);
    QCOMPARE(savedAs.value().workspaceId(), originalIdentity);
    QCOMPARE(savedAs.value().location(), expectedSaveAs);
    QCOMPARE(port.currentDatabasePath(), expectedSaveAs);
    QCOMPARE(utf8(services.currentDatabasePath()), expectedSaveAs);
    QVERIFY(services.hasOpenDatabase());
    QVERIFY2(
        snapshotHasWalAndOpaqueRows(saveAsPath, &snapshotError),
        qPrintable(snapshotError)
        );

    QVERIFY(port.closeDatabase(savedAs.value()));
    const auto reopenedSource = port.openDatabase(utf8(sourcePath));
    QVERIFY(reopenedSource);
    QCOMPARE(reopenedSource.value().location(), expectedSource);
    QCOMPARE(port.currentDatabasePath(), expectedSource);
    QVERIFY(services.hasOpenDatabase());
    QVERIFY2(
        snapshotHasWalAndOpaqueRows(sourcePath, &snapshotError),
        qPrintable(snapshotError)
        );
    QVERIFY(port.closeDatabase(reopenedSource.value()));
    QVERIFY(oldReader.database().rollback());
}

void NextPlatformApplicationServicesWorkspacePortTests::
snapshotFailurePreservesExistingDestinationSentinel()
{
    const QString sourcePath = m_directory.filePath(
        QStringLiteral("snapshot-failure/source.tps")
        );
    const QString destinationPath = m_directory.filePath(
        QStringLiteral("snapshot-failure/destination.tps")
        );
    const QByteArray invalidSqlite =
        QByteArray("SQLite format 3\0", 16)
        + QByteArray("invalid SQLite contents");
    const QByteArray sentinel("destination sentinel survives snapshot failure");
    QVERIFY(QDir().mkpath(QFileInfo(sourcePath).absolutePath()));
    writeInvalidWorkspace(sourcePath, invalidSqlite);
    writeInvalidWorkspace(destinationPath, sentinel);

    const Status result = DatabaseFileOperations::copyDatabaseFile(
        sourcePath,
        destinationPath
        );

    QVERIFY(!result);
    QFile destination(destinationPath);
    QVERIFY(destination.open(QIODevice::ReadOnly));
    QCOMPARE(destination.readAll(), sentinel);
    destination.close();
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
