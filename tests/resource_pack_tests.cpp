#include "core/resource_packs/resource_pack_manifest.h"
#include "core/resource_packs/resource_pack_manager.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

namespace
{
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

QByteArray manifestJson(
    QJsonObject campuses = artifactObject()
    )
{
    QJsonObject packs;
    packs.insert(QStringLiteral("campuses"), campuses);
    packs.insert(
        QStringLiteral("templates"),
        artifactObject(
            QStringLiteral("1.2.0"),
            QStringLiteral("https://example.com/templates.rcc"),
            QStringLiteral("templates.rcc")
            )
        );
    packs.insert(
        QStringLiteral("roster-designs"),
        artifactObject(
            QStringLiteral("2.0.0"),
            QStringLiteral("https://example.com/roster-designs.rcc"),
            QStringLiteral("roster-designs.rcc")
            )
        );

    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), 1);
    root.insert(QStringLiteral("packs"), packs);

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}
}

class ResourcePackTests : public QObject
{
    Q_OBJECT

private slots:
    void parsesKnownPacks();
    void requiresConfiguredPacks();
    void rejectsUnsafeArtifacts();
    void stagesAndMountsValidRccPack();
};

void ResourcePackTests::parsesKnownPacks()
{
    const auto manifest =
        ResourcePackManifest::fromJson(
            manifestJson(),
            {
                QStringLiteral("campuses"),
                QStringLiteral("templates"),
                QStringLiteral("roster-designs")
            }
            );

    QVERIFY(manifest.has_value());
    QCOMPARE(manifest->schemaVersion(), 1);

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
}

QTEST_MAIN(ResourcePackTests)

#include "resource_pack_tests.moc"
