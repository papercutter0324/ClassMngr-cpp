#include "core/application_services.h"
#include "next/platform/application_services_workspace_port.h"

#include <QFile>
#include <QFileInfo>
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

} // namespace

class NextPlatformApplicationServicesWorkspacePortTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void openAcceptsUtf8PathAndNormalizesHandle();
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

    const auto result = port.openDatabase(utf8(requested));

    QVERIFY(result);
    QCOMPARE(result.value().workspaceId(), expected);
    QCOMPARE(result.value().location(), expected);
    QCOMPARE(port.currentDatabasePath(), expected);
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
    const auto opened = port.openDatabase(
        utf8(m_directory.filePath(QStringLiteral("close.tps")))
        );
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
    const auto opened = port.openDatabase(
        utf8(m_directory.filePath(QStringLiteral("save.tps")))
        );
    QVERIFY(opened);

    const auto saved = port.saveDatabase(opened.value());
    QVERIFY(saved);

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
    const auto opened = port.openDatabase(utf8(sourcePath));
    QVERIFY(opened);
    const std::string originalIdentity = opened.value().workspaceId();
    const std::string expectedDestination = normalizedPath(destinationPath);

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
    const auto opened = port.openDatabase(utf8(sourcePath));
    QVERIFY(opened);
    const std::string currentPath = port.currentDatabasePath();
    const std::string expectedDestination = normalizedPath(destinationPath);

    const auto exported = port.exportDatabaseAs(
        opened.value(),
        utf8(destinationPath)
        );

    QVERIFY(exported);
    QCOMPARE(exported.value(), expectedDestination);
    QCOMPARE(port.currentDatabasePath(), currentPath);
    QCOMPARE(utf8(services.currentDatabasePath()), currentPath);
    QVERIFY(QFile::exists(destinationPath));
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
