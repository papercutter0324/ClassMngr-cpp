#pragma once

#include <QString>

// A compact record for the legacy-backed Sub Prep details read. A nonpositive
// teacherId means that no teacher row is assigned or resolvable.
struct SubPrepClassDetailsRecord final
{
    int classId = -1;
    int teacherId = -1;
    QString classNotes;
    QString teacherKr;
    QString teacherEn;
    QString teacherPreferredRomanization;
    QString teacherPreferredName;
    QString roomNumber;
    QString wifiName;
    QString wifiPassword;
    QString internetType;
    QString zoomId;
    QString zoomPassword;
    QString projectionType;
    QString teacherNotes;
};
