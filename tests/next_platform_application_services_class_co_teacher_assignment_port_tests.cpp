#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "next/application/class_co_teacher_assignment_use_case.h"
#include "next/platform/application_services_class_co_teacher_assignment_port.h"

#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("co-teacher-assignment-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

int createTeacher(ApplicationServices& services, const QString& name)
{
    Teacher teacher;
    teacher.teacherEn = name;
    teacher.preferredName = name;
    const auto saved = services.teacherService()->save(teacher);
    return saved ? *saved : -1;
}

bool sameTimes(const QList<ClassTime>& left, const QList<ClassTime>& right)
{
    if (left.size() != right.size())
    {
        return false;
    }

    for (qsizetype index = 0; index < left.size(); ++index)
    {
        if (left[index].day != right[index].day
            || left[index].startTime != right[index].startTime
            || left[index].endTime != right[index].endTime)
        {
            return false;
        }
    }

    return true;
}

bool sameClassFieldsExceptTeacher(
    const ClassInfo& actual,
    const ClassInfo& expected
    )
{
    return actual.classId == expected.classId
        && actual.classGrade == expected.classGrade
        && actual.classLevel == expected.classLevel
        && actual.readingBook == expected.readingBook
        && actual.essayBook == expected.essayBook
        && actual.classColor == expected.classColor
        && actual.fontColor == expected.fontColor
        && actual.notes == expected.notes
        && actual.timeFillerActivities == expected.timeFillerActivities
        && sameTimes(actual.classTimes, expected.classTimes)
        && sameTimes(actual.intensiveTimes, expected.intensiveTimes);
}

}

class NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void assignmentUpdatesOnlyTeacherAndSupportsUnassignedState();
    void unavailableSessionReturnsStructuredFailure();
};

void NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests::
assignmentUpdatesOnlyTeacherAndSupportsUnassignedState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const auto createdClass = services.classService()->create(
        QStringLiteral("Co-Teacher Assignment Test")
        );
    QVERIFY(createdClass);

    const int firstTeacherId = createTeacher(
        services,
        QStringLiteral("First Teacher")
        );
    const int secondTeacherId = createTeacher(
        services,
        QStringLiteral("Second Teacher")
        );
    QVERIFY(firstTeacherId > 0);
    QVERIFY(secondTeacherId > 0);

    const auto loaded = services.classService()->classInfo(*createdClass);
    QVERIFY(loaded);
    ClassInfo original = *loaded;
    original.teacherId = firstTeacherId;
    original.classGrade = QStringLiteral("E4");
    original.classLevel = QStringLiteral("Theseus");
    original.classColor = QStringLiteral("#123456");
    original.fontColor = QStringLiteral("#654321");
    original.notes = QStringLiteral("Preserved class notes");
    original.timeFillerActivities = QStringLiteral("Preserved activities");
    original.classTimes = {
        {QStringLiteral("Monday"), QStringLiteral("9:00 AM"),
         QStringLiteral("9:50 AM")},
        {QStringLiteral("Wednesday"), QStringLiteral("9:00 AM"),
         QStringLiteral("9:50 AM")}
    };
    original.intensiveTimes = {
        {QStringLiteral("Friday"), QStringLiteral("10:00 AM"),
         QStringLiteral("10:50 AM")}
    };
    const Status initialSave = services.classService()->saveClassInfo(original);
    QVERIFY(initialSave);

    Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);
    const auto assigned = Application::ClassCoTeacherAssignmentUseCase::execute(
        *createdClass,
        secondTeacherId,
        port
        );
    QVERIFY(assigned);

    auto afterAssign = services.classService()->classInfo(*createdClass);
    QVERIFY(afterAssign);
    QCOMPARE(afterAssign->teacherId, secondTeacherId);
    QVERIFY(sameClassFieldsExceptTeacher(*afterAssign, original));

    const auto unassigned =
        Application::ClassCoTeacherAssignmentUseCase::execute(
            *createdClass,
            -1,
            port
            );
    QVERIFY(unassigned);

    const auto afterUnassign = services.classService()->classInfo(*createdClass);
    QVERIFY(afterUnassign);
    QCOMPARE(afterUnassign->teacherId, -1);
    QVERIFY(sameClassFieldsExceptTeacher(*afterUnassign, original));
}

void NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests::
unavailableSessionReturnsStructuredFailure()
{
    ApplicationServices services;
    Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);

    const auto result = Application::ClassCoTeacherAssignmentUseCase::execute(
        1,
        -1,
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(QString::fromStdString(result.error().message).contains(
        QStringLiteral("unavailable")
        ));
}

QTEST_MAIN(NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests)

#include "next_platform_application_services_class_co_teacher_assignment_port_tests.moc"
