#include "next/application/my_info_campus_directory_query_port.h"
#include "next/platform/my_info_campus_directory_query_adapter.h"

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
    const QString& campusName
    )
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), id);
    object.insert(QStringLiteral("campus_name"), campusName);
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}
}

class NextPlatformMyInfoCampusDirectoryQueryAdapterTests final : public QObject
{
    Q_OBJECT

private slots:
    void campusesPreserveRepositoryOrderAndOwningUtf8Metadata();
    void blankNamesFallbackToTrimmedIds();
    void malformedAndDefaultRecordsFollowRepositoryBehavior();
    void emptyAndMissingDirectoriesReturnNoCampuses();
    void adapterImplementsQtFreeOwningMetadataPort();
};

void NextPlatformMyInfoCampusDirectoryQueryAdapterTests::
campusesPreserveRepositoryOrderAndOwningUtf8Metadata()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    // File creation order differs from the repository's campus-name order.
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
                    )
            )
        );

    const ClassMngr::Next::Platform::
        MyInfoCampusDirectoryQueryAdapter adapter(directory.path());
    const auto campuses = adapter.loadCampuses();

    QCOMPARE(campuses.size(), std::size_t(3));
    QCOMPARE(
        campuses.at(0).id,
        QString::fromUtf8(" id-\xEC\x84\x9C\xEC\x9A\xB8-01 ")
            .toUtf8().toStdString()
        );
    QCOMPARE(
        campuses.at(0).displayName,
        (
            QStringLiteral("Alpha ")
            + QString::fromUtf8(
                "\xEC\x84\x9C\xEC\x9A\xB8 \xEC\xBA\xA0\xED\x8D\xBC\xEC\x8A\xA4"
                )
            ).toUtf8().toStdString()
        );
    QCOMPARE(campuses.at(1).id, std::string("id-beta"));
    QCOMPARE(campuses.at(1).displayName, std::string("Beta Campus"));
    QCOMPARE(campuses.at(2).id, std::string("id-zulu"));
    QCOMPARE(campuses.at(2).displayName, std::string("Zulu Campus"));
}

void NextPlatformMyInfoCampusDirectoryQueryAdapterTests::
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
        MyInfoCampusDirectoryQueryAdapter adapter(directory.path());
    const auto campuses = adapter.loadCampuses();

    QCOMPARE(campuses.size(), std::size_t(1));
    QCOMPARE(campuses.front().id, std::string("  fallback-id  "));
    QCOMPARE(campuses.front().displayName, std::string("fallback-id"));
}

void NextPlatformMyInfoCampusDirectoryQueryAdapterTests::
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
        MyInfoCampusDirectoryQueryAdapter adapter(directory.path());
    const auto campuses = adapter.loadCampuses();

    QCOMPARE(campuses.size(), std::size_t(1));
    QCOMPARE(campuses.front().id, std::string("id-valid"));
    QCOMPARE(campuses.front().displayName, std::string("Valid Campus"));
}

void NextPlatformMyInfoCampusDirectoryQueryAdapterTests::
emptyAndMissingDirectoriesReturnNoCampuses()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const ClassMngr::Next::Platform::
        MyInfoCampusDirectoryQueryAdapter emptyAdapter(directory.path());
    QVERIFY(emptyAdapter.loadCampuses().empty());

    const QString missingDirectory =
        QDir(directory.path()).filePath(QStringLiteral("missing"));
    const ClassMngr::Next::Platform::
        MyInfoCampusDirectoryQueryAdapter missingAdapter(missingDirectory);
    QVERIFY(missingAdapter.loadCampuses().empty());
}

void NextPlatformMyInfoCampusDirectoryQueryAdapterTests::
adapterImplementsQtFreeOwningMetadataPort()
{
    using Metadata =
        ClassMngr::Next::Application::MyInfoCampusMetadata;
    using Port =
        ClassMngr::Next::Application::MyInfoCampusDirectoryQueryPort;
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

    QVERIFY(true);
}

QTEST_APPLESS_MAIN(NextPlatformMyInfoCampusDirectoryQueryAdapterTests)

#include "next_platform_my_info_campus_directory_query_adapter_tests.moc"
