#include "next/application/class_transfer_export_query.h"
#include <QtTest/QtTest>
using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
namespace
{
Domain::ClassId id(int value) { return *Domain::ClassId::fromString(std::to_string(value)); }
Domain::TeacherId teacherId(int value) { return *Domain::TeacherId::fromString(std::to_string(value)); }
Domain::OperationError error(const char* message) { return {Domain::ErrorCode::Technical, message, true}; }
ClassExportSourceClass item(int value, std::optional<Domain::TeacherId> teacher = {})
{
    ClassExportSourceClass result{id(value), static_cast<std::size_t>(value - 1), u"Class", {}, {}, {}, {}};
    ClassExportSourceInfo info;
    info.classGrade = u"E4"; info.classLevel = u"Theseus";
    info.sourceClassId = value; info.sourceTeacherId = 99;
    info.teacherEn = u"private teacher metadata"; info.wifiPassword = u"private password";
    result.info = ClassExportStage<ClassExportSourceInfo>::value(info);
    result.roster = ClassExportStage<ClassExportRoster>::value({{u"English", u"Korean", u"Notes"}, {70, 80, 90}, {{u"Alex", u"\uAE40\uBBFC\uC218", u"full cell"}}});
    result.teacher = ClassExportStage<std::optional<Domain::TeacherId>>::value(teacher);
    result.evaluations = ClassExportStage<std::vector<ClassExportEvaluation>>::value({{u"Second", {{u"sparse", u"value"}}}, {u"First", {}}});
    return result;
}
struct Port : ClassTransferExportSourceReadPort
{
    ClassTransferExportSource source;
    std::optional<Domain::OperationError> failure;
    ClassTransferExportRequest seen;
    int calls = 0;
    ClassTransferExportSourceResult readSource(const ClassTransferExportRequest& request) override
    {
        ++calls; seen = request;
        return failure ? ClassTransferExportSourceResult::failure(*failure) : ClassTransferExportSourceResult::success(source);
    }
};
}
class ClassTransferExportQueryTests : public QObject
{
    Q_OBJECT
private slots:
    void preservesRequestAndAssemblesFirstSeenKeysAndFullPayload();
    void sourceFailuresPropagate();
    void errorsReplayInOriginalClassOrder_data();
    void errorsReplayInOriginalClassOrder();
    void stagedFailurePrecedesDeferredSelectionAndUnattemptedValues();
    void emptyAndDuplicateRequestsRemainSourceOwned();
    void noImportProjectionCapsApply();
};
void ClassTransferExportQueryTests::preservesRequestAndAssemblesFirstSeenKeysAndFullPayload()
{
    Port port;
    port.source.exportedAtUtcMilliseconds = 1'234;
    port.source.classes = {item(2, teacherId(9)), item(1, teacherId(3)), item(4, teacherId(9)), item(3)};
    for (std::size_t i = 0; i < port.source.classes.size(); ++i) port.source.classes[i].selectedIndex = i;
    ClassExportTeacherProfile first; first.teacherEn = u"First"; first.notes = u"all profile fields stay owned";
    ClassExportTeacherProfile second; second.teacherEn = u"Second";
    port.source.teachers = {{teacherId(3), ClassExportStage<ClassExportTeacherProfile>::value(second)},
        {teacherId(9), ClassExportStage<ClassExportTeacherProfile>::value(first)}};
    const ClassTransferExportRequest request{{id(2), id(1), id(4), id(3)}};
    auto result = ClassTransferExportQuery(port).execute(request);
    QVERIFY(result); QCOMPARE(port.calls, 1); QVERIFY(port.seen.classIds == request.classIds);
    const auto& output = result.value();
    QCOMPARE(output.version, 1); QVERIFY(output.exportedAtUtc == "1970-01-01T00:00:01.234Z");
    QCOMPARE(output.teachers.size(), std::size_t(2));
    QVERIFY(output.teachers[0].key == u"teacher-1" && output.teachers[0].teacher.teacherEn == u"First");
    QVERIFY(output.teachers[1].key == u"teacher-2" && output.teachers[1].teacher.teacherEn == u"Second");
    QVERIFY(output.classes[0].teacherKey == u"teacher-1" && output.classes[1].teacherKey == u"teacher-2");
    QVERIFY(output.classes[2].teacherKey == u"teacher-1" && output.classes[3].teacherKey.empty());
    QVERIFY(output.classes[3].key == u"class-4");
    QVERIFY(output.classes[0].roster.columnWidths == std::vector<int>({70, 80, 90}));
    QVERIFY(output.classes[0].roster.rows[0][2] == u"full cell");
    QVERIFY(output.classes[0].evaluations[0].name == u"Second" && output.classes[0].evaluations[1].name == u"First");
    port.source.classes[0].name = u"mutated";
    QVERIFY(output.classes[0].name == u"Class");
}
void ClassTransferExportQueryTests::sourceFailuresPropagate()
{
    Port port; port.failure = error("transaction failure");
    auto result = ClassTransferExportQuery(port).execute({{id(1)}});
    QVERIFY(!result); QVERIFY(result.error() == *port.failure); QCOMPARE(port.calls, 1);
}
void ClassTransferExportQueryTests::errorsReplayInOriginalClassOrder_data()
{
    QTest::addColumn<int>("stage");
    QTest::newRow("info") << 0; QTest::newRow("roster") << 1;
    QTest::newRow("teacher") << 2; QTest::newRow("evaluation") << 3;
}
void ClassTransferExportQueryTests::errorsReplayInOriginalClassOrder()
{
    QFETCH(int, stage);
    Port port; port.source.classes = {item(1, teacherId(1)), item(2)};
    port.source.teachers = {{teacherId(1), ClassExportStage<ClassExportTeacherProfile>::value({})}};
    port.source.classes[1].info = ClassExportStage<ClassExportSourceInfo>::error(error("later-info"));
    if (stage == 0)
    {
        port.source.classes[0].info = ClassExportStage<ClassExportSourceInfo>::error(error("first"));
        port.source.classes[0].roster = {}; port.source.classes[0].teacher = {}; port.source.classes[0].evaluations = {};
    }
    if (stage == 1)
    {
        port.source.classes[0].roster = ClassExportStage<ClassExportRoster>::error(error("first"));
        port.source.classes[0].teacher = {}; port.source.classes[0].evaluations = {};
    }
    if (stage == 2) port.source.teachers[0].profile = ClassExportStage<ClassExportTeacherProfile>::error(error("first"));
    if (stage == 3) port.source.classes[0].evaluations = ClassExportStage<std::vector<ClassExportEvaluation>>::error(error("first"));
    const auto result = ClassTransferExportQuery(port).execute({{id(1), id(2)}});
    QVERIFY(!result); QVERIFY(result.error().message == "first");
}
void ClassTransferExportQueryTests::stagedFailurePrecedesDeferredSelectionAndUnattemptedValues()
{
    Port port; port.source.classes = {item(1, teacherId(1)), {id(2), 1, u"not attempted", {}, {}, {}, {}}};
    port.source.teachers = {{teacherId(1), ClassExportStage<ClassExportTeacherProfile>::error(error("teacher"))}};
    port.source.classes[0].evaluations = ClassExportStage<std::vector<ClassExportEvaluation>>::error(error("evaluation"));
    port.source.deferredSelectionOrLookupError = error("selection");
    auto result = ClassTransferExportQuery(port).execute({{id(1), id(1)}});
    QVERIFY(!result); QVERIFY(result.error().message == "teacher");
    port.source.teacherBatchError = error("batch");
    result = ClassTransferExportQuery(port).execute({{id(1), id(1)}});
    QVERIFY(!result); QVERIFY(result.error().message == "batch");
    port.source = {}; port.source.deferredSelectionOrLookupError = error("selection");
    result = ClassTransferExportQuery(port).execute({});
    QVERIFY(!result); QVERIFY(result.error().message == "selection");
}
void ClassTransferExportQueryTests::emptyAndDuplicateRequestsRemainSourceOwned()
{
    Port port; port.failure = error("source selection error");
    ClassTransferExportQuery query(port);
    QVERIFY(!query.execute({})); QVERIFY(port.seen.classIds.empty());
    const ClassTransferExportRequest request{{id(2), id(2), id(0)}};
    QVERIFY(!query.execute(request)); QVERIFY(port.seen.classIds == request.classIds); QCOMPARE(port.calls, 2);
}
void ClassTransferExportQueryTests::noImportProjectionCapsApply()
{
    Port port;
    ClassTransferExportRequest request;
    for (int i = 1; i <= 4'097; ++i) { request.classIds.push_back(id(i)); port.source.classes.push_back(item(i)); }
    auto result = ClassTransferExportQuery(port).execute(request);
    QVERIFY(result); QCOMPARE(result.value().classes.size(), std::size_t(4'097));
}
QTEST_APPLESS_MAIN(ClassTransferExportQueryTests)
#include "next_application_class_transfer_export_query_tests.moc"
