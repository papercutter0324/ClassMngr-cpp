#include "next/application/class_transfer_export_query.h"
#include "next/application/class_transfer_keys.h"
#include <QtTest/QtTest>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>
using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
static_assert(std::is_same_v<decltype(std::declval<ClassExportTeacher>().key), ClassExportTeacherKey>);
static_assert(std::is_same_v<decltype(std::declval<ClassExportClass>().key), ClassExportClassKey>);
static_assert(std::is_same_v<decltype(std::declval<ClassExportClass>().teacherKey), ClassExportTeacherKey>);
static_assert(!std::is_same_v<ClassExportTeacherKey, ClassExportText>);
static_assert(!std::is_same_v<ClassExportClassKey, ClassExportText>);
static_assert(!std::is_same_v<ClassExportTeacherKey, ClassTransferTeacherKey>);
static_assert(!std::is_same_v<ClassExportClassKey, ClassTransferClassKey>);
static_assert(!std::is_convertible_v<ClassExportTeacherKey, ClassTransferTeacherKey>);
static_assert(!std::is_convertible_v<ClassExportClassKey, ClassTransferClassKey>);
static_assert(std::is_same_v<
    decltype(std::declval<ClassTransferPortablePackage>().exportedAtUtc),
    ClassTransferExportedAtUtc>);
static_assert(std::is_same_v<
    decltype(std::declval<ClassTransferPortablePackage>().version),
    ClassTransferPackageVersion>);
static_assert(std::is_same_v<
    decltype(std::declval<ClassTransferPackageVersion>().value()),
    int>);
static_assert(std::is_same_v<
    decltype(std::declval<ClassExportRoster>().columnWidths),
    std::vector<ClassExportRosterColumnWidth>>);
static_assert(std::is_same_v<
    decltype(std::declval<ClassExportRosterColumnWidth>().rawValue()),
    int>);
static_assert(!std::is_convertible_v<int, ClassExportRosterColumnWidth>);
static_assert(!std::is_convertible_v<ClassExportRosterColumnWidth, int>);
static_assert(!std::is_convertible_v<int, ClassTransferPackageVersion>);
static_assert(!std::is_convertible_v<ClassTransferPackageVersion, int>);
static_assert(std::is_same_v<
    decltype(std::declval<ClassTransferExportedAtUtc>().value()),
    const std::string&>);
static_assert(std::is_same_v<
    decltype(std::declval<ClassTransferExportUtcMilliseconds>().value()),
    std::int64_t>);
static_assert(!std::is_convertible_v<std::int64_t, ClassTransferExportUtcMilliseconds>);
static_assert(!std::is_convertible_v<ClassTransferExportUtcMilliseconds, std::int64_t>);
static_assert(std::is_same_v<
    decltype(ClassExportSourceClass::selectedIndex),
    ClassTransferExportSelectionOrdinal
    >);
static_assert(std::is_same_v<
    decltype(ClassTransferExportSelectionOrdinal{}.rawValue()),
    std::size_t
    >);
static_assert(!std::is_convertible_v<
    std::size_t,
    ClassTransferExportSelectionOrdinal
    >);
static_assert(!std::is_convertible_v<
    ClassTransferExportSelectionOrdinal,
    std::size_t
    >);
constexpr ClassTransferExportSelectionOrdinal DefaultSelectionOrdinal{};
constexpr ClassTransferExportSelectionOrdinal ExplicitSelectionOrdinal{4};
constexpr ClassTransferExportSelectionOrdinal EqualSelectionOrdinal{4};
constexpr ClassTransferExportSelectionOrdinal DifferentSelectionOrdinal{19};
static_assert(DefaultSelectionOrdinal.rawValue() == 0);
static_assert(ExplicitSelectionOrdinal.rawValue() == 4);
static_assert(ExplicitSelectionOrdinal == EqualSelectionOrdinal);
static_assert(ExplicitSelectionOrdinal != DifferentSelectionOrdinal);
constexpr ClassExportRosterColumnWidth DefaultRosterColumnWidth{};
constexpr ClassExportRosterColumnWidth MinimumRosterColumnWidth{std::numeric_limits<int>::min()};
constexpr ClassExportRosterColumnWidth MaximumRosterColumnWidth{std::numeric_limits<int>::max()};
static_assert(DefaultRosterColumnWidth.rawValue() == 0);
static_assert(MinimumRosterColumnWidth.rawValue() == std::numeric_limits<int>::min());
static_assert(MaximumRosterColumnWidth.rawValue() == std::numeric_limits<int>::max());
static_assert(MinimumRosterColumnWidth == ClassExportRosterColumnWidth{std::numeric_limits<int>::min()});
static_assert(MinimumRosterColumnWidth != MaximumRosterColumnWidth);
namespace
{
Domain::ClassId id(int value) { return *Domain::ClassId::fromString(std::to_string(value)); }
Domain::TeacherId teacherId(int value) { return *Domain::TeacherId::fromString(std::to_string(value)); }
Domain::OperationError error(const char* message) { return {Domain::ErrorCode::Technical, message, true}; }
ClassExportSourceClass item(int value, std::optional<Domain::TeacherId> teacher = {})
{
    ClassExportSourceClass result{
        id(value),
        ClassTransferExportSelectionOrdinal{
            static_cast<std::size_t>(value - 1)
            },
        u"Class", {}, {}, {}, {}};
    ClassExportSourceInfo info;
    info.classGrade = u"E4"; info.classLevel = u"Theseus";
    info.sourceClassId = value; info.sourceTeacherId = 99;
    info.teacherEn = u"private teacher metadata"; info.wifiPassword = u"private password";
    result.info = ClassExportStage<ClassExportSourceInfo>::value(info);
    result.roster = ClassExportStage<ClassExportRoster>::value({
        {u"English", u"Korean", u"Notes"},
        {ClassExportRosterColumnWidth{70}, ClassExportRosterColumnWidth{80}, ClassExportRosterColumnWidth{90}},
        {{u"Alex", u"\uAE40\uBBFC\uC218", u"full cell"}}});
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
    void rosterWidthsKeepSignedValuesAndOrder();
    void packageVersionIsAnExactSignedIntValue();
    void utcMillisecondsValuePreservesFullSignedRangeAndIsOpaque();
    void formatsUnixEpochAndNegativeMillisecondsInUtc();
    void sourceFailuresPropagate();
    void errorsReplayInOriginalClassOrder_data();
    void errorsReplayInOriginalClassOrder();
    void stagedFailurePrecedesDeferredSelectionAndUnattemptedValues();
    void incompleteClassStagesAreTechnicalFailures_data();
    void incompleteClassStagesAreTechnicalFailures();
    void missingReferencedTeacherIsTechnicalFailure();
    void unattemptedReferencedTeacherProfileIsTechnicalFailure();
    void emptyAndDuplicateRequestsRemainSourceOwned();
    void noImportProjectionCapsApply();
    void outputClassKeysUseExplicitSelectionOrdinals();
};
void ClassTransferExportQueryTests::preservesRequestAndAssemblesFirstSeenKeysAndFullPayload()
{
    Port port;
    port.source.exportedAtUtcMilliseconds = ClassTransferExportUtcMilliseconds{1'234};
    port.source.classes = {item(2, teacherId(9)), item(1, teacherId(3)), item(4, teacherId(9)), item(3)};
    for (std::size_t i = 0; i < port.source.classes.size(); ++i)
        port.source.classes[i].selectedIndex = ClassTransferExportSelectionOrdinal{i};
    const std::u16string exactPayload{u'A', char16_t(0), char16_t(0xD800), u'B',
        char16_t(0xDC00), char16_t(0xD83D), char16_t(0xDE80), u'\n'};
    port.source.classes[0].name = exactPayload;
    ClassExportTeacherProfile first; first.teacherEn = u"First"; first.notes = u"all profile fields stay owned";
    ClassExportTeacherProfile second; second.teacherEn = u"Second";
    port.source.teachers = {{teacherId(3), ClassExportStage<ClassExportTeacherProfile>::value(second)},
        {teacherId(9), ClassExportStage<ClassExportTeacherProfile>::value(first)}};
    const ClassTransferExportRequest request{{id(2), id(1), id(4), id(3)}};
    auto result = ClassTransferExportQuery(port).execute(request);
    QVERIFY(result); QCOMPARE(port.calls, 1); QVERIFY(port.seen.classIds == request.classIds);
    const auto& output = result.value();
    QCOMPARE(output.version.value(), 1); QVERIFY(output.exportedAtUtc.value() == "1970-01-01T00:00:01.234Z");
    QCOMPARE(output.teachers.size(), std::size_t(2));
    QVERIFY(output.teachers[0].key.value() == u"teacher-1" && output.teachers[0].teacher.teacherEn == u"First");
    QVERIFY(output.teachers[1].key.value() == u"teacher-2" && output.teachers[1].teacher.teacherEn == u"Second");
    QVERIFY(output.classes[0].teacherKey.value() == output.teachers[0].key.value());
    QVERIFY(output.classes[1].teacherKey.value() == output.teachers[1].key.value());
    QVERIFY(output.classes[2].teacherKey.value() == output.teachers[0].key.value());
    QVERIFY(output.classes[3].teacherKey.value().empty());
    for (std::size_t i = 0; i < output.classes.size(); ++i)
        QVERIFY(output.classes[i].key.value() == (u"class-" + std::u16string(1, char16_t(u'1' + i))));
    QVERIFY(output.classes[0].name == exactPayload);
    QVERIFY(output.classes[0].roster.columnWidths == (std::vector<ClassExportRosterColumnWidth>{
        ClassExportRosterColumnWidth{70}, ClassExportRosterColumnWidth{80}, ClassExportRosterColumnWidth{90}}));
    QVERIFY(output.classes[0].roster.rows[0][2] == u"full cell");
    QVERIFY(output.classes[0].evaluations[0].name == u"Second" && output.classes[0].evaluations[1].name == u"First");
    port.source.classes[0].name = u"mutated";
    QVERIFY(output.classes[0].name == exactPayload);
}
void ClassTransferExportQueryTests::rosterWidthsKeepSignedValuesAndOrder()
{
    Port port;
    port.source.classes = {item(1)};
    ClassExportRoster roster;
    roster.columns = {u"first", u"second", u"third", u"fourth"};
    const std::vector<int> expected{
        std::numeric_limits<int>::min(), -17, 0, std::numeric_limits<int>::max()};
    for (const int width : expected)
        roster.columnWidths.push_back(ClassExportRosterColumnWidth{width});
    port.source.classes.front().roster = ClassExportStage<ClassExportRoster>::value(std::move(roster));

    const auto result = ClassTransferExportQuery(port).execute({{id(1)}});
    QVERIFY(result);
    const auto& widths = result.value().classes.front().roster.columnWidths;
    QCOMPARE(widths.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
        QCOMPARE(widths[index].rawValue(), expected[index]);
}
void ClassTransferExportQueryTests::packageVersionIsAnExactSignedIntValue()
{
    const ClassTransferPortablePackage package;
    const ClassTransferPackageVersion minimum{std::numeric_limits<int>::min()};
    const ClassTransferPackageVersion maximum{std::numeric_limits<int>::max()};
    QCOMPARE(package.version.value(), 1);
    QCOMPARE(minimum.value(), std::numeric_limits<int>::min());
    QCOMPARE(maximum.value(), std::numeric_limits<int>::max());
    QVERIFY(minimum == ClassTransferPackageVersion{std::numeric_limits<int>::min()});
    QVERIFY(maximum == ClassTransferPackageVersion{std::numeric_limits<int>::max()});
    QVERIFY(minimum != maximum);
}
void ClassTransferExportQueryTests::utcMillisecondsValuePreservesFullSignedRangeAndIsOpaque()
{
    const ClassTransferExportUtcMilliseconds zero;
    const ClassTransferExportUtcMilliseconds minimum{
        std::numeric_limits<std::int64_t>::min()};
    const ClassTransferExportUtcMilliseconds maximum{
        std::numeric_limits<std::int64_t>::max()};
    QVERIFY(zero.value() == 0);
    QVERIFY(minimum.value() == std::numeric_limits<std::int64_t>::min());
    QVERIFY(maximum.value() == std::numeric_limits<std::int64_t>::max());
    QVERIFY(zero == ClassTransferExportUtcMilliseconds{0});
    QVERIFY(minimum == ClassTransferExportUtcMilliseconds{
        std::numeric_limits<std::int64_t>::min()});
    QVERIFY(maximum == ClassTransferExportUtcMilliseconds{
        std::numeric_limits<std::int64_t>::max()});
    QVERIFY(minimum != maximum);
}
void ClassTransferExportQueryTests::formatsUnixEpochAndNegativeMillisecondsInUtc()
{
    const auto exportedAt = [](const std::int64_t milliseconds) {
        Port port;
        port.source.exportedAtUtcMilliseconds = ClassTransferExportUtcMilliseconds{milliseconds};
        return ClassTransferExportQuery(port).execute({});
    };

    const auto epoch = exportedAt(0);
    QVERIFY(epoch);
    QVERIFY(epoch.value().exportedAtUtc.value() == "1970-01-01T00:00:00.000Z");

    const auto positive = exportedAt(1'234);
    QVERIFY(positive);
    QVERIFY(positive.value().exportedAtUtc.value() == "1970-01-01T00:00:01.234Z");

    const auto negative = exportedAt(-1);
    QVERIFY(negative);
    QVERIFY(negative.value().exportedAtUtc.value() == "1969-12-31T23:59:59.999Z");
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
    Port port; port.source.classes = {
        item(1, teacherId(1)),
        {
            id(2),
            ClassTransferExportSelectionOrdinal{1},
            u"not attempted",
            {}, {}, {}, {}
        }
    };
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

    port.source.classes = {item(1)};
    port.source.classes.front().roster = {};
    result = ClassTransferExportQuery(port).execute({{id(1)}});
    QVERIFY(!result);
    QVERIFY(result.error().message == "selection");
}
void ClassTransferExportQueryTests::incompleteClassStagesAreTechnicalFailures_data()
{
    QTest::addColumn<int>("stage");
    QTest::newRow("class-info") << 0;
    QTest::newRow("class-roster") << 1;
    QTest::newRow("class-teacher") << 2;
    QTest::newRow("class-evaluations") << 3;
}
void ClassTransferExportQueryTests::incompleteClassStagesAreTechnicalFailures()
{
    QFETCH(int, stage);

    Port port;
    port.source.classes = {item(1)};
    auto& sourceClass = port.source.classes.front();
    switch (stage)
    {
    case 0:
        sourceClass.info = {};
        break;
    case 1:
        sourceClass.roster = {};
        break;
    case 2:
        sourceClass.teacher = {};
        break;
    case 3:
        sourceClass.evaluations = {};
        break;
    default:
        QFAIL("Unexpected class stage data row");
    }

    const auto result = ClassTransferExportQuery(port).execute({{id(1)}});
    QVERIFY(!result);
    QVERIFY(result.error().code == Domain::ErrorCode::Technical);
    QVERIFY(!result.error().recoverable);
}
void ClassTransferExportQueryTests::missingReferencedTeacherIsTechnicalFailure()
{
    Port port;
    port.source.classes = {item(1, teacherId(7))};

    const auto result = ClassTransferExportQuery(port).execute({{id(1)}});
    QVERIFY(!result);
    QVERIFY(result.error().code == Domain::ErrorCode::Technical);
    QVERIFY(!result.error().recoverable);
}
void ClassTransferExportQueryTests::
unattemptedReferencedTeacherProfileIsTechnicalFailure()
{
    Port port;
    port.source.classes = {item(1, teacherId(7))};
    port.source.teachers = {{teacherId(7), {}}};

    const auto result = ClassTransferExportQuery(port).execute({{id(1)}});
    QVERIFY(!result);
    QVERIFY(result.error().code == Domain::ErrorCode::Technical);
    QVERIFY(!result.error().recoverable);
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
void ClassTransferExportQueryTests::outputClassKeysUseExplicitSelectionOrdinals()
{
    Port port;
    port.source.classes = {item(9), item(3), item(7)};
    port.source.classes[0].selectedIndex = ClassTransferExportSelectionOrdinal{6};
    port.source.classes[1].selectedIndex = ClassTransferExportSelectionOrdinal{0};
    port.source.classes[2].selectedIndex = ClassTransferExportSelectionOrdinal{3};
    const ClassTransferExportRequest request{{id(9), id(3), id(7)}};

    const auto result = ClassTransferExportQuery(port).execute(request);
    QVERIFY(result);
    QVERIFY(port.seen.classIds == request.classIds);
    QCOMPARE(result.value().classes.size(), std::size_t(3));
    QCOMPARE(result.value().classes[0].key.value(), std::u16string(u"class-7"));
    QCOMPARE(result.value().classes[1].key.value(), std::u16string(u"class-1"));
    QCOMPARE(result.value().classes[2].key.value(), std::u16string(u"class-4"));
}
QTEST_APPLESS_MAIN(ClassTransferExportQueryTests)
#include "next_application_class_transfer_export_query_tests.moc"
