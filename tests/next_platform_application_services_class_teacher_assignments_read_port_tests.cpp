#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/class_info.h"
#include "domain/models/teacher.h"
#include "next/platform/application_services_class_teacher_assignments_read_port.h"

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
        QStringLiteral("class-teacher-assignments-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Domain::TeacherId teacherId(const int value)
{
    const auto parsed = Domain::TeacherId::fromString(std::to_string(value));
    if (!parsed)
    {
        qFatal("Database teacher ID must have a typed representation.");
    }
    return *parsed;
}

}

class NextPlatformApplicationServicesClassTeacherAssignmentsReadPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void readsAssignedAndUnassignedRegularClassesInRepositoryOrder();
    void reportsRepositoryFailureAsTechnical();
    void reportsUnavailableSessionAsStructuredNotFound();
};

void NextPlatformApplicationServicesClassTeacherAssignmentsReadPortTests::
readsAssignedAndUnassignedRegularClassesInRepositoryOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);

    TeacherRepository* const teachers = session->teacherRepository();
    ClassRepository* const classes = session->classRepository();
    ClassInfoRepository* const classInfo = session->classInfoRepository();
    QVERIFY(teachers);
    QVERIFY(classes);
    QVERIFY(classInfo);

    Teacher teacher;
    teacher.teacherKr = QStringLiteral("Assigned");
    teacher.teacherEn = QStringLiteral("Assigned");
    const auto savedTeacherId = teachers->createTeacher(teacher);
    QVERIFY(savedTeacherId);

    const auto assignedClassId = classes->createClass(
        QStringLiteral("B assigned")
        );
    QVERIFY(assignedClassId);
    const auto unassignedClassId = classes->createClass(
        QStringLiteral("A unassigned")
        );
    QVERIFY(unassignedClassId);

    ClassInfo assignedInfo;
    assignedInfo.classId = assignedClassId.value();
    assignedInfo.teacherId = savedTeacherId.value();
    QVERIFY(classInfo->saveClassInfo(assignedInfo));

    Platform::ApplicationServicesClassTeacherAssignmentsReadPort port(
        &services
        );
    const auto result = port.readClassTeacherAssignments();

    QVERIFY(result);
    QCOMPARE(result.value().assignments.size(), std::size_t(2));
    QVERIFY(!result.value().assignments[0].teacherId);
    QVERIFY(result.value().assignments[1].teacherId);
    QCOMPARE(
        result.value().assignments[1].teacherId->value(),
        teacherId(savedTeacherId.value()).value()
        );
}

void NextPlatformApplicationServicesClassTeacherAssignmentsReadPortTests::
reportsRepositoryFailureAsTechnical()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlQuery dropClasses(services.databaseSession()->database());
    QVERIFY(dropClasses.exec(QStringLiteral("DROP TABLE classes")));

    Platform::ApplicationServicesClassTeacherAssignmentsReadPort port(
        &services
        );
    const auto result = port.readClassTeacherAssignments();

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().message.empty());
    QVERIFY(!result.error().recoverable);
}

void NextPlatformApplicationServicesClassTeacherAssignmentsReadPortTests::
reportsUnavailableSessionAsStructuredNotFound()
{
    ApplicationServices services;
    Platform::ApplicationServicesClassTeacherAssignmentsReadPort port(
        &services
        );

    const auto result = port.readClassTeacherAssignments();

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QCOMPARE(
        result.error().message,
        std::string(
            "The active database session for class teacher assignments is unavailable."
            )
        );
    QVERIFY(result.error().recoverable);
}

QTEST_GUILESS_MAIN(
    NextPlatformApplicationServicesClassTeacherAssignmentsReadPortTests
    )

#include "next_platform_application_services_class_teacher_assignments_read_port_tests.moc"
