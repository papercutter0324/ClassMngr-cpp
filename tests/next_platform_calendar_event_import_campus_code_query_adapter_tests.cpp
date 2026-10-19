#include "next/application/calendar_event_import_campus_code_query_port.h"
#include "next/platform/calendar_event_import_campus_code_query_adapter.h"

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
    const QString& name,
    const QString& code
    )
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), id);
    object.insert(QStringLiteral("campus_name"), name);
    object.insert(QStringLiteral("campus_code"), code);
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}
}

class NextPlatformCalendarEventImportCampusCodeQueryTests final : public QObject
{
    Q_OBJECT

private slots:
    void codesPreserveRepositoryOrderAndLegacyFiltering();
    void emptyAndMissingDirectoriesReturnNoCodes();
    void portReturnsOwningUtf8Values();
};

void NextPlatformCalendarEventImportCampusCodeQueryTests::codesPreserveRepositoryOrderAndLegacyFiltering()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    // File order deliberately differs from repository campus-name order.
    writeFile(
        directory.path(),
        QStringLiteral("zulu.json"),
        campusRecord(
            QStringLiteral("zulu"),
            QStringLiteral("Zulu Campus"),
            QStringLiteral("  마지막-code  ")
            )
        );
    writeFile(
        directory.path(),
        QStringLiteral("beta.json"),
        campusRecord(
            QStringLiteral("beta"),
            QStringLiteral("Beta Campus"),
            QStringLiteral(" same ")
            )
        );
    writeFile(
        directory.path(),
        QStringLiteral("alpha.json"),
        campusRecord(
            QStringLiteral("alpha"),
            QStringLiteral("Alpha Campus"),
            QStringLiteral("  서울-01  ")
            )
        );
    writeFile(
        directory.path(),
        QStringLiteral("gamma.json"),
        campusRecord(
            QStringLiteral("gamma"),
            QStringLiteral("Gamma Campus"),
            QStringLiteral(" 서울-01 ")
            )
        );
    writeFile(
        directory.path(),
        QStringLiteral("delta.json"),
        campusRecord(
            QStringLiteral("delta"),
            QStringLiteral("Delta Campus"),
            QStringLiteral(" \t ")
            )
        );
    writeFile(
        directory.path(),
        QStringLiteral("epsilon.json"),
        campusRecord(
            QStringLiteral("epsilon"),
            QStringLiteral("Epsilon Campus"),
            QStringLiteral("same")
            )
        );
    writeFile(
        directory.path(),
        QStringLiteral("eta.json"),
        campusRecord(
            QStringLiteral("eta"),
            QStringLiteral("Eta Campus"),
            QStringLiteral("Same")
            )
        );
    writeFile(
        directory.path(),
        QStringLiteral("iota.json"),
        campusRecord(
            QStringLiteral("iota"),
            QStringLiteral("Iota Campus"),
            QStringLiteral("SAME")
            )
        );
    writeFile(
        directory.path(),
        QStringLiteral("default.json"),
        campusRecord(
            QStringLiteral("default"),
            QStringLiteral("Default"),
            QStringLiteral("ignored-default-file")
            )
        );
    writeFile(
        directory.path(),
        QStringLiteral("default-record.json"),
        campusRecord(
            QStringLiteral("campus-default"),
            QStringLiteral("Default"),
            QStringLiteral("ignored-default-record")
            )
        );
    writeFile(
        directory.path(),
        QStringLiteral("malformed.json"),
        QByteArrayLiteral("{ not json")
        );

    const ClassMngr::Next::Platform::
        CalendarEventImportCampusCodeQueryAdapter adapter(directory.path());

    using CampusCode =
        ClassMngr::Next::Application::CalendarEventImportCampusCode;
    const std::vector<CampusCode> codes = adapter.loadCampusCodes();
    QCOMPARE(codes.size(), std::size_t(5));
    QCOMPARE(
        codes.at(0).value(),
        QStringLiteral("서울-01").toUtf8().toStdString()
        );
    QCOMPARE(codes.at(1).value(), std::string("same"));
    QCOMPARE(codes.at(2).value(), std::string("Same"));
    QCOMPARE(codes.at(3).value(), std::string("SAME"));
    QCOMPARE(
        codes.at(4).value(),
        QStringLiteral("마지막-code").toUtf8().toStdString()
        );
}

void NextPlatformCalendarEventImportCampusCodeQueryTests::emptyAndMissingDirectoriesReturnNoCodes()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const ClassMngr::Next::Platform::
        CalendarEventImportCampusCodeQueryAdapter emptyAdapter(directory.path());
    QVERIFY(emptyAdapter.loadCampusCodes().empty());

    const QString missingDirectory =
        QDir(directory.path()).filePath(QStringLiteral("missing"));
    const ClassMngr::Next::Platform::
        CalendarEventImportCampusCodeQueryAdapter missingAdapter(
            missingDirectory
            );
    QVERIFY(missingAdapter.loadCampusCodes().empty());
}

void NextPlatformCalendarEventImportCampusCodeQueryTests::portReturnsOwningUtf8Values()
{
    using Port =
        ClassMngr::Next::Application::CalendarEventImportCampusCodeQueryPort;
    static_assert(
        std::is_same_v<
            decltype(std::declval<const Port&>().loadCampusCodes()),
            std::vector<
                ClassMngr::Next::Application::CalendarEventImportCampusCode
                >
            >
        );

    QVERIFY(true);
}

QTEST_APPLESS_MAIN(NextPlatformCalendarEventImportCampusCodeQueryTests)

#include "next_platform_calendar_event_import_campus_code_query_adapter_tests.moc"
