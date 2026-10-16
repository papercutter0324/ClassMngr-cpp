#include "features/sub_prep/ui/sub_prep_class_information_model.h"

#include <QtTest>

namespace
{
ClassTime meeting(
    const QString& day,
    const QString& start
    )
{
    ClassTime value;
    value.day = day;
    value.startTime = start;
    value.endTime = QStringLiteral("4:50 PM");
    return value;
}

SubPrepClassInformation::SourceClass sourceClass(
    int classId,
    const Teacher& teacher,
    const QString& grade,
    const QString& level,
    const QList<ClassTime>& regularTimes,
    const QList<ClassTime>& intensiveTimes = {}
    )
{
    SubPrepClassInformation::SourceClass source;
    source.classroom.id = classId;
    source.classroom.name = level;
    source.info.classId = classId;
    source.info.teacherId = teacher.id;
    source.info.classGrade = grade;
    source.info.classLevel = level;
    source.info.classTimes = regularTimes;
    source.info.intensiveTimes = intensiveTimes;
    source.info.notes = QStringLiteral("Class %1 notes").arg(classId);
    source.teacher = teacher;
    source.studentCount = classId;
    return source;
}
}

class SubPrepClassInformationModelTests : public QObject
{
    Q_OBJECT

private slots:
    void parsesDefaultDisplayTimeFormats_data();
    void parsesDefaultDisplayTimeFormats();
    void trimsTimeWhitespaceAndOmitsSeconds();
    void formatsSharedAndDifferentMeetingTimes();
    void paddedAndLowercaseWeekdaysKeepOrderingAndFallbackLabels();
    void weekendAndFallbackWeekdaysKeepExpectedLabels_data();
    void weekendAndFallbackWeekdaysKeepExpectedLabels();
    void filtersHiddenDaysAndInvalidTimes();
    void groupsSortsAndDeduplicatesVisibleClasses();
    void filtersInvalidUnavailableHiddenAndDuplicateRowsBeforeOrdering();
    void ordersPreparedClassKeysAndRetainsFirstClassStyleFacts();
    void unknownConfiguredRanksSortAfterKnownRanks();
    void collapsesRepeatedClassLabelsInTeacherHeading();
    void selectedScheduleModeControlsDetails();
    void fallsBackForMissingTeacherAndClassNames();
};

void SubPrepClassInformationModelTests
    ::parsesDefaultDisplayTimeFormats_data()
{
    QTest::addColumn<QString>("startTime");
    QTest::addColumn<QString>("expected");

    QTest::newRow("12-hour-space-one-digit")
        << QStringLiteral("4:05 PM") << QStringLiteral("Mon 4:05pm");
    QTest::newRow("12-hour-compact-one-digit")
        << QStringLiteral("4:05PM") << QStringLiteral("Mon 4:05pm");
    QTest::newRow("12-hour-space-two-digits")
        << QStringLiteral("04:05 PM") << QStringLiteral("Mon 4:05pm");
    QTest::newRow("12-hour-compact-two-digits")
        << QStringLiteral("04:05PM") << QStringLiteral("Mon 4:05pm");
    QTest::newRow("24-hour-one-digit")
        << QStringLiteral("4:05") << QStringLiteral("Mon 4:05am");
    QTest::newRow("24-hour-two-digits")
        << QStringLiteral("04:05") << QStringLiteral("Mon 4:05am");
    QTest::newRow("24-hour-seconds-one-digit")
        << QStringLiteral("4:05:37") << QStringLiteral("Mon 4:05am");
    QTest::newRow("24-hour-seconds-two-digits")
        << QStringLiteral("04:05:37") << QStringLiteral("Mon 4:05am");
}

void SubPrepClassInformationModelTests
    ::parsesDefaultDisplayTimeFormats()
{
    QFETCH(QString, startTime);
    QFETCH(QString, expected);

    QCOMPARE(
        SubPrepClassInformation::formatMeetingTimes(
            {meeting(QStringLiteral("Monday"), startTime)},
            {QStringLiteral("Monday")}
            ),
        expected
        );
}

void SubPrepClassInformationModelTests
    ::trimsTimeWhitespaceAndOmitsSeconds()
{
    QCOMPARE(
        SubPrepClassInformation::formatMeetingTimes(
            {
                meeting(
                    QStringLiteral("Monday"),
                    QStringLiteral(" \t16:05:37\r\n")
                    )
            },
            {QStringLiteral("Monday")}
            ),
        QStringLiteral("Mon 4:05pm")
        );
}

void SubPrepClassInformationModelTests
    ::formatsSharedAndDifferentMeetingTimes()
{
    const QList<ClassTime> times{
        meeting(QStringLiteral("Friday"), QStringLiteral("5:00 PM")),
        meeting(QStringLiteral("Wednesday"), QStringLiteral("4:00 PM")),
        meeting(QStringLiteral("Monday"), QStringLiteral("6:00 PM")),
        meeting(QStringLiteral("Monday"), QStringLiteral("4:00 PM")),
        meeting(QStringLiteral("Monday"), QStringLiteral("4:00 PM")),
        meeting(QStringLiteral("Tuesday"), QStringLiteral("5:00 PM")),
        meeting(QStringLiteral("Friday"), QStringLiteral("6:00 PM")),
        meeting(QStringLiteral("Thursday"), QStringLiteral("3:00 PM"))
    };

    QCOMPARE(
        SubPrepClassInformation::formatMeetingTimes(
            times,
            {
                QStringLiteral("Monday"),
                QStringLiteral("Tuesday"),
                QStringLiteral("Wednesday"),
                QStringLiteral("Thursday"),
                QStringLiteral("Friday")
            }
            ),
        QStringLiteral("MonWed 4pm & MonFri 6pm & TuesFri 5pm & Thurs 3pm")
        );
}

void SubPrepClassInformationModelTests
    ::paddedAndLowercaseWeekdaysKeepOrderingAndFallbackLabels()
{
    const QList<ClassTime> times{
        meeting(QStringLiteral("monday"), QStringLiteral("6:00 PM")),
        meeting(QStringLiteral("Friday"), QStringLiteral("3:00 PM")),
        meeting(QStringLiteral(" Monday "), QStringLiteral("4:00 PM")),
        meeting(QStringLiteral("Tuesday"), QStringLiteral("4:30 PM")),
        meeting(QStringLiteral("Monday"), QStringLiteral("5:00 PM"))
    };

    QCOMPARE(
        SubPrepClassInformation::formatMeetingTimes(
            times,
            {
                QStringLiteral("monday"),
                QStringLiteral("Friday"),
                QStringLiteral(" Monday "),
                QStringLiteral("Tuesday"),
                QStringLiteral("Monday")
            }
            ),
        QStringLiteral(
            "Monday 4pm & Mon 5pm & Tues 4:30pm & Fri 3pm & monday 6pm"
            )
        );
}

void SubPrepClassInformationModelTests
    ::weekendAndFallbackWeekdaysKeepExpectedLabels_data()
{
    QTest::addColumn<QString>("day");
    QTest::addColumn<QString>("expected");

    QTest::newRow("canonical-saturday")
        << QStringLiteral("Saturday") << QStringLiteral("Sat 4pm");
    QTest::newRow("canonical-sunday")
        << QStringLiteral("Sunday") << QStringLiteral("Sun 4pm");
    QTest::newRow("padded-unknown")
        << QStringLiteral(" Funday ") << QStringLiteral("Funday 4pm");
    QTest::newRow("empty-weekday")
        << QString() << QStringLiteral(" 4pm");
}

void SubPrepClassInformationModelTests
    ::weekendAndFallbackWeekdaysKeepExpectedLabels()
{
    QFETCH(QString, day);
    QFETCH(QString, expected);

    QCOMPARE(
        SubPrepClassInformation::formatMeetingTimes(
            {meeting(day, QStringLiteral("4:00 PM"))},
            {day}
            ),
        expected
        );
}

void SubPrepClassInformationModelTests
    ::filtersHiddenDaysAndInvalidTimes()
{
    const QList<ClassTime> times{
        meeting(QStringLiteral("Monday"), QStringLiteral("4:05 PM")),
        meeting(QStringLiteral("Saturday"), QStringLiteral("10:00 AM")),
        meeting(QStringLiteral("Wednesday"), QStringLiteral("not a time"))
    };

    QCOMPARE(
        SubPrepClassInformation::formatMeetingTimes(
            times,
            {QStringLiteral("Monday"), QStringLiteral("Wednesday")}
            ),
        QStringLiteral("Mon 4:05pm")
        );

    QCOMPARE(
        SubPrepClassInformation::formatMeetingTimes(
            times,
            {QStringLiteral("Tuesday")}
            ),
        QStringLiteral("N/A")
        );
}

void SubPrepClassInformationModelTests
    ::groupsSortsAndDeduplicatesVisibleClasses()
{
    Teacher alice;
    alice.id = 10;
    alice.teacherEn = QStringLiteral(" Alice ");
    alice.teacherKr = QStringLiteral("앨리스");
    alice.preferredName = QStringLiteral("Alice Kim");
    alice.notes = QStringLiteral("One teacher note");

    Teacher bob;
    bob.id = 20;
    bob.teacherEn = QStringLiteral(" Bob ");

    QList<SubPrepClassInformation::SourceClass> sources{
        sourceClass(
            6,
            alice,
            QStringLiteral("E6"),
            QStringLiteral("Poseidon"),
            {meeting(QStringLiteral("Friday"), QStringLiteral("5:00 PM"))}
            ),
        sourceClass(
            4,
            alice,
            QStringLiteral("E4"),
            QStringLiteral("Hercules"),
            {meeting(QStringLiteral("Monday"), QStringLiteral("4:00 PM"))}
            ),
        sourceClass(
            4,
            alice,
            QStringLiteral("E4"),
            QStringLiteral("Hercules"),
            {meeting(QStringLiteral("Monday"), QStringLiteral("4:00 PM"))}
            ),
        sourceClass(
            2,
            bob,
            QStringLiteral("M2"),
            QStringLiteral("Major"),
            {meeting(QStringLiteral("Tuesday"), QStringLiteral("6:00 PM"))}
            ),
        sourceClass(
            99,
            bob,
            QStringLiteral("M3"),
            QStringLiteral("Minor"),
            {meeting(QStringLiteral("Wednesday"), QStringLiteral("6:00 PM"))}
            )
    };
    sources[2].studentCount = 44;
    sources[2].info.notes = QStringLiteral("Later duplicate notes");

    SubPrepClassInformation::BuildOptions options;
    options.visibleClassIds = {2, 4, 6};
    options.visibleDays = {
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday")
    };

    const auto groups =
        SubPrepClassInformation::build(
            sources,
            options
            );

    QCOMPARE(groups.size(), 2);
    QCOMPARE(groups.at(0).displayName, QStringLiteral("Alice Kim"));
    QCOMPARE(
        groups.at(0).classListText,
        QStringLiteral("E4 Hercules / E6 Poseidon")
        );
    QCOMPARE(groups.at(0).classes.size(), 2);
    QCOMPARE(groups.at(0).classes.at(0).classId, 4);
    QCOMPARE(groups.at(0).classes.at(0).studentCount, 4);
    QCOMPARE(
        groups.at(0).classes.at(0).info.notes,
        QStringLiteral("Class 4 notes")
        );
    QCOMPARE(
        groups.at(0).teacher.notes,
        QStringLiteral("One teacher note")
        );
    QCOMPARE(groups.at(1).displayName, QStringLiteral("Bob"));
    QCOMPARE(groups.at(1).classes.size(), 1);
    QCOMPARE(groups.at(1).classes.first().classId, 2);
}

void SubPrepClassInformationModelTests
    ::filtersInvalidUnavailableHiddenAndDuplicateRowsBeforeOrdering()
{
    Teacher validTeacher;
    validTeacher.id = 5;
    validTeacher.teacherEn = QStringLiteral("Valid Teacher");

    Teacher duplicateTeacher;
    duplicateTeacher.id = 6;
    duplicateTeacher.teacherEn = QStringLiteral("Duplicate Teacher");

    Teacher unavailableTeacher;
    unavailableTeacher.id = 0;

    Teacher invalidTeacher;
    invalidTeacher.id = -1;

    auto firstDuplicate = sourceClass(
        1,
        validTeacher,
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        {meeting(QStringLiteral("Monday"), QStringLiteral("4:00 PM"))}
        );
    firstDuplicate.studentCount = 11;
    firstDuplicate.info.notes = QStringLiteral("First duplicate row");

    auto laterDuplicate = sourceClass(
        1,
        duplicateTeacher,
        QStringLiteral("E5"),
        QStringLiteral("Apollo"),
        {meeting(QStringLiteral("Monday"), QStringLiteral("3:00 PM"))}
        );
    laterDuplicate.studentCount = 99;
    laterDuplicate.info.notes = QStringLiteral("Later duplicate row");

    auto unassignedTeacher = sourceClass(
        2,
        validTeacher,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        {}
        );
    unassignedTeacher.info.teacherId = 0;

    auto unavailable = sourceClass(
        3,
        unavailableTeacher,
        QStringLiteral("E4"),
        QStringLiteral("Odysseus"),
        {}
        );
    unavailable.info.teacherId = 8;

    auto invalidTeacherId = sourceClass(
        4,
        invalidTeacher,
        QStringLiteral("E4"),
        QStringLiteral("Hercules"),
        {}
        );

    const auto groups = SubPrepClassInformation::build(
        {
            firstDuplicate,
            laterDuplicate,
            sourceClass(
                0,
                validTeacher,
                QStringLiteral("E4"),
                QStringLiteral("Theseus"),
                {}
                ),
            sourceClass(
                -2,
                validTeacher,
                QStringLiteral("E4"),
                QStringLiteral("Theseus"),
                {}
                ),
            unassignedTeacher,
            unavailable,
            invalidTeacherId,
            sourceClass(
                5,
                validTeacher,
                QStringLiteral("E4"),
                QStringLiteral("Hercules"),
                {}
                )
        },
        {
            .visibleClassIds = {1, 2, 3, 4},
            .visibleDays = {QStringLiteral("Monday")}
        }
        );

    QCOMPARE(groups.size(), 1);
    QCOMPARE(groups.first().teacher.id, 5);
    QCOMPARE(groups.first().classes.size(), 1);
    QCOMPARE(groups.first().classes.first().classId, 1);
    QCOMPARE(groups.first().classes.first().studentCount, 11);
    QCOMPARE(
        groups.first().classes.first().info.notes,
        QStringLiteral("First duplicate row")
        );
}

void SubPrepClassInformationModelTests
    ::ordersPreparedClassKeysAndRetainsFirstClassStyleFacts()
{
    Teacher teacher;
    teacher.id = 1;
    teacher.teacherEn = QStringLiteral("Susan");

    auto lateSameMinute = sourceClass(
        12,
        teacher,
        QStringLiteral(" E4 "),
        QStringLiteral(" Theseus "),
        {meeting(QStringLiteral("Monday"), QStringLiteral("5:00:59"))}
        );
    lateSameMinute.info.classColor = QStringLiteral("#999999");

    auto laterGrade = sourceClass(
        90,
        teacher,
        QStringLiteral("E5"),
        QStringLiteral("Apollo"),
        {meeting(QStringLiteral("Monday"), QStringLiteral("3:00 PM"))}
        );

    auto laterConfiguredLevel = sourceClass(
        10,
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Hercules"),
        {meeting(QStringLiteral("Monday"), QStringLiteral("4:00 PM"))}
        );

    auto noVisibleValidMeeting = sourceClass(
        13,
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        {
            meeting(QStringLiteral("Monday"), QStringLiteral("not a time")),
            meeting(QStringLiteral("Tuesday"), QStringLiteral("4:00 PM"))
        }
        );

    auto firstSorted = sourceClass(
        11,
        teacher,
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        {meeting(QStringLiteral("Monday"), QStringLiteral("5:00:02"))}
        );
    firstSorted.info.classColor = QStringLiteral("#123456");
    firstSorted.info.fontColor = QStringLiteral("#ABCDEF");

    const auto groups = SubPrepClassInformation::build(
        {
            lateSameMinute,
            laterGrade,
            laterConfiguredLevel,
            noVisibleValidMeeting,
            firstSorted
        },
        {
            .visibleClassIds = {10, 11, 12, 13, 90},
            .visibleDays = {QStringLiteral("Monday")}
        }
        );

    QCOMPARE(groups.size(), 1);
    QCOMPARE(groups.first().classes.size(), 5);
    QCOMPARE(groups.first().classes.at(0).classId, 11);
    QCOMPARE(groups.first().classes.at(1).classId, 12);
    QCOMPARE(groups.first().classes.at(2).classId, 13);
    QCOMPARE(groups.first().classes.at(2).timeText, QStringLiteral("N/A"));
    QCOMPARE(groups.first().classes.at(3).classId, 10);
    QCOMPARE(groups.first().classes.at(4).classId, 90);
    QCOMPARE(
        groups.first().classes.first().info.classColor,
        QStringLiteral("#123456")
        );
    QCOMPARE(
        groups.first().classes.first().info.fontColor,
        QStringLiteral("#ABCDEF")
        );
    QCOMPARE(groups.first().classes.at(0).timeText, QStringLiteral("Mon 5am"));
    QCOMPARE(groups.first().classes.at(1).timeText, QStringLiteral("Mon 5am"));
}

void SubPrepClassInformationModelTests::unknownConfiguredRanksSortAfterKnownRanks()
{
    Teacher teacher;
    teacher.id = 1;
    teacher.teacherEn = QStringLiteral("Susan");

    const auto groups = SubPrepClassInformation::build(
        {
            sourceClass(
                1,
                teacher,
                QStringLiteral("E4"),
                QStringLiteral("Unlisted Level"),
                {meeting(QStringLiteral("Monday"), QStringLiteral("4:00 PM"))}
                ),
            sourceClass(
                2,
                teacher,
                QStringLiteral("E4"),
                QStringLiteral("Theseus"),
                {meeting(QStringLiteral("Monday"), QStringLiteral("5:00 PM"))}
                ),
            sourceClass(
                3,
                teacher,
                QStringLiteral("Unlisted Grade"),
                QStringLiteral("Theseus"),
                {meeting(QStringLiteral("Monday"), QStringLiteral("3:00 PM"))}
                )
        },
        {
            .visibleClassIds = {1, 2, 3},
            .visibleDays = {QStringLiteral("Monday")}
        }
        );

    QCOMPARE(groups.size(), 1);
    QCOMPARE(groups.first().classes.size(), 3);
    QCOMPARE(groups.first().classes.at(0).classId, 2);
    QCOMPARE(groups.first().classes.at(1).classId, 1);
    QCOMPARE(groups.first().classes.at(2).classId, 3);
}

void SubPrepClassInformationModelTests
    ::selectedScheduleModeControlsDetails()
{
    Teacher teacher;
    teacher.id = 1;
    teacher.teacherEn = QStringLiteral("Susan");

    const auto source =
        sourceClass(
            7,
            teacher,
            QStringLiteral("E5"),
            QStringLiteral("Zeus"),
            {meeting(QStringLiteral("Monday"), QStringLiteral("4:00 PM"))},
            {meeting(QStringLiteral("Wednesday"), QStringLiteral("10:00 AM"))}
            );

    SubPrepClassInformation::BuildOptions options;
    options.visibleClassIds = {7};
    options.visibleDays = {QStringLiteral("Wednesday")};
    options.useIntensive = true;

    const auto groups =
        SubPrepClassInformation::build(
            {source},
            options
            );

    QCOMPARE(groups.size(), 1);
    QCOMPARE(
        groups.first().classes.first().timeText,
        QStringLiteral("Wed 10am")
        );
}

void SubPrepClassInformationModelTests
    ::collapsesRepeatedClassLabelsInTeacherHeading()
{
    Teacher teacher;
    teacher.id = 1;
    teacher.teacherEn = QStringLiteral("Emma");

    SubPrepClassInformation::BuildOptions options;
    options.visibleClassIds = {1, 2};
    options.visibleDays = {QStringLiteral("Monday")};

    const auto groups =
        SubPrepClassInformation::build(
            {
                sourceClass(
                    1,
                    teacher,
                    QStringLiteral("M1"),
                    QStringLiteral("Major"),
                    {meeting(QStringLiteral("Monday"), QStringLiteral("4:00 PM"))}
                    ),
                sourceClass(
                    2,
                    teacher,
                    QStringLiteral("M1"),
                    QStringLiteral("Major"),
                    {meeting(QStringLiteral("Monday"), QStringLiteral("5:00 PM"))}
                    )
            },
            options
            );

    QCOMPARE(groups.size(), 1);
    QCOMPARE(groups.first().classes.size(), 2);
    QCOMPARE(groups.first().classListText, QStringLiteral("M1 Major"));
    QCOMPARE(groups.first().classes.at(0).classId, 1);
    QCOMPARE(groups.first().classes.at(1).classId, 2);
    QCOMPARE(
        groups.first().classes.at(0).info.notes,
        QStringLiteral("Class 1 notes")
        );
    QCOMPARE(
        groups.first().classes.at(1).info.notes,
        QStringLiteral("Class 2 notes")
        );
    QCOMPARE(groups.first().classes.at(0).timeText, QStringLiteral("Mon 4pm"));
    QCOMPARE(groups.first().classes.at(1).timeText, QStringLiteral("Mon 5pm"));
}

void SubPrepClassInformationModelTests
    ::fallsBackForMissingTeacherAndClassNames()
{
    Teacher teacher;
    teacher.id = 1;
    teacher.teacherKr = QStringLiteral("김선생");

    auto source =
        sourceClass(
            1,
            teacher,
            QString(),
            QString(),
            {meeting(QStringLiteral("Monday"), QStringLiteral("4:00 PM"))}
            );

    SubPrepClassInformation::BuildOptions options;
    options.visibleClassIds = {1};
    options.visibleDays = {QStringLiteral("Monday")};

    auto groups =
        SubPrepClassInformation::build(
            {source},
            options
            );

    QCOMPARE(groups.first().displayName, QStringLiteral("김선생"));
    QCOMPARE(groups.first().classListText, QStringLiteral("N/A"));

    source.teacher.teacherKr.clear();
    groups =
        SubPrepClassInformation::build(
            {source},
            options
            );

    QCOMPARE(groups.first().displayName, QStringLiteral("N/A"));
}

QTEST_APPLESS_MAIN(SubPrepClassInformationModelTests)

#include "sub_prep_class_information_model_tests.moc"
