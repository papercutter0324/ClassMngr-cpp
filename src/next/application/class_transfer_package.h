#pragma once

#include "next/application/class_transfer_export_keys.h"

#include <string>
#include <utility>
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
struct ClassExportTeacher { ClassExportTeacherKey key; ClassExportTeacherProfile teacher; };
struct ClassExportClass
{
    ClassExportClassKey key;
    ClassExportText name;
    ClassExportTeacherKey teacherKey;
    ClassExportInfo info;
    ClassExportRoster roster;
    std::vector<ClassExportEvaluation> evaluations;
};

// Canonical serialized UTC timestamp text for exported packages. This owns
// exact text and deliberately does not parse or normalize it.
class ClassTransferExportedAtUtc final
{
public:
    ClassTransferExportedAtUtc() = default;

    explicit ClassTransferExportedAtUtc(std::string value)
        : m_value(std::move(value))
    {
    }

    [[nodiscard]] const std::string& value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(
        const ClassTransferExportedAtUtc&,
        const ClassTransferExportedAtUtc&
        ) = default;

private:
    std::string m_value;
};

struct ClassTransferPortablePackage
{
    int version = 1;
    ClassTransferExportedAtUtc exportedAtUtc;
    std::vector<ClassExportTeacher> teachers;
    std::vector<ClassExportClass> classes;
};

} // namespace ClassMngr::Next::Application
