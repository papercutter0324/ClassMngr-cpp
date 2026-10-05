#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/class_transfer_repository.h"
#include "features/classes/services/class_transfer_json_codec.h"
#if __has_include("next/platform/application_services_class_transfer_export_source_read_port.h")
#include "next/platform/application_services_class_transfer_export_source_read_port.h"
#define CLASSMNGR_TYPED_EXPORT 1
#endif
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest/QtTest>

namespace
{
struct Fixture
{
    QTemporaryDir directory;
    ApplicationServices services;
    QList<int> ids;
    QSqlDatabase database() { return services.dataService()->databaseSession()->database(); }
    bool sql(const QString& text)
    {
        QSqlQuery query(database());
        if (query.exec(text)) return true;
        qWarning().noquote() << query.lastError().text(); return false;
    }
    bool open()
    {
        if (!directory.isValid() || !services.openDatabase(directory.filePath(QStringLiteral("export.db")))) return false;
        DataService& data = *services.dataService();
        QList<int> teachers;
        for (const auto& name : {QStringLiteral("Alice"), QStringLiteral("Bob")})
        {
            Teacher teacher;
            teacher.teacherEn = name; teacher.teacherKr = QStringLiteral("\uAE40\uBBFC\uC218");
            teacher.preferredRomanization = name + QStringLiteral(" Romanized");
            teacher.preferredName = teacher.preferredRomanization; teacher.roomNumber = QStringLiteral("501");
            teacher.birthday = QStringLiteral("02-29"); teacher.phoneNumber = QStringLiteral("010-1234-5678");
            teacher.wifiName = name + QStringLiteral(" WiFi"); teacher.wifiPassword = QStringLiteral("wifi-secret");
            teacher.internetType = QStringLiteral("Both"); teacher.zoomId = name + QStringLiteral(".zoom");
            teacher.zoomPassword = QStringLiteral("zoom-secret"); teacher.projectionType = QStringLiteral("Any");
            teacher.notes = QStringLiteral("Full \uD55C\uAE00 profile\nSecond line \U0001F393");
            auto result = data.createTeacher(teacher); if (!result) return false; teachers.append(*result);
        }
        const QStringList days{QStringLiteral("Monday"), QStringLiteral("Tuesday"), QStringLiteral("Wednesday"), QStringLiteral("Friday")};
        for (int i = 0; i < 4; ++i)
        {
            auto created = data.createClass(QStringLiteral("Stored class %1").arg(i + 1));
            if (!created) return false; ids.append(*created);
            ClassInfo info; info.classId = *created; info.teacherId = i == 3 ? -1 : teachers.at(i == 1 ? 1 : 0);
            info.classGrade = QStringLiteral("E4"); info.classLevel = QStringLiteral("Theseus");
            info.readingBook = QStringLiteral("Reading Explorer 1"); info.essayBook = QStringLiteral("4A");
            info.classColor = QStringLiteral("#123456"); info.fontColor = QStringLiteral("#FEDCBA");
            info.notes = QStringLiteral("Class notes\n\uD55C\uAE00"); info.timeFillerActivities = QStringLiteral("Word chain");
            info.classTimes = {{days.at(i), QStringLiteral("4:00 PM"), QStringLiteral("4:50 PM")},
                {days.at(i), QStringLiteral("5:00 PM"), QStringLiteral("5:50 PM")}};
            if (i == 0) info.intensiveTimes = {{QStringLiteral("Saturday"), QStringLiteral("10:00 AM"), QStringLiteral("10:50 AM")}};
            if (!data.saveClassInfo(info)) return false;
            Roster roster; roster.columns = Roster::BaseColumns;
            roster.columns.append({QStringLiteral("Notes"), QStringLiteral("Allergies"), QStringLiteral("Last")});
            roster.columnWidths = {71, 82, 93, 104, 115, 126, 137, 148, 159};
            roster.rows = {{QStringLiteral("Student %1").arg(i + 1), QStringLiteral("\uAE40\uBBFC\uC218"), {}, {}, {}, {},
                QStringLiteral("Roster note\nline"), QStringLiteral("None"), QStringLiteral("Last value")}};
            if (!data.saveRoster(*created, roster)) return false;
        }
        if (!sql(QStringLiteral("INSERT INTO roster_data(class_id,row_index,col_index,value) VALUES(%1,28,8,'Sparse last')").arg(ids.first()))) return false;
        for (const auto& name : {QStringLiteral("Zulu"), QStringLiteral("Alpha")})
        {
            QSqlQuery insert(database()); insert.prepare(QStringLiteral("INSERT INTO speaking_evaluations(class_id,evaluation_name) VALUES(?,?)"));
            insert.addBindValue(ids.first()); insert.addBindValue(name); if (!insert.exec()) return false;
            const int evaluation = insert.lastInsertId().toInt();
            if (!sql(QStringLiteral("INSERT INTO speaking_eval_data(evaluation_id,row_index,col_0,col_1,col_10) VALUES(%1,24,'25','Last evaluation student','Tail note')").arg(evaluation))) return false;
            if (!sql(QStringLiteral("INSERT INTO speaking_eval_data(evaluation_id,row_index,col_0,col_1,col_9) VALUES(%1,0,'1','First evaluation student','Full comment')").arg(evaluation))) return false;
        }
        return true;
    }
};
Result<QJsonObject> exportJson(ApplicationServices& services, const QList<int>& ids, const QString& path = {})
{
#ifdef CLASSMNGR_TYPED_EXPORT
    using namespace ClassMngr::Next;
    Application::ClassTransferExportRequest request;
    for (const int id : ids) request.classIds.push_back(*Domain::ClassId::fromString(std::to_string(id)));
    Platform::ApplicationServicesClassTransferExportSourceReadPort port(services);
    auto package = Application::ClassTransferExportQuery(port).execute(request);
    if (!package) return std::unexpected(QString::fromUtf8(package.error().message.data(), static_cast<qsizetype>(package.error().message.size())));
    if (!path.isEmpty()) { auto saved = ClassTransferJsonCodec::saveFile(path, package.value()); if (!saved) return std::unexpected(saved.error()); }
    return ClassTransferJsonCodec::toJson(package.value());
#else
    auto package = services.dataService()->databaseSession()->classTransferRepository()->buildPackage(ids);
    if (!package) return std::unexpected(package.error());
    if (!path.isEmpty()) { auto saved = ClassTransferJsonCodec::saveFile(path, *package); if (!saved) return std::unexpected(saved.error()); }
    return ClassTransferJsonCodec::toJson(*package);
#endif
}
QJsonObject normalized(QJsonObject object)
{
    object[QStringLiteral("exported_at_utc")] = QStringLiteral("<UTC timestamp>");
    return object;
}
void logTranscript(const QJsonObject& value)
{
    const QString json = QString::fromUtf8(QJsonDocument(value).toJson(QJsonDocument::Compact));
    QString ascii;
    for (const QChar c : json)
        if (c.unicode() > 127) ascii += QStringLiteral("\\u%1").arg(static_cast<unsigned int>(c.unicode()), 4, 16, QLatin1Char('0'));
        else ascii += c;
    qInfo().noquote() << "F354_TRANSCRIPT" << ascii;
}
}
class ClassTransferExportCodecParityTests : public QObject
{
    Q_OBJECT
private slots:
    void completeExportPreservesSchemaOrderAndEveryPayload();
    void failurePrecedence_data();
    void failurePrecedence();
    void fileErrorsRetainTheirMessages();
};
void ClassTransferExportCodecParityTests::completeExportPreservesSchemaOrderAndEveryPayload()
{
    Fixture fixture; QVERIFY(fixture.open());
    const QList<int> order{fixture.ids[1], fixture.ids[0], fixture.ids[2], fixture.ids[3]};
    const QString path = fixture.directory.filePath(QStringLiteral("export.json"));
    auto actual = exportJson(fixture.services, order, path);
    QVERIFY2(actual, actual ? "" : qPrintable(actual.error()));
    auto legacy = fixture.services.dataService()->databaseSession()->classTransferRepository()->buildPackage(order);
    QVERIFY(legacy); QCOMPARE(normalized(*actual), normalized(ClassTransferJsonCodec::toJson(*legacy)));
    QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(normalized(QJsonDocument::fromJson(file.readAll()).object()), normalized(*actual));
    QCOMPARE(actual->keys(), (QStringList{QStringLiteral("classes"), QStringLiteral("exported_at_utc"), QStringLiteral("format"), QStringLiteral("teachers"), QStringLiteral("version")}));
    const auto date = QDateTime::fromString(actual->value(QStringLiteral("exported_at_utc")).toString(), Qt::ISODateWithMs);
    QVERIFY(date.isValid()); QCOMPARE(date.timeSpec(), Qt::UTC); QCOMPARE(actual->value(QStringLiteral("version")).toInt(), 1);
    const auto teachers = actual->value(QStringLiteral("teachers")).toArray(); QCOMPARE(teachers.size(), 2);
    QCOMPARE(teachers[0].toObject().value(QStringLiteral("key")).toString(), QStringLiteral("teacher-1"));
    QCOMPARE(teachers[0].toObject().value(QStringLiteral("teacher_en")).toString(), QStringLiteral("Bob"));
    QCOMPARE(teachers[1].toObject().value(QStringLiteral("teacher_en")).toString(), QStringLiteral("Alice"));
    QCOMPARE(teachers[0].toObject().size(), 15);
    const auto classes = actual->value(QStringLiteral("classes")).toArray(); QCOMPARE(classes.size(), 4);
    for (int i = 0; i < classes.size(); ++i)
    {
        const auto item = classes[i].toObject();
        QCOMPARE(item.value(QStringLiteral("key")).toString(), QStringLiteral("class-%1").arg(i + 1));
        QCOMPARE(item.value(QStringLiteral("name")).toString(), QStringLiteral("Stored class %1").arg(order[i]));
        QCOMPARE(item.value(QStringLiteral("teacher_ref")).toString(), i == 0 ? QStringLiteral("teacher-1") : i == 3 ? QString() : QStringLiteral("teacher-2"));
        const auto info = item.value(QStringLiteral("info")).toObject(); QCOMPARE(info.size(), 10);
        QVERIFY(!info.contains(QStringLiteral("class_id"))); QVERIFY(!info.contains(QStringLiteral("teacher_id")));
        QVERIFY(!info.contains(QStringLiteral("wifi_password"))); QVERIFY(!info.contains(QStringLiteral("teacher_en")));
        QCOMPARE(info.value(QStringLiteral("regular_times")).toArray().size(), 2);
    }
    const auto first = classes[1].toObject();
    const auto roster = first.value(QStringLiteral("roster")).toObject();
    QCOMPARE(roster.value(QStringLiteral("columns")).toArray().size(), 9);
    QCOMPARE(roster.value(QStringLiteral("column_widths")).toArray().last().toInt(), 159);
    QCOMPARE(roster.value(QStringLiteral("rows")).toArray().size(), 29);
    QCOMPARE(roster.value(QStringLiteral("rows")).toArray()[28].toArray()[8].toString(), QStringLiteral("Sparse last"));
    const auto evaluations = first.value(QStringLiteral("speaking_evaluations")).toArray(); QCOMPARE(evaluations.size(), 2);
    QCOMPARE(evaluations[0].toObject().value(QStringLiteral("name")).toString(), QStringLiteral("Zulu"));
    QCOMPARE(evaluations[1].toObject().value(QStringLiteral("name")).toString(), QStringLiteral("Alpha"));
    const auto rows = evaluations[0].toObject().value(QStringLiteral("rows")).toArray(); QCOMPARE(rows.size(), 25);
    QCOMPARE(rows[1].toArray().size(), 11); QCOMPARE(rows[1].toArray()[1].toString(), QString());
    QCOMPARE(rows[24].toArray()[10].toString(), QStringLiteral("Tail note"));
    logTranscript({{QStringLiteral("case"), QStringLiteral("complete-export")}, {QStringLiteral("package"), normalized(*actual)}});
}
void ClassTransferExportCodecParityTests::failurePrecedence_data()
{
    QTest::addColumn<int>("scenario");
    QTest::newRow("negative-id") << 0; QTest::newRow("empty") << 1;
    QTest::newRow("duplicate") << 2; QTest::newRow("missing-class") << 3;
    QTest::newRow("teacher-before-duplicate") << 4; QTest::newRow("evaluation-before-duplicate") << 5;
    QTest::newRow("info-before-duplicate") << 6; QTest::newRow("roster-before-teacher") << 7;
}
void ClassTransferExportCodecParityTests::failurePrecedence()
{
    QFETCH(int, scenario); Fixture fixture; QVERIFY(fixture.open());
    QList<int> ids{fixture.ids.first(), fixture.ids.first()};
    if (scenario == 0) ids = {-1};
    if (scenario == 1) ids = {};
    if (scenario == 3) ids = {fixture.ids.first(), 99999};
    if (scenario == 4 || scenario == 7)
    {
        QVERIFY(fixture.sql(QStringLiteral("PRAGMA foreign_keys=OFF")));
        QVERIFY(fixture.sql(QStringLiteral("UPDATE class_info SET teacher_id=99999 WHERE class_id=%1").arg(fixture.ids.first())));
    }
    if (scenario == 5) QVERIFY(fixture.sql(QStringLiteral("DROP TABLE speaking_eval_data")));
    if (scenario == 6) QVERIFY(fixture.sql(QStringLiteral("DROP TABLE class_times")));
    if (scenario == 7) QVERIFY(fixture.sql(QStringLiteral("DROP TABLE roster_data")));
    auto legacy = fixture.services.dataService()->databaseSession()->classTransferRepository()->buildPackage(ids);
    auto actual = exportJson(fixture.services, ids);
    QVERIFY(!legacy); QVERIFY(!actual); QCOMPARE(actual.error(), legacy.error());
    if (scenario == 4) QVERIFY(!actual.error().contains(QStringLiteral("selection")));
    if (scenario == 5) QVERIFY(actual.error().contains(QStringLiteral("speaking evaluation rows")));
    if (scenario == 6) QVERIFY(!actual.error().contains(QStringLiteral("selection")));
    if (scenario == 7) QVERIFY(actual.error().contains(QStringLiteral("roster"), Qt::CaseInsensitive));
    auto database = fixture.database(); QVERIFY(database.transaction()); QVERIFY(database.rollback());
    logTranscript({{QStringLiteral("case"), QString::fromLatin1(QTest::currentDataTag())}, {QStringLiteral("error"), actual.error()}});
}
void ClassTransferExportCodecParityTests::fileErrorsRetainTheirMessages()
{
    Fixture fixture; QVERIFY(fixture.open());
    auto result = exportJson(fixture.services, fixture.ids, fixture.directory.path());
    QVERIFY(!result); QVERIFY(result.error().startsWith(QStringLiteral("Unable to open the class package for writing:\n")));
}
QTEST_MAIN(ClassTransferExportCodecParityTests)
#include "class_transfer_export_codec_parity_tests.moc"
