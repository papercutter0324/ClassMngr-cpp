#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "next/application/class_co_teacher_assignment_use_case.h"
#include "next/platform/application_services_class_co_teacher_assignment_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>
#include <vector>

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

int createClass(ApplicationServices& services, const QString& name)
{
    const auto created = services.classService()->create(name);
    return created ? *created : -1;
}

int createTeacher(ApplicationServices& services, const QString& name)
{
    Teacher teacher;
    teacher.teacherEn = name;
    teacher.preferredName = name;
    const auto saved = services.teacherService()->save(teacher);
    return saved ? *saved : -1;
}

ClassInfo newClassInfo(
    ApplicationServices& services,
    const int classId,
    const int teacherId = -1
    )
{
    ClassInfoRepository* const repository =
        services.databaseSession()->classInfoRepository();
    if (!repository)
    {
        qFatal("Class info repository must be available in test setup.");
    }

    const auto loaded = repository->loadClassInfo(classId);
    if (!loaded)
    {
        qFatal("Class info must load during test setup.");
    }

    ClassInfo info = *loaded;
    info.teacherId = teacherId;
    info.classGrade = QStringLiteral("E4");
    info.classLevel = QStringLiteral("Theseus");
    info.readingBook.clear();
    info.essayBook.clear();
    info.classColor = QStringLiteral("#123456");
    info.fontColor = QStringLiteral("#654321");
    info.notes = QStringLiteral("Preserved class notes");
    info.timeFillerActivities = QStringLiteral("Preserved activities");
    return info;
}

bool saveClassInfo(
    ApplicationServices& services,
    const ClassInfo& info,
    QString* failureMessage = nullptr
    )
{
    ClassInfoRepository* const repository =
        services.databaseSession()->classInfoRepository();
    if (!repository)
    {
        if (failureMessage)
        {
            *failureMessage = QStringLiteral("Class info repository unavailable");
        }
        return false;
    }

    const Status saved = repository->saveClassInfo(info);
    if (!saved && failureMessage)
    {
        *failureMessage = saved.error();
    }
    return saved.has_value();
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

void copyTeacherMetadata(ClassInfo& info, const Teacher& teacher)
{
    info.teacherKr = teacher.teacherKr;
    info.teacherEn = teacher.teacherEn;
    info.teacherPreferredName = teacher.preferredName;
    info.roomNumber = teacher.roomNumber;
    info.wifiName = teacher.wifiName;
    info.wifiPassword = teacher.wifiPassword;
    info.internetType = teacher.internetType;
    info.zoomId = teacher.zoomId;
    info.zoomPassword = teacher.zoomPassword;
    info.projectionType = teacher.projectionType;
}

bool sameTeacherMetadata(const ClassInfo& actual, const ClassInfo& expected)
{
    return actual.teacherKr == expected.teacherKr
        && actual.teacherEn == expected.teacherEn
        && actual.teacherPreferredName == expected.teacherPreferredName
        && actual.roomNumber == expected.roomNumber
        && actual.wifiName == expected.wifiName
        && actual.wifiPassword == expected.wifiPassword
        && actual.internetType == expected.internetType
        && actual.zoomId == expected.zoomId
        && actual.zoomPassword == expected.zoomPassword
        && actual.projectionType == expected.projectionType;
}

bool teacherMetadataIsEmpty(const ClassInfo& actual)
{
    const ClassInfo emptyMetadata;
    return sameTeacherMetadata(actual, emptyMetadata);
}

bool sameStoredClassFieldsExceptTeacherId(
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

bool sameCompleteClassInfo(const ClassInfo& actual, const ClassInfo& expected)
{
    return actual.teacherId == expected.teacherId
        && sameStoredClassFieldsExceptTeacherId(actual, expected)
        && sameTeacherMetadata(actual, expected);
}

}

class NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void assignmentUpdatesOnlyTeacherAndSupportsUnassignedState();
    void malformedClassIdIsRejectedWithoutChangingPersistedFields();
    void invalidLoadedClassInfoIsRejectedWithoutWriting();
    void regularScheduleConflictUsesLegacyMessageAndPreservesState();
    void intensiveScheduleConflictUsesLegacyMessageAndPreservesState();
    void regularConflictPrecedesIntensiveConflict();
    void repositoryFailureRollsBackClassInfoAndSchedules();
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
    Teacher secondTeacher;
    secondTeacher.teacherEn = QStringLiteral("Second Teacher");
    secondTeacher.preferredName = secondTeacher.teacherEn;
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

    const auto persistedOriginal = services.databaseSession()
        ->classInfoRepository()
        ->loadClassInfo(*createdClass);
    QVERIFY(persistedOriginal);

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
    ClassInfo expectedAssigned = *persistedOriginal;
    copyTeacherMetadata(expectedAssigned, secondTeacher);
    QVERIFY(sameStoredClassFieldsExceptTeacherId(
        *afterAssign,
        *persistedOriginal
        ));
    QVERIFY(sameTeacherMetadata(*afterAssign, expectedAssigned));

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
    QVERIFY(sameStoredClassFieldsExceptTeacherId(
        *afterUnassign,
        *persistedOriginal
        ));
    QVERIFY(teacherMetadataIsEmpty(*afterUnassign));
}

void NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests::
malformedClassIdIsRejectedWithoutChangingPersistedFields()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(
        services,
        QStringLiteral("Malformed Co-Teacher Assignment ID")
        );
    const int originalTeacherId = createTeacher(
        services,
        QStringLiteral("Same Teacher")
        );
    const int assignedTeacherId = createTeacher(
        services,
        QStringLiteral("Same Teacher")
        );
    QVERIFY(classId > 0);
    QVERIFY(originalTeacherId > 0);
    QVERIFY(assignedTeacherId > 0);

    ClassInfo original = newClassInfo(
        services,
        classId,
        originalTeacherId
        );
    original.classTimes = {{
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:50 AM")
    }};
    QVERIFY(saveClassInfo(services, original));

    ClassInfoRepository* const repository =
        services.databaseSession()->classInfoRepository();
    QVERIFY(repository);
    const auto savedBefore = repository->loadClassInfo(classId);
    QVERIFY(savedBefore);

    Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);
    const std::vector<std::string> malformedIds{
        "0",
        "-1",
        "+1",
        "1x"
    };
    for (const std::string& malformedId : malformedIds)
    {
        const auto typedClassId = Domain::ClassId::fromString(malformedId);
        QVERIFY(typedClassId);
        const auto result = port.assignClassCoTeacher({
            .classId = *typedClassId,
            .teacherId = Domain::TeacherId::fromString(
                std::to_string(assignedTeacherId)
                )
        });
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }

    const auto zeroTeacherId = Domain::TeacherId::fromString("0");
    QVERIFY(zeroTeacherId);
    const auto invalidTeacher = port.assignClassCoTeacher({
        .classId = *Domain::ClassId::fromString(std::to_string(classId)),
        .teacherId = *zeroTeacherId
    });
    QVERIFY(!invalidTeacher);
    QCOMPARE(invalidTeacher.error().code, Domain::ErrorCode::InvalidInput);

    const auto savedAfter = repository->loadClassInfo(classId);
    QVERIFY(savedAfter);
    QVERIFY(sameCompleteClassInfo(*savedAfter, *savedBefore));

    const auto leadingZeroClassId = Domain::ClassId::fromString(
        "0" + std::to_string(classId)
        );
    QVERIFY(leadingZeroClassId);
    const auto legacyPositiveIdResult = port.assignClassCoTeacher({
        .classId = *leadingZeroClassId,
        .teacherId = Domain::TeacherId::fromString(
            std::to_string(assignedTeacherId)
            )
    });
    QVERIFY(legacyPositiveIdResult);

    const auto savedAfterLegacyPositiveId = repository->loadClassInfo(classId);
    QVERIFY(savedAfterLegacyPositiveId);
    QVERIFY(sameStoredClassFieldsExceptTeacherId(
        *savedAfterLegacyPositiveId,
        *savedBefore
        ));
    QCOMPARE(savedAfterLegacyPositiveId->teacherId, assignedTeacherId);
    QVERIFY(sameTeacherMetadata(*savedAfterLegacyPositiveId, *savedBefore));
}

void NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests::
invalidLoadedClassInfoIsRejectedWithoutWriting()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(
        services,
        QStringLiteral("Invalid Co-Teacher Assignment State")
        );
    const int originalTeacherId = createTeacher(
        services,
        QStringLiteral("Original Teacher")
        );
    const int assignedTeacherId = createTeacher(
        services,
        QStringLiteral("Assigned Teacher")
        );
    QVERIFY(classId > 0);
    QVERIFY(originalTeacherId > 0);
    QVERIFY(assignedTeacherId > 0);

    ClassInfo original = newClassInfo(
        services,
        classId,
        originalTeacherId
        );
    original.classTimes = {{
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:50 AM")
    }};
    QVERIFY(saveClassInfo(services, original));

    const QString invalidNotes(10001, QChar(u'x'));
    QSqlQuery corruptStoredNotes(services.databaseSession()->database());
    corruptStoredNotes.prepare(QStringLiteral(
        "UPDATE class_info SET notes=? WHERE class_id=?"
        ));
    corruptStoredNotes.addBindValue(invalidNotes);
    corruptStoredNotes.addBindValue(classId);
    QVERIFY2(
        corruptStoredNotes.exec(),
        qPrintable(corruptStoredNotes.lastError().text())
        );
    QCOMPARE(corruptStoredNotes.numRowsAffected(), 1);

    ClassInfoRepository* const repository =
        services.databaseSession()->classInfoRepository();
    QVERIFY(repository);
    const auto savedBefore = repository->loadClassInfo(classId);
    QVERIFY(savedBefore);
    QCOMPARE(savedBefore->notes, invalidNotes);

    Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);
    const auto result = Application::ClassCoTeacherAssignmentUseCase::execute(
        classId,
        assignedTeacherId,
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    const QString message = QString::fromStdString(result.error().message);
    QVERIFY(message.contains(
        QStringLiteral("Class information validation failed")
        ));
    QVERIFY(message.contains(QStringLiteral("notes")));

    const auto savedAfter = repository->loadClassInfo(classId);
    QVERIFY(savedAfter);
    QVERIFY(sameCompleteClassInfo(*savedAfter, *savedBefore));
}

void NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests::
regularScheduleConflictUsesLegacyMessageAndPreservesState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int conflictSourceId = createClass(
        services,
        QStringLiteral("Regular Conflict Source")
        );
    const int selectedClassId = createClass(
        services,
        QStringLiteral("Regular Conflict Selected")
        );
    const int assignedTeacherId = createTeacher(
        services,
        QStringLiteral("Assigned Teacher")
        );
    QVERIFY(conflictSourceId > 0);
    QVERIFY(selectedClassId > 0);
    QVERIFY(assignedTeacherId > 0);

    ClassInfo source = newClassInfo(services, conflictSourceId);
    source.classTimes = {{
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:50 AM")
    }};
    QVERIFY(saveClassInfo(services, source));

    ClassInfo selected = newClassInfo(services, selectedClassId);
    selected.classTimes = source.classTimes;
    QVERIFY(saveClassInfo(services, selected));

    ClassInfoRepository* const repository =
        services.databaseSession()->classInfoRepository();
    QVERIFY(repository);
    const auto savedBefore = repository->loadClassInfo(selectedClassId);
    QVERIFY(savedBefore);

    Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);
    const auto result = Application::ClassCoTeacherAssignmentUseCase::execute(
        selectedClassId,
        assignedTeacherId,
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QCOMPARE(
        QString::fromStdString(result.error().message),
        QStringLiteral(
            "Class schedule conflict: Monday 9:00 AM\u20139:50 AM conflicts with "
            "Regular Conflict Source."
            )
        );

    const auto savedAfter = repository->loadClassInfo(selectedClassId);
    QVERIFY(savedAfter);
    QVERIFY(sameCompleteClassInfo(*savedAfter, *savedBefore));
}

void NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests::
intensiveScheduleConflictUsesLegacyMessageAndPreservesState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int conflictSourceId = createClass(
        services,
        QStringLiteral("Intensive Conflict Source")
        );
    const int selectedClassId = createClass(
        services,
        QStringLiteral("Intensive Conflict Selected")
        );
    const int assignedTeacherId = createTeacher(
        services,
        QStringLiteral("Assigned Teacher")
        );
    QVERIFY(conflictSourceId > 0);
    QVERIFY(selectedClassId > 0);
    QVERIFY(assignedTeacherId > 0);

    ClassInfo source = newClassInfo(services, conflictSourceId);
    source.intensiveTimes = {{
        QStringLiteral("Friday"),
        QStringLiteral("10:00 AM"),
        QStringLiteral("10:50 AM")
    }};
    QVERIFY(saveClassInfo(services, source));

    ClassInfo selected = newClassInfo(services, selectedClassId);
    selected.intensiveTimes = {{
        QStringLiteral("Friday"),
        QStringLiteral("10:10 AM"),
        QStringLiteral("10:40 AM")
    }};
    QString saveError;
    QVERIFY2(
        saveClassInfo(services, selected, &saveError),
        qPrintable(saveError)
        );

    ClassInfoRepository* const repository =
        services.databaseSession()->classInfoRepository();
    QVERIFY(repository);
    const auto savedBefore = repository->loadClassInfo(selectedClassId);
    QVERIFY(savedBefore);

    Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);
    const auto result = Application::ClassCoTeacherAssignmentUseCase::execute(
        selectedClassId,
        assignedTeacherId,
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QCOMPARE(
        QString::fromStdString(result.error().message),
        QStringLiteral(
            "Class schedule conflict: Friday 10:10 AM\u201310:40 AM conflicts with "
            "Intensive Conflict Source."
            )
        );

    const auto savedAfter = repository->loadClassInfo(selectedClassId);
    QVERIFY(savedAfter);
    QVERIFY(sameCompleteClassInfo(*savedAfter, *savedBefore));
}

void NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests::
regularConflictPrecedesIntensiveConflict()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int conflictSourceId = createClass(
        services,
        QStringLiteral("Both Schedule Conflict Source")
        );
    const int selectedClassId = createClass(
        services,
        QStringLiteral("Both Schedule Conflict Selected")
        );
    const int assignedTeacherId = createTeacher(
        services,
        QStringLiteral("Assigned Teacher")
        );
    QVERIFY(conflictSourceId > 0);
    QVERIFY(selectedClassId > 0);
    QVERIFY(assignedTeacherId > 0);

    ClassInfo source = newClassInfo(services, conflictSourceId);
    source.classTimes = {{
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:50 AM")
    }};
    source.intensiveTimes = {{
        QStringLiteral("Friday"),
        QStringLiteral("10:00 AM"),
        QStringLiteral("10:50 AM")
    }};
    QVERIFY(saveClassInfo(services, source));

    ClassInfo selected = newClassInfo(services, selectedClassId);
    selected.classTimes = source.classTimes;
    selected.intensiveTimes = {{
        QStringLiteral("Friday"),
        QStringLiteral("10:10 AM"),
        QStringLiteral("10:40 AM")
    }};
    QVERIFY(saveClassInfo(services, selected));

    ClassInfoRepository* const repository =
        services.databaseSession()->classInfoRepository();
    QVERIFY(repository);
    const auto savedBefore = repository->loadClassInfo(selectedClassId);
    QVERIFY(savedBefore);

    Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);
    const auto result = Application::ClassCoTeacherAssignmentUseCase::execute(
        selectedClassId,
        assignedTeacherId,
        port
        );
    QVERIFY(!result);
    QCOMPARE(
        QString::fromStdString(result.error().message),
        QStringLiteral(
            "Class schedule conflict: Monday 9:00 AM\u20139:50 AM conflicts with "
            "Both Schedule Conflict Source."
            )
        );

    const auto savedAfter = repository->loadClassInfo(selectedClassId);
    QVERIFY(savedAfter);
    QVERIFY(sameCompleteClassInfo(*savedAfter, *savedBefore));
}

void NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests::
repositoryFailureRollsBackClassInfoAndSchedules()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int selectedClassId = createClass(
        services,
        QStringLiteral("Transactional Co-Teacher Assignment")
        );
    const int originalTeacherId = createTeacher(
        services,
        QStringLiteral("Original Teacher")
        );
    const int assignedTeacherId = createTeacher(
        services,
        QStringLiteral("Assigned Teacher")
        );
    QVERIFY(selectedClassId > 0);
    QVERIFY(originalTeacherId > 0);
    QVERIFY(assignedTeacherId > 0);

    ClassInfo original = newClassInfo(
        services,
        selectedClassId,
        originalTeacherId
        );
    original.classTimes = {{
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:50 AM")
    }};
    original.intensiveTimes = {{
        QStringLiteral("Friday"),
        QStringLiteral("10:00 AM"),
        QStringLiteral("10:50 AM")
    }};
    QVERIFY(saveClassInfo(services, original));

    ClassInfoRepository* const repository =
        services.databaseSession()->classInfoRepository();
    QVERIFY(repository);
    const auto savedBefore = repository->loadClassInfo(selectedClassId);
    QVERIFY(savedBefore);

    QSqlQuery installFailure(services.databaseSession()->database());
    QVERIFY2(
        installFailure.exec(QStringLiteral(
            "CREATE TRIGGER fail_co_teacher_intensive_schedule_insert "
            "BEFORE INSERT ON class_intensive_times "
            "BEGIN SELECT RAISE(FAIL, 'forced co-teacher save failure'); END"
            )),
        qPrintable(installFailure.lastError().text())
        );

    Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);
    const auto result = Application::ClassCoTeacherAssignmentUseCase::execute(
        selectedClassId,
        assignedTeacherId,
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(QString::fromStdString(result.error().message).contains(
        QStringLiteral("forced co-teacher save failure")
        ));

    const auto savedAfter = repository->loadClassInfo(selectedClassId);
    QVERIFY(savedAfter);
    QVERIFY(sameCompleteClassInfo(*savedAfter, *savedBefore));
}

void NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests::
unavailableSessionReturnsStructuredFailure()
{
    ApplicationServices services;
    Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);

    const auto unavailable =
        Application::ClassCoTeacherAssignmentUseCase::execute(
        1,
        -1,
        port
        );
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(QString::fromStdString(unavailable.error().message).contains(
        QStringLiteral("unavailable")
        ));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(createClass(services, QStringLiteral("Closed Session Class")) > 0);
    services.closeDatabase();

    const auto closed = Application::ClassCoTeacherAssignmentUseCase::execute(
        1,
        -1,
        port
        );
    QVERIFY(!closed);
    QCOMPARE(closed.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(QString::fromStdString(closed.error().message).contains(
        QStringLiteral("unavailable")
        ));
}

QTEST_MAIN(NextPlatformApplicationServicesClassCoTeacherAssignmentPortTests)

#include "next_platform_application_services_class_co_teacher_assignment_port_tests.moc"
