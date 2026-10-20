#include "features/sub_prep/ui/sub_prep_print_source_mapper.h"

#include <QTest>

#include <tuple>

namespace Application = ClassMngr::Next::Application;
namespace Domain = ClassMngr::Next::Domain;

namespace
{
Application::SubPrepPrintSourceRequest requestFor(
    const std::string& classId = "17",
    const Application::ScheduleViewMode mode =
        Application::ScheduleViewMode::Regular
    )
{
    return {
        .selectedClassIds = {*Domain::ClassId::fromString(classId)},
        .selectedDays = {Application::SubPrepWeekday::Monday},
        .mode = mode
    };
}

Application::SubPrepPrintTeacher teacherFor(
    const std::string& id = "4"
    )
{
    Application::SubPrepPrintTeacher teacher{
        .id = *Domain::TeacherId::fromString(id)
    };
    teacher.englishName = "Jamie Lee";
    teacher.koreanName = "\xEA\xB9\x80\xEC\x9E\xAC\xEB\xAF\xB8";
    teacher.preferredRomanization = "Jamie Lee";
    teacher.preferredName = "Custom Name";
    teacher.room = "Room 4";
    teacher.wifiName = "Network 4";
    teacher.wifiPassword = "wifi-secret";
    teacher.internetType = "LAN";
    teacher.zoomId = "zoom-4";
    teacher.zoomPassword = "zoom-secret";
    teacher.projectionType = "HDMI";
    teacher.teacherNotes = "Teacher notes";
    return teacher;
}

Application::SubPrepPrintClass classFor(
    const std::string& id = "17",
    const std::string& teacherId = "4"
    )
{
    return {
        .id = *Domain::ClassId::fromString(id),
        .teacherId = Domain::TeacherId::fromString(teacherId),
        .grade = "E4",
        .level = "Odysseus",
        .classNotes = "Class notes",
        .classColor = "#123456",
        .fontColor = "#ABCDEF",
        .studentCount = Application::ClassSummaryStudentCount{19},
        .meetings = {
            {
                .weekday = Application::SubPrepWeekday::Monday,
                .startTime = "9:00 AM",
                .endTime = "9:50 AM"
            }
        }
    };
}

class FakePrintSourceReadPort final
    : public Application::SubPrepPrintSourceReadPort
{
public:
    Application::SubPrepPrintSourceInput input;

    Application::SubPrepPrintSourceReadResult loadSource(
        const Application::SubPrepPrintSourceRequest&
        ) override
    {
        return Application::SubPrepPrintSourceReadResult::success(input);
    }
};
}

class SubPrepPrintSourceMapperTests final : public QObject
{
    Q_OBJECT

private slots:
    void mapsBoundedSourceIntoExistingPrintModel();
    void preservesTeacherDisplayNameFallbackOrder();
    void mapsSelectedModeIntoTheRendererModel();
    void mapsSelectedDaysAndModeIntoOrderedDetails();
    void rejectsIdentifiersTheLegacyRendererCannotRepresent();
};

void SubPrepPrintSourceMapperTests::mapsBoundedSourceIntoExistingPrintModel()
{
    const Application::SubPrepPrintSourceRequest request = requestFor();
    FakePrintSourceReadPort port;
    port.input.teachers = {teacherFor()};
    port.input.classes = {classFor()};

    const auto source = Application::SubPrepPrintSourceQuery(port).execute(request);
    QVERIFY(source);

    const auto mapped = SubPrepPrintSourceMapper::toClassInformation(
        source.value(),
        request
        );
    QVERIFY(mapped);
    QCOMPARE(mapped.value().size(), 1);

    const SubPrepClassInformation::TeacherGroup& group = mapped.value().first();
    QCOMPARE(group.displayName, QStringLiteral("Custom Name"));
    QCOMPARE(group.teacher.teacherEn, QStringLiteral("Jamie Lee"));
    QCOMPARE(group.teacher.teacherKr, QString::fromUtf8(
        "\xEA\xB9\x80\xEC\x9E\xAC\xEB\xAF\xB8"
        ));
    QCOMPARE(group.teacher.roomNumber, QStringLiteral("Room 4"));
    QCOMPARE(group.teacher.wifiName, QStringLiteral("Network 4"));
    QCOMPARE(group.teacher.wifiPassword, QStringLiteral("wifi-secret"));
    QCOMPARE(group.teacher.internetType, QStringLiteral("LAN"));
    QCOMPARE(group.teacher.zoomId, QStringLiteral("zoom-4"));
    QCOMPARE(group.teacher.zoomPassword, QStringLiteral("zoom-secret"));
    QCOMPARE(group.teacher.projectionType, QStringLiteral("HDMI"));
    QCOMPARE(group.teacher.notes, QStringLiteral("Teacher notes"));
    QCOMPARE(group.classes.size(), 1);
    QCOMPARE(group.classes.first().classId, 17);
    QCOMPARE(group.classes.first().classLabel, QStringLiteral("E4 Odysseus"));
    QCOMPARE(group.classes.first().studentCount, 19);
    QCOMPARE(group.classes.first().info.notes, QStringLiteral("Class notes"));
    QCOMPARE(group.classes.first().info.classColor, QStringLiteral("#123456"));
    QCOMPARE(group.classes.first().info.fontColor, QStringLiteral("#ABCDEF"));
    QCOMPARE(group.classes.first().timeText, QStringLiteral("Mon 9am"));

    FakePrintSourceReadPort zeroCountPort;
    zeroCountPort.input.teachers = {teacherFor()};
    auto zeroCountClass = classFor();
    zeroCountClass.studentCount = Application::ClassSummaryStudentCount{};
    zeroCountPort.input.classes = {zeroCountClass};
    const auto zeroCountSource =
        Application::SubPrepPrintSourceQuery(zeroCountPort).execute(request);
    QVERIFY(zeroCountSource);
    const auto zeroCountMapped = SubPrepPrintSourceMapper::toClassInformation(
        zeroCountSource.value(),
        request
        );
    QVERIFY(zeroCountMapped);
    QCOMPARE(
        zeroCountMapped.value().first().classes.first().studentCount,
        0
        );
}

void SubPrepPrintSourceMapperTests::preservesTeacherDisplayNameFallbackOrder()
{
    const Application::SubPrepPrintSourceRequest request = requestFor();

    for (const auto& [preferred, romanization, english, korean, expected] : {
             std::tuple{
                 std::string(""),
                 std::string("Preferred Romanization"),
                 std::string("English Name"),
                 std::string("Korean Name"),
                 QStringLiteral("English Name")
             },
             std::tuple{
                 std::string(""),
                 std::string(""),
                 std::string("English Name"),
                 std::string("Korean Name"),
                 QStringLiteral("English Name")
             },
             std::tuple{
                 std::string(""),
                 std::string("Preferred Romanization"),
                 std::string(""),
                 std::string("Korean Name"),
                 QStringLiteral("Preferred Romanization")
             },
             std::tuple{
                 std::string(""),
                 std::string(""),
                 std::string(""),
                 std::string("Korean Name"),
                 QStringLiteral("Korean Name")
             }
         })
    {
        FakePrintSourceReadPort port;
        auto teacher = teacherFor();
        teacher.preferredName = preferred;
        teacher.preferredRomanization = romanization;
        teacher.englishName = english;
        teacher.koreanName = korean;
        port.input.teachers = {teacher};
        port.input.classes = {classFor()};

        const auto source = Application::SubPrepPrintSourceQuery(port).execute(request);
        QVERIFY(source);
        const auto mapped = SubPrepPrintSourceMapper::toClassInformation(
            source.value(),
            request
            );
        QVERIFY(mapped);
        QCOMPARE(mapped.value().first().displayName, expected);
    }
}

void SubPrepPrintSourceMapperTests::mapsSelectedModeIntoTheRendererModel()
{
    const Application::SubPrepPrintSourceRequest request = requestFor(
        "17",
        Application::ScheduleViewMode::Intensive
        );
    FakePrintSourceReadPort port;
    port.input.teachers = {teacherFor()};
    auto classRecord = classFor();
    classRecord.meetings.front().startTime = "4:00 PM";
    classRecord.meetings.front().endTime = "4:50 PM";
    port.input.classes = {classRecord};

    const auto source = Application::SubPrepPrintSourceQuery(port).execute(request);
    QVERIFY(source);
    const auto mapped = SubPrepPrintSourceMapper::toClassInformation(
        source.value(),
        request
        );
    QVERIFY(mapped);
    QCOMPARE(
        mapped.value().first().classes.first().timeText,
        QStringLiteral("Mon 4pm")
        );
    QVERIFY(mapped.value().first().classes.first().info.classTimes.isEmpty());
}

void SubPrepPrintSourceMapperTests::mapsSelectedDaysAndModeIntoOrderedDetails()
{
    Application::SubPrepPrintSourceRequest request = requestFor(
        "17",
        Application::ScheduleViewMode::Intensive
        );
    request.selectedClassIds.push_back(*Domain::ClassId::fromString("18"));
    request.selectedDays = {
        Application::SubPrepWeekday::Monday,
        Application::SubPrepWeekday::Wednesday
    };

    FakePrintSourceReadPort port;
    port.input.teachers = {teacherFor()};

    auto laterClass = classFor("17");
    laterClass.level = "Odysseus";
    laterClass.classNotes = "Later class details";
    laterClass.classColor = "#111111";
    laterClass.meetings = {
        {
            .weekday = Application::SubPrepWeekday::Wednesday,
            .startTime = "4:00 PM",
            .endTime = "4:50 PM"
        }
    };

    auto firstClass = classFor("18");
    firstClass.level = "Odysseus";
    firstClass.classNotes = "First class details";
    firstClass.classColor = "#123456";
    firstClass.fontColor = "#ABCDEF";
    firstClass.meetings = {
        {
            .weekday = Application::SubPrepWeekday::Monday,
            .startTime = "not a time",
            .endTime = "not a time"
        },
        {
            .weekday = Application::SubPrepWeekday::Wednesday,
            .startTime = "10:00 AM",
            .endTime = "10:50 AM"
        }
    };
    port.input.classes = {laterClass, firstClass};

    const auto source = Application::SubPrepPrintSourceQuery(port).execute(request);
    QVERIFY(source);
    const auto mapped = SubPrepPrintSourceMapper::toClassInformation(
        source.value(),
        request
        );
    QVERIFY(mapped);
    QCOMPARE(mapped.value().size(), 1);

    const SubPrepClassInformation::TeacherGroup& group = mapped.value().first();
    QCOMPARE(group.classes.size(), 2);
    QCOMPARE(group.classes.at(0).classId, 18);
    QCOMPARE(group.classes.at(0).classLabel, QStringLiteral("E4 Odysseus"));
    QCOMPARE(group.classes.at(0).timeText, QStringLiteral("Wed 10am"));
    QCOMPARE(group.classes.at(0).info.notes, QStringLiteral("First class details"));
    QCOMPARE(group.classes.at(0).info.classColor, QStringLiteral("#123456"));
    QCOMPARE(group.classes.at(0).info.fontColor, QStringLiteral("#ABCDEF"));
    QCOMPARE(group.classes.at(1).classId, 17);
    QCOMPARE(group.classes.at(1).timeText, QStringLiteral("Wed 4pm"));
    QCOMPARE(group.classes.at(1).info.notes, QStringLiteral("Later class details"));
    QCOMPARE(group.classListText, QStringLiteral("E4 Odysseus"));
    QVERIFY(group.classes.at(0).info.classTimes.isEmpty());
    QVERIFY(group.classes.at(0).info.intensiveTimes.size() == 2);
}

void SubPrepPrintSourceMapperTests::
rejectsIdentifiersTheLegacyRendererCannotRepresent()
{
    for (const std::string& classIdentifier : {
             std::string("class-17"),
             std::string("017")
         })
    {
        const auto request = requestFor(classIdentifier);
        FakePrintSourceReadPort port;
        port.input.teachers = {teacherFor()};
        port.input.classes = {classFor(classIdentifier)};

        const auto source =
            Application::SubPrepPrintSourceQuery(port).execute(request);
        QVERIFY(source);
        const auto mapped = SubPrepPrintSourceMapper::toClassInformation(
            source.value(),
            request
            );
        QVERIFY(!mapped);
        QCOMPARE(mapped.error().code, Domain::ErrorCode::Validation);
    }
}

QTEST_APPLESS_MAIN(SubPrepPrintSourceMapperTests)

#include "sub_prep_print_source_mapper_tests.moc"
