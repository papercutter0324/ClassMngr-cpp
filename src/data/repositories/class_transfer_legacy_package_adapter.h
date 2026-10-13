#pragma once

#include "domain/models/class_transfer.h"
#include "next/application/class_transfer_package.h"

#include <utility>

namespace ClassMngr::Data::Legacy
{
namespace ClassTransferPackageDetail
{
namespace Application = ClassMngr::Next::Application;

inline Application::ClassExportRows portableRows(const QList<QStringList>& rows)
{
    Application::ClassExportRows result;
    result.reserve(rows.size());
    for (const auto& row : rows)
    {
        std::vector<Application::ClassExportText> cells;
        cells.reserve(row.size());
        for (const auto& cell : row) cells.push_back(cell.toStdU16String());
        result.push_back(std::move(cells));
    }
    return result;
}

inline QList<QStringList> legacyRows(const Application::ClassExportRows& rows)
{
    QList<QStringList> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
    {
        QStringList cells;
        cells.reserve(row.size());
        for (const auto& cell : row) cells.append(QString::fromStdU16String(cell));
        result.append(std::move(cells));
    }
    return result;
}

inline std::vector<Application::ClassExportTime> portableTimes(const QList<ClassTime>& times)
{
    std::vector<Application::ClassExportTime> result;
    result.reserve(times.size());
    for (const auto& time : times)
        result.push_back({time.day.toStdU16String(), time.startTime.toStdU16String(),
            time.endTime.toStdU16String()});
    return result;
}

inline QList<ClassTime> legacyTimes(const std::vector<Application::ClassExportTime>& times)
{
    QList<ClassTime> result;
    result.reserve(times.size());
    for (const auto& time : times)
        result.append({QString::fromStdU16String(time.day), QString::fromStdU16String(time.startTime),
            QString::fromStdU16String(time.endTime)});
    return result;
}
} // namespace ClassTransferPackageDetail

// These conversions perform no validation or normalization. The UI supplies a
// codec-parsed package; the Platform edge retains the existing validator.
[[nodiscard]] inline ClassMngr::Next::Application::ClassTransferPortablePackage
toPortableClassTransferPackage(::ClassTransferPackage package)
{
    using namespace ClassTransferPackageDetail;
    Application::ClassTransferPortablePackage result;
    result.version = package.version;
    result.exportedAtUtc = package.exportedAtUtc.toUTC().toString(Qt::ISODateWithMs).toStdString();
    result.teachers.reserve(package.teachers.size());
    for (const auto& source : package.teachers)
    {
        Application::ClassExportTeacher output;
        output.key = source.key.toStdU16String();
        output.teacher.teacherKr = source.teacher.teacherKr.toStdU16String();
        output.teacher.teacherEn = source.teacher.teacherEn.toStdU16String();
        output.teacher.preferredRomanization = source.teacher.preferredRomanization.toStdU16String();
        output.teacher.preferredName = source.teacher.preferredName.toStdU16String();
        output.teacher.roomNumber = source.teacher.roomNumber.toStdU16String();
        output.teacher.birthday = source.teacher.birthday.toStdU16String();
        output.teacher.phoneNumber = source.teacher.phoneNumber.toStdU16String();
        output.teacher.wifiName = source.teacher.wifiName.toStdU16String();
        output.teacher.wifiPassword = source.teacher.wifiPassword.toStdU16String();
        output.teacher.internetType = source.teacher.internetType.toStdU16String();
        output.teacher.zoomId = source.teacher.zoomId.toStdU16String();
        output.teacher.zoomPassword = source.teacher.zoomPassword.toStdU16String();
        output.teacher.projectionType = source.teacher.projectionType.toStdU16String();
        output.teacher.notes = source.teacher.notes.toStdU16String();
        result.teachers.push_back(std::move(output));
    }
    result.classes.reserve(package.classes.size());
    for (const auto& source : package.classes)
    {
        Application::ClassExportClass output;
        output.key = source.key.toStdU16String();
        output.name = source.name.toStdU16String();
        output.teacherKey = source.teacherKey.toStdU16String();
        output.info.classGrade = source.info.classGrade.toStdU16String();
        output.info.classLevel = source.info.classLevel.toStdU16String();
        output.info.readingBook = source.info.readingBook.toStdU16String();
        output.info.essayBook = source.info.essayBook.toStdU16String();
        output.info.classColor = source.info.classColor.toStdU16String();
        output.info.fontColor = source.info.fontColor.toStdU16String();
        output.info.notes = source.info.notes.toStdU16String();
        output.info.timeFillerActivities = source.info.timeFillerActivities.toStdU16String();
        output.info.classTimes = portableTimes(source.info.classTimes);
        output.info.intensiveTimes = portableTimes(source.info.intensiveTimes);
        output.roster.columns.reserve(source.roster.columns.size());
        for (const auto& column : source.roster.columns)
            output.roster.columns.push_back(column.toStdU16String());
        output.roster.columnWidths.assign(source.roster.columnWidths.begin(), source.roster.columnWidths.end());
        output.roster.rows = portableRows(source.roster.rows);
        output.evaluations.reserve(source.evaluations.size());
        for (const auto& evaluation : source.evaluations)
            output.evaluations.push_back({evaluation.name.toStdU16String(), portableRows(evaluation.rows)});
        result.classes.push_back(std::move(output));
    }
    return result;
}

[[nodiscard]] inline ::ClassTransferPackage toLegacyClassTransferPackage(
    ClassMngr::Next::Application::ClassTransferPortablePackage package)
{
    using namespace ClassTransferPackageDetail;
    ::ClassTransferPackage result;
    result.version = package.version;
    result.exportedAtUtc = QDateTime::fromString(
        QString::fromStdString(package.exportedAtUtc), Qt::ISODateWithMs).toUTC();
    result.teachers.reserve(package.teachers.size());
    for (const auto& source : package.teachers)
    {
        ::ClassTransferTeacher output;
        output.key = QString::fromStdU16String(source.key);
        output.teacher.teacherKr = QString::fromStdU16String(source.teacher.teacherKr);
        output.teacher.teacherEn = QString::fromStdU16String(source.teacher.teacherEn);
        output.teacher.preferredRomanization = QString::fromStdU16String(source.teacher.preferredRomanization);
        output.teacher.preferredName = QString::fromStdU16String(source.teacher.preferredName);
        output.teacher.roomNumber = QString::fromStdU16String(source.teacher.roomNumber);
        output.teacher.birthday = QString::fromStdU16String(source.teacher.birthday);
        output.teacher.phoneNumber = QString::fromStdU16String(source.teacher.phoneNumber);
        output.teacher.wifiName = QString::fromStdU16String(source.teacher.wifiName);
        output.teacher.wifiPassword = QString::fromStdU16String(source.teacher.wifiPassword);
        output.teacher.internetType = QString::fromStdU16String(source.teacher.internetType);
        output.teacher.zoomId = QString::fromStdU16String(source.teacher.zoomId);
        output.teacher.zoomPassword = QString::fromStdU16String(source.teacher.zoomPassword);
        output.teacher.projectionType = QString::fromStdU16String(source.teacher.projectionType);
        output.teacher.notes = QString::fromStdU16String(source.teacher.notes);
        result.teachers.append(std::move(output));
    }
    result.classes.reserve(package.classes.size());
    for (const auto& source : package.classes)
    {
        ::ClassTransferClass output;
        output.key = QString::fromStdU16String(source.key);
        output.name = QString::fromStdU16String(source.name);
        output.teacherKey = QString::fromStdU16String(source.teacherKey);
        output.info.classGrade = QString::fromStdU16String(source.info.classGrade);
        output.info.classLevel = QString::fromStdU16String(source.info.classLevel);
        output.info.readingBook = QString::fromStdU16String(source.info.readingBook);
        output.info.essayBook = QString::fromStdU16String(source.info.essayBook);
        output.info.classColor = QString::fromStdU16String(source.info.classColor);
        output.info.fontColor = QString::fromStdU16String(source.info.fontColor);
        output.info.notes = QString::fromStdU16String(source.info.notes);
        output.info.timeFillerActivities = QString::fromStdU16String(source.info.timeFillerActivities);
        output.info.classTimes = legacyTimes(source.info.classTimes);
        output.info.intensiveTimes = legacyTimes(source.info.intensiveTimes);
        output.roster.columns.reserve(source.roster.columns.size());
        for (const auto& column : source.roster.columns)
            output.roster.columns.append(QString::fromStdU16String(column));
        output.roster.columnWidths.assign(source.roster.columnWidths.begin(), source.roster.columnWidths.end());
        output.roster.rows = legacyRows(source.roster.rows);
        output.evaluations.reserve(source.evaluations.size());
        for (const auto& evaluation : source.evaluations)
            output.evaluations.append({QString::fromStdU16String(evaluation.name), legacyRows(evaluation.rows)});
        result.classes.append(std::move(output));
    }
    return result;
}

} // namespace ClassMngr::Data::Legacy
