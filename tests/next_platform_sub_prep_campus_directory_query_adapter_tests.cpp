#include "next/application/sub_prep_campus_directory_query_port.h"
#include "next/platform/sub_prep_campus_directory_query_adapter.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
void writeFile(
    const QString& directory,
    const QString& fileName,
    const QByteArray& contents
    )
{
    QFile file(QDir(directory).filePath(fileName));
    if (!file.open(QIODevice::WriteOnly))
    {
        qFatal("Unable to create campus fixture file");
    }
    if (file.write(contents) != contents.size())
    {
        qFatal("Unable to write campus fixture file");
    }
}

QByteArray campusRecord(
    const QString& id,
    const QString& campusName,
    const QString& officeNumber = QString(),
    const QString& wifiName = QString(),
    const QString& wifiPassword = QString(),
    const QString& photocopierCode = QString()
    )
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), id);
    object.insert(QStringLiteral("campus_name"), campusName);
    object.insert(QStringLiteral("office_number"), officeNumber);
    object.insert(QStringLiteral("office_wifi"), wifiName);
    object.insert(QStringLiteral("office_wifi_password"), wifiPassword);
    object.insert(QStringLiteral("photocopier_code"), photocopierCode);
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}
}

class NextPlatformSubPrepCampusDirectoryQueryAdapterTests final : public QObject
{
    Q_OBJECT

private slots:
    void campusesPreserveRepositoryOrderAndAllUtf8Details();
    void blankNamesFallbackToTrimmedIds();
    void malformedAndDefaultRecordsFollowRepositoryBehavior();
    void emptyAndMissingDirectoriesReturnNoCampuses();
    void adapterImplementsOwningApplicationPort();
};

void NextPlatformSubPrepCampusDirectoryQueryAdapterTests::
campusesPreserveRepositoryOrderAndAllUtf8Details()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    // Deliberately write files out of the repository's campus-name order.
    writeFile(
        directory.path(),
        QStringLiteral("zulu.json"),
        campusRecord(QStringLiteral("id-zulu"), QStringLiteral("Zulu Campus"))
        );
    writeFile(
        directory.path(),
        QStringLiteral("beta.json"),
        campusRecord(QStringLiteral("id-beta"), QStringLiteral("Beta Campus"))
        );
    writeFile(
        directory.path(),
        QStringLiteral("alpha.json"),
        campusRecord(
            QString::fromUtf8(" id-\xEC\x84\x9C\xEC\x9A\xB8-01 "),
            QStringLiteral(" Alpha ")
                + QString::fromUtf8(
                    "\xEC\x84\x9C\xEC\x9A\xB8 \xEC\xBA\xA0\xED\x8D\xBC\xEC\x8A\xA4 "
                    ),
            QString::fromUtf8("\xEC\x82\xAC\xEB\xAC\xB4\xEC\x8B\xA4-418"),
            QString::fromUtf8("Native-\xEC\xBA\xA0\xED\x8D\xBC\xEC\x8A\xA4"),
            QString::fromUtf8("pw-\xEC\x95\x94\xED\x98\xB8"),
            QString::fromUtf8("\xEB\xB3\xB5\xEC\x82\xAC-01")
            )
        );

    const ClassMngr::Next::Platform::
        SubPrepCampusDirectoryQueryAdapter adapter(directory.path());
    const auto campuses = adapter.loadCampuses();

    QCOMPARE(campuses.size(), std::size_t(3));
    const auto& alpha = campuses.at(0);
    QCOMPARE(
        alpha.id,
        QString::fromUtf8(" id-\xEC\x84\x9C\xEC\x9A\xB8-01 ")
            .toUtf8().toStdString()
        );
    QCOMPARE(
        alpha.displayName,
        (
            QStringLiteral("Alpha ")
            + QString::fromUtf8(
                "\xEC\x84\x9C\xEC\x9A\xB8 \xEC\xBA\xA0\xED\x8D\xBC\xEC\x8A\xA4"
                )
            ).toUtf8().toStdString()
        );
    QCOMPARE(
        alpha.officeNumber,
        QString::fromUtf8("\xEC\x82\xAC\xEB\xAC\xB4\xEC\x8B\xA4-418")
            .toUtf8().toStdString()
        );
    QCOMPARE(
        alpha.wifiName,
        QString::fromUtf8("Native-\xEC\xBA\xA0\xED\x8D\xBC\xEC\x8A\xA4")
            .toUtf8().toStdString()
        );
    QCOMPARE(
        alpha.wifiPassword,
        QString::fromUtf8("pw-\xEC\x95\x94\xED\x98\xB8")
            .toUtf8().toStdString()
        );
    QCOMPARE(
        alpha.photocopierCode,
        QString::fromUtf8("\xEB\xB3\xB5\xEC\x82\xAC-01")
            .toUtf8().toStdString()
        );

    QCOMPARE(campuses.at(1).id, std::string("id-beta"));
    QCOMPARE(campuses.at(1).displayName, std::string("Beta Campus"));
    QCOMPARE(campuses.at(2).id, std::string("id-zulu"));
    QCOMPARE(campuses.at(2).displayName, std::string("Zulu Campus"));
}

void NextPlatformSubPrepCampusDirectoryQueryAdapterTests::
blankNamesFallbackToTrimmedIds()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    writeFile(
        directory.path(),
        QStringLiteral("fallback.json"),
        campusRecord(
            QStringLiteral("  fallback-id  "),
            QStringLiteral(" \t ")
            )
        );

    const ClassMngr::Next::Platform::
        SubPrepCampusDirectoryQueryAdapter adapter(directory.path());
    const auto campuses = adapter.loadCampuses();

    QCOMPARE(campuses.size(), std::size_t(1));
    QCOMPARE(campuses.front().id, std::string("  fallback-id  "));
    QCOMPARE(campuses.front().displayName, std::string("fallback-id"));
}

void NextPlatformSubPrepCampusDirectoryQueryAdapterTests::
malformedAndDefaultRecordsFollowRepositoryBehavior()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    writeFile(
        directory.path(),
        QStringLiteral("default.json"),
        campusRecord(QStringLiteral("file-default"), QStringLiteral("File Default"))
        );
    writeFile(
        directory.path(),
        QStringLiteral("record-default.json"),
        campusRecord(QStringLiteral("DEFAULT"), QStringLiteral("Default Record"))
        );
    writeFile(
        directory.path(),
        QStringLiteral("malformed.json"),
        QByteArrayLiteral("{ not json")
        );
    writeFile(
        directory.path(),
        QStringLiteral("valid.json"),
        campusRecord(QStringLiteral("id-valid"), QStringLiteral("Valid Campus"))
        );

    const ClassMngr::Next::Platform::
        SubPrepCampusDirectoryQueryAdapter adapter(directory.path());
    const auto campuses = adapter.loadCampuses();

    QCOMPARE(campuses.size(), std::size_t(1));
    QCOMPARE(campuses.front().id, std::string("id-valid"));
    QCOMPARE(campuses.front().displayName, std::string("Valid Campus"));
}

void NextPlatformSubPrepCampusDirectoryQueryAdapterTests::
emptyAndMissingDirectoriesReturnNoCampuses()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const ClassMngr::Next::Platform::
        SubPrepCampusDirectoryQueryAdapter emptyAdapter(directory.path());
    QVERIFY(emptyAdapter.loadCampuses().empty());

    const QString missingDirectory =
        QDir(directory.path()).filePath(QStringLiteral("missing"));
    const ClassMngr::Next::Platform::
        SubPrepCampusDirectoryQueryAdapter missingAdapter(missingDirectory);
    QVERIFY(missingAdapter.loadCampuses().empty());
}

void NextPlatformSubPrepCampusDirectoryQueryAdapterTests::
adapterImplementsOwningApplicationPort()
{
    using Metadata =
        ClassMngr::Next::Application::SubPrepCampusMetadata;
    using Port =
        ClassMngr::Next::Application::SubPrepCampusDirectoryQueryPort;
    static_assert(
        std::is_same_v<
            decltype(std::declval<const Port&>().loadCampuses()),
            std::vector<Metadata>
            >
        );
    static_assert(std::is_same_v<decltype(Metadata::id), std::string>);
    static_assert(
        std::is_same_v<decltype(Metadata::displayName), std::string>
        );
    static_assert(
        std::is_same_v<decltype(Metadata::officeNumber), std::string>
        );
    static_assert(std::is_same_v<decltype(Metadata::wifiName), std::string>);
    static_assert(
        std::is_same_v<decltype(Metadata::wifiPassword), std::string>
        );
    static_assert(
        std::is_same_v<decltype(Metadata::photocopierCode), std::string>
        );

    QVERIFY(true);
}

QTEST_APPLESS_MAIN(NextPlatformSubPrepCampusDirectoryQueryAdapterTests)

#include "next_platform_sub_prep_campus_directory_query_adapter_tests.moc"
