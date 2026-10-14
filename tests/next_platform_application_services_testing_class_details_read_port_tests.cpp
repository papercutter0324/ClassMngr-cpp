#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "data/repositories/testing_class_repository.h"
#include "next/application/testing_class_details_read_query.h"
#include "next/platform/application_services_testing_class_details_read_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>
#include <QVariant>

#include <string>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("testing-class-details-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Domain::ClassId classId(const int value)
{
    const auto parsed = Domain::ClassId::fromString(std::to_string(value));
    if (!parsed)
    {
        qFatal("Database class ID must have a typed representation.");
    }
    return *parsed;
}

Domain::TeacherId teacherId(const qint64 value)
{
    const auto parsed = Domain::TeacherId::fromString(std::to_string(value));
    if (!parsed)
    {
        qFatal("Database teacher ID must have a typed representation.");
    }
    return *parsed;
}

Application::TestingClassDetailsReadResult readDetails(
    ApplicationServices& services,
    const int id
    )
{
    Platform::ApplicationServicesTestingClassDetailsReadPort port(services);
    return Application::TestingClassDetailsReadQueryHandler::execute(
        {.classId = classId(id)},
        port
        );
}

TestingClass testingClass(const QString& name)
{
    TestingClass value;
    value.name = name;
    value.grade = QStringLiteral("M2");
    value.level = QStringLiteral("Mixed (High)");
    value.room = QStringLiteral("Room 204");
    value.classColor = QStringLiteral("#123456");
    value.fontColor = QStringLiteral("#FEDCBA");
    value.notes = QStringLiteral("Original notes \uD14C\uC2A4\uD2B8");
    return value;
}

void verifyNotFound(
    const Application::TestingClassDetailsReadResult& result
    )
{
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(!result.error().message.empty());
}

}

class NextPlatformApplicationServicesTestingClassDetailsReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void mapsEveryFieldFromTheActiveSessionRepository();
    void mapsAnAbsentTeacherAsAnEmptyOptional();
    void mapsMissingClassInfoToDefaultDetails();
    void mapsStoredZeroTeacherIdAsUnassigned();
    void rejectsMalformedStoredTeacherIdentifiers();
    void reportsUnavailableSessionsAsNotFound();
    void reportsDeletedParentClassesAsNotFoundAndPreservesLegacyDiagnostic();
    void reportsMissingTestingClassesAsWarningsWithoutFallback();
    void forwardsRepositoryFailuresAsNonNotFoundErrors();
    void rejectsNonCanonicalLegacyIdentifiers();
};

void NextPlatformApplicationServicesTestingClassDetailsReadPortTests::
mapsEveryFieldFromTheActiveSessionRepository()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    TestingClassRepository* const repository =
        session->testingClassRepository();
    QVERIFY(repository);

    constexpr qint64 wideTeacherId = 2'147'483'648LL;
    QSqlQuery insertTeacher(session->database());
    QVERIFY(insertTeacher.prepare(QStringLiteral(R"(
        INSERT INTO teachers (id, teacher_kr, teacher_en)
        VALUES (?, ?, ?)
    )")));
    insertTeacher.addBindValue(QVariant::fromValue<qlonglong>(wideTeacherId));
    insertTeacher.addBindValue(QStringLiteral("\uAE40\uC120\uC0DD\uB2D8"));
    insertTeacher.addBindValue(QStringLiteral("Test Teacher"));
    QVERIFY2(insertTeacher.exec(), qPrintable(insertTeacher.lastError().text()));

    TestingClass value = testingClass(
        QStringLiteral("Writing Lab \uC2E4\uD5D8")
        );
    value.grade = QStringLiteral("E5");
    value.level = QStringLiteral("Song's");
    value.room = QStringLiteral("Library \uC2E4\uD5D8\uC2E4");
    value.notes = QStringLiteral("Keep exact notes \uC0C1\uC138 \uC8FC\uC758");
    const auto createdClass = repository->createTestingClass(value);
    QVERIFY(createdClass);

    QSqlQuery assignTeacher(session->database());
    QVERIFY(assignTeacher.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id=? WHERE class_id=?")));
    assignTeacher.addBindValue(QVariant::fromValue<qlonglong>(wideTeacherId));
    assignTeacher.addBindValue(*createdClass);
    QVERIFY2(assignTeacher.exec(), qPrintable(assignTeacher.lastError().text()));
    QCOMPARE(assignTeacher.numRowsAffected(), 1);

    QSqlQuery verifyStoredTeacherId(session->database());
    QVERIFY(verifyStoredTeacherId.prepare(QStringLiteral(R"(
        SELECT typeof(teachers.id), teachers.id,
               typeof(class_info.teacher_id), class_info.teacher_id
        FROM teachers
        INNER JOIN class_info ON class_info.teacher_id = teachers.id
        WHERE class_info.class_id = ?
    )")));
    verifyStoredTeacherId.addBindValue(*createdClass);
    QVERIFY2(verifyStoredTeacherId.exec(),
             qPrintable(verifyStoredTeacherId.lastError().text()));
    QVERIFY(verifyStoredTeacherId.next());
    QCOMPARE(
        verifyStoredTeacherId.value(0).toString(),
        QStringLiteral("integer")
        );
    QCOMPARE(verifyStoredTeacherId.value(1).toLongLong(), wideTeacherId);
    QCOMPARE(
        verifyStoredTeacherId.value(2).toString(),
        QStringLiteral("integer")
        );
    QCOMPARE(verifyStoredTeacherId.value(3).toLongLong(), wideTeacherId);
    QVERIFY(!verifyStoredTeacherId.next());

    const TestingClassDetailsReadMetrics metricsBefore =
        repository->testingClassDetailsReadMetrics();
    const auto result = readDetails(services, *createdClass);
    const TestingClassDetailsReadMetrics metricsAfter =
        repository->testingClassDetailsReadMetrics();

    QVERIFY(result);
    QCOMPARE(metricsAfter.callCount - metricsBefore.callCount, 1);
    QCOMPARE(metricsAfter.statementCount - metricsBefore.statementCount, 1);
    const Application::TestingClassDetailsSnapshot expected{
        .classId = classId(*createdClass),
        .name = u"Writing Lab \uC2E4\uD5D8",
        .grade = u"E5",
        .level = u"Song's",
        .room = u"Library \uC2E4\uD5D8\uC2E4",
        .teacherId = teacherId(wideTeacherId),
        .classColor = u"#123456",
        .fontColor = u"#FEDCBA",
        .notes = u"Keep exact notes \uC0C1\uC138 \uC8FC\uC758"
    };
    QVERIFY(result.value() == expected);
    QCOMPARE(result.value().classId, classId(*createdClass));
    QCOMPARE(result.value().name, std::u16string(u"Writing Lab \uC2E4\uD5D8"));
    QCOMPARE(result.value().grade, std::u16string(u"E5"));
    QCOMPARE(result.value().level, std::u16string(u"Song's"));
    QCOMPARE(
        result.value().room,
        std::u16string(u"Library \uC2E4\uD5D8\uC2E4")
        );
    QVERIFY(result.value().teacherId.has_value());
    QCOMPARE(result.value().teacherId.value(), teacherId(wideTeacherId));
    QCOMPARE(result.value().classColor, std::u16string(u"#123456"));
    QCOMPARE(result.value().fontColor, std::u16string(u"#FEDCBA"));
    QCOMPARE(
        result.value().notes,
        std::u16string(u"Keep exact notes \uC0C1\uC138 \uC8FC\uC758")
        );
}

void NextPlatformApplicationServicesTestingClassDetailsReadPortTests::
mapsAnAbsentTeacherAsAnEmptyOptional()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    TestingClassRepository* const repository =
        services.databaseSession()->testingClassRepository();
    QVERIFY(repository);
    const auto createdClass = repository->createTestingClass(
        testingClass(QStringLiteral("No Teacher Class"))
        );
    QVERIFY(createdClass);

    const auto result = readDetails(services, *createdClass);

    QVERIFY(result);
    QVERIFY(!result.value().teacherId.has_value());
}

void NextPlatformApplicationServicesTestingClassDetailsReadPortTests::
mapsMissingClassInfoToDefaultDetails()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    TestingClassRepository* const repository =
        session->testingClassRepository();
    QVERIFY(repository);
    const auto createdClass = repository->createTestingClass(
        testingClass(QStringLiteral("Testing Class Without Class Info"))
        );
    QVERIFY(createdClass);

    QSqlQuery removeClassInfo(session->database());
    removeClassInfo.prepare(QStringLiteral(
        "DELETE FROM class_info WHERE class_id=?"
        ));
    removeClassInfo.addBindValue(*createdClass);
    QVERIFY2(removeClassInfo.exec(),
             qPrintable(removeClassInfo.lastError().text()));
    QCOMPARE(removeClassInfo.numRowsAffected(), 1);

    const auto result = readDetails(services, *createdClass);

    QVERIFY(result);
    QCOMPARE(result.value().classId, classId(*createdClass));
    QCOMPARE(
        result.value().name,
        std::u16string(u"Testing Class Without Class Info")
        );
    QCOMPARE(result.value().grade, std::u16string{});
    QCOMPARE(result.value().level, std::u16string{});
    QCOMPARE(result.value().room, std::u16string(u"Room 204"));
    QVERIFY(!result.value().teacherId.has_value());
    QCOMPARE(result.value().classColor, std::u16string(u"#FFFFFF"));
    QCOMPARE(result.value().fontColor, std::u16string(u"#000000"));
    QCOMPARE(result.value().notes, std::u16string{});
}

void NextPlatformApplicationServicesTestingClassDetailsReadPortTests::
mapsStoredZeroTeacherIdAsUnassigned()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    TestingClassRepository* const repository =
        services.databaseSession()->testingClassRepository();
    QVERIFY(repository);
    const auto createdClass = repository->createTestingClass(
        testingClass(QStringLiteral("Zero Teacher ID Class"))
        );
    QVERIFY(createdClass);

    QSqlQuery addLegacyZeroTeacher(
        services.databaseSession()->database()
        );
    QVERIFY(addLegacyZeroTeacher.exec(QStringLiteral(
        "INSERT INTO teachers (id, teacher_en) "
        "VALUES (0, 'Legacy Unassigned')"
        )));

    QSqlQuery setLegacyZeroTeacherId(
        services.databaseSession()->database()
        );
    setLegacyZeroTeacherId.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id=0 WHERE class_id=?"
        ));
    setLegacyZeroTeacherId.addBindValue(*createdClass);
    QVERIFY2(setLegacyZeroTeacherId.exec(),
             qPrintable(setLegacyZeroTeacherId.lastError().text()));

    const auto legacyRead = repository->loadTestingClass(*createdClass);
    QVERIFY(legacyRead);
    QCOMPARE(legacyRead->teacherId, 0);

    const auto result = readDetails(services, *createdClass);

    QVERIFY(result);
    QVERIFY(!result.value().teacherId.has_value());
}

void NextPlatformApplicationServicesTestingClassDetailsReadPortTests::
rejectsMalformedStoredTeacherIdentifiers()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    TestingClassRepository* const repository =
        services.databaseSession()->testingClassRepository();
    QVERIFY(repository);
    const auto createdClass = repository->createTestingClass(
        testingClass(QStringLiteral("Malformed Teacher ID Class"))
        );
    QVERIFY(createdClass);

    QSqlQuery storeMalformedTeacherId(
        services.databaseSession()->database()
        );
    QVERIFY2(storeMalformedTeacherId.exec(QStringLiteral(
                 "PRAGMA foreign_keys=OFF"
                 )),
             qPrintable(storeMalformedTeacherId.lastError().text()));
    storeMalformedTeacherId.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id=? WHERE class_id=?"));
    storeMalformedTeacherId.addBindValue(QStringLiteral("not-a-number"));
    storeMalformedTeacherId.addBindValue(*createdClass);
    QVERIFY2(storeMalformedTeacherId.exec(),
             qPrintable(storeMalformedTeacherId.lastError().text()));
    QCOMPARE(storeMalformedTeacherId.numRowsAffected(), 1);

    const auto stored = repository->loadTestingClass(*createdClass);
    QVERIFY(!stored);
    QVERIFY(stored.error().contains(QStringLiteral("teacher identifier"),
                                   Qt::CaseInsensitive));
    const auto listed = repository->loadTestingClasses();
    QVERIFY(!listed);
    QCOMPARE(listed.error(), stored.error());

    const auto result = readDetails(services, *createdClass);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(QString::fromStdString(result.error().message).contains(
        QStringLiteral("teacher identifier"), Qt::CaseInsensitive));
}

void NextPlatformApplicationServicesTestingClassDetailsReadPortTests::
reportsUnavailableSessionsAsNotFound()
{
    Platform::ApplicationServicesTestingClassDetailsReadPort nullPort(
        static_cast<ApplicationServices*>(nullptr)
        );
    const auto nullResult =
        Application::TestingClassDetailsReadQueryHandler::execute(
            {.classId = classId(42)},
            nullPort
            );
    verifyNotFound(nullResult);

    ApplicationServices unopenedServices;
    Platform::ApplicationServicesTestingClassDetailsReadPort unopenedPort(
        unopenedServices
        );
    const auto unopenedResult =
        Application::TestingClassDetailsReadQueryHandler::execute(
            {.classId = classId(42)},
            unopenedPort
            );
    verifyNotFound(unopenedResult);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices closedServices;
    QVERIFY(closedServices.openDatabase(databasePath(directory)));
    closedServices.closeDatabase();
    Platform::ApplicationServicesTestingClassDetailsReadPort closedPort(
        closedServices
        );
    const auto closedResult =
        Application::TestingClassDetailsReadQueryHandler::execute(
            {.classId = classId(42)},
            closedPort
            );
    verifyNotFound(closedResult);
}

void NextPlatformApplicationServicesTestingClassDetailsReadPortTests::
reportsDeletedParentClassesAsNotFoundAndPreservesLegacyDiagnostic()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    TestingClassRepository* const repository =
        session->testingClassRepository();
    QVERIFY(repository);
    const auto createdClass = repository->createTestingClass(
        testingClass(QStringLiteral("Testing Class With Deleted Parent"))
        );
    QVERIFY(createdClass);

    QVERIFY(services.classService()->remove(*createdClass));

    QSqlQuery counts(session->database());
    QVERIFY(counts.prepare(QStringLiteral(R"(
        SELECT
            (SELECT COUNT(*) FROM classes WHERE id=?),
            (SELECT COUNT(*) FROM class_info WHERE class_id=?),
            (SELECT COUNT(*) FROM testing_classes WHERE class_id=?)
    )")));
    counts.addBindValue(*createdClass);
    counts.addBindValue(*createdClass);
    counts.addBindValue(*createdClass);
    QVERIFY2(counts.exec(), qPrintable(counts.lastError().text()));
    QVERIFY(counts.next());
    QCOMPARE(counts.value(0).toInt(), 0);
    QCOMPARE(counts.value(1).toInt(), 0);
    QCOMPARE(counts.value(2).toInt(), 0);

    const auto legacyResult =
        repository->loadTestingClassDetailsReadRecord(*createdClass);
    QVERIFY(!legacyResult);
    QCOMPARE(
        legacyResult.error(),
        QStringLiteral("The testing class was not found.")
        );

    const TestingClassDetailsReadMetrics metricsBefore =
        repository->testingClassDetailsReadMetrics();
    const auto result = readDetails(services, *createdClass);
    const TestingClassDetailsReadMetrics metricsAfter =
        repository->testingClassDetailsReadMetrics();

    verifyNotFound(result);
    QCOMPARE(metricsAfter.callCount - metricsBefore.callCount, 1);
    QCOMPARE(metricsAfter.statementCount - metricsBefore.statementCount, 1);
    QCOMPARE(
        QString::fromStdString(result.error().message),
        QStringLiteral("The testing class was not found.")
        );
}

void NextPlatformApplicationServicesTestingClassDetailsReadPortTests::
reportsMissingTestingClassesAsWarningsWithoutFallback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    TestingClassRepository* const repository =
        services.databaseSession()->testingClassRepository();
    QVERIFY(repository);
    const auto createdClass = repository->createTestingClass(
        testingClass(QStringLiteral("Testing Class Removed From Roster"))
        );
    QVERIFY(createdClass);

    QSqlQuery removeTestingClass(
        services.databaseSession()->database()
        );
    removeTestingClass.prepare(QStringLiteral(
        "DELETE FROM testing_classes WHERE class_id=?"
        ));
    removeTestingClass.addBindValue(*createdClass);
    QVERIFY(removeTestingClass.exec());

    QSqlQuery verifyParentExists(
        services.databaseSession()->database()
        );
    verifyParentExists.prepare(QStringLiteral(
        "SELECT 1 FROM classes WHERE id=?"
        ));
    verifyParentExists.addBindValue(*createdClass);
    QVERIFY2(verifyParentExists.exec(),
             qPrintable(verifyParentExists.lastError().text()));
    QVERIFY(verifyParentExists.next());

    const auto legacyResult =
        repository->loadTestingClassDetailsReadRecord(*createdClass);
    QVERIFY(!legacyResult);
    QCOMPARE(
        legacyResult.error(),
        QStringLiteral("The testing class was not found.")
        );

    const TestingClassDetailsReadMetrics metricsBefore =
        repository->testingClassDetailsReadMetrics();
    const auto result = readDetails(services, *createdClass);
    const TestingClassDetailsReadMetrics metricsAfter =
        repository->testingClassDetailsReadMetrics();

    QVERIFY(!result);
    QCOMPARE(metricsAfter.callCount - metricsBefore.callCount, 1);
    QCOMPARE(metricsAfter.statementCount - metricsBefore.statementCount, 1);
    QVERIFY(result.error().code != Domain::ErrorCode::NotFound);
    const QString errorText = QString::fromStdString(result.error().message);
    QCOMPARE(errorText, QStringLiteral("The testing class was not found."));
}

void NextPlatformApplicationServicesTestingClassDetailsReadPortTests::
forwardsRepositoryFailuresAsNonNotFoundErrors()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    TestingClassRepository* const repository =
        services.databaseSession()->testingClassRepository();
    QVERIFY(repository);
    const auto createdClass = repository->createTestingClass(
        testingClass(QStringLiteral("Repository Failure Source"))
        );
    QVERIFY(createdClass);

    QSqlQuery dropTestingClasses(
        services.databaseSession()->database()
        );
    QVERIFY(dropTestingClasses.exec(QStringLiteral(
        "DROP TABLE testing_classes"
        )));

    const auto result = readDetails(services, *createdClass);

    QVERIFY(!result);
    QVERIFY(result.error().code != Domain::ErrorCode::NotFound);
    QVERIFY(!result.error().message.empty());
}

void NextPlatformApplicationServicesTestingClassDetailsReadPortTests::
rejectsNonCanonicalLegacyIdentifiers()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    Platform::ApplicationServicesTestingClassDetailsReadPort port(services);

    const auto result =
        Application::TestingClassDetailsReadQueryHandler::execute(
            {
                .classId = *Domain::ClassId::fromString("042")
            },
            port
            );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
}

QTEST_GUILESS_MAIN(
    NextPlatformApplicationServicesTestingClassDetailsReadPortTests
    )

#include "next_platform_application_services_testing_class_details_read_port_tests.moc"
