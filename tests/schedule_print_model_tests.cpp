#include "features/schedule/ui/schedule_view_model.h"

#include <QtTest>

namespace
{
ScheduleEntry entry(
    int classId = 1
    )
{
    ScheduleEntry scheduleEntry;
    scheduleEntry.classId = classId;
    scheduleEntry.teacherKr = QStringLiteral("박지혜");
    scheduleEntry.roomNumber = QStringLiteral("413");
    scheduleEntry.classGrade = QStringLiteral("E5");
    scheduleEntry.classLevel = QStringLiteral("Zeus");
    scheduleEntry.classColor = QStringLiteral("#C9D8A6");
    scheduleEntry.fontColor = QStringLiteral("#000000");

    return scheduleEntry;
}

ScheduleBuildResult blankResult(
    const QStringList& days,
    const QStringList& rowLabels
    )
{
    ScheduleBuildResult result;
    result.days = days;

    for (const QString& day : days)
    {
        result.schedule.insert(
            day,
            {}
            );
    }

    for (const QString& rowLabel : rowLabels)
    {
        result.rows.append(
            { rowLabel }
            );
    }

    return result;
}

ScheduleViewRequest requestFor(
    const QStringList& days,
    bool useIntensive = false
    )
{
    ScheduleViewRequest request;
    request.days = days;
    request.displayMode =
        useIntensive
            ? ScheduleDisplayMode::Intensive
            : ScheduleDisplayMode::Regular;

    return request;
}

void setRowState(
    ScheduleViewRequest& request,
    const QStringList& days,
    const QString& rowLabel,
    const QString& state
    )
{
    for (const QString& day : days)
    {
        request.slotStateOverrides.insert(
            scheduleSlotKey(day, rowLabel),
            state
            );
    }
}
}

class SchedulePrintModelTests : public QObject
{
    Q_OBJECT

private slots:
    void visibleDaysCanIncludeWeekends();
    void slotDefaultsMatchRegularAndIntensiveModes();
    void persistedOverridesRespectTogglingRules();
    void intensiveTrimmingRemovesOnlyOuterEmptyRows();
    void intensiveNoFilteringKeepsAllRows();
    void regularSchedulesIgnoreIntensiveTrimming();
    void footerTotalsMatchExcelScreenshotConvention();
    void teacherRoomLineRespectsSelectedNameLanguage();
    void testingModeFiltersAffectedGradesAndPreservesPriority();
    void testingClassAssignmentsOverrideUnderlyingCells();
};

void SchedulePrintModelTests::visibleDaysCanIncludeWeekends()
{
    QCOMPARE(
        visibleScheduleDays(false),
        QStringList({
            QStringLiteral("Monday"),
            QStringLiteral("Tuesday"),
            QStringLiteral("Wednesday"),
            QStringLiteral("Thursday"),
            QStringLiteral("Friday")
        })
        );

    QCOMPARE(
        visibleScheduleDays(true),
        QStringList({
            QStringLiteral("Monday"),
            QStringLiteral("Tuesday"),
            QStringLiteral("Wednesday"),
            QStringLiteral("Thursday"),
            QStringLiteral("Friday"),
            QStringLiteral("Saturday"),
            QStringLiteral("Sunday")
        })
        );
}

void SchedulePrintModelTests::slotDefaultsMatchRegularAndIntensiveModes()
{
    QCOMPARE(
        scheduleDefaultSlotState(
            QStringLiteral("Monday"),
            QStringLiteral("15:00"),
            false
            ),
        scheduleEmptySlotState()
        );
    QCOMPARE(
        scheduleDefaultSlotState(
            QStringLiteral("Monday"),
            QStringLiteral("16:00"),
            false
            ),
        scheduleEssaySlotState()
        );
    QCOMPARE(
        scheduleDefaultSlotState(
            QStringLiteral("Saturday"),
            QStringLiteral("16:00"),
            false
            ),
        scheduleEmptySlotState()
        );
    QCOMPARE(
        scheduleDefaultSlotState(
            QStringLiteral("Monday"),
            QStringLiteral("09:00"),
            true
            ),
        scheduleEssaySlotState()
        );
}

void SchedulePrintModelTests::persistedOverridesRespectTogglingRules()
{
    const QStringList days{
        QStringLiteral("Monday"),
        QStringLiteral("Saturday")
    };
    ScheduleBuildResult result =
        blankResult(
            days,
            { QStringLiteral("16:00") }
            );

    ScheduleViewRequest request =
        requestFor(
            days,
            false
            );
    request.slotStateOverrides.insert(
        scheduleSlotKey(
            QStringLiteral("Monday"),
            QStringLiteral("16:00")
            ),
        scheduleLunchSlotState()
        );
    request.slotStateOverrides.insert(
        scheduleSlotKey(
            QStringLiteral("Saturday"),
            QStringLiteral("16:00")
            ),
        scheduleLunchSlotState()
        );

    ScheduleViewModel model =
        buildScheduleViewModel(
            result,
            request
            );

    QCOMPARE(
        model.rows.first().cells.at(0).slotState,
        scheduleEssaySlotState()
        );
    QCOMPARE(
        model.rows.first().cells.at(1).slotState,
        scheduleLunchSlotState()
        );

    request.displayMode =
        ScheduleDisplayMode::Intensive;
    model =
        buildScheduleViewModel(
            result,
            request
            );

    QCOMPARE(
        model.rows.first().cells.at(0).slotState,
        scheduleLunchSlotState()
        );
}

void SchedulePrintModelTests
    ::intensiveTrimmingRemovesOnlyOuterEmptyRows()
{
    const QStringList days =
        visibleScheduleDays(false);
    ScheduleBuildResult result =
        blankResult(
            days,
            {
                QStringLiteral("15:00"),
                QStringLiteral("16:00"),
                QStringLiteral("17:00"),
                QStringLiteral("18:00"),
                QStringLiteral("19:00"),
                QStringLiteral("20:00"),
                QStringLiteral("21:00")
            }
            );

    result.schedule[QStringLiteral("Monday")][QStringLiteral("16:00")]
        .append(entry(1));

    ScheduleViewRequest request =
        requestFor(
            days,
            true
            );
    request.rowFilter =
        ScheduleRowFilter::TrimEmptyOuterRows;

    for (const ScheduleRow& row : result.rows)
    {
        setRowState(
            request,
            days,
            row.label,
            scheduleEmptySlotState()
            );
    }

    request.slotStateOverrides.insert(
        scheduleSlotKey(
            QStringLiteral("Wednesday"),
            QStringLiteral("18:00")
            ),
        scheduleEssaySlotState()
        );
    request.slotStateOverrides.insert(
        scheduleSlotKey(
            QStringLiteral("Friday"),
            QStringLiteral("20:00")
            ),
        scheduleLunchSlotState()
        );

    const ScheduleViewModel model =
        buildScheduleViewModel(
            result,
            request
            );

    QCOMPARE(model.rows.size(), 5);
    QCOMPARE(model.rows.first().timeLabel, QStringLiteral("16:00"));
    QCOMPARE(model.rows.at(1).timeLabel, QStringLiteral("17:00"));
    QCOMPARE(model.rows.at(2).timeLabel, QStringLiteral("18:00"));
    QCOMPARE(model.rows.at(3).timeLabel, QStringLiteral("19:00"));
    QCOMPARE(model.rows.last().timeLabel, QStringLiteral("20:00"));
}

void SchedulePrintModelTests::intensiveNoFilteringKeepsAllRows()
{
    const QStringList days =
        visibleScheduleDays(false);
    ScheduleBuildResult result =
        blankResult(
            days,
            {
                QStringLiteral("09:00"),
                QStringLiteral("10:00")
            }
            );
    ScheduleViewRequest request =
        requestFor(days, true);

    for (const ScheduleRow& row : result.rows)
    {
        setRowState(
            request,
            days,
            row.label,
            scheduleEmptySlotState()
            );
    }

    const ScheduleViewModel model =
        buildScheduleViewModel(result, request);

    QCOMPARE(model.rows.size(), 2);
    QCOMPARE(model.rows.first().timeLabel, QStringLiteral("09:00"));
    QCOMPARE(model.rows.last().timeLabel, QStringLiteral("10:00"));
}

void SchedulePrintModelTests
    ::regularSchedulesIgnoreIntensiveTrimming()
{
    const QStringList days =
        visibleScheduleDays(false);
    const ScheduleBuildResult result =
        blankResult(
            days,
            {
                QStringLiteral("15:00"),
                QStringLiteral("16:00")
            }
            );
    ScheduleViewRequest request =
        requestFor(days, false);
    request.rowFilter =
        ScheduleRowFilter::TrimEmptyOuterRows;

    const ScheduleViewModel model =
        buildScheduleViewModel(result, request);

    QCOMPARE(model.rows.size(), 2);
}

void SchedulePrintModelTests::footerTotalsMatchExcelScreenshotConvention()
{
    const QStringList days =
        visibleScheduleDays(false);
    ScheduleBuildResult result =
        blankResult(
            days,
            {
                QStringLiteral("16:00"),
                QStringLiteral("17:00"),
                QStringLiteral("18:00"),
                QStringLiteral("19:00"),
                QStringLiteral("20:00"),
                QStringLiteral("21:00")
            }
            );

    int classId = 1;
    int classCells = 0;

    for (int row = 0; row < result.rows.size(); ++row)
    {
        for (int day = 0; day < days.size(); ++day)
        {
            if (classCells >= 15)
            {
                break;
            }

            result.schedule[days.at(day)][result.rows.at(row).label]
                .append(entry(classId++));
            ++classCells;
        }
    }

    const ScheduleViewModel model =
        buildScheduleViewModel(
            result,
            requestFor(days)
            );

    QCOMPARE(model.summary.essayBlocks, 15);
    QCOMPARE(model.summary.scheduledBlocks, 30);
}

void SchedulePrintModelTests::teacherRoomLineRespectsSelectedNameLanguage()
{
    ScheduleEntry scheduleEntry = entry();
    scheduleEntry.teacherEn = QStringLiteral("Jihye Park");
    scheduleEntry.teacherPreferredName = QStringLiteral("J. Park");

    QCOMPARE(
        scheduleTeacherRoomLine(scheduleEntry, false),
        QStringLiteral("박지혜 413")
        );
    QCOMPARE(
        scheduleTeacherRoomLine(scheduleEntry, true),
        QStringLiteral("J. Park 413")
        );

    scheduleEntry.teacherPreferredName.clear();
    scheduleEntry.teacherEn.clear();
    QCOMPARE(
        scheduleTeacherRoomLine(scheduleEntry, true),
        QStringLiteral("박지혜 413")
        );
}

void SchedulePrintModelTests
    ::testingModeFiltersAffectedGradesAndPreservesPriority()
{
    const QStringList days{
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday")
    };
    ScheduleBuildResult result =
        blankResult(
            days,
            {QStringLiteral("16:00")}
            );

    ScheduleEntry elementary = entry(1);
    elementary.classGrade = QStringLiteral("E5");
    ScheduleEntry m2 = entry(2);
    m2.classGrade = QStringLiteral(" m2 ");
    ScheduleEntry m1 = entry(3);
    m1.classGrade = QStringLiteral("M1");
    ScheduleEntry m3 = entry(4);
    m3.classGrade = QStringLiteral("m3");

    result.schedule[QStringLiteral("Monday")]
        [QStringLiteral("16:00")] = {elementary, m2};
    result.schedule[QStringLiteral("Tuesday")]
        [QStringLiteral("16:00")] = {m2};
    result.schedule[QStringLiteral("Wednesday")]
        [QStringLiteral("16:00")] = {m1};
    result.schedule[QStringLiteral("Thursday")]
        [QStringLiteral("16:00")] = {m3};

    ScheduleViewRequest request;
    request.days = days;
    request.displayMode =
        ScheduleDisplayMode::Testing;
    const auto plainAssignment =
        [](const QString& day, const QString& room)
        {
            TestingAssignmentView view;
            view.assignment.day = day;
            view.assignment.startTime = QStringLiteral("16:00");
            view.assignment.room = room;
            return view;
        };
    request.testingAssignments.insert(
        scheduleSlotKey(
            QStringLiteral("Monday"),
            QStringLiteral("16:00")
            ),
        plainAssignment(
            QStringLiteral("Monday"),
            QStringLiteral("Hidden Room")
            )
        );
    request.testingAssignments.insert(
        scheduleSlotKey(
            QStringLiteral("Tuesday"),
            QStringLiteral("16:00")
            ),
        plainAssignment(
            QStringLiteral("Tuesday"),
            QStringLiteral("402")
            )
        );
    request.testingAssignments.insert(
        scheduleSlotKey(
            QStringLiteral("Wednesday"),
            QStringLiteral("16:00")
            ),
        plainAssignment(
            QStringLiteral("Wednesday"),
            QStringLiteral("Library")
            )
        );

    ScheduleViewModel model =
        buildScheduleViewModel(result, request);

    const ScheduleRowView& row =
        model.rows.first();
    QVERIFY(row.cells.at(0).entries.isEmpty());
    QCOMPARE(
        row.cells.at(0).slotState,
        scheduleTestingSlotState()
        );
    QCOMPARE(
        row.cells.at(0).testingRoom,
        QStringLiteral("Hidden Room")
        );
    QCOMPARE(
        row.cells.at(1).slotState,
        scheduleTestingSlotState()
        );
    QCOMPARE(
        row.cells.at(1).testingRoom,
        QStringLiteral("402")
        );
    QVERIFY(row.cells.at(2).entries.isEmpty());
    QCOMPARE(
        row.cells.at(2).slotState,
        scheduleTestingSlotState()
        );
    QCOMPARE(
        row.cells.at(3).slotState,
        scheduleEssaySlotState()
        );
    QVERIFY(
        row.cells.at(3).testingBlockCreationEnabled
        );
    QCOMPARE(model.summary.testingBlocks, 3);

    request.testingAffectsM1 = true;
    model =
        buildScheduleViewModel(result, request);

    QCOMPARE(
        model.rows.first().cells.at(2).slotState,
        scheduleTestingSlotState()
        );
    QCOMPARE(
        model.rows.first().cells.at(2).testingRoom,
        QStringLiteral("Library")
        );
}

void SchedulePrintModelTests
    ::testingClassAssignmentsOverrideUnderlyingCells()
{
    ScheduleBuildResult result =
        blankResult(
            {QStringLiteral("Monday")},
            {QStringLiteral("16:00")}
            );
    result.schedule[QStringLiteral("Monday")]
        [QStringLiteral("16:00")] = {entry(7)};

    ScheduleEntry special;
    special.kind = ScheduleEntryKind::TestingClass;
    special.classId = 91;
    special.className = QStringLiteral("Writing Lab");
    special.classGrade = QStringLiteral("M2");
    special.classLevel = QStringLiteral("Mixed (All)");
    special.teacherEn = QStringLiteral("Ms Han");
    special.roomNumber = QStringLiteral("Library");
    special.classColor = QStringLiteral("#123456");
    special.fontColor = QStringLiteral("#FFFFFF");

    TestingAssignmentView assignment;
    assignment.assignment.day = QStringLiteral("Monday");
    assignment.assignment.startTime = QStringLiteral("16:00");
    assignment.assignment.kind =
        TestingAssignmentKind::SpecialClass;
    assignment.assignment.classId = special.classId;
    assignment.testingClassEntry = special;

    ScheduleViewRequest request;
    request.days = {QStringLiteral("Monday")};
    request.displayMode = ScheduleDisplayMode::Testing;
    request.testingAssignments.insert(
        scheduleSlotKey(
            QStringLiteral("Monday"),
            QStringLiteral("16:00")
            ),
        assignment
        );

    const ScheduleViewModel testingModel =
        buildScheduleViewModel(result, request);
    const ScheduleCellView& testingCell =
        testingModel.rows.first().cells.first();
    QVERIFY(testingCell.testingClassAssignment);
    QCOMPARE(testingCell.testingClassId, 91);
    QCOMPARE(testingCell.entries.size(), 1);
    QCOMPARE(
        testingCell.entries.first().className,
        QStringLiteral("Writing Lab")
        );
    QCOMPARE(testingModel.summary.testingClassBlocks, 1);
    QCOMPARE(testingModel.summary.testingBlocks, 0);
    QCOMPARE(testingModel.summary.scheduledBlocks, 1);

    request.testingAssignments.clear();
    const ScheduleViewModel restoredModel =
        buildScheduleViewModel(result, request);
    const ScheduleCellView& restoredCell =
        restoredModel.rows.first().cells.first();
    QVERIFY(!restoredCell.testingClassAssignment);
    QCOMPARE(restoredCell.entries.size(), 1);
    QCOMPARE(restoredCell.entries.first().classId, 7);

    request.testingAssignments.insert(
        scheduleSlotKey(
            QStringLiteral("Monday"),
            QStringLiteral("16:00")
            ),
        assignment
        );
    request.displayMode = ScheduleDisplayMode::Regular;
    const ScheduleViewModel regularModel =
        buildScheduleViewModel(result, request);
    QCOMPARE(
        regularModel.rows.first().cells.first().entries.first().classId,
        7
        );
}

QTEST_MAIN(SchedulePrintModelTests)

#include "schedule_print_model_tests.moc"
