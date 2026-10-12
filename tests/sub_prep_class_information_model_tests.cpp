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
    void filtersHiddenDaysAndInvalidTimes();
    void groupsSortsAndDeduplicatesVisibleClasses();
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
    alice.teacherEn = QStringLiteral("Alice");
    alice.teacherKr = QStringLiteral("앨리스");
    alice.preferredName = QStringLiteral("Alice Kim");
    alice.notes = QStringLiteral("One teacher note");

    Teacher bob;
    bob.id = 20;
    bob.teacherEn = QStringLiteral("Bob");

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
