#include "core/resource_packs/resource_pack_manifest.h"
#include "core/resource_packs/resource_pack_manager.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

namespace
{
QStringList expectedPackIds()
{
    return {
        QStringLiteral("campuses"),
        QStringLiteral("templates"),
        QStringLiteral("roster-designs"),
        QStringLiteral("documents")
    };
}

QJsonObject artifactObject(
    const QString& version = QStringLiteral("1.1.0"),
    const QString& url = QStringLiteral("https://example.com/campuses.rcc"),
    const QString& fileName = QStringLiteral("campuses.rcc"),
    const QString& sha256 = QString(64, QLatin1Char('a')),
    qint64 sizeBytes = 1024
    )
{
    QJsonObject artifact;
    artifact.insert(QStringLiteral("version"), version);
    artifact.insert(QStringLiteral("url"), url);
    artifact.insert(QStringLiteral("fileName"), fileName);
    artifact.insert(QStringLiteral("sha256"), sha256);
    artifact.insert(QStringLiteral("sizeBytes"), sizeBytes);
    return artifact;
}

QJsonObject artifactObjectForPack(
    const QString& packId
    )
{
    const QString fileName =
        packId + QStringLiteral(".rcc");

    return artifactObject(
        QStringLiteral("1.1.0"),
        QStringLiteral("https://example.com/%1").arg(fileName),
        fileName
        );
}

QByteArray manifestJson(
    QJsonObject campuses = artifactObject()
    )
{
    QJsonObject packs;

    for (const QString& packId : expectedPackIds())
    {
        packs.insert(
            packId,
            artifactObjectForPack(packId)
            );
    }

    packs.insert(
        QStringLiteral("campuses"),
        campuses
        );

    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), 1);
    root.insert(QStringLiteral("packs"), packs);

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

bool installCampusBaseline(
    QTemporaryDir* directory
    )
{
    return directory
        && directory->isValid()
        && QFile::copy(
            QStringLiteral(CLASSMNGR_TEST_CAMPUS_PACK_PATH),
            QDir(directory->path()).filePath(
                QStringLiteral("campuses.rcc")
                )
            );
}
}

class ResourcePackTests : public QObject
{
    Q_OBJECT

private slots:
    void knowsConfiguredPacks();
    void parsesKnownPacks();
    void requiresConfiguredPacks();
    void rejectsUnsafeArtifacts();
    void stagesAndMountsValidRccPack();
    void retainsPackUntilFinalLeaseRelease();
    void fallsBackToBaselineWhenUpdateIsInvalid();
    void failsWhenNoValidPackExists();
    void removesRetiredPackArtifacts();
};

void ResourcePackTests::knowsConfiguredPacks()
{
    ResourcePackManager manager(
        QStringLiteral("unused")
        );

    QCOMPARE(
        manager.knownPackIds(),
        expectedPackIds()
        );
    QCOMPARE(
        manager.currentVersion(QStringLiteral("documents")).toString(),
        QStringLiteral("1.0.0")
        );
}

void ResourcePackTests::parsesKnownPacks()
{
    QStringList sortedExpectedPackIds =
        expectedPackIds();
    sortedExpectedPackIds.sort();

    const auto manifest =
        ResourcePackManifest::fromJson(
            manifestJson(),
            expectedPackIds()
            );

    QVERIFY(manifest.has_value());
    QCOMPARE(manifest->schemaVersion(), 1);
    QCOMPARE(
        manifest->packIds(),
        sortedExpectedPackIds
        );

    const auto campuses =
        manifest->artifact(QStringLiteral("campuses"));

    QVERIFY(campuses.has_value());
    QCOMPARE(campuses->version.toString(), QStringLiteral("1.1.0"));
    QCOMPARE(campuses->sizeBytes, 1024);
}

void ResourcePackTests::requiresConfiguredPacks()
{
    const auto manifest =
        ResourcePackManifest::fromJson(
            manifestJson(),
            {QStringLiteral("missing-pack")}
            );

    QVERIFY(!manifest.has_value());
    QVERIFY(
        manifest.error().contains(
            QStringLiteral("missing-pack")
            )
        );
}

void ResourcePackTests::rejectsUnsafeArtifacts()
{
    QVERIFY(
        !ResourcePackManifest::fromJson(
            manifestJson(
                artifactObject(
                    QStringLiteral("1.1.0"),
                    QStringLiteral("http://example.com/campuses.rcc")
                    )
                )
            ).has_value()
        );

    QVERIFY(
        !ResourcePackManifest::fromJson(
            manifestJson(
                artifactObject(
                    QStringLiteral("1.1.0"),
                    QStringLiteral("https://example.com/campuses.rcc"),
                    QStringLiteral("../campuses.rcc")
                    )
                )
            ).has_value()
        );

    QVERIFY(
        !ResourcePackManifest::fromJson(
            manifestJson(
                artifactObject(
                    QStringLiteral("1.1.0"),
                    QStringLiteral("https://example.com/campuses.rcc"),
                    QStringLiteral("campuses.rcc"),
                    QStringLiteral("not-a-checksum")
                    )
                )
            ).has_value()
        );

    QVERIFY(
        !ResourcePackManifest::fromJson(
            manifestJson(
                artifactObject(
                    QStringLiteral("1.1.0"),
                    QStringLiteral("https://example.com/campuses.rcc"),
                    QStringLiteral("campuses.rcc"),
                    QString(64, QLatin1Char('a')),
                    0
                    )
                )
            ).has_value()
        );
}

void ResourcePackTests::stagesAndMountsValidRccPack()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString downloadPath =
        temporaryDirectory.filePath(
            QStringLiteral("campuses.download")
            );

    QVERIFY(
        QFile::copy(
            QStringLiteral(CLASSMNGR_TEST_CAMPUS_PACK_PATH),
            downloadPath
            )
        );

    QFile downloadedFile(downloadPath);
    QVERIFY(downloadedFile.open(QIODevice::ReadOnly));

    ResourcePackArtifact artifact;
    artifact.id =
        QStringLiteral("campuses");
    artifact.version =
        Version(1, 1, 0);
    artifact.fileName =
        QStringLiteral("campuses-1.1.0.rcc");
    artifact.sha256 =
        QString::fromLatin1(
            QCryptographicHash::hash(
                downloadedFile.readAll(),
                QCryptographicHash::Sha256
                ).toHex()
            );
    artifact.sizeBytes =
        downloadedFile.size();
    downloadedFile.close();

    ResourcePackManager stagingManager(
        temporaryDirectory.path()
        );

    const Status stageStatus =
        stagingManager.stagePack(
            artifact,
            downloadPath
            );

    if (!stageStatus)
    {
        QFAIL(qPrintable(stageStatus.error()));
    }

    ResourcePackManager mountedManager(
        temporaryDirectory.path()
        );

    const Status initializeStatus =
        mountedManager.initialize();

    if (!initializeStatus)
    {
        QFAIL(qPrintable(initializeStatus.error()));
    }
    QCOMPARE(
        mountedManager.currentVersion(QStringLiteral("campuses")).toString(),
        QStringLiteral("1.1.0")
        );
    QVERIFY(!mountedManager.isMounted(QStringLiteral("campuses")));

    auto lease = mountedManager.acquire(QStringLiteral("campuses"));
    if (!lease)
    {
        QFAIL(qPrintable(lease.error()));
    }

    QCOMPARE(
        mountedManager.activeRoot(QStringLiteral("campuses")),
        QStringLiteral(":/resource-packs/campuses")
        );

    QFile marker(
        mountedManager.activeRoot(QStringLiteral("campuses"))
            + QStringLiteral("/marker.txt")
        );
    QVERIFY(marker.open(QIODevice::ReadOnly));
    QCOMPARE(
        marker.readAll().trimmed(),
        QByteArrayLiteral("external campus pack")
        );

    lease->reset();
    QVERIFY(!mountedManager.isMounted(QStringLiteral("campuses")));
}

void ResourcePackTests::retainsPackUntilFinalLeaseRelease()
{
    QTemporaryDir temporaryDirectory;
    QTemporaryDir baselineDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QVERIFY(installCampusBaseline(&baselineDirectory));
    ResourcePackManager manager(
        temporaryDirectory.path(),
        baselineDirectory.path()
        );

    auto firstLease = manager.acquire(QStringLiteral("campuses"));
    QVERIFY(firstLease.has_value());
    auto secondLease = manager.acquire(QStringLiteral("campuses"));
    QVERIFY(secondLease.has_value());
    QVERIFY(manager.isMounted(QStringLiteral("campuses")));

    firstLease->reset();
    QVERIFY(manager.isMounted(QStringLiteral("campuses")));

    secondLease->reset();
    QVERIFY(!manager.isMounted(QStringLiteral("campuses")));
}

void ResourcePackTests::fallsBackToBaselineWhenUpdateIsInvalid()
{
    QTemporaryDir temporaryDirectory;
    QTemporaryDir baselineDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QVERIFY(installCampusBaseline(&baselineDirectory));

    const QString downloadPath = temporaryDirectory.filePath(
        QStringLiteral("campuses.download")
        );
    QVERIFY(QFile::copy(
        QStringLiteral(CLASSMNGR_TEST_CAMPUS_PACK_PATH),
        downloadPath
        ));

    QFile downloadedFile(downloadPath);
    QVERIFY(downloadedFile.open(QIODevice::ReadOnly));
    ResourcePackArtifact artifact;
    artifact.id = QStringLiteral("campuses");
    artifact.version = Version(1, 1, 0);
    artifact.fileName = QStringLiteral("campuses-1.1.0.rcc");
    artifact.sha256 = QString::fromLatin1(
        QCryptographicHash::hash(
            downloadedFile.readAll(),
            QCryptographicHash::Sha256
            ).toHex()
        );
    artifact.sizeBytes = downloadedFile.size();
    downloadedFile.close();

    ResourcePackManager stagingManager(temporaryDirectory.path());
    QVERIFY(stagingManager.stagePack(artifact, downloadPath).has_value());

    const QString stagedPath = temporaryDirectory.filePath(
        QStringLiteral("campuses-1.1.0.rcc")
        );
    QFile stagedFile(stagedPath);
    QVERIFY(stagedFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QVERIFY(stagedFile.write(QByteArrayLiteral("corrupt")) > 0);
    stagedFile.close();

    ResourcePackManager manager(
        temporaryDirectory.path(),
        baselineDirectory.path()
        );
    // Metadata discovery is deliberately lightweight. The corrupt artifact
    // remains unmounted until it is requested, when its checksum is checked
    // and the baseline pack is used instead.
    QVERIFY(manager.initialize().has_value());
    QVERIFY(!manager.isMounted(QStringLiteral("campuses")));

    auto lease = manager.acquire(QStringLiteral("campuses"));
    QVERIFY(lease.has_value());
    QFile marker(QDir(lease->root()).filePath(QStringLiteral("marker.txt")));
    QVERIFY(marker.open(QIODevice::ReadOnly));
    QCOMPARE(marker.readAll().trimmed(), QByteArrayLiteral("external campus pack"));
    lease->reset();
    QVERIFY(!QFileInfo::exists(stagedPath));
    QVERIFY(!QFileInfo::exists(
        temporaryDirectory.filePath(QStringLiteral("campuses.json"))
        ));
}

void ResourcePackTests::failsWhenNoValidPackExists()
{
    QTemporaryDir storageDirectory;
    QTemporaryDir baselineDirectory;
    QVERIFY(storageDirectory.isValid());
    QVERIFY(baselineDirectory.isValid());

    ResourcePackManager manager(
        storageDirectory.path(),
        baselineDirectory.path()
        );
    const auto lease = manager.acquire(QStringLiteral("campuses"));
    QVERIFY(!lease.has_value());
    QVERIFY(
        lease.error().contains(
            QStringLiteral("unavailable")
            )
        );
}

void ResourcePackTests::removesRetiredPackArtifacts()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString metadataPath =
        temporaryDirectory.filePath(QStringLiteral("guides.json"));
    const QString packPath =
        temporaryDirectory.filePath(QStringLiteral("guides-1.0.0.rcc"));

    QFile metadataFile(metadataPath);
    QVERIFY(metadataFile.open(QIODevice::WriteOnly));
    QVERIFY(metadataFile.write(QByteArrayLiteral("{}")) > 0);
    metadataFile.close();

    QFile packFile(packPath);
    QVERIFY(packFile.open(QIODevice::WriteOnly));
    QVERIFY(packFile.write(QByteArrayLiteral("retired")) > 0);
    packFile.close();

    ResourcePackManager manager(
        temporaryDirectory.path()
        );

    const Status status =
        manager.initialize();

    if (!status)
    {
        QFAIL(qPrintable(status.error()));
    }

    QVERIFY(!QFileInfo::exists(metadataPath));
    QVERIFY(!QFileInfo::exists(packPath));
}

QTEST_MAIN(ResourcePackTests)

#include "resource_pack_tests.moc"
