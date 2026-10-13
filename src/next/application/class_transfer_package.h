#pragma once

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{
// Full, unbounded package data shared by import apply and export. UTF-16
// preserves every stored QString code unit. Local database identity and
// teacher-derived class fields are absent from the portable package format.
using ClassExportText = std::u16string;
using ClassExportRows = std::vector<std::vector<ClassExportText>>;
struct ClassExportTime { ClassExportText day, startTime, endTime; };
struct ClassExportInfo
{
    ClassExportText classGrade, classLevel, readingBook, essayBook, classColor, fontColor;
    ClassExportText notes, timeFillerActivities;
    std::vector<ClassExportTime> classTimes, intensiveTimes;
};
struct ClassExportRoster
{
    std::vector<ClassExportText> columns;
    std::vector<int> columnWidths;
    ClassExportRows rows;
};
struct ClassExportEvaluation { ClassExportText name; ClassExportRows rows; };
struct ClassExportTeacherProfile
{
    ClassExportText teacherKr, teacherEn, preferredRomanization, preferredName;
    ClassExportText roomNumber, birthday, phoneNumber, wifiName, wifiPassword, internetType;
    ClassExportText zoomId, zoomPassword, projectionType, notes;
};
struct ClassExportTeacher { ClassExportText key; ClassExportTeacherProfile teacher; };
struct ClassExportClass
{
    ClassExportText key, name, teacherKey;
    ClassExportInfo info;
    ClassExportRoster roster;
    std::vector<ClassExportEvaluation> evaluations;
};
struct ClassTransferPortablePackage
{
    int version = 1;
    std::string exportedAtUtc;
    std::vector<ClassExportTeacher> teachers;
    std::vector<ClassExportClass> classes;
};

using ClassTransferExportPackage = ClassTransferPortablePackage;

} // namespace ClassMngr::Next::Application
