#include "next/application/calendar_page_campus_directory_query_port.h"
#include "next/platform/calendar_page_campus_directory_query_adapter.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <type_traits>
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
    const std::optional<QString>& campusCode = std::nullopt
    )
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), id);
    object.insert(QStringLiteral("campus_name"), campusName);
    if (campusCode)
    {
        object.insert(QStringLiteral("campus_code"), *campusCode);
    }
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}
}

class NextPlatformCalendarPageCampusDirectoryQueryTests final : public QObject
{
    Q_OBJECT

private slots:
    void campusesPreserveRepositoryOrderAndOwningUtf8Metadata();
    void malformedDefaultAndBlankRecordsFollowRepositoryBehavior();
    void emptyAndMissingDirectoriesReturnNoCampuses();
    void portExposesOnlyOwningUtf8Metadata();
};

void NextPlatformCalendarPageCampusDirectoryQueryTests::
campusesPreserveRepositoryOrderAndOwningUtf8Metadata()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    // File order deliberately differs from repository campus-name order.
    writeFile(
        directory.path(),
        QStringLiteral("zulu.json"),
        campusRecord(
            QStringLiteral("id-zulu"),
            QStringLiteral("Zulu Campus"),
            QString::fromUtf8("\xEC\xBD\x94\xEB\x93\x9C-Z")
            )
        );
    writeFile(
        directory.path(),
        QStringLiteral("beta.json"),
        campusRecord(
            QStringLiteral("id-beta"),
            QStringLiteral("Beta Campus"),
            QStringLiteral(" \t ")
            )
        );
    writeFile(
        directory.path(),
        QStringLiteral("alpha.json"),
        campusRecord(
            QString::fromUtf8(
                "id-\xEC\x84\x9C\xEC\x9A\xB8-01"
                ),
            QStringLiteral("Alpha ")
                + QString::fromUtf8(
                    "\xEC\x84\x9C\xEC\x9A\xB8 "
                    "\xEC\xBA\xA0\xED\x8D\xBC\xEC\x8A\xA4"
                    ),
            QString::fromUtf8("\xEC\x84\x9C\xEC\x9A\xB8-01")
            )
        );
    writeFile(
        directory.path(),
        QStringLiteral("missing-code.json"),
        campusRecord(
            QStringLiteral("id-missing"),
            QStringLiteral("Missing Code")
            )
        );

    const ClassMngr::Next::Platform::
        CalendarPageCampusDirectoryQueryAdapter adapter(directory.path());

    const auto campuses = adapter.loadCampuses();
    QCOMPARE(campuses.size(), std::size_t(4));
    QCOMPARE(
        campuses.at(0).id,
        QString::fromUtf8("id-\xEC\x84\x9C\xEC\x9A\xB8-01")
            .toUtf8().toStdString()
        );
    QCOMPARE(
        campuses.at(0).campusName,
        (
            QStringLiteral("Alpha ")
            + QString::fromUtf8(
                "\xEC\x84\x9C\xEC\x9A\xB8 "
                "\xEC\xBA\xA0\xED\x8D\xBC\xEC\x8A\xA4"
                )
            ).toUtf8().toStdString()
        );
    QVERIFY(campuses.at(0).campusCode.has_value());
    QCOMPARE(
        *campuses.at(0).campusCode,
        QString::fromUtf8("\xEC\x84\x9C\xEC\x9A\xB8-01")
            .toUtf8().toStdString()
        );

    QCOMPARE(campuses.at(1).id, std::string("id-beta"));
    QCOMPARE(campuses.at(1).campusName, std::string("Beta Campus"));
    QVERIFY(campuses.at(1).campusCode.has_value());
    QCOMPARE(*campuses.at(1).campusCode, std::string(" \t "));

    QCOMPARE(campuses.at(2).id, std::string("id-missing"));
    QCOMPARE(campuses.at(2).campusName, std::string("Missing Code"));
    QVERIFY(!campuses.at(2).campusCode.has_value());

    QCOMPARE(campuses.at(3).id, std::string("id-zulu"));
    QCOMPARE(campuses.at(3).campusName, std::string("Zulu Campus"));
    QVERIFY(campuses.at(3).campusCode.has_value());
    QCOMPARE(
        *campuses.at(3).campusCode,
        QString::fromUtf8("\xEC\xBD\x94\xEB\x93\x9C-Z")
            .toUtf8().toStdString()
        );
}

void NextPlatformCalendarPageCampusDirectoryQueryTests::
malformedDefaultAndBlankRecordsFollowRepositoryBehavior()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    writeFile(
        directory.path(),
        QStringLiteral("default.json"),
        campusRecord(
            QStringLiteral("file-default"),
            QStringLiteral("File Default"),
            QStringLiteral("omitted")
            )
        );
    writeFile(
        directory.path(),
        QStringLiteral("record-default.json"),
        campusRecord(
            QStringLiteral("DEFAULT"),
            QStringLiteral("Default Record"),
            QStringLiteral("omitted")
            )
        );
    writeFile(
        directory.path(),
        QStringLiteral("malformed.json"),
        QByteArrayLiteral("{ not json")
        );
    writeFile(
        directory.path(),
        QStringLiteral("blank.json"),
        campusRecord(
            QStringLiteral("blank-id"),
            QString(),
            QString()
            )
        );

    const ClassMngr::Next::Platform::
        CalendarPageCampusDirectoryQueryAdapter adapter(directory.path());

    const auto campuses = adapter.loadCampuses();
    QCOMPARE(campuses.size(), std::size_t(1));
    QCOMPARE(campuses.at(0).id, std::string("blank-id"));
    QVERIFY(campuses.at(0).campusName.empty());
    QVERIFY(!campuses.at(0).campusCode.has_value());
}

void NextPlatformCalendarPageCampusDirectoryQueryTests::
emptyAndMissingDirectoriesReturnNoCampuses()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const ClassMngr::Next::Platform::
        CalendarPageCampusDirectoryQueryAdapter emptyAdapter(directory.path());
    QVERIFY(emptyAdapter.loadCampuses().empty());

    const QString missingDirectory =
        QDir(directory.path()).filePath(QStringLiteral("missing"));
    const ClassMngr::Next::Platform::
        CalendarPageCampusDirectoryQueryAdapter missingAdapter(missingDirectory);
    QVERIFY(missingAdapter.loadCampuses().empty());
}

void NextPlatformCalendarPageCampusDirectoryQueryTests::
portExposesOnlyOwningUtf8Metadata()
{
    using Metadata =
        ClassMngr::Next::Application::CalendarPageCampusMetadata;
    using Port =
        ClassMngr::Next::Application::CalendarPageCampusDirectoryQueryPort;
    static_assert(
        std::is_same_v<
            decltype(std::declval<const Port&>().loadCampuses()),
            std::vector<Metadata>
            >
        );
    static_assert(std::is_same_v<decltype(Metadata::id), std::string>);
    static_assert(
        std::is_same_v<decltype(Metadata::campusName), std::string>
        );
    static_assert(
        std::is_same_v<
            decltype(Metadata::campusCode),
            std::optional<std::string>
            >
        );

    QVERIFY(true);
}

QTEST_APPLESS_MAIN(NextPlatformCalendarPageCampusDirectoryQueryTests)

#include "next_platform_calendar_page_campus_directory_query_adapter_tests.moc"
