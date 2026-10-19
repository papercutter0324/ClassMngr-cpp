#include "data/repositories/class_transfer_legacy_package_adapter.h"
#include "features/classes/services/class_transfer_json_codec.h"
#include "next/application/class_transfer_apply.h"

#include <QJsonArray>
#include <QtTest/QtTest>

#include <type_traits>
#include <utility>

namespace Application = ClassMngr::Next::Application;
namespace Legacy = ClassMngr::Data::Legacy;
static_assert(std::is_same_v<decltype(Application::ClassTransferApplyCommand::package),
    Application::ClassTransferPortablePackage>);
static_assert(std::is_same_v<decltype(std::declval<Application::ClassExportTeacher>().key),
    Application::ClassExportTeacherKey>);
static_assert(std::is_same_v<decltype(std::declval<Application::ClassExportClass>().key),
    Application::ClassExportClassKey>);
static_assert(std::is_same_v<decltype(std::declval<Application::ClassExportClass>().teacherKey),
    Application::ClassExportTeacherKey>);
static_assert(!std::is_same_v<Application::ClassExportTeacherKey, Application::ClassExportText>);
static_assert(!std::is_same_v<Application::ClassExportClassKey, Application::ClassExportText>);
static_assert(!std::is_same_v<Application::ClassExportTeacherKey, Application::ClassTransferTeacherKey>);
static_assert(!std::is_same_v<Application::ClassExportClassKey, Application::ClassTransferClassKey>);
static_assert(std::is_same_v<decltype(std::declval<Application::ClassTransferTeacherKey>().value()),
    const std::string&>);
static_assert(std::is_same_v<decltype(std::declval<Application::ClassTransferClassKey>().value()),
    const std::string&>);

namespace
{
QString marker(const QString& field, const int item)
{
    return QStringLiteral("%1/%2 \uD55C\uAE00 \U0001F393\nsecond line").arg(item).arg(field);
}
QJsonArray times(const int item, const QString& kind)
{
    QJsonArray result;
    for (const QString& suffix : {QStringLiteral("second"), QStringLiteral("first")})
        result.append(QJsonObject{
            {QStringLiteral("day"), marker(kind + suffix + QStringLiteral("day"), item)},
            {QStringLiteral("start_time"), marker(kind + suffix + QStringLiteral("start"), item)},
            {QStringLiteral("end_time"), marker(kind + suffix + QStringLiteral("end"), item)}});
    return result;
}
QJsonObject packageJson()
{
    QJsonArray teachers, classes;
    for (const int item : {2, 1})
    {
        QJsonObject teacher{{QStringLiteral("key"), QStringLiteral("teacher-%1").arg(item)}};
        teacher.insert(QStringLiteral("teacher_kr"), marker(QStringLiteral("teacher_kr"), item));
        teacher.insert(QStringLiteral("teacher_en"), marker(QStringLiteral("teacher_en"), item));
        teacher.insert(QStringLiteral("preferred_romanization"), marker(QStringLiteral("preferred_romanization"), item));
        teacher.insert(QStringLiteral("preferred_name"), marker(QStringLiteral("preferred_name"), item));
        teacher.insert(QStringLiteral("room_number"), marker(QStringLiteral("room_number"), item));
        teacher.insert(QStringLiteral("birthday"), marker(QStringLiteral("birthday"), item));
        teacher.insert(QStringLiteral("phone_number"), marker(QStringLiteral("phone_number"), item));
        teacher.insert(QStringLiteral("wifi_name"), marker(QStringLiteral("wifi_name"), item));
        teacher.insert(QStringLiteral("wifi_password"), marker(QStringLiteral("wifi_password"), item));
        teacher.insert(QStringLiteral("internet_type"), marker(QStringLiteral("internet_type"), item));
        teacher.insert(QStringLiteral("zoom_id"), marker(QStringLiteral("zoom_id"), item));
        teacher.insert(QStringLiteral("zoom_password"), marker(QStringLiteral("zoom_password"), item));
        teacher.insert(QStringLiteral("projection_type"), marker(QStringLiteral("projection_type"), item));
        teacher.insert(QStringLiteral("notes"), marker(QStringLiteral("notes"), item));
        teachers.append(teacher);
        QJsonObject info;
        info.insert(QStringLiteral("class_grade"), marker(QStringLiteral("class_grade"), item));
        info.insert(QStringLiteral("class_level"), marker(QStringLiteral("class_level"), item));
        info.insert(QStringLiteral("reading_book"), marker(QStringLiteral("reading_book"), item));
        info.insert(QStringLiteral("essay_book"), marker(QStringLiteral("essay_book"), item));
        info.insert(QStringLiteral("class_color"), marker(QStringLiteral("class_color"), item));
        info.insert(QStringLiteral("font_color"), marker(QStringLiteral("font_color"), item));
        info.insert(QStringLiteral("notes"), marker(QStringLiteral("notes"), item));
        info.insert(QStringLiteral("time_filler_activities"), marker(QStringLiteral("time_filler_activities"), item));
        info.insert(QStringLiteral("regular_times"), times(item, QStringLiteral("regular")));
        info.insert(QStringLiteral("intensive_times"), times(item, QStringLiteral("intensive")));
        QJsonArray evaluations;
        for (const int evaluation : {7, 3})
        {
            QJsonArray rows;
            for (const int row : {4, 2})
            {
                QJsonArray cells;
                for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
                    cells.append(column == 0 ? QString{} : marker(
                        QStringLiteral("eval-%1/row-%2/col-%3").arg(evaluation).arg(row).arg(column), item));
                rows.append(cells);
            }
            evaluations.append(QJsonObject{
                {QStringLiteral("name"), marker(QStringLiteral("eval-%1").arg(evaluation), item)},
                {QStringLiteral("rows"), rows}});
        }
        classes.append(QJsonObject{
            {QStringLiteral("key"), QStringLiteral("class-%1").arg(item)},
            {QStringLiteral("name"), marker(QStringLiteral("class_name"), item)},
            {QStringLiteral("teacher_ref"), QStringLiteral("teacher-%1").arg(item)},
            {QStringLiteral("info"), info},
            {QStringLiteral("roster"), QJsonObject{
                {QStringLiteral("columns"), QJsonArray{marker(QStringLiteral("col-B"), item), QString{}, marker(QStringLiteral("col-A"), item)}},
                {QStringLiteral("column_widths"), QJsonArray{317, 0, 91}},
                {QStringLiteral("rows"), QJsonArray{
                    QJsonArray{marker(QStringLiteral("row-2-B"), item), QString{}, marker(QStringLiteral("row-2-A"), item)},
                    QJsonArray{QString{}, marker(QStringLiteral("row-1-blank-column"), item), QString{}}}}}},
            {QStringLiteral("speaking_evaluations"), evaluations}});
    }
    return {{QStringLiteral("format"), QStringLiteral("ClassMngr Classes")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("exported_at_utc"), QStringLiteral("2026-10-14T09:23:45.678+09:00")},
        {QStringLiteral("teachers"), teachers}, {QStringLiteral("classes"), classes}};
}
} // namespace

class ClassTransferApplyPackageAdapterTests final : public QObject
{
    Q_OBJECT
private slots:
    void preservesEveryImportedFieldAndOrdering();
    void preservesUtf16CodeUnitsAndUnboundedRowShapes();
    void preservesVersionAndInvalidTimestamp();
};

void ClassTransferApplyPackageAdapterTests::preservesEveryImportedFieldAndOrdering()
{
    const QJsonObject json = packageJson();
    auto parsed = ClassTransferJsonCodec::fromJson(json);
    QVERIFY2(parsed, parsed ? "" : qPrintable(parsed.error()));
    auto portable = Legacy::toPortableClassTransferPackage(std::move(*parsed));
    QCOMPARE(portable.version, 1);
    QCOMPARE(
        portable.exportedAtUtc.value(),
        std::string("2026-10-14T00:23:45.678Z")
        );
    QCOMPARE(portable.teachers.size(), std::size_t(2));
    QCOMPARE(portable.classes.size(), std::size_t(2));
    for (std::size_t index = 0; index < 2; ++index)
    {
        const int item = index == 0 ? 2 : 1;
        const auto& teacher = portable.teachers[index];
        QCOMPARE(QString::fromStdU16String(teacher.key.value()), QStringLiteral("teacher-%1").arg(item));
        QCOMPARE(QString::fromStdU16String(teacher.teacher.teacherKr), marker(QStringLiteral("teacher_kr"), item));
        QCOMPARE(QString::fromStdU16String(teacher.teacher.teacherEn), marker(QStringLiteral("teacher_en"), item));
        QCOMPARE(QString::fromStdU16String(teacher.teacher.preferredRomanization), marker(QStringLiteral("preferred_romanization"), item));
        QCOMPARE(QString::fromStdU16String(teacher.teacher.preferredName), marker(QStringLiteral("preferred_name"), item));
        QCOMPARE(QString::fromStdU16String(teacher.teacher.roomNumber), marker(QStringLiteral("room_number"), item));
        QCOMPARE(QString::fromStdU16String(teacher.teacher.birthday), marker(QStringLiteral("birthday"), item));
        QCOMPARE(QString::fromStdU16String(teacher.teacher.phoneNumber), marker(QStringLiteral("phone_number"), item));
        QCOMPARE(QString::fromStdU16String(teacher.teacher.wifiName), marker(QStringLiteral("wifi_name"), item));
        QCOMPARE(QString::fromStdU16String(teacher.teacher.wifiPassword), marker(QStringLiteral("wifi_password"), item));
        QCOMPARE(QString::fromStdU16String(teacher.teacher.internetType), marker(QStringLiteral("internet_type"), item));
        QCOMPARE(QString::fromStdU16String(teacher.teacher.zoomId), marker(QStringLiteral("zoom_id"), item));
        QCOMPARE(QString::fromStdU16String(teacher.teacher.zoomPassword), marker(QStringLiteral("zoom_password"), item));
        QCOMPARE(QString::fromStdU16String(teacher.teacher.projectionType), marker(QStringLiteral("projection_type"), item));
        QCOMPARE(QString::fromStdU16String(teacher.teacher.notes), marker(QStringLiteral("notes"), item));
        const auto& output = portable.classes[index];
        QCOMPARE(QString::fromStdU16String(output.key.value()), QStringLiteral("class-%1").arg(item));
        QCOMPARE(QString::fromStdU16String(output.name), marker(QStringLiteral("class_name"), item));
        QCOMPARE(QString::fromStdU16String(output.teacherKey.value()), QStringLiteral("teacher-%1").arg(item));
        QCOMPARE(QString::fromStdU16String(output.info.classGrade), marker(QStringLiteral("class_grade"), item));
        QCOMPARE(QString::fromStdU16String(output.info.classLevel), marker(QStringLiteral("class_level"), item));
        QCOMPARE(QString::fromStdU16String(output.info.readingBook), marker(QStringLiteral("reading_book"), item));
        QCOMPARE(QString::fromStdU16String(output.info.essayBook), marker(QStringLiteral("essay_book"), item));
        QCOMPARE(QString::fromStdU16String(output.info.classColor), marker(QStringLiteral("class_color"), item));
        QCOMPARE(QString::fromStdU16String(output.info.fontColor), marker(QStringLiteral("font_color"), item));
        QCOMPARE(QString::fromStdU16String(output.info.notes), marker(QStringLiteral("notes"), item));
        QCOMPARE(QString::fromStdU16String(output.info.timeFillerActivities), marker(QStringLiteral("time_filler_activities"), item));
        for (const auto& [kind, values] : {
                 std::pair{QStringLiteral("regular"), &output.info.classTimes},
                 std::pair{QStringLiteral("intensive"), &output.info.intensiveTimes}})
        {
            QCOMPARE(values->size(), std::size_t(2));
            for (std::size_t time = 0; time < 2; ++time)
            {
                const QString suffix = time == 0 ? QStringLiteral("second") : QStringLiteral("first");
                QCOMPARE(QString::fromStdU16String(values->at(time).day), marker(kind + suffix + QStringLiteral("day"), item));
                QCOMPARE(QString::fromStdU16String(values->at(time).startTime), marker(kind + suffix + QStringLiteral("start"), item));
                QCOMPARE(QString::fromStdU16String(values->at(time).endTime), marker(kind + suffix + QStringLiteral("end"), item));
            }
        }
        QCOMPARE(output.roster.columns.size(), std::size_t(3));
        QCOMPARE(QString::fromStdU16String(output.roster.columns[0]), marker(QStringLiteral("col-B"), item));
        QVERIFY(output.roster.columns[1].empty());
        QCOMPARE(QString::fromStdU16String(output.roster.columns[2]), marker(QStringLiteral("col-A"), item));
        QVERIFY(output.roster.columnWidths == std::vector<int>({317, 0, 91}));
        QCOMPARE(output.roster.rows.size(), std::size_t(2));
        QCOMPARE(output.roster.rows[0].size(), std::size_t(3));
        QCOMPARE(output.roster.rows[1].size(), std::size_t(3));
        QCOMPARE(QString::fromStdU16String(output.roster.rows[0][0]), marker(QStringLiteral("row-2-B"), item));
        QVERIFY(output.roster.rows[0][1].empty());
        QCOMPARE(QString::fromStdU16String(output.roster.rows[0][2]), marker(QStringLiteral("row-2-A"), item));
        QVERIFY(output.roster.rows[1][0].empty());
        QCOMPARE(QString::fromStdU16String(output.roster.rows[1][1]), marker(QStringLiteral("row-1-blank-column"), item));
        QVERIFY(output.roster.rows[1][2].empty());
        QCOMPARE(output.evaluations.size(), std::size_t(2));
        for (std::size_t evaluation = 0; evaluation < 2; ++evaluation)
        {
            const int evalId = evaluation == 0 ? 7 : 3;
            const auto& value = output.evaluations[evaluation];
            QCOMPARE(QString::fromStdU16String(value.name), marker(QStringLiteral("eval-%1").arg(evalId), item));
            QCOMPARE(value.rows.size(), std::size_t(2));
            for (std::size_t row = 0; row < 2; ++row)
            {
                QCOMPARE(value.rows[row].size(), std::size_t(SpeakingEval::ColumnCount));
                for (int col = 0; col < SpeakingEval::ColumnCount; ++col)
                    QCOMPARE(QString::fromStdU16String(value.rows[row][col]), col == 0 ? QString{} : marker(
                        QStringLiteral("eval-%1/row-%2/col-%3").arg(evalId).arg(row == 0 ? 4 : 2).arg(col), item));
            }
        }
    }
    QJsonObject expected = json;
    expected.insert(QStringLiteral("exported_at_utc"), QStringLiteral("2026-10-14T00:23:45.678Z"));
    QCOMPARE(ClassTransferJsonCodec::toJson(portable), expected);
    const auto reconstructed = Legacy::toLegacyClassTransferPackage(std::move(portable));
    QCOMPARE(reconstructed.exportedAtUtc.timeSpec(), Qt::UTC);
    QCOMPARE(reconstructed.exportedAtUtc.toString(Qt::ISODateWithMs), QStringLiteral("2026-10-14T00:23:45.678Z"));
    QCOMPARE(ClassTransferJsonCodec::toJson(reconstructed), expected);
}

void ClassTransferApplyPackageAdapterTests::preservesUtf16CodeUnitsAndUnboundedRowShapes()
{
    // Include an embedded NUL, both lone surrogate code units, and a valid
    // supplementary pair. Conversion must not pass through UTF-8 replacement.
    const std::u16string units{u'A', char16_t(0), char16_t(0xD800), u'B',
        char16_t(0xDC00), char16_t(0xD83D), char16_t(0xDE80), u'\n'};
    ::ClassTransferPackage source;
    source.exportedAtUtc = QDateTime::fromString(QStringLiteral("2000-02-29T00:00:00.001Z"), Qt::ISODateWithMs);
    ClassTransferTeacher teacher;
    teacher.key = QString::fromStdU16String(units);
    teacher.teacher.notes = teacher.key;
    source.teachers.append(teacher);
    ClassTransferClass output;
    output.key = teacher.key;
    output.name = teacher.key;
    output.teacherKey = teacher.key;
    output.info.notes = teacher.key;
    output.roster.columns = {teacher.key};
    output.roster.columnWidths = {-1, 400};
    output.roster.rows = {{teacher.key, QString{}}, {}, {teacher.key}};
    output.evaluations.append({teacher.key, {{}, {teacher.key}}});
    source.classes.append(output);
    auto portable = Legacy::toPortableClassTransferPackage(std::move(source));
    QVERIFY(portable.teachers.front().key.value() == units);
    QVERIFY(portable.teachers.front().teacher.notes == units);
    const auto& value = portable.classes.front();
    QVERIFY(value.key.value() == units && value.name == units
        && value.teacherKey.value() == units && value.info.notes == units);
    QVERIFY(value.roster.columns.front() == units);
    QVERIFY(value.roster.columnWidths == std::vector<int>({-1, 400}));
    QVERIFY(value.roster.rows == Application::ClassExportRows({{units, {}}, {}, {units}}));
    QVERIFY(value.evaluations.front().name == units);
    QVERIFY(value.evaluations.front().rows == Application::ClassExportRows({{}, {units}}));
    const auto legacy = Legacy::toLegacyClassTransferPackage(std::move(portable));
    QCOMPARE(legacy.teachers.front().key.toStdU16String(), units);
    QCOMPARE(legacy.teachers.front().teacher.notes.toStdU16String(), units);
    QCOMPARE(legacy.classes.front().name.toStdU16String(), units);
    QCOMPARE(legacy.classes.front().info.notes.toStdU16String(), units);
    QCOMPARE(legacy.classes.front().roster.rows, output.roster.rows);
    QCOMPARE(legacy.classes.front().roster.columns, output.roster.columns);
    QCOMPARE(legacy.classes.front().roster.columnWidths, output.roster.columnWidths);
    QCOMPARE(legacy.classes.front().evaluations.front().rows, output.evaluations.front().rows);
    QCOMPARE(legacy.classes.front().evaluations.front().name.toStdU16String(), units);
}

void ClassTransferApplyPackageAdapterTests::preservesVersionAndInvalidTimestamp()
{
    ::ClassTransferPackage package;
    package.version = 37;
    auto portable = Legacy::toPortableClassTransferPackage(std::move(package));
    QCOMPARE(portable.version, 37);
    QVERIFY(portable.exportedAtUtc.value().empty());
    const auto legacy = Legacy::toLegacyClassTransferPackage(std::move(portable));
    QCOMPARE(legacy.version, 37);
    QVERIFY(!legacy.exportedAtUtc.isValid());
}

QTEST_APPLESS_MAIN(ClassTransferApplyPackageAdapterTests)
#include "class_transfer_apply_package_adapter_tests.moc"
