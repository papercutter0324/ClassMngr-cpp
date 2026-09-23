#pragma once

#include <QList>
#include <QString>

struct SubPrepScheduleMeetingRecord final
{
    QString day;
    QString startTime;
};

// A bounded ClassInfoRepository result for one visible Sub Prep class.
// Teacher facts are copied from the joined teacher row and meetings include
// only the requested mode and weekdays.
struct SubPrepClassSummaryRecord final
{
    int classId = -1;
    int teacherId = -1;
    QString classGrade;
    QString classLevel;
    QString teacherKr;
    QString teacherEn;
    QString teacherPreferredRomanization;
    QString teacherPreferredName;
    QString teacherRoomNumber;
    QString teacherWifiName;
    QString teacherWifiPassword;
    QString teacherInternetType;
    QString teacherZoomId;
    QString teacherZoomPassword;
    QString teacherProjectionType;
    QString teacherNotes;
    qint64 studentCount = 0;
    QList<SubPrepScheduleMeetingRecord> meetings;
};
