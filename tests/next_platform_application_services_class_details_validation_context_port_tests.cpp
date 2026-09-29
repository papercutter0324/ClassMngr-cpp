#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "next/application/class_details_validation_context_query.h"
#include "next/platform/application_services_class_details_validation_context_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("class-details-validation-context-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Domain::ClassId classId(const int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

int createClass(ApplicationServices& services, const QString& name)
{
    return services.classService()->create(name).value_or(-1);
}

bool seedClassInfo(ApplicationServices& services, const int classIdValue)
{
    const auto info = services.classService()->classInfo(classIdValue);
    return info
        && services.classService()->saveClassInfo(*info).has_value();
}

Application::ClassDetailsValidationContextResult readContext(
    ApplicationServices* services,
    const Domain::ClassId& requestedClassId
    )
{
    Platform::ApplicationServicesClassDetailsValidationContextPort port(
        services
        );
    return Application::ClassDetailsValidationContextQuery::execute(
        requestedClassId,
        port
        );
}

void verifyUnavailable(
    const Application::ClassDetailsValidationContextResult& result
    )
{
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(!result.error().message.empty());
}

}

class NextPlatformApplicationServicesClassDetailsValidationContextPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void readsSentinelTeacherIdAndExactTextFromActiveRepository();
    void missingClassInfoRowPreservesRepositoryDefaults();
    void unavailableSessionIsNotFoundWithoutFallback();
    void repositoryErrorIsTechnical();
};

void NextPlatformApplicationServicesClassDetailsValidationContextPortTests::
readsSentinelTeacherIdAndExactTextFromActiveRepository()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int id = createClass(
        services,
        QStringLiteral("Validation Context Raw Fields")
        );
    QVERIFY(id > 0);
    QVERIFY(seedClassInfo(services, id));

    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session && session->isOpen());
    QSqlQuery foreignKeys(session->database());
    QVERIFY2(foreignKeys.exec(QStringLiteral("PRAGMA foreign_keys = OFF")),
        qPrintable(foreignKeys.lastError().text()));

    const QString notes = QStringLiteral("  \uD55C\uAE00 notes\n  ");
    const QString activities = QStringLiteral("\u00a0activities\u00a0");
    QSqlQuery update(session->database());
    update.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id=?, notes=?, "
        "time_filler_activities=? WHERE class_id=?"
        ));
    update.addBindValue(0);
    update.addBindValue(notes);
    update.addBindValue(activities);
    update.addBindValue(id);
    QVERIFY2(update.exec(), qPrintable(update.lastError().text()));
    QCOMPARE(update.numRowsAffected(), 1);

    const auto result = readContext(&services, classId(id));
    QVERIFY(result);
    QCOMPARE(result.value().requestedClassId.value(), std::to_string(id));
    QCOMPARE(result.value().matchedClassId.value(), std::to_string(id));
    QCOMPARE(result.value().teacherId, 0);
    QCOMPARE(result.value().notes, notes.toStdU16String());
    QCOMPARE(
        result.value().timeFillerActivities,
        activities.toStdU16String()
        );

    QSqlQuery negativeSentinel(session->database());
    negativeSentinel.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id=? WHERE class_id=?"
        ));
    negativeSentinel.addBindValue(-2);
    negativeSentinel.addBindValue(id);
    QVERIFY2(negativeSentinel.exec(),
        qPrintable(negativeSentinel.lastError().text()));
    QCOMPARE(negativeSentinel.numRowsAffected(), 1);

    const auto negativeResult = readContext(&services, classId(id));
    QVERIFY(negativeResult);
    QCOMPARE(negativeResult.value().teacherId, -2);
    QCOMPARE(negativeResult.value().notes, notes.toStdU16String());
    QCOMPARE(
        negativeResult.value().timeFillerActivities,
        activities.toStdU16String()
        );
}

void NextPlatformApplicationServicesClassDetailsValidationContextPortTests::
missingClassInfoRowPreservesRepositoryDefaults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int id = createClass(
        services,
        QStringLiteral("Validation Context Missing Info")
        );
    QVERIFY(id > 0);
    QVERIFY(seedClassInfo(services, id));

    QSqlQuery remove(services.databaseSession()->database());
    remove.prepare(QStringLiteral("DELETE FROM class_info WHERE class_id=?"));
    remove.addBindValue(id);
    QVERIFY2(remove.exec(), qPrintable(remove.lastError().text()));
    QCOMPARE(remove.numRowsAffected(), 1);

    const auto result = readContext(&services, classId(id));
    QVERIFY(result);
    QCOMPARE(result.value().requestedClassId.value(), std::to_string(id));
    QCOMPARE(result.value().matchedClassId.value(), std::to_string(id));
    QCOMPARE(result.value().teacherId, -1);
    QVERIFY(result.value().notes.empty());
    QVERIFY(result.value().timeFillerActivities.empty());
}

void NextPlatformApplicationServicesClassDetailsValidationContextPortTests::
unavailableSessionIsNotFoundWithoutFallback()
{
    verifyUnavailable(readContext(nullptr, classId(42)));

    ApplicationServices unopenedServices;
    QVERIFY(unopenedServices.dataService());
    verifyUnavailable(readContext(&unopenedServices, classId(42)));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices closedServices;
    QVERIFY(closedServices.openDatabase(databasePath(directory)));
    DatabaseSession* const session = closedServices.databaseSession();
    QVERIFY(session && session->isOpen());
    closedServices.closeDatabase();
    QVERIFY(!session->isOpen());
    verifyUnavailable(readContext(&closedServices, classId(42)));
}

void NextPlatformApplicationServicesClassDetailsValidationContextPortTests::
repositoryErrorIsTechnical()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int id = createClass(
        services,
        QStringLiteral("Validation Context Repository Error")
        );
    QVERIFY(id > 0);

    QSqlQuery query(services.databaseSession()->database());
    QVERIFY2(query.exec(QStringLiteral("DROP TABLE class_info")),
        qPrintable(query.lastError().text()));

    const auto result = readContext(&services, classId(id));
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().message.empty());
}

QTEST_MAIN(
    NextPlatformApplicationServicesClassDetailsValidationContextPortTests
    )

#include "next_platform_application_services_class_details_validation_context_port_tests.moc"
