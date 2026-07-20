#include "data/data_service.h"
#include "core/utils/file_name_utils.h"
#include "features/classes/services/class_transfer_json_codec.h"
#include "features/classes/ui/class_export_dialog.h"
#include "features/classes/ui/class_import_dialog.h"

#include <QComboBox>
#include <QFile>
#include <QJsonDocument>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>
#include <QtTest>

namespace
{
Teacher completeTeacher(
    const QString& englishName = QStringLiteral("Alex Kim")
    )
{
    Teacher teacher;
    teacher.teacherKr = QStringLiteral("김알렉스");
    teacher.teacherEn = englishName;
    teacher.preferredRomanization = QStringLiteral("Gim Allekseu");
    teacher.roomNumber = QStringLiteral("504");
    teacher.birthday = QStringLiteral("02-29");
    teacher.phoneNumber = QStringLiteral("010-1234-5678");
    teacher.wifiName = QStringLiteral("Campus WiFi");
    teacher.wifiPassword = QStringLiteral("wifi-password");
    teacher.internetType = QStringLiteral("Both");
    teacher.zoomId = QStringLiteral("123 456 7890");
    teacher.zoomPassword = QStringLiteral("zoom-password");
    teacher.projectionType = QStringLiteral("Any");
    teacher.notes = QStringLiteral("Teacher notes\nwith a second line.");
    return teacher;
}

ClassInfo completeClassInfo(
    int classId,
    int teacherId,
    const QString& grade,
    const QString& level,
    const QString& day,
    const QString& startTime = QStringLiteral("4:00 PM"),
    const QString& endTime = QStringLiteral("4:50 PM")
    )
{
    ClassInfo info;
    info.classId = classId;
    info.teacherId = teacherId;
    info.classGrade = grade;
    info.classLevel = level;
    info.readingBook = QStringLiteral("Reading Explorer 3");
    info.essayBook = QStringLiteral("4C");
    info.classColor = QStringLiteral("#123456");
    info.fontColor = QStringLiteral("#FEDCBA");
    info.notes = QStringLiteral("수업 노트\nSecond line");
    info.timeFillerActivities = QStringLiteral("Word chain");
    info.classTimes.append({day, startTime, endTime});
    info.intensiveTimes.append({
        day,
        QStringLiteral("10:00 AM"),
        QStringLiteral("10:55 AM")
    });
    return info;
}

Roster completeRoster(
    const QString& studentName
    )
{
    Roster roster;
    roster.columns = {
        QStringLiteral("English"),
        QStringLiteral("Korean"),
        QStringLiteral("Memo")
    };
    roster.columnWidths = {180, 190, 240};
    roster.rows.append({
        studentName,
        QStringLiteral("학생"),
        QStringLiteral("Needs extra practice")
    });
    roster.rows.append({QString(), QString(), QString()});
    return roster;
}

SpeakingEvalRows completeEvaluation(
    const QString& studentName
    )
{
    SpeakingEvalRows rows = SpeakingEval::emptyRows();
    rows[0][SpeakingEval::toInt(
        SpeakingEvalColumn::EnglishName)] = studentName;
    rows[0][SpeakingEval::toInt(
        SpeakingEvalColumn::KoreanName)] = QStringLiteral("학생");
    rows[0][SpeakingEval::toInt(
        SpeakingEvalColumn::Grammar)] = QStringLiteral("A");
    rows[0][SpeakingEval::toInt(
        SpeakingEvalColumn::Comments)] = QStringLiteral("Excellent progress.");
    return rows;
}

int addCompleteClass(
    DataService& service,
    int teacherId,
    const QString& storedName,
    const QString& grade,
    const QString& level,
    const QString& day,
    const QString& studentName,
    const QString& evaluationName = QStringLiteral("Custom Evaluation"),
    const QString& startTime = QStringLiteral("4:00 PM"),
    const QString& endTime = QStringLiteral("4:50 PM")
    )
{
    const int classId = service.createClass(storedName);
    const ClassInfo info = completeClassInfo(
        classId, teacherId, grade, level, day, startTime, endTime);
    if (!service.saveClassInfo(info))
    {
        return -1;
    }

    service.saveRoster(classId, completeRoster(studentName));

    if (!service.saveSpeakingEval(
            classId,
            evaluationName,
            completeEvaluation(studentName)))
    {
        return -1;
    }

    return classId;
}

ClassImportPlan createAllPlan(
    const ClassTransferPackage& package
    )
{
    ClassImportPlan plan;

    for (int index = 0; index < package.classes.size(); ++index)
    {
        plan.classes.append({index, ClassImportAction::Create, -1});
    }

    for (const ClassTransferTeacher& teacher : package.teachers)
    {
        plan.teachers.append({
            teacher.key, TeacherImportAction::Create, -1});
    }

    return plan;
}
}

class ClassTransferTests : public QObject
{
    Q_OBJECT

private slots:
    void jsonRoundTripPreservesCompletePackage();
    void importsCompleteClassesAndDeduplicatesTeacher();
    void previewMatchesCourseAndTeacherIgnoringSchedule();
    void replacementRetainsIdAndClearsOldChildren();
    void teacherReplacementImportsCompleteSnapshot();
    void scheduleConflictLeavesDestinationUnchanged();
    void importedClassesConflictAtomically();
    void databaseFailureRollsBackAllWrites();
    void incompleteCourseSignatureDoesNotMatch();
    void codecRejectsMalformedAndUnsupportedPackages();
    void exportDialogStartsClearAndSortsClassesAlphabetically();
    void filesystemSafeJsonFileName();
    void importDialogRequiresAmbiguousTeacherResolution();
};

void ClassTransferTests::jsonRoundTripPreservesCompletePackage()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ClassTransferPackage package;
    package.exportedAtUtc = QDateTime::fromString(
        QStringLiteral("2026-07-20T12:34:56.789Z"), Qt::ISODateWithMs);
    package.teachers.append({QStringLiteral("teacher-1"), completeTeacher()});

    ClassTransferClass transferClass;
    transferClass.key = QStringLiteral("class-1");
    transferClass.name = QStringLiteral("Stored 이름");
    transferClass.teacherKey = QStringLiteral("teacher-1");
    transferClass.info = completeClassInfo(
        -1,
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Monday")
        );
    transferClass.roster = completeRoster(QStringLiteral("Jamie"));
    transferClass.evaluations.append({
        QStringLiteral("Arbitrary Evaluation Name"),
        completeEvaluation(QStringLiteral("Jamie"))
    });
    package.classes.append(transferClass);

    const QString path = directory.filePath(
        QStringLiteral("roundtrip.classmngr-classes.json"));
    QVERIFY(ClassTransferJsonCodec::saveFile(path, package).has_value());

    const auto loaded = ClassTransferJsonCodec::loadFile(path);
    QVERIFY2(loaded.has_value(),
             loaded ? "" : qPrintable(loaded.error()));
    QCOMPARE(loaded->version, ClassTransferPackage::CurrentVersion);
    QCOMPARE(loaded->teachers.size(), 1);
    QCOMPARE(loaded->classes.size(), 1);
    QCOMPARE(loaded->teachers.first().teacher.wifiPassword,
             QStringLiteral("wifi-password"));
    QCOMPARE(loaded->teachers.first().teacher.preferredRomanization,
             QStringLiteral("Gim Allekseu"));
    QCOMPARE(loaded->teachers.first().teacher.birthday,
             QStringLiteral("02-29"));
    QCOMPARE(loaded->teachers.first().teacher.phoneNumber,
             QStringLiteral("010-1234-5678"));
    QCOMPARE(loaded->teachers.first().teacher.zoomPassword,
             QStringLiteral("zoom-password"));
    QCOMPARE(loaded->teachers.first().teacher.notes,
             QStringLiteral("Teacher notes\nwith a second line."));
    QCOMPARE(loaded->classes.first().name, QStringLiteral("Stored 이름"));
    QCOMPARE(loaded->classes.first().info.notes,
             QStringLiteral("수업 노트\nSecond line"));
    QCOMPARE(loaded->classes.first().info.classTimes.size(), 1);
    QCOMPARE(loaded->classes.first().info.intensiveTimes.size(), 1);
    QCOMPARE(loaded->classes.first().roster.columnWidths,
             QVector<int>({180, 190, 240}));
    QCOMPARE(loaded->classes.first().evaluations.first().name,
             QStringLiteral("Arbitrary Evaluation Name"));
    QCOMPARE(
        loaded->classes.first().evaluations.first().rows[0][
            SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)],
        QStringLiteral("Jamie")
        );

    QJsonObject withUnknownField = ClassTransferJsonCodec::toJson(package);
    withUnknownField.insert(QStringLiteral("future_field"), 42);
    QVERIFY(ClassTransferJsonCodec::fromJson(withUnknownField).has_value());

    QJsonObject legacyJson = ClassTransferJsonCodec::toJson(package);
    QJsonArray legacyTeachers =
        legacyJson.value(QStringLiteral("teachers")).toArray();
    QJsonObject legacyTeacher = legacyTeachers.first().toObject();
    legacyTeacher.remove(QStringLiteral("preferred_romanization"));
    legacyTeacher.remove(QStringLiteral("birthday"));
    legacyTeacher.remove(QStringLiteral("phone_number"));
    legacyTeachers.replace(0, legacyTeacher);
    legacyJson.insert(QStringLiteral("teachers"), legacyTeachers);

    const auto legacyPackage =
        ClassTransferJsonCodec::fromJson(legacyJson);
    QVERIFY(legacyPackage.has_value());
    QVERIFY(
        legacyPackage->teachers.first().teacher
            .preferredRomanization.isEmpty()
        );
    QVERIFY(legacyPackage->teachers.first().teacher.birthday.isEmpty());
    QVERIFY(legacyPackage->teachers.first().teacher.phoneNumber.isEmpty());
}

void ClassTransferTests::importsCompleteClassesAndDeduplicatesTeacher()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());

    const int teacherId = service.createTeacher(completeTeacher());
    const int firstClassId = addCompleteClass(
        service,
        teacherId,
        QStringLiteral("Source A"),
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral("Monday"),
        QStringLiteral("Jamie")
        );
    const int secondClassId = addCompleteClass(
        service,
        teacherId,
        QStringLiteral("Source B"),
        QStringLiteral("E5"),
        QStringLiteral("Apollo"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Morgan")
        );
    QVERIFY(firstClassId > 0);
    QVERIFY(secondClassId > 0);

    const auto package = service.buildClassTransferPackage(
        {firstClassId, secondClassId});
    QVERIFY2(package.has_value(),
             package ? "" : qPrintable(package.error()));
    QCOMPARE(package->teachers.size(), 1);
    QCOMPARE(package->classes.size(), 2);

    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());
    const auto summary = service.importClasses(
        *package, createAllPlan(*package));
    QVERIFY2(summary.has_value(),
             summary ? "" : qPrintable(summary.error()));
    QCOMPARE(summary->createdClassIds.size(), 2);
    QCOMPARE(summary->replacedClassIds.size(), 0);
    QCOMPARE(service.getAllTeachers().size(), 1);

    const int importedFirstId = summary->createdClassIds.first();
    const int importedSecondId = summary->createdClassIds.last();
    QVERIFY(importedFirstId != firstClassId || importedSecondId != secondClassId
            || service.currentDatabasePath().endsWith(
                QStringLiteral("destination.db")));
    const ClassInfo importedFirst = service.loadClassInfo(importedFirstId);
    const ClassInfo importedSecond = service.loadClassInfo(importedSecondId);
    QCOMPARE(importedFirst.teacherId, importedSecond.teacherId);
    QCOMPARE(importedFirst.notes, QStringLiteral("수업 노트\nSecond line"));
    QCOMPARE(importedFirst.classColor, QStringLiteral("#123456"));
    QCOMPARE(service.getTeacher(importedFirst.teacherId).wifiPassword,
             QStringLiteral("wifi-password"));
    QCOMPARE(service.getTeacher(importedFirst.teacherId).birthday,
             QStringLiteral("02-29"));
    QCOMPARE(service.loadRoster(importedFirstId).rows.first().first(),
             QStringLiteral("Jamie"));
    QCOMPARE(
        service.loadSpeakingEval(
            importedFirstId, QStringLiteral("Custom Evaluation"))[0][
                SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)],
        QStringLiteral("Jamie")
        );
}

void ClassTransferTests::previewMatchesCourseAndTeacherIgnoringSchedule()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());
    const int sourceTeacher = service.createTeacher(completeTeacher());
    const int sourceClass = addCompleteClass(
        service,
        sourceTeacher,
        QString(),
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Monday"),
        QStringLiteral("Jamie")
        );
    const auto package = service.buildClassTransferPackage({sourceClass});
    QVERIFY(package.has_value());

    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());
    const int destinationTeacher = service.createTeacher(completeTeacher());
    const int destinationClass = addCompleteClass(
        service,
        destinationTeacher,
        QString(),
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Friday"),
        QStringLiteral("Old Student")
        );

    const auto preview = service.previewClassImport(*package);
    QVERIFY(preview.has_value());
    QCOMPARE(preview->teachers.size(), 1);
    QCOMPARE(preview->teachers.first().matchingTeacherIds,
             QList<int>({destinationTeacher}));
    QCOMPARE(preview->classes.size(), 1);
    QCOMPARE(preview->classes.first().matchingClassIds,
             QList<int>({destinationClass}));
}

void ClassTransferTests::replacementRetainsIdAndClearsOldChildren()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());
    const int sourceTeacher = service.createTeacher(completeTeacher());
    const int sourceClass = addCompleteClass(
        service,
        sourceTeacher,
        QStringLiteral("Imported Stored Name"),
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Monday"),
        QStringLiteral("New Student"),
        QStringLiteral("Imported Evaluation")
        );
    const auto package = service.buildClassTransferPackage({sourceClass});
    QVERIFY(package.has_value());

    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());
    Teacher localTeacher = completeTeacher();
    localTeacher.wifiPassword = QStringLiteral("local-password");
    const int destinationTeacher = service.createTeacher(localTeacher);
    const int destinationClass = addCompleteClass(
        service,
        destinationTeacher,
        QStringLiteral("Old Stored Name"),
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Monday"),
        QStringLiteral("Old Student"),
        QStringLiteral("Destination Only Evaluation")
        );

    ClassImportPlan plan;
    plan.classes.append({
        0, ClassImportAction::Replace, destinationClass});
    plan.teachers.append({
        package->teachers.first().key,
        TeacherImportAction::KeepExisting,
        destinationTeacher
    });

    const auto summary = service.importClasses(*package, plan);
    QVERIFY2(summary.has_value(),
             summary ? "" : qPrintable(summary.error()));
    QCOMPARE(summary->replacedClassIds,
             QList<int>({destinationClass}));
    QCOMPARE(service.getClasses().size(), 1);
    QCOMPARE(service.getClassById(destinationClass).name,
             QStringLiteral("Imported Stored Name"));
    QCOMPARE(service.loadClassInfo(destinationClass).classTimes.first().day,
             QStringLiteral("Monday"));
    QCOMPARE(service.loadRoster(destinationClass).rows.first().first(),
             QStringLiteral("New Student"));
    QVERIFY(service.loadSpeakingEval(
        destinationClass,
        QStringLiteral("Destination Only Evaluation")).isEmpty());
    QVERIFY(!service.loadSpeakingEval(
        destinationClass,
        QStringLiteral("Imported Evaluation")).isEmpty());
    QCOMPARE(service.getTeacher(destinationTeacher).wifiPassword,
             QStringLiteral("local-password"));
}

void ClassTransferTests::teacherReplacementImportsCompleteSnapshot()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());
    const int sourceTeacher = service.createTeacher(completeTeacher());
    const int sourceClass = addCompleteClass(
        service,
        sourceTeacher,
        QString(),
        QStringLiteral("E5"),
        QStringLiteral("Apollo"),
        QStringLiteral("Monday"),
        QStringLiteral("Jamie")
        );
    const auto package = service.buildClassTransferPackage({sourceClass});
    QVERIFY(package.has_value());

    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());
    Teacher localTeacher = completeTeacher();
    localTeacher.wifiPassword = QStringLiteral("outdated");
    localTeacher.notes = QStringLiteral("Outdated notes");
    const int destinationTeacher = service.createTeacher(localTeacher);
    const int destinationClass = addCompleteClass(
        service,
        destinationTeacher,
        QString(),
        QStringLiteral("E5"),
        QStringLiteral("Apollo"),
        QStringLiteral("Friday"),
        QStringLiteral("Old")
        );

    ClassImportPlan plan;
    plan.classes.append({
        0, ClassImportAction::Replace, destinationClass});
    plan.teachers.append({
        package->teachers.first().key,
        TeacherImportAction::ReplaceExisting,
        destinationTeacher
    });
    const auto summary = service.importClasses(*package, plan);
    QVERIFY2(summary.has_value(),
             summary ? "" : qPrintable(summary.error()));
    QCOMPARE(service.getTeacher(destinationTeacher).wifiPassword,
             QStringLiteral("wifi-password"));
    QCOMPARE(
        service.getTeacher(destinationTeacher).preferredRomanization,
        QStringLiteral("Gim Allekseu")
        );
    QCOMPARE(service.getTeacher(destinationTeacher).phoneNumber,
             QStringLiteral("010-1234-5678"));
    QCOMPARE(service.getTeacher(destinationTeacher).notes,
             QStringLiteral("Teacher notes\nwith a second line."));
}

void ClassTransferTests::scheduleConflictLeavesDestinationUnchanged()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());
    const int sourceTeacher = service.createTeacher(
        completeTeacher(QStringLiteral("Source Teacher")));
    const int sourceClass = addCompleteClass(
        service,
        sourceTeacher,
        QString(),
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral("Monday"),
        QStringLiteral("Source Student")
        );
    const auto package = service.buildClassTransferPackage({sourceClass});
    QVERIFY(package.has_value());

    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());
    const int destinationTeacher = service.createTeacher(
        completeTeacher(QStringLiteral("Destination Teacher")));
    const int destinationClass = addCompleteClass(
        service,
        destinationTeacher,
        QString(),
        QStringLiteral("E6"),
        QStringLiteral("Gaia"),
        QStringLiteral("Monday"),
        QStringLiteral("Destination Student")
        );
    QVERIFY(destinationClass > 0);
    const int teachersBefore = service.getAllTeachers().size();
    const int classesBefore = service.getClasses().size();

    const auto result = service.importClasses(
        *package, createAllPlan(*package));
    QVERIFY(!result.has_value());
    QVERIFY(result.error().contains(QStringLiteral("Schedule conflicts")));
    QCOMPARE(service.getAllTeachers().size(), teachersBefore);
    QCOMPARE(service.getClasses().size(), classesBefore);
    QCOMPARE(service.loadRoster(destinationClass).rows.first().first(),
             QStringLiteral("Destination Student"));
}

void ClassTransferTests::importedClassesConflictAtomically()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());
    const int sourceTeacher = service.createTeacher(completeTeacher());
    const int sourceClass = addCompleteClass(
        service,
        sourceTeacher,
        QString(),
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral("Monday"),
        QStringLiteral("Source Student")
        );
    const auto originalPackage = service.buildClassTransferPackage({sourceClass});
    QVERIFY(originalPackage.has_value());

    ClassTransferPackage package = *originalPackage;
    ClassTransferClass conflictingClass = package.classes.first();
    conflictingClass.key = QStringLiteral("class-2");
    conflictingClass.info.classGrade = QStringLiteral("E5");
    conflictingClass.info.classLevel = QStringLiteral("Apollo");
    conflictingClass.info.intensiveTimes.first().day = QStringLiteral("Tuesday");
    package.classes.append(conflictingClass);

    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());
    const auto result = service.importClasses(
        package, createAllPlan(package));
    QVERIFY(!result.has_value());
    QVERIFY(result.error().contains(QStringLiteral("Schedule conflicts")));
    QVERIFY(service.getClasses().isEmpty());
    QVERIFY(service.getAllTeachers().isEmpty());
}

void ClassTransferTests::databaseFailureRollsBackAllWrites()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());
    const int sourceTeacher = service.createTeacher(completeTeacher());
    const int sourceClass = addCompleteClass(
        service,
        sourceTeacher,
        QString(),
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral("Monday"),
        QStringLiteral("Source Student")
        );
    const auto originalPackage = service.buildClassTransferPackage({sourceClass});
    QVERIFY(originalPackage.has_value());

    ClassTransferPackage package = *originalPackage;
    package.classes.first().evaluations.append(
        package.classes.first().evaluations.first());

    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());
    const auto result = service.importClasses(
        package, createAllPlan(package));
    QVERIFY(!result.has_value());
    QVERIFY(service.getClasses().isEmpty());
    QVERIFY(service.getAllTeachers().isEmpty());
}

void ClassTransferTests::incompleteCourseSignatureDoesNotMatch()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());
    const int teacherId = service.createTeacher(completeTeacher());
    const int sourceClass = service.createClass(QString());
    ClassInfo sourceInfo;
    sourceInfo.classId = sourceClass;
    sourceInfo.teacherId = teacherId;
    sourceInfo.classGrade = QStringLiteral("E4");
    QVERIFY(service.saveClassInfo(sourceInfo));
    const auto package = service.buildClassTransferPackage({sourceClass});
    QVERIFY(package.has_value());

    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());
    const int destinationTeacher = service.createTeacher(completeTeacher());
    const int destinationClass = service.createClass(QString());
    ClassInfo destinationInfo;
    destinationInfo.classId = destinationClass;
    destinationInfo.teacherId = destinationTeacher;
    destinationInfo.classGrade = QStringLiteral("E4");
    QVERIFY(service.saveClassInfo(destinationInfo));

    const auto preview = service.previewClassImport(*package);
    QVERIFY(preview.has_value());
    QVERIFY(preview->classes.first().matchingClassIds.isEmpty());
}

void ClassTransferTests::codecRejectsMalformedAndUnsupportedPackages()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString malformedPath = directory.filePath(
        QStringLiteral("malformed.json"));
    QFile malformedFile(malformedPath);
    QVERIFY(malformedFile.open(QIODevice::WriteOnly | QIODevice::Text));
    malformedFile.write("{not-json");
    malformedFile.close();
    QVERIFY(!ClassTransferJsonCodec::loadFile(malformedPath).has_value());

    ClassTransferPackage package;
    package.exportedAtUtc = QDateTime::currentDateTimeUtc();
    ClassTransferClass transferClass;
    transferClass.key = QStringLiteral("class-1");
    transferClass.info = completeClassInfo(
        -1, -1, QStringLiteral("E4"), QStringLiteral("Theseus"),
        QStringLiteral("Monday"));
    transferClass.roster.columns = {};
    transferClass.roster.columnWidths = {};
    package.classes.append(transferClass);

    QJsonObject json = ClassTransferJsonCodec::toJson(package);
    json.insert(QStringLiteral("version"), 99);
    QVERIFY(!ClassTransferJsonCodec::fromJson(json).has_value());

    json = ClassTransferJsonCodec::toJson(package);
    QJsonArray classes = json.value(QStringLiteral("classes")).toArray();
    QJsonObject firstClass = classes.first().toObject();
    firstClass.insert(QStringLiteral("teacher_ref"),
                      QStringLiteral("missing-teacher"));
    classes.replace(0, firstClass);
    json.insert(QStringLiteral("classes"), classes);
    QVERIFY(!ClassTransferJsonCodec::fromJson(json).has_value());
}

void ClassTransferTests::exportDialogStartsClearAndSortsClassesAlphabetically()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("dialog.db"))).has_value());
    const int zuluClass = service.createClass(QStringLiteral("Zulu"));
    const int alphaClass = service.createClass(QStringLiteral("Alpha"));
    const int mikeClass = service.createClass(QStringLiteral("Mike"));

    ClassInfo zuluInfo;
    zuluInfo.classId = zuluClass;
    zuluInfo.classGrade = QStringLiteral("Zulu");
    QVERIFY(service.saveClassInfo(zuluInfo));

    ClassInfo alphaInfo;
    alphaInfo.classId = alphaClass;
    alphaInfo.classGrade = QStringLiteral("Alpha");
    QVERIFY(service.saveClassInfo(alphaInfo));

    ClassInfo mikeInfo;
    mikeInfo.classId = mikeClass;
    mikeInfo.classGrade = QStringLiteral("Mike");
    QVERIFY(service.saveClassInfo(mikeInfo));

    ClassExportDialog dialog(&service);
    QCOMPARE(dialog.selectedClassIds(), QList<int>());

    auto* classList = dialog.findChild<QListWidget*>(
        QStringLiteral("classExportList"));
    QVERIFY(classList);
    QCOMPARE(classList->count(), 3);
    QCOMPARE(classList->item(0)->data(Qt::UserRole).toInt(), alphaClass);
    QCOMPARE(classList->item(1)->data(Qt::UserRole).toInt(), mikeClass);
    QCOMPARE(classList->item(2)->data(Qt::UserRole).toInt(), zuluClass);

    for (int index = 0; index < classList->count(); ++index)
    {
        QCOMPARE(classList->item(index)->checkState(), Qt::Unchecked);
    }

    auto* exportButton = dialog.findChild<QPushButton*>(
        QStringLiteral("exportClassesButton"));
    QVERIFY(exportButton);
    QVERIFY(!exportButton->isEnabled());

    classList->item(1)->setCheckState(Qt::Checked);
    QCOMPARE(dialog.selectedClassIds(), QList<int>({mikeClass}));
    QVERIFY(exportButton->isEnabled());
}

void ClassTransferTests::filesystemSafeJsonFileName()
{
    QCOMPARE(
        FileNameUtils::filesystemSafeJsonFileName(
            QStringLiteral("Class: A/B?.json"), QStringLiteral("Class")),
        QStringLiteral("Class_ A_B_.json")
        );
    QCOMPARE(
        FileNameUtils::filesystemSafeJsonFileName(
            QStringLiteral("CON"), QStringLiteral("Class")),
        QStringLiteral("_CON.json")
        );
    QCOMPARE(
        FileNameUtils::filesystemSafeJsonFileName(
            QStringLiteral("LPT9.json"), QStringLiteral("Class")),
        QStringLiteral("_LPT9.json")
        );
    QCOMPARE(
        FileNameUtils::filesystemSafeJsonFileName(
            QStringLiteral("  ...  "), QStringLiteral("Fallback")),
        QStringLiteral("Fallback.json")
        );
    QCOMPARE(
        FileNameUtils::filesystemSafeJsonFileName(
            QStringLiteral("Lesson.JSON"), QStringLiteral("Class")),
        QStringLiteral("Lesson.json")
        );
    QCOMPARE(
        FileNameUtils::filesystemSafeJsonFileName(
            QStringLiteral("A%1B").arg(QChar(1)), QStringLiteral("Class")),
        QStringLiteral("A_B.json")
        );

    const QString longName(300, u'a');
    const QString safeLongName = FileNameUtils::filesystemSafeJsonFileName(
        longName, QStringLiteral("Class"));
    QVERIFY(safeLongName.endsWith(QStringLiteral(".json")));
    QVERIFY(safeLongName.toUtf8().size() <= 245);
}

void ClassTransferTests::importDialogRequiresAmbiguousTeacherResolution()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());
    const int sourceTeacher = service.createTeacher(completeTeacher());
    const int sourceClass = service.createClass(QString());
    ClassInfo sourceInfo;
    sourceInfo.classId = sourceClass;
    sourceInfo.teacherId = sourceTeacher;
    sourceInfo.classGrade = QStringLiteral("E4");
    sourceInfo.classLevel = QStringLiteral("Theseus");
    QVERIFY(service.saveClassInfo(sourceInfo));
    const auto package = service.buildClassTransferPackage({sourceClass});
    QVERIFY(package.has_value());

    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());
    QVERIFY(service.createTeacher(completeTeacher()) > 0);
    QVERIFY(service.createTeacher(completeTeacher()) > 0);
    const auto preview = service.previewClassImport(*package);
    QVERIFY(preview.has_value());
    QCOMPARE(preview->teachers.first().matchingTeacherIds.size(), 2);

    ClassImportDialog dialog(&service, *package, *preview);
    auto* importButton = dialog.findChild<QPushButton*>(
        QStringLiteral("importClassesButton"));
    auto* teacherChoice = dialog.findChild<QComboBox*>(
        QStringLiteral("teacherImportChoice_")
            + package->teachers.first().key);
    QVERIFY(importButton);
    QVERIFY(teacherChoice);
    QVERIFY(!importButton->isEnabled());

    teacherChoice->setCurrentIndex(1);
    QVERIFY(importButton->isEnabled());
    QCOMPARE(dialog.importPlan().teachers.first().action,
             TeacherImportAction::Create);
}

QTEST_MAIN(ClassTransferTests)

#include "class_transfer_tests.moc"
