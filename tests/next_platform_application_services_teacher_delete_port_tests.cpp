#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/teacher.h"
#include "next/application/teacher_delete.h"
#include "next/platform/application_services_teacher_delete_port.h"

#include <QSqlDatabase>
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
        QStringLiteral("teacher-delete-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Application::TeacherDeleteRequest request(const int value)
{
    const auto id = Domain::TeacherId::fromString(std::to_string(value));
    if (!id)
    {
        qFatal("Database teacher ID must have a typed representation.");
    }
    return {.teacherId = *id};
}

int createTeacher(TeacherRepository* repository, const QString& name)
{
    if (!repository)
    {
        return -1;
    }
    Teacher teacher;
    teacher.teacherKr = name;
    teacher.teacherEn = name;
    teacher.preferredName = name;
    const auto created = repository->createTeacher(teacher);
    return created ? *created : -1;
}

bool execute(
    QSqlDatabase database,
    const QString& sql,
    const QList<QVariant>& bindings = {}
    )
{
    QSqlQuery query(database);
    if (!query.prepare(sql))
    {
        return false;
    }
    for (const QVariant& binding : bindings)
    {
        query.addBindValue(binding);
    }
    return query.exec();
}

int insertClass(
    QSqlDatabase database,
    const QString& name,
    const int teacherId,
    const QString& day
    )
{
    QSqlQuery classQuery(database);
    classQuery.prepare(QStringLiteral("INSERT INTO classes (name) VALUES (?)"));
    classQuery.addBindValue(name);
    if (!classQuery.exec())
    {
        return -1;
    }
    const int classId = classQuery.lastInsertId().toInt();

    QSqlQuery info(database);
    info.prepare(QStringLiteral(
        "INSERT INTO class_info "
        "(class_id, teacher_id, class_grade, class_level, reading_book, notes) "
        "VALUES (?, ?, 'E4', 'Orion', 'Reader 1', 'Keep class notes')"
        ));
    info.addBindValue(classId);
    info.addBindValue(teacherId);
    if (!info.exec())
    {
        return -1;
    }

    QSqlQuery schedule(database);
    schedule.prepare(QStringLiteral(
        "INSERT INTO class_times "
        "(class_id, day, start_time, end_time) VALUES (?, ?, '09:00', '10:00')"
        ));
    schedule.addBindValue(classId);
    schedule.addBindValue(day);
    if (!schedule.exec())
    {
        return -1;
    }
    return classId;
}

QVariant value(
    QSqlDatabase database,
    const QString& sql,
    const int id
    )
{
    QSqlQuery query(database);
    query.prepare(sql);
    query.addBindValue(id);
    if (!query.exec() || !query.next())
    {
        return {};
    }
    return query.value(0);
}

int scalar(
    QSqlDatabase database,
    const QString& sql,
    const int id
    )
{
    return value(database, sql, id).toInt();
}

}

class NextPlatformApplicationServicesTeacherDeletePortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void deletesTeacherAndClearsOnlyItsAssignments();
    void rollsBackAssignmentClearingWhenTeacherDeleteFails();
    void reportsNullUnavailableAndClosedSessionsWithoutFallback();
};

void NextPlatformApplicationServicesTeacherDeletePortTests::
deletesTeacherAndClearsOnlyItsAssignments()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    TeacherRepository* const repository = session->teacherRepository();
    QVERIFY(repository);

    const int targetId = createTeacher(repository, QStringLiteral("Delete Target"));
    const int siblingId = createTeacher(repository, QStringLiteral("Keep Sibling"));
    QVERIFY(targetId > 0);
    QVERIFY(siblingId > 0);
    const int targetClass = insertClass(
        session->database(),
        QStringLiteral("Target Assignment"),
        targetId,
        QStringLiteral("Monday")
        );
    const int secondTargetClass = insertClass(
        session->database(),
        QStringLiteral("Second Target Assignment"),
        targetId,
        QStringLiteral("Wednesday")
        );
    const int siblingClass = insertClass(
        session->database(),
        QStringLiteral("Sibling Assignment"),
        siblingId,
        QStringLiteral("Tuesday")
        );
    QVERIFY(targetClass > 0);
    QVERIFY(secondTargetClass > 0);
    QVERIFY(siblingClass > 0);

    Platform::ApplicationServicesTeacherDeletePort port(services);
    const auto result = port.deleteTeacher(request(targetId));

    QVERIFY(result);
    QCOMPARE(scalar(session->database(),
                    QStringLiteral("SELECT COUNT(*) FROM teachers WHERE id=?"),
                    targetId), 0);
    QCOMPARE(scalar(session->database(),
                    QStringLiteral("SELECT COUNT(*) FROM teachers WHERE id=?"),
                    siblingId), 1);

    QSqlQuery targetInfo(session->database());
    targetInfo.prepare(QStringLiteral(
        "SELECT teacher_id, class_grade, class_level, reading_book, notes "
        "FROM class_info WHERE class_id=?"
        ));
    targetInfo.addBindValue(targetClass);
    QVERIFY(targetInfo.exec());
    QVERIFY(targetInfo.next());
    QVERIFY(targetInfo.value(0).isNull());
    QCOMPARE(targetInfo.value(1).toString(), QStringLiteral("E4"));
    QCOMPARE(targetInfo.value(2).toString(), QStringLiteral("Orion"));
    QCOMPARE(targetInfo.value(3).toString(), QStringLiteral("Reader 1"));
    QCOMPARE(targetInfo.value(4).toString(), QStringLiteral("Keep class notes"));

    QCOMPARE(scalar(session->database(),
                    QStringLiteral("SELECT COUNT(*) FROM class_info WHERE class_id=?"),
                    secondTargetClass), 1);
    QVERIFY(value(session->database(),
                  QStringLiteral("SELECT teacher_id FROM class_info WHERE class_id=?"),
                  secondTargetClass).isNull());
    QCOMPARE(scalar(session->database(),
                    QStringLiteral("SELECT COUNT(*) FROM classes WHERE id=?"),
                    secondTargetClass), 1);
    QCOMPARE(scalar(session->database(),
                    QStringLiteral("SELECT COUNT(*) FROM class_times WHERE class_id=?"),
                    secondTargetClass), 1);

    QCOMPARE(scalar(session->database(),
                    QStringLiteral("SELECT COUNT(*) FROM classes WHERE id=?"),
                    targetClass), 1);
    QCOMPARE(scalar(session->database(),
                    QStringLiteral("SELECT COUNT(*) FROM class_times WHERE class_id=?"),
                    targetClass), 1);
    QCOMPARE(scalar(session->database(),
                    QStringLiteral("SELECT teacher_id FROM class_info WHERE class_id=?"),
                    siblingClass), siblingId);
    QCOMPARE(scalar(session->database(),
                    QStringLiteral("SELECT COUNT(*) FROM class_times WHERE class_id=?"),
                    siblingClass), 1);
}

void NextPlatformApplicationServicesTeacherDeletePortTests::
rollsBackAssignmentClearingWhenTeacherDeleteFails()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    TeacherRepository* const repository = session->teacherRepository();
    QVERIFY(repository);

    const int targetId = createTeacher(repository, QStringLiteral("Rollback Target"));
    const int siblingId = createTeacher(repository, QStringLiteral("Rollback Sibling"));
    QVERIFY(targetId > 0);
    QVERIFY(siblingId > 0);
    const int targetClass = insertClass(
        session->database(),
        QStringLiteral("Rollback Target Assignment"),
        targetId,
        QStringLiteral("Wednesday")
        );
    const int siblingClass = insertClass(
        session->database(),
        QStringLiteral("Rollback Sibling Assignment"),
        siblingId,
        QStringLiteral("Thursday")
        );
    QVERIFY(targetClass > 0);
    QVERIFY(siblingClass > 0);
    QVERIFY(execute(
        session->database(),
        QStringLiteral(
            "CREATE TRIGGER fail_teacher_delete "
            "BEFORE DELETE ON teachers WHEN OLD.id=%1 "
            "BEGIN SELECT RAISE(ABORT, 'F363 injected teacher delete failure'); END"
            ).arg(targetId)
        ));

    Platform::ApplicationServicesTeacherDeletePort port(services);
    const auto result = port.deleteTeacher(request(targetId));

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().recoverable);
    QVERIFY(result.error().message.find("F363 injected teacher delete failure")
            != std::string::npos);
    QCOMPARE(scalar(session->database(),
                    QStringLiteral("SELECT COUNT(*) FROM teachers WHERE id=?"),
                    targetId), 1);
    QCOMPARE(scalar(session->database(),
                    QStringLiteral("SELECT teacher_id FROM class_info WHERE class_id=?"),
                    targetClass), targetId);
    QCOMPARE(scalar(session->database(),
                    QStringLiteral("SELECT teacher_id FROM class_info WHERE class_id=?"),
                    siblingClass), siblingId);
    QCOMPARE(scalar(session->database(),
                    QStringLiteral("SELECT COUNT(*) FROM class_times WHERE class_id=?"),
                    targetClass), 1);
    QCOMPARE(scalar(session->database(),
                    QStringLiteral("SELECT COUNT(*) FROM class_times WHERE class_id=?"),
                    siblingClass), 1);
}

void NextPlatformApplicationServicesTeacherDeletePortTests::
reportsNullUnavailableAndClosedSessionsWithoutFallback()
{
    Platform::ApplicationServicesTeacherDeletePort nullPort(
        static_cast<ApplicationServices*>(nullptr)
        );
    const auto nullResult = nullPort.deleteTeacher(request(42));
    QVERIFY(!nullResult);
    QCOMPARE(nullResult.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(!nullResult.error().message.empty());

    ApplicationServices unavailableServices;
    Platform::ApplicationServicesTeacherDeletePort unavailablePort(
        unavailableServices
        );
    const auto unavailableResult = unavailablePort.deleteTeacher(request(42));
    QVERIFY(!unavailableResult);
    QCOMPARE(unavailableResult.error().code, Domain::ErrorCode::NotFound);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices closedServices;
    QVERIFY(closedServices.openDatabase(databasePath(directory)));
    closedServices.closeDatabase();
    Platform::ApplicationServicesTeacherDeletePort closedPort(closedServices);
    const auto closedResult = closedPort.deleteTeacher(request(42));
    QVERIFY(!closedResult);
    QCOMPARE(closedResult.error().code, Domain::ErrorCode::NotFound);
}

QTEST_GUILESS_MAIN(NextPlatformApplicationServicesTeacherDeletePortTests)

#include "next_platform_application_services_teacher_delete_port_tests.moc"
