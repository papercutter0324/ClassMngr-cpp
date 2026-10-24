#include "core/application_services.h"
#include "data/database/workspace_store.h"
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
    void preservesWideTeacherIdentitiesAndIntClassIds();
    void readsAssignedAndUnassignedRegularClassesInRepositoryOrder();
    void reportsRepositoryFailureAsTechnical();
    void reportsUnavailableSessionAsStructuredNotFound();
};

void NextPlatformApplicationServicesClassTeacherAssignmentsReadPortTests::
preservesWideTeacherIdentitiesAndIntClassIds()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    WorkspaceStore* const store = services.workspaceStore();
    Platform::ApplicationServicesClassTeacherAssignmentsReadPort port(&services);
    int expectedClassId = 0;
    for (const qint64 id : {qint64{2147483648LL}, qint64{9223372036854775807LL}})
    {
        QSqlQuery seed(store->database());
        seed.prepare(QStringLiteral("INSERT INTO teachers (id, teacher_en) VALUES (?, 'Wide Teacher')"));
        seed.addBindValue(id);
        QVERIFY(seed.exec());
        const auto createdClass = store->classRepository()->createClass(QString::number(id));
        QVERIFY(createdClass);
        expectedClassId = *createdClass;
        seed.prepare(QStringLiteral("INSERT INTO class_info (class_id, teacher_id) VALUES (?, ?)"));
        seed.addBindValue(*createdClass);
        seed.addBindValue(id);
        QVERIFY(seed.exec());
    }
    const auto records = store->classInfoRepository()->loadClassTeacherAssignmentRecords();
    QVERIFY(records);
    QCOMPARE(records->size(), 2);
    QCOMPARE(records->at(0).teacherId, qint64{2147483648LL});
    QCOMPARE(records->at(1).teacherId, qint64{9223372036854775807LL});
    QCOMPARE(records->at(1).classId, expectedClassId);
    const auto result = port.readClassTeacherAssignments();
    QVERIFY(result);
    QCOMPARE(result.value().assignments.size(), std::size_t{2});
    QCOMPARE(result.value().assignments[0].teacherId->value(), std::string("2147483648"));
    QCOMPARE(result.value().assignments[1].teacherId->value(), std::string("9223372036854775807"));
}

void NextPlatformApplicationServicesClassTeacherAssignmentsReadPortTests::
readsAssignedAndUnassignedRegularClassesInRepositoryOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);

    TeacherRepository* const teachers = store->teacherRepository();
    ClassRepository* const classes = store->classRepository();
    ClassInfoRepository* const classInfo = store->classInfoRepository();
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
    QSqlQuery dropClasses(services.workspaceStore()->database());
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
