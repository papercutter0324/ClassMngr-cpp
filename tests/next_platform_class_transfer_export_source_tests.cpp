#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "data/database/database_session.h"
#include "features/classes/services/class_transfer_json_codec.h"
#include "next/platform/application_services_class_transfer_export_source_read_port.h"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVariant>
#include <QtTest/QtTest>
using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
namespace
{
Domain::ClassId id(int value) { return *Domain::ClassId::fromString(std::to_string(value)); }
}
class ClassTransferExportSourceTests : public QObject
{
    Q_OBJECT
private slots:
    void successfulSourceHasEveryRequiredStageAndSanitizedProjection();
    void outOfRangeIntegerRosterWidthFailsTheRosterStage();
    void malformedRosterRowIndexReturnsEstablishedReadError();
    void fractionalRosterRowIndexReturnsEstablishedReadError();
    void sparseIntegerRosterRowRemainsValid();
    void outOfRangeRosterRowIndexReturnsEstablishedReadError();
    void wideTeacherIdentityKeepsFullProfileAndSharedPortableReference();
    void partialEvaluationHeaderFetchErrorFailsEvaluationStage();
    void partialEvaluationMatrixFetchErrorFailsEvaluationStage();
    void wideEvaluationIdsKeepTheirOwnExportRows();
    void stagedErrorRollsBackAndKeepsDownstreamNotAttempted();
    void beginAndUnavailableErrorsStayAtSourceBoundary();
};
void ClassTransferExportSourceTests::successfulSourceHasEveryRequiredStageAndSanitizedProjection()
{
    QTemporaryDir directory; QVERIFY(directory.isValid()); ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath(QStringLiteral("source.db"))));
    auto* repository = services.databaseSession()->classTransferRepository();
    QSqlQuery insert(services.databaseSession()->database());
    QVERIFY(insert.exec(QStringLiteral("INSERT INTO classes(name) VALUES('Unassigned')")));
    const int value = insert.lastInsertId().toInt();
    Platform::ApplicationServicesClassTransferExportSourceReadPort port(services);
    const qint64 earliestExportTimestamp = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    auto source = port.readSource({{id(value)}}); QVERIFY(source);
    const qint64 latestExportTimestamp = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    QVERIFY(source.value().exportedAtUtcMilliseconds.value() >= earliestExportTimestamp);
    QVERIFY(source.value().exportedAtUtcMilliseconds.value() <= latestExportTimestamp);
    QCOMPARE(source.value().classes.size(), std::size_t(1));
    const auto& item = source.value().classes.front();
    QVERIFY(item.id == id(value)); QCOMPARE(item.info.state(), ClassExportStageState::Value);
    QCOMPARE(item.roster.state(), ClassExportStageState::Value);
    QCOMPARE(item.teacher.state(), ClassExportStageState::Value);
    QCOMPARE(item.evaluations.state(), ClassExportStageState::Value);
    QVERIFY(!item.teacher.value()); QVERIFY(source.value().teachers.empty());
    auto package = ClassTransferExportQuery(port).execute({{id(value)}}); QVERIFY(package);
    QVERIFY(package.value().classes[0].teacherKey.value().empty());
    QVERIFY(package.value().classes[0].info.classColor == u"#FFFFFF");
    auto legacy = repository->buildPackage({value}); QVERIFY(legacy);
    QCOMPARE(legacy->classes[0].info.classId, -1); QCOMPARE(legacy->classes[0].info.teacherId, -1);
    auto database = services.databaseSession()->database(); QVERIFY(database.transaction()); QVERIFY(database.rollback());
}
void ClassTransferExportSourceTests::outOfRangeIntegerRosterWidthFailsTheRosterStage()
{
    QTemporaryDir directory; QVERIFY(directory.isValid()); ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath(QStringLiteral("source.db"))));
    auto database = services.databaseSession()->database();

    QSqlQuery insertClass(database);
    QVERIFY(insertClass.exec(QStringLiteral(
        "INSERT INTO classes(name) VALUES('Out of range roster width')")));
    const int classId = insertClass.lastInsertId().toInt();

    QSqlQuery insertColumn(database);
    QVERIFY(insertColumn.prepare(QStringLiteral(
        "INSERT INTO roster_columns(class_id,name,position,width) VALUES(?, 'Student', 0, ?)")));
    insertColumn.addBindValue(classId);
    insertColumn.addBindValue(QVariant::fromValue<qlonglong>(4294967297LL));
    QVERIFY2(insertColumn.exec(), qPrintable(insertColumn.lastError().text()));

    QSqlQuery storedWidth(database);
    QVERIFY(storedWidth.prepare(QStringLiteral(
        "SELECT width, typeof(width) AS storage_type FROM roster_columns WHERE class_id=?")));
    storedWidth.addBindValue(classId);
    QVERIFY2(storedWidth.exec(), qPrintable(storedWidth.lastError().text()));
    QVERIFY(storedWidth.next());
    QCOMPARE(storedWidth.value("storage_type").toString(), QStringLiteral("integer"));
    QCOMPARE(storedWidth.value("width").toLongLong(), qlonglong(4294967297LL));
    QVERIFY(!storedWidth.next());

    Platform::ApplicationServicesClassTransferExportSourceReadPort port(services);
    const auto source = port.readSource({{id(classId)}});
    QVERIFY(source);
    QCOMPARE(source.value().classes.size(), std::size_t(1));
    const auto& roster = source.value().classes.front().roster;
    QCOMPARE(roster.state(), ClassExportStageState::Error);
    QCOMPARE(roster.error().code, Domain::ErrorCode::Technical);
    const QString diagnostic = QString::fromStdString(roster.error().message);
    QVERIFY2(
        diagnostic.contains(QStringLiteral("class id %1").arg(classId))
            && diagnostic.contains(QStringLiteral("width"), Qt::CaseInsensitive)
            && diagnostic.contains(QStringLiteral("4294967297")),
        qPrintable(diagnostic)
        );

    const auto exported = ClassTransferExportQuery(port).execute({{id(classId)}});
    QVERIFY(!exported);
    QCOMPARE(exported.error().code, Domain::ErrorCode::Technical);
    QVERIFY(QString::fromStdString(exported.error().message).contains(
        QStringLiteral("width"), Qt::CaseInsensitive));
}
void ClassTransferExportSourceTests::malformedRosterRowIndexReturnsEstablishedReadError()
{
    QTemporaryDir directory; QVERIFY(directory.isValid()); ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath(QStringLiteral("source.db"))));
    auto database = services.databaseSession()->database();
    QSqlQuery insertClass(database);
    QVERIFY(insertClass.exec(QStringLiteral("INSERT INTO classes(name) VALUES('Malformed roster row index')")));
    const int classId = insertClass.lastInsertId().toInt();

    QSqlQuery insertColumn(database);
    QVERIFY(insertColumn.prepare(QStringLiteral(
        "INSERT INTO roster_columns(class_id,name,position,width) VALUES(?, 'Student', 0, 120)")));
    insertColumn.addBindValue(classId);
    QVERIFY2(insertColumn.exec(), qPrintable(insertColumn.lastError().text()));

    QSqlQuery insertValidRow(database);
    QVERIFY(insertValidRow.prepare(QStringLiteral(
        "INSERT INTO roster_data(class_id,row_index,col_index,value) VALUES(?, 0, 0, 'Valid row zero')")));
    insertValidRow.addBindValue(classId);
    QVERIFY2(insertValidRow.exec(), qPrintable(insertValidRow.lastError().text()));

    QSqlQuery insertMalformedRow(database);
    QVERIFY(insertMalformedRow.prepare(QStringLiteral(
        "INSERT INTO roster_data(class_id,row_index,col_index,value) VALUES(?, ?, 0, 'Malformed value')")));
    insertMalformedRow.addBindValue(classId);
    insertMalformedRow.addBindValue(QStringLiteral("not-a-row-index"));
    QVERIFY2(insertMalformedRow.exec(), qPrintable(insertMalformedRow.lastError().text()));

    QSqlQuery storedRowIndex(database);
    QVERIFY(storedRowIndex.exec(QStringLiteral(
        "SELECT typeof(row_index), row_index FROM roster_data "
        "WHERE class_id=%1 AND value='Malformed value'").arg(classId)));
    QVERIFY(storedRowIndex.next());
    QCOMPARE(storedRowIndex.value(0).toString(), QStringLiteral("text"));
    QCOMPARE(storedRowIndex.value(1).toString(), QStringLiteral("not-a-row-index"));

    Platform::ApplicationServicesClassTransferExportSourceReadPort port(services);
    auto source = port.readSource({{id(classId)}}); QVERIFY(source);
    QCOMPARE(source.value().classes.size(), std::size_t(1));
    const auto& roster = source.value().classes.front().roster;
    QCOMPARE(roster.state(), ClassExportStageState::Error);
    QCOMPARE(roster.error().message,
        QStringLiteral("Loading roster data failed for class id %1: invalid row index.")
            .arg(classId).toStdString());
}
void ClassTransferExportSourceTests::fractionalRosterRowIndexReturnsEstablishedReadError()
{
    QTemporaryDir directory; QVERIFY(directory.isValid()); ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath(QStringLiteral("source.db"))));
    auto database = services.databaseSession()->database();
    QSqlQuery insertClass(database);
    QVERIFY(insertClass.exec(QStringLiteral("INSERT INTO classes(name) VALUES('Fractional roster row index')")));
    const int classId = insertClass.lastInsertId().toInt();

    QSqlQuery insertColumn(database);
    QVERIFY(insertColumn.prepare(QStringLiteral(
        "INSERT INTO roster_columns(class_id,name,position,width) VALUES(?, 'Student', 0, 120)")));
    insertColumn.addBindValue(classId);
    QVERIFY2(insertColumn.exec(), qPrintable(insertColumn.lastError().text()));

    QSqlQuery insertValidRow(database);
    QVERIFY(insertValidRow.prepare(QStringLiteral(
        "INSERT INTO roster_data(class_id,row_index,col_index,value) VALUES(?, 0, 0, 'Valid row zero')")));
    insertValidRow.addBindValue(classId);
    QVERIFY2(insertValidRow.exec(), qPrintable(insertValidRow.lastError().text()));

    QSqlQuery insertFractionalRow(database);
    QVERIFY(insertFractionalRow.prepare(QStringLiteral(
        "INSERT INTO roster_data(class_id,row_index,col_index,value) VALUES(?, ?, 0, 'Fractional value')")));
    insertFractionalRow.addBindValue(classId);
    insertFractionalRow.addBindValue(0.25);
    QVERIFY2(insertFractionalRow.exec(), qPrintable(insertFractionalRow.lastError().text()));

    QSqlQuery storedRows(database);
    QVERIFY(storedRows.exec(QStringLiteral(
        "SELECT typeof(row_index), row_index, value FROM roster_data "
        "WHERE class_id=%1 ORDER BY row_index").arg(classId)));
    QVERIFY(storedRows.next());
    QCOMPARE(storedRows.value(0).toString(), QStringLiteral("integer"));
    QCOMPARE(storedRows.value(1).toLongLong(), qlonglong(0));
    QCOMPARE(storedRows.value(2).toString(), QStringLiteral("Valid row zero"));
    QVERIFY(storedRows.next());
    QCOMPARE(storedRows.value(0).toString(), QStringLiteral("real"));
    QCOMPARE(storedRows.value(1).toDouble(), 0.25);
    QCOMPARE(storedRows.value(2).toString(), QStringLiteral("Fractional value"));
    QVERIFY(!storedRows.next());

    Platform::ApplicationServicesClassTransferExportSourceReadPort port(services);
    auto source = port.readSource({{id(classId)}}); QVERIFY(source);
    QCOMPARE(source.value().classes.size(), std::size_t(1));
    const auto& roster = source.value().classes.front().roster;
    QCOMPARE(roster.state(), ClassExportStageState::Error);
    QCOMPARE(roster.error().message,
        QStringLiteral("Loading roster data failed for class id %1: invalid row index.")
            .arg(classId).toStdString());
}
void ClassTransferExportSourceTests::sparseIntegerRosterRowRemainsValid()
{
    QTemporaryDir directory; QVERIFY(directory.isValid()); ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath(QStringLiteral("source.db"))));
    auto database = services.databaseSession()->database();
    QSqlQuery insertClass(database);
    QVERIFY(insertClass.exec(QStringLiteral("INSERT INTO classes(name) VALUES('Sparse integer roster row')")));
    const int classId = insertClass.lastInsertId().toInt();

    QSqlQuery insertColumn(database);
    QVERIFY(insertColumn.prepare(QStringLiteral(
        "INSERT INTO roster_columns(class_id,name,position,width) VALUES(?, 'Student', 0, 120)")));
    insertColumn.addBindValue(classId);
    QVERIFY2(insertColumn.exec(), qPrintable(insertColumn.lastError().text()));

    QSqlQuery insertSparseRow(database);
    QVERIFY(insertSparseRow.prepare(QStringLiteral(
        "INSERT INTO roster_data(class_id,row_index,col_index,value) VALUES(?, 3, 0, 'Sparse row three')")));
    insertSparseRow.addBindValue(classId);
    QVERIFY2(insertSparseRow.exec(), qPrintable(insertSparseRow.lastError().text()));

    QSqlQuery storedRow(database);
    QVERIFY(storedRow.exec(QStringLiteral(
        "SELECT typeof(row_index), row_index FROM roster_data WHERE class_id=%1")
            .arg(classId)));
    QVERIFY(storedRow.next());
    QCOMPARE(storedRow.value(0).toString(), QStringLiteral("integer"));
    QCOMPARE(storedRow.value(1).toInt(), 3);
    QVERIFY(!storedRow.next());

    Platform::ApplicationServicesClassTransferExportSourceReadPort port(services);
    auto source = port.readSource({{id(classId)}}); QVERIFY(source);
    QCOMPARE(source.value().classes.size(), std::size_t(1));
    const auto& roster = source.value().classes.front().roster;
    QCOMPARE(roster.state(), ClassExportStageState::Value);
    QCOMPARE(roster.value().rows.size(), 4);
    QCOMPARE(roster.value().rows[0][0], std::u16string());
    QCOMPARE(roster.value().rows[1][0], std::u16string());
    QCOMPARE(roster.value().rows[2][0], std::u16string());
    QCOMPARE(roster.value().rows[3][0], std::u16string(u"Sparse row three"));
}
void ClassTransferExportSourceTests::outOfRangeRosterRowIndexReturnsEstablishedReadError()
{
    QTemporaryDir directory; QVERIFY(directory.isValid()); ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath(QStringLiteral("source.db"))));
    auto database = services.databaseSession()->database();
    QSqlQuery insertClass(database);
    QVERIFY(insertClass.exec(QStringLiteral("INSERT INTO classes(name) VALUES('Out of range roster row index')")));
    const int classId = insertClass.lastInsertId().toInt();

    QSqlQuery insertColumn(database);
    QVERIFY(insertColumn.prepare(QStringLiteral(
        "INSERT INTO roster_columns(class_id,name,position,width) VALUES(?, 'Student', 0, 120)")));
    insertColumn.addBindValue(classId);
    QVERIFY2(insertColumn.exec(), qPrintable(insertColumn.lastError().text()));

    QSqlQuery insertValidRow(database);
    QVERIFY(insertValidRow.prepare(QStringLiteral(
        "INSERT INTO roster_data(class_id,row_index,col_index,value) VALUES(?, 0, 0, 'Valid row zero')")));
    insertValidRow.addBindValue(classId);
    QVERIFY2(insertValidRow.exec(), qPrintable(insertValidRow.lastError().text()));

    QSqlQuery insertOutOfRangeRow(database);
    QVERIFY(insertOutOfRangeRow.prepare(QStringLiteral(
        "INSERT INTO roster_data(class_id,row_index,col_index,value) VALUES(?, ?, 0, 'Out of range value')")));
    insertOutOfRangeRow.addBindValue(classId);
    insertOutOfRangeRow.addBindValue(QVariant::fromValue<qlonglong>(2147483648LL));
    QVERIFY2(insertOutOfRangeRow.exec(), qPrintable(insertOutOfRangeRow.lastError().text()));

    QSqlQuery storedRowIndex(database);
    QVERIFY(storedRowIndex.exec(QStringLiteral(
        "SELECT typeof(row_index), row_index FROM roster_data "
        "WHERE class_id=%1 AND value='Out of range value'").arg(classId)));
    QVERIFY(storedRowIndex.next());
    QCOMPARE(storedRowIndex.value(0).toString(), QStringLiteral("integer"));
    QCOMPARE(storedRowIndex.value(1).toLongLong(), qlonglong(2147483648LL));

    Platform::ApplicationServicesClassTransferExportSourceReadPort port(services);
    auto source = port.readSource({{id(classId)}}); QVERIFY(source);
    QCOMPARE(source.value().classes.size(), std::size_t(1));
    const auto& roster = source.value().classes.front().roster;
    QCOMPARE(roster.state(), ClassExportStageState::Error);
    QCOMPARE(roster.error().message,
        QStringLiteral("Loading roster data failed for class id %1: row index 2147483648 exceeds the supported range.")
            .arg(classId).toStdString());
}
void ClassTransferExportSourceTests::wideTeacherIdentityKeepsFullProfileAndSharedPortableReference()
{
    QTemporaryDir directory; QVERIFY(directory.isValid()); ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath(QStringLiteral("source.db"))));
    auto database = services.databaseSession()->database();
    constexpr qint64 teacherId = 2147483648LL;
    const auto typedTeacherId = *Domain::TeacherId::fromString("2147483648");
    const ClassExportTeacherProfile expected{
        .teacherKr = u"\uAE40\uBBFC\uC218",
        .teacherEn = u"Wide Teacher",
        .preferredRomanization = u"Kim Min Su",
        .preferredName = u"Min Su",
        .roomNumber = u"501",
        .birthday = u"02-29",
        .phoneNumber = u"010-1234-5678",
        .wifiName = u"Teacher WiFi",
        .wifiPassword = u"wifi-secret",
        .internetType = u"Both",
        .zoomId = u"teacher.zoom",
        .zoomPassword = u"zoom-secret",
        .projectionType = u"Any",
        .notes = u"Full profile\nSecond line"
    };
    QSqlQuery insertTeacher(database);
    QVERIFY(insertTeacher.prepare(QStringLiteral(
        "INSERT INTO teachers (id, teacher_kr, teacher_en, preferred_romanization, preferred_name, room_number, "
        "birthday, phone_number, wifi_name, wifi_password, internet_type, zoom_id, zoom_password, projection_type, notes) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")));
    insertTeacher.addBindValue(QVariant::fromValue(teacherId));
    insertTeacher.addBindValue(QString::fromStdU16String(expected.teacherKr));
    insertTeacher.addBindValue(QString::fromStdU16String(expected.teacherEn));
    insertTeacher.addBindValue(QString::fromStdU16String(expected.preferredRomanization));
    insertTeacher.addBindValue(QString::fromStdU16String(expected.preferredName));
    insertTeacher.addBindValue(QString::fromStdU16String(expected.roomNumber));
    insertTeacher.addBindValue(QString::fromStdU16String(expected.birthday));
    insertTeacher.addBindValue(QString::fromStdU16String(expected.phoneNumber));
    insertTeacher.addBindValue(QString::fromStdU16String(expected.wifiName));
    insertTeacher.addBindValue(QString::fromStdU16String(expected.wifiPassword));
    insertTeacher.addBindValue(QString::fromStdU16String(expected.internetType));
    insertTeacher.addBindValue(QString::fromStdU16String(expected.zoomId));
    insertTeacher.addBindValue(QString::fromStdU16String(expected.zoomPassword));
    insertTeacher.addBindValue(QString::fromStdU16String(expected.projectionType));
    insertTeacher.addBindValue(QString::fromStdU16String(expected.notes));
    QVERIFY2(insertTeacher.exec(), qPrintable(insertTeacher.lastError().text()));
    QList<int> classes;
    for (const auto& name : {
             QStringLiteral("First wide class \U0001F393"),
             QStringLiteral("Second wide class \U0001F393")})
    {
        QSqlQuery insertClass(database);
        insertClass.prepare(QStringLiteral("INSERT INTO classes(name) VALUES (?)"));
        insertClass.addBindValue(name);
        QVERIFY2(insertClass.exec(), qPrintable(insertClass.lastError().text()));
        const int classId = insertClass.lastInsertId().toInt();
        classes.append(classId);
        QSqlQuery insertInfo(database);
        insertInfo.prepare(QStringLiteral(
            "INSERT INTO class_info(class_id,teacher_id,class_grade,class_level,notes) VALUES (?,?,'E4','Perseus',?)"));
        insertInfo.addBindValue(classId);
        insertInfo.addBindValue(QVariant::fromValue(teacherId));
        insertInfo.addBindValue(name + QStringLiteral(" notes"));
        QVERIFY2(insertInfo.exec(), qPrintable(insertInfo.lastError().text()));
    }
    QSqlQuery stored(database);
    QVERIFY(stored.exec(QStringLiteral(
        "SELECT typeof(teachers.id), teachers.id, typeof(class_info.teacher_id), class_info.teacher_id "
        "FROM teachers JOIN class_info ON class_info.teacher_id=teachers.id "
        "WHERE teachers.id=2147483648 ORDER BY class_info.class_id")));
    for (int index = 0; index < classes.size(); ++index)
    {
        QVERIFY(stored.next());
        QCOMPARE(stored.value(0).toString(), QStringLiteral("integer"));
        QCOMPARE(stored.value(1).toLongLong(), teacherId);
        QCOMPARE(stored.value(2).toString(), QStringLiteral("integer"));
        QCOMPARE(stored.value(3).toLongLong(), teacherId);
    }
    QVERIFY(!stored.next());
    // Reverse selection order to verify source and generated class key order.
    const ClassTransferExportRequest request{{id(classes[1]), id(classes[0])}};
    Platform::ApplicationServicesClassTransferExportSourceReadPort port(services);
    auto source = port.readSource(request); QVERIFY(source);
    QCOMPARE(source.value().classes.size(), std::size_t(2));
    QCOMPARE(source.value().teachers.size(), std::size_t(1));
    QVERIFY(source.value().teachers[0].id == typedTeacherId);
    QCOMPARE(source.value().teachers[0].profile.state(), ClassExportStageState::Value);
    const auto verifyProfile = [&expected](const ClassExportTeacherProfile& actual)
    {
        QCOMPARE(actual.teacherKr, expected.teacherKr);
        QCOMPARE(actual.teacherEn, expected.teacherEn);
        QCOMPARE(actual.preferredRomanization, expected.preferredRomanization);
        QCOMPARE(actual.preferredName, expected.preferredName);
        QCOMPARE(actual.roomNumber, expected.roomNumber);
        QCOMPARE(actual.birthday, expected.birthday);
        QCOMPARE(actual.phoneNumber, expected.phoneNumber);
        QCOMPARE(actual.wifiName, expected.wifiName);
        QCOMPARE(actual.wifiPassword, expected.wifiPassword);
        QCOMPARE(actual.internetType, expected.internetType);
        QCOMPARE(actual.zoomId, expected.zoomId);
        QCOMPARE(actual.zoomPassword, expected.zoomPassword);
        QCOMPARE(actual.projectionType, expected.projectionType);
        QCOMPARE(actual.notes, expected.notes);
    };
    verifyProfile(source.value().teachers[0].profile.value());
    for (std::size_t index = 0; index < source.value().classes.size(); ++index)
    {
        const auto& item = source.value().classes[index];
        QVERIFY(item.id == request.classIds[index]);
        QCOMPARE(item.selectedIndex, index);
        QCOMPARE(item.info.state(), ClassExportStageState::Value);
        QCOMPARE(item.info.value().sourceClassId, classes[1 - static_cast<int>(index)]);
        QCOMPARE(item.info.value().sourceTeacherId, std::int64_t(teacherId));
        QCOMPARE(item.teacher.state(), ClassExportStageState::Value);
        QVERIFY(item.teacher.value());
        QVERIFY(*item.teacher.value() == typedTeacherId);
        QCOMPARE(item.roster.state(), ClassExportStageState::Value);
        QCOMPARE(item.evaluations.state(), ClassExportStageState::Value);
    }
    auto package = ClassTransferExportQuery(port).execute(request); QVERIFY(package);
    QCOMPARE(package.value().teachers.size(), std::size_t(1));
    QCOMPARE(package.value().classes.size(), std::size_t(2));
    QCOMPARE(package.value().teachers[0].key.value(), std::u16string(u"teacher-1"));
    verifyProfile(package.value().teachers[0].teacher);
    QCOMPARE(package.value().classes[0].key.value(), std::u16string(u"class-1"));
    QCOMPARE(package.value().classes[1].key.value(), std::u16string(u"class-2"));
    QCOMPARE(package.value().classes[0].name, std::u16string(u"Second wide class \U0001F393"));
    QCOMPARE(package.value().classes[1].name, std::u16string(u"First wide class \U0001F393"));
    for (const auto& item : package.value().classes)
        QCOMPARE(item.teacherKey.value(), package.value().teachers[0].key.value());
    const auto json = ClassTransferJsonCodec::toJson(package.value());
    const auto teachersJson = json.value(QStringLiteral("teachers")).toArray();
    const auto classesJson = json.value(QStringLiteral("classes")).toArray();
    QCOMPARE(teachersJson.size(), 1);
    QCOMPARE(teachersJson[0].toObject().size(), 15); // Portable key and all 14 profile fields.
    QVERIFY(!teachersJson[0].toObject().contains(QStringLiteral("id")));
    for (const auto& item : classesJson)
    {
        const auto object = item.toObject();
        QCOMPARE(object.value(QStringLiteral("teacher_ref")).toString(), QStringLiteral("teacher-1"));
        QVERIFY(!object.value(QStringLiteral("info")).toObject().contains(QStringLiteral("teacher_id")));
    }
    const auto bytes = QJsonDocument(json).toJson(QJsonDocument::Compact);
    QVERIFY(!bytes.contains("2147483648"));
    QVERIFY(!bytes.contains("-2147483648"));
    auto legacy = services.databaseSession()->classTransferRepository()->buildPackage({classes[1], classes[0]});
    QVERIFY(legacy);
    QCOMPARE(legacy->teachers.size(), 1);
    QCOMPARE(legacy->teachers[0].teacher.id, -1);
    for (const auto& item : legacy->classes)
    {
        QCOMPARE(item.teacherKey, QStringLiteral("teacher-1"));
        QCOMPARE(item.info.classId, -1);
        QCOMPARE(item.info.teacherId, -1);
    }
    auto legacyJson = ClassTransferJsonCodec::toJson(*legacy);
    legacyJson.remove(QStringLiteral("exported_at_utc"));
    auto typedJson = json;
    typedJson.remove(QStringLiteral("exported_at_utc"));
    QCOMPARE(legacyJson, typedJson);
    QVERIFY(database.transaction()); QVERIFY(database.rollback());
}
void ClassTransferExportSourceTests::wideEvaluationIdsKeepTheirOwnExportRows()
{
    QTemporaryDir directory; QVERIFY(directory.isValid()); ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath(QStringLiteral("source.db"))));
    auto database = services.databaseSession()->database();
    QSqlQuery insertClass(database);
    QVERIFY(insertClass.exec(QStringLiteral("INSERT INTO classes(name) VALUES('Wide Evaluation IDs')")));
    const int classId = insertClass.lastInsertId().toInt();

    struct EvaluationFixture
    {
        qlonglong id;
        QString name;
        QString marker;
    };
    const EvaluationFixture fixtures[] = {
        {2147483648LL, QStringLiteral("Wide evaluation A"), QStringLiteral("Marker A")},
        {2147483649LL, QStringLiteral("Wide evaluation B"), QStringLiteral("Marker B")}
    };
    for (const auto& fixture : fixtures)
    {
        QSqlQuery insertEvaluation(database);
        insertEvaluation.prepare(QStringLiteral(
            "INSERT INTO speaking_evaluations (id, class_id, evaluation_name) VALUES (?, ?, ?)"));
        insertEvaluation.addBindValue(QVariant::fromValue<qlonglong>(fixture.id));
        insertEvaluation.addBindValue(classId);
        insertEvaluation.addBindValue(fixture.name);
        QVERIFY2(insertEvaluation.exec(), qPrintable(insertEvaluation.lastError().text()));

        QSqlQuery insertRow(database);
        insertRow.prepare(QStringLiteral(
            "INSERT INTO speaking_eval_data (evaluation_id, row_index, col_0) VALUES (?, 0, ?)"));
        insertRow.addBindValue(QVariant::fromValue<qlonglong>(fixture.id));
        insertRow.addBindValue(fixture.marker);
        QVERIFY2(insertRow.exec(), qPrintable(insertRow.lastError().text()));
    }

    QSqlQuery storedIds(database);
    QVERIFY(storedIds.exec(QStringLiteral(
        "SELECT typeof(id), id FROM speaking_evaluations "
        "WHERE id IN (2147483648, 2147483649) ORDER BY id")));
    for (const auto& fixture : fixtures)
    {
        QVERIFY(storedIds.next());
        QCOMPARE(storedIds.value(0).toString(), QStringLiteral("integer"));
        QCOMPARE(storedIds.value(1).toLongLong(), fixture.id);
    }
    QVERIFY(!storedIds.next());

    Platform::ApplicationServicesClassTransferExportSourceReadPort port(services);
    auto package = ClassTransferExportQuery(port).execute({{id(classId)}});
    QVERIFY(package);
    QCOMPARE(package.value().classes.size(), std::size_t(1));
    const auto& evaluations = package.value().classes.front().evaluations;
    QCOMPARE(evaluations.size(), std::size_t(2));

    bool foundA = false;
    bool foundB = false;
    for (const auto& evaluation : evaluations)
    {
        QVERIFY(!evaluation.rows.empty());
        QVERIFY(!evaluation.rows.front().empty());
        if (evaluation.name == u"Wide evaluation A")
        {
            foundA = true;
            QCOMPARE(evaluation.rows.front().front(), std::u16string(u"Marker A"));
        }
        else if (evaluation.name == u"Wide evaluation B")
        {
            foundB = true;
            QCOMPARE(evaluation.rows.front().front(), std::u16string(u"Marker B"));
        }
        else
        {
            QFAIL("Unexpected exported evaluation name");
        }
    }
    QVERIFY(foundA);
    QVERIFY(foundB);
}
void ClassTransferExportSourceTests::partialEvaluationHeaderFetchErrorFailsEvaluationStage()
{
    QTemporaryDir directory; QVERIFY(directory.isValid()); ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath(QStringLiteral("source.db"))));
    auto database = services.databaseSession()->database();
    QSqlQuery insertClass(database);
    QVERIFY(insertClass.exec(QStringLiteral(
        "INSERT INTO classes(name) VALUES('Partial evaluation header fetch')")));
    const int classId = insertClass.lastInsertId().toInt();

    QSqlQuery setup(database);
    QVERIFY2(setup.exec(QStringLiteral(
        "CREATE TEMP TABLE class_transfer_evaluation_payloads ("
        "id INTEGER NOT NULL, class_id INTEGER NOT NULL, payload TEXT NOT NULL)")),
        qPrintable(setup.lastError().text()));
    QVERIFY2(setup.exec(QStringLiteral(
        "CREATE INDEX class_transfer_evaluation_payload_order "
        "ON class_transfer_evaluation_payloads(class_id, id)")),
        qPrintable(setup.lastError().text()));
    QVERIFY2(setup.exec(QStringLiteral(
        "CREATE TEMP VIEW speaking_evaluations AS "
        "SELECT id, class_id, json_extract(payload, '$.name') AS evaluation_name "
        "FROM class_transfer_evaluation_payloads")),
        qPrintable(setup.lastError().text()));

    QSqlQuery insertPayload(database);
    QVERIFY(insertPayload.prepare(QStringLiteral(
        "INSERT INTO class_transfer_evaluation_payloads(id, class_id, payload) "
        "VALUES(?, ?, ?)")));
    insertPayload.addBindValue(101);
    insertPayload.addBindValue(classId);
    insertPayload.addBindValue(QStringLiteral("{\"name\":\"Valid header\"}"));
    QVERIFY2(insertPayload.exec(), qPrintable(insertPayload.lastError().text()));
    QSqlQuery insertMalformedPayload(database);
    QVERIFY(insertMalformedPayload.prepare(QStringLiteral(
        "INSERT INTO class_transfer_evaluation_payloads(id, class_id, payload) "
        "VALUES(?, ?, ?)")));
    insertMalformedPayload.addBindValue(102);
    insertMalformedPayload.addBindValue(classId);
    insertMalformedPayload.addBindValue(QStringLiteral("{"));
    QVERIFY2(insertMalformedPayload.exec(),
        qPrintable(insertMalformedPayload.lastError().text()));

    QSqlQuery evaluationQuery(database);
    QVERIFY(evaluationQuery.prepare(QStringLiteral(R"(
        SELECT id, evaluation_name
        FROM speaking_evaluations
        WHERE class_id=?
        ORDER BY id
    )")));
    evaluationQuery.addBindValue(classId);
    QVERIFY2(evaluationQuery.exec(), qPrintable(evaluationQuery.lastError().text()));
    QVERIFY(evaluationQuery.next());
    QCOMPARE(evaluationQuery.value(QStringLiteral("id")).toLongLong(), qlonglong(101));
    QCOMPARE(evaluationQuery.value(QStringLiteral("evaluation_name")).toString(),
        QStringLiteral("Valid header"));
    QVERIFY(!evaluationQuery.next());
    QVERIFY(evaluationQuery.lastError().isValid());
    const QString queryError = evaluationQuery.lastError().text();
    QVERIFY2(queryError.contains(QStringLiteral("malformed JSON"), Qt::CaseInsensitive),
        qPrintable(queryError));
    evaluationQuery.finish();

    Platform::ApplicationServicesClassTransferExportSourceReadPort port(services);
    const auto source = port.readSource({{id(classId)}});
    QVERIFY(source);
    QCOMPARE(source.value().classes.size(), std::size_t(1));
    const auto& evaluations = source.value().classes.front().evaluations;
    QCOMPARE(evaluations.state(), ClassExportStageState::Error);
    QCOMPARE(evaluations.error().code, Domain::ErrorCode::Technical);
    const QString diagnostic = QString::fromStdString(evaluations.error().message);
    QVERIFY2(diagnostic.startsWith(QStringLiteral(
        "Unable to read speaking evaluations: "))
        && diagnostic.contains(QStringLiteral("malformed JSON"), Qt::CaseInsensitive),
        qPrintable(diagnostic));
}
void ClassTransferExportSourceTests::partialEvaluationMatrixFetchErrorFailsEvaluationStage()
{
    QTemporaryDir directory; QVERIFY(directory.isValid()); ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath(QStringLiteral("source.db"))));
    auto database = services.databaseSession()->database();
    QSqlQuery insertClass(database);
    QVERIFY(insertClass.exec(QStringLiteral(
        "INSERT INTO classes(name) VALUES('Partial evaluation matrix fetch')")));
    const int classId = insertClass.lastInsertId().toInt();

    QSqlQuery insertEvaluation(database);
    QVERIFY(insertEvaluation.prepare(QStringLiteral(
        "INSERT INTO speaking_evaluations(class_id, evaluation_name) VALUES(?, ?)")));
    insertEvaluation.addBindValue(classId);
    insertEvaluation.addBindValue(QStringLiteral("Valid header"));
    QVERIFY2(insertEvaluation.exec(), qPrintable(insertEvaluation.lastError().text()));
    const qlonglong evaluationId = insertEvaluation.lastInsertId().toLongLong();

    QSqlQuery setup(database);
    QVERIFY2(setup.exec(QStringLiteral(
        "CREATE TEMP TABLE class_transfer_evaluation_row_payloads ("
        "evaluation_id INTEGER NOT NULL, row_index INTEGER NOT NULL, payload TEXT NOT NULL)")),
        qPrintable(setup.lastError().text()));
    QVERIFY2(setup.exec(QStringLiteral(
        "CREATE INDEX class_transfer_evaluation_row_order "
        "ON class_transfer_evaluation_row_payloads(evaluation_id, row_index)")),
        qPrintable(setup.lastError().text()));
    QVERIFY2(setup.exec(QStringLiteral(
        "CREATE TEMP VIEW speaking_eval_data AS "
        "SELECT evaluation_id, row_index, "
        "json_extract(payload, '$.col_0') AS col_0, "
        "NULL AS col_1, NULL AS col_2, NULL AS col_3, NULL AS col_4, "
        "NULL AS col_5, NULL AS col_6, NULL AS col_7, NULL AS col_8, "
        "NULL AS col_9, NULL AS col_10 "
        "FROM class_transfer_evaluation_row_payloads")),
        qPrintable(setup.lastError().text()));

    QSqlQuery insertPayload(database);
    QVERIFY(insertPayload.prepare(QStringLiteral(
        "INSERT INTO class_transfer_evaluation_row_payloads "
        "(evaluation_id, row_index, payload) VALUES(?, ?, ?)")));
    insertPayload.addBindValue(evaluationId);
    insertPayload.addBindValue(0);
    insertPayload.addBindValue(QStringLiteral("{\"col_0\":\"Valid first matrix row\"}"));
    QVERIFY2(insertPayload.exec(), qPrintable(insertPayload.lastError().text()));
    QSqlQuery insertMalformedPayload(database);
    QVERIFY(insertMalformedPayload.prepare(QStringLiteral(
        "INSERT INTO class_transfer_evaluation_row_payloads "
        "(evaluation_id, row_index, payload) VALUES(?, ?, ?)")));
    insertMalformedPayload.addBindValue(evaluationId);
    insertMalformedPayload.addBindValue(1);
    insertMalformedPayload.addBindValue(QStringLiteral("{"));
    QVERIFY2(insertMalformedPayload.exec(),
        qPrintable(insertMalformedPayload.lastError().text()));

    QSqlQuery rowQuery(database);
    QVERIFY(rowQuery.prepare(QStringLiteral(R"(
        SELECT evaluation_id, row_index,
            typeof(row_index) AS row_index_storage_type,
            col_0, col_1, col_2, col_3, col_4, col_5,
            col_6, col_7, col_8, col_9, col_10
        FROM speaking_eval_data
        WHERE evaluation_id IN (
            SELECT id
            FROM speaking_evaluations
            WHERE class_id=?
        )
        ORDER BY evaluation_id, row_index
    )")));
    rowQuery.addBindValue(classId);
    QVERIFY2(rowQuery.exec(), qPrintable(rowQuery.lastError().text()));
    QVERIFY(rowQuery.next());
    QCOMPARE(rowQuery.value(QStringLiteral("evaluation_id")).toLongLong(), evaluationId);
    QCOMPARE(rowQuery.value(QStringLiteral("row_index")).toInt(), 0);
    QCOMPARE(rowQuery.value(QStringLiteral("row_index_storage_type")).toString(),
        QStringLiteral("integer"));
    QCOMPARE(rowQuery.value(QStringLiteral("col_0")).toString(),
        QStringLiteral("Valid first matrix row"));
    QVERIFY(!rowQuery.next());
    QVERIFY(rowQuery.lastError().isValid());
    const QString queryError = rowQuery.lastError().text();
    QVERIFY2(queryError.contains(QStringLiteral("malformed JSON"), Qt::CaseInsensitive),
        qPrintable(queryError));
    rowQuery.finish();

    Platform::ApplicationServicesClassTransferExportSourceReadPort port(services);
    const auto source = port.readSource({{id(classId)}});
    QVERIFY(source);
    QCOMPARE(source.value().classes.size(), std::size_t(1));
    const auto& evaluations = source.value().classes.front().evaluations;
    QCOMPARE(evaluations.state(), ClassExportStageState::Error);
    QCOMPARE(evaluations.error().code, Domain::ErrorCode::Technical);
    const QString diagnostic = QString::fromStdString(evaluations.error().message);
    QVERIFY2(diagnostic.startsWith(QStringLiteral(
        "Unable to read speaking evaluation rows: "))
        && diagnostic.contains(QStringLiteral("malformed JSON"), Qt::CaseInsensitive),
        qPrintable(diagnostic));
}
void ClassTransferExportSourceTests::stagedErrorRollsBackAndKeepsDownstreamNotAttempted()
{
    QTemporaryDir directory; QVERIFY(directory.isValid()); ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath(QStringLiteral("source.db"))));
    auto database = services.databaseSession()->database(); QSqlQuery sql(database);
    QVERIFY(sql.exec(QStringLiteral("INSERT INTO classes(name) VALUES('First')"))); const int first = sql.lastInsertId().toInt();
    QVERIFY(sql.exec(QStringLiteral("INSERT INTO classes(name) VALUES('Second')"))); const int second = sql.lastInsertId().toInt();
    QVERIFY(sql.exec(QStringLiteral("DROP TABLE class_times")));
    Platform::ApplicationServicesClassTransferExportSourceReadPort port(services);
    auto source = port.readSource({{id(first), id(second), id(-1)}}); QVERIFY(source);
    QCOMPARE(source.value().classes.size(), std::size_t(2));
    QVERIFY(source.value().deferredSelectionOrLookupError);
    for (const auto& item : source.value().classes)
    {
        QCOMPARE(item.info.state(), ClassExportStageState::Error);
        QCOMPARE(item.roster.state(), ClassExportStageState::Value);
        QCOMPARE(item.teacher.state(), ClassExportStageState::NotAttempted);
        QCOMPARE(item.evaluations.state(), ClassExportStageState::NotAttempted);
    }
    auto replay = ClassTransferExportQuery(port).execute({{id(first), id(second), id(-1)}}); QVERIFY(!replay);
    QVERIFY(replay.error().message == source.value().classes[0].info.error().message);
    QVERIFY(database.transaction()); QVERIFY(database.rollback());
}
void ClassTransferExportSourceTests::beginAndUnavailableErrorsStayAtSourceBoundary()
{
    ApplicationServices services;
    Platform::ApplicationServicesClassTransferExportSourceReadPort port(services);
    auto missing = port.readSource({{id(1)}}); QVERIFY(!missing);
    const auto legacy = services.classService()->buildTransferPackage({1}); QVERIFY(!legacy);
    QVERIFY(missing.error().message == "No Teacher Profile service is available.");
    QCOMPARE(QString::fromStdString(missing.error().message), legacy.error());
    QTemporaryDir directory; QVERIFY(directory.isValid()); QVERIFY(services.openDatabase(directory.filePath(QStringLiteral("source.db"))));
    auto database = services.databaseSession()->database(); QVERIFY(database.transaction());
    auto result = port.readSource({{id(1)}}); QVERIFY(!result);
    QVERIFY(result.error().message.starts_with("Unable to start the class export transaction: "));
    QVERIFY(database.rollback());
}
QTEST_GUILESS_MAIN(ClassTransferExportSourceTests)
#include "next_platform_class_transfer_export_source_tests.moc"
