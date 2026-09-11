#include "classmngr/engine/sub_prep_document.h"

#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace
{
using namespace classmngr::engine;

bool expect(bool condition, std::string_view message)
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineSubPrepDocumentTests: "
              << message
              << '\n';
    return false;
}

bool same(
    const ClassTime& left,
    const ClassTime& right
    )
{
    return left.day == right.day
        && left.startTime == right.startTime
        && left.endTime == right.endTime;
}

bool same(
    const Teacher& left,
    const Teacher& right
    )
{
    return left.id == right.id
        && left.teacherKr == right.teacherKr
        && left.teacherEn == right.teacherEn
        && left.preferredRomanization == right.preferredRomanization
        && left.preferredName == right.preferredName
        && left.roomNumber == right.roomNumber
        && left.birthday == right.birthday
        && left.phoneNumber == right.phoneNumber
        && left.wifiName == right.wifiName
        && left.wifiPassword == right.wifiPassword
        && left.internetType == right.internetType
        && left.zoomId == right.zoomId
        && left.zoomPassword == right.zoomPassword
        && left.projectionType == right.projectionType
        && left.notes == right.notes;
}

bool same(
    const ClassInfo& left,
    const ClassInfo& right
    )
{
    if (left.classId != right.classId
        || left.teacherId != right.teacherId
        || left.teacherKr != right.teacherKr
        || left.teacherEn != right.teacherEn
        || left.teacherPreferredName != right.teacherPreferredName
        || left.roomNumber != right.roomNumber
        || left.wifiName != right.wifiName
        || left.wifiPassword != right.wifiPassword
        || left.internetType != right.internetType
        || left.zoomId != right.zoomId
        || left.zoomPassword != right.zoomPassword
        || left.projectionType != right.projectionType
        || left.classGrade != right.classGrade
        || left.classLevel != right.classLevel
        || left.readingBook != right.readingBook
        || left.essayBook != right.essayBook
        || left.classColor != right.classColor
        || left.fontColor != right.fontColor
        || left.notes != right.notes
        || left.timeFillerActivities != right.timeFillerActivities
        || left.classTimes.size() != right.classTimes.size()
        || left.intensiveTimes.size() != right.intensiveTimes.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < left.classTimes.size(); ++index)
    {
        if (!same(left.classTimes[index], right.classTimes[index]))
        {
            return false;
        }
    }
    for (std::size_t index = 0; index < left.intensiveTimes.size(); ++index)
    {
        if (!same(left.intensiveTimes[index], right.intensiveTimes[index]))
        {
            return false;
        }
    }
    return true;
}

bool same(
    const SubPrepClassDetails& left,
    const SubPrepClassDetails& right
    )
{
    return left.classId == right.classId
        && same(left.info, right.info)
        && left.studentCount == right.studentCount
        && left.classLabel == right.classLabel
        && left.timeText == right.timeText;
}

bool same(
    const SubPrepTeacherGroup& left,
    const SubPrepTeacherGroup& right
    )
{
    if (!same(left.teacher, right.teacher)
        || left.displayName != right.displayName
        || left.classListText != right.classListText
        || left.classes.size() != right.classes.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < left.classes.size(); ++index)
    {
        if (!same(left.classes[index], right.classes[index]))
        {
            return false;
        }
    }
    return true;
}

bool same(
    const ScheduleReportEntry& left,
    const ScheduleReportEntry& right
    )
{
    return left.classId == right.classId
        && left.kind == right.kind
        && left.className == right.className
        && left.teacherKr == right.teacherKr
        && left.teacherEn == right.teacherEn
        && left.teacherPreferredName == right.teacherPreferredName
        && left.roomNumber == right.roomNumber
        && left.classGrade == right.classGrade
        && left.classLevel == right.classLevel
        && left.classColor == right.classColor
        && left.fontColor == right.fontColor;
}

bool same(
    const ScheduleReportCell& left,
    const ScheduleReportCell& right
    )
{
    if (left.day != right.day
        || left.timeLabel != right.timeLabel
        || left.defaultSlotState != right.defaultSlotState
        || left.slotState != right.slotState
        || left.testingRoom != right.testingRoom
        || left.testingClassAssignment != right.testingClassAssignment
        || left.testingClassId != right.testingClassId
        || left.slotTogglingEnabled != right.slotTogglingEnabled
        || left.testingBlockCreationEnabled
            != right.testingBlockCreationEnabled
        || left.entries.size() != right.entries.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < left.entries.size(); ++index)
    {
        if (!same(left.entries[index], right.entries[index]))
        {
            return false;
        }
    }
    return true;
}

bool same(
    const ScheduleReportRowView& left,
    const ScheduleReportRowView& right
    )
{
    if (left.timeLabel != right.timeLabel
        || left.timeRangeLabel != right.timeRangeLabel
        || left.maxEntryCount != right.maxEntryCount
        || left.cells.size() != right.cells.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < left.cells.size(); ++index)
    {
        if (!same(left.cells[index], right.cells[index]))
        {
            return false;
        }
    }
    return true;
}

bool same(
    const ScheduleReportModel& left,
    const ScheduleReportModel& right
    )
{
    if (left.days != right.days
        || left.uses55Endings != right.uses55Endings
        || left.summary.essayBlocks != right.summary.essayBlocks
        || left.summary.testingBlocks != right.summary.testingBlocks
        || left.summary.testingClassBlocks
            != right.summary.testingClassBlocks
        || left.summary.scheduledBlocks != right.summary.scheduledBlocks
        || left.rows.size() != right.rows.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < left.rows.size(); ++index)
    {
        if (!same(left.rows[index], right.rows[index]))
        {
            return false;
        }
    }
    return true;
}

ScheduleReportEntry scheduleEntry(
    int classId,
    ScheduleReportEntryKind kind,
    std::string className
    )
{
    ScheduleReportEntry entry;
    entry.classId = classId;
    entry.kind = kind;
    entry.className = std::move(className);
    entry.teacherKr = "ê¹ì ì";
    entry.teacherEn = "Teacher En";
    entry.teacherPreferredName = "Preferred Teacher";
    entry.roomNumber = "Room 101";
    entry.classGrade = "E6";
    entry.classLevel = "ìì¤ 6";
    entry.classColor = "#123456";
    entry.fontColor = "#FEDCBA";
    return entry;
}

ClassInfo classInfo()
{
    ClassInfo info;
    info.classId = 901;
    info.teacherId = 77;
    info.teacherKr = "ê¹ì ì";
    info.teacherEn = "Teacher En";
    info.teacherPreferredName = "Preferred Teacher";
    info.roomNumber = "Room 101";
    info.wifiName = "Wi-Fi ê°ë¨";
    info.wifiPassword = "wíssword";
    info.internetType = "Ethernet";
    info.zoomId = "zoom-ç¨æ·";
    info.zoomPassword = "å¯ç ";
    info.projectionType = "USB-C";
    info.classGrade = "E6";
    info.classLevel = "ìì¤ 6";
    info.readingBook = "Reading ææ";
    info.essayBook = "Essay ìë£";
    info.classColor = "#123456";
    info.fontColor = "#FEDCBA";
    info.classTimes = {
        {"Monday", "4:00 PM", "4:50 PM"},
        {"ììì¼", "5:00 PM", "5:55 PM"}
    };
    info.intensiveTimes = {
        {"Friday", "10:00 AM", "10:55 AM"},
        {"Friday", "11:00 AM", "11:55 AM"}
    };
    info.notes = "Class notes â bilingual";
    info.timeFillerActivities = "íì¤í¸ activity";
    return info;
}

Teacher teacher()
{
    Teacher result;
    result.id = 77;
    result.teacherKr = "ê¹ì ì";
    result.teacherEn = "Teacher En";
    result.preferredRomanization = "Kim Seonsaeng";
    result.preferredName = "Kim ì ì";
    result.roomNumber = "Room 101";
    result.birthday = "1980-01-02";
    result.phoneNumber = "+82-10-1234-5678";
    result.wifiName = "Teacher Wi-Fi";
    result.wifiPassword = "teacher-password";
    result.internetType = "Ethernet";
    result.zoomId = "teacher-zoom";
    result.zoomPassword = "teacher-å¯ç ";
    result.projectionType = "USB-C";
    result.notes = "Teacher notes â bilingual";
    return result;
}

SubPrepTeacherGroup teacherGroup(
    std::string displayName,
    std::string classListText
    )
{
    SubPrepClassDetails firstClass;
    firstClass.classId = 901;
    firstClass.info = classInfo();
    firstClass.studentCount = 18;
    firstClass.classLabel = "E6 ìì¤ 6";
    firstClass.timeText = "Mon 4pm & ì 5pm";

    SubPrepClassDetails secondClass = firstClass;
    secondClass.classId = 902;
    secondClass.studentCount = 19;
    secondClass.classLabel = "E7 Advanced";
    secondClass.timeText = "Fri 10am";

    SubPrepTeacherGroup result;
    result.teacher = teacher();
    result.displayName = std::move(displayName);
    result.classListText = std::move(classListText);
    result.classes = {firstClass, secondClass, firstClass};
    return result;
}
} // namespace

int main()
{
    SubPrepDocumentRequest request;
    request.campus.officeNumber = "ìì¸ Office 7";
    request.campus.officeWifi = "Campus ìì´íì´";
    request.campus.officeWifiPassword = "ç§ë° password";
    request.campus.photocopierCode = "ë³µì¬-42";
    request.zoom.loginId = "æì¬@example.com";
    request.zoom.password = "å¹³å¹³å¯ç ";
    request.classMaterials = "ìì materials / bilingual";
    request.gradingInstructions = "ì±ì  instructions";
    request.specialInstructions = "Special ì£¼ì: preserve order";
    request.subNotes = "Sub notes â ì¤ë³µ and order";

    ScheduleReportEntry firstEntry = scheduleEntry(
        11,
        ScheduleReportEntryKind::RegularClass,
        "ìì A"
        );
    ScheduleReportEntry secondEntry = scheduleEntry(
        22,
        ScheduleReportEntryKind::TestingClass,
        "Testing B"
        );

    ScheduleReportCell firstCell;
    firstCell.day = "ììì¼";
    firstCell.timeLabel = "4:00 PM";
    firstCell.defaultSlotState = "essay";
    firstCell.slotState = "testing";
    firstCell.testingRoom = "Room ë°ë¤";
    firstCell.testingClassAssignment = true;
    firstCell.testingClassId = 22;
    firstCell.slotTogglingEnabled = true;
    firstCell.testingBlockCreationEnabled = false;
    firstCell.entries = {firstEntry, secondEntry, firstEntry};

    ScheduleReportCell secondCell = firstCell;
    secondCell.day = "Friday";
    secondCell.timeLabel = "5:00 PM";
    secondCell.defaultSlotState = "lunch";
    secondCell.slotState = "empty";
    secondCell.testingRoom = "Room 202";
    secondCell.testingClassAssignment = false;
    secondCell.testingClassId = -1;
    secondCell.slotTogglingEnabled = false;
    secondCell.testingBlockCreationEnabled = true;
    secondCell.entries = {secondEntry};

    ScheduleReportRowView firstRow;
    firstRow.timeLabel = "4:00 PM";
    firstRow.timeRangeLabel = "4:00 - 4:50 PM";
    firstRow.maxEntryCount = 3;
    firstRow.cells = {firstCell, secondCell, firstCell};

    ScheduleReportRowView secondRow = firstRow;
    secondRow.timeLabel = "5:00 PM";
    secondRow.timeRangeLabel = "5:00 - 5:55 PM";
    secondRow.maxEntryCount = 1;
    secondRow.cells = {secondCell};

    request.schedule.days = {"Friday", "Monday", "Friday"};
    request.schedule.rows = {firstRow, secondRow, firstRow};
    request.schedule.uses55Endings = true;
    request.schedule.summary.essayBlocks = 12;
    request.schedule.summary.testingBlocks = 7;
    request.schedule.summary.testingClassBlocks = 5;
    request.schedule.summary.scheduledBlocks = 19;

    request.classInformation = {
        teacherGroup("First ê·¸ë£¹â", "E6 / E7"),
        teacherGroup("Second ê·¸ë£¹", "Duplicate candidate"),
        teacherGroup("First ê·¸ë£¹â", "E6 / E7")
    };

    const SubPrepDocument result = SubPrepDocumentService::build(request);

    bool passed = true;
    passed &= expect(
        result.campus.officeNumber == request.campus.officeNumber
            && result.campus.officeWifi == request.campus.officeWifi
            && result.campus.officeWifiPassword
                == request.campus.officeWifiPassword
            && result.campus.photocopierCode
                == request.campus.photocopierCode,
        "campus fields were not preserved"
        );
    passed &= expect(
        result.zoom.loginId == request.zoom.loginId
            && result.zoom.password == request.zoom.password
            && result.classMaterials == request.classMaterials
            && result.gradingInstructions == request.gradingInstructions
            && result.specialInstructions == request.specialInstructions
            && result.subNotes == request.subNotes,
        "document text fields were not preserved"
        );
    passed &= expect(
        same(result.schedule, request.schedule),
        "schedule rows, cells, entries, or summary flags changed"
        );
    passed &= expect(
        result.classInformation.size() == request.classInformation.size(),
        "teacher-group count changed"
        );
    if (result.classInformation.size() == request.classInformation.size())
    {
        for (std::size_t index = 0; index < request.classInformation.size();
             ++index)
        {
            passed &= expect(
                same(result.classInformation[index], request.classInformation[index]),
                "teacher-group or nested class information changed"
                );
        }
    }

    return passed ? 0 : 1;
}
