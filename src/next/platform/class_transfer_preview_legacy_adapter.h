#pragma once

#include "domain/models/class_transfer.h"
#include "next/application/class_transfer_preview_query.h"

#include <QString>

#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

[[nodiscard]] inline Application::ClassTransferPreviewRequest
classTransferPreviewRequest(const ::ClassTransferPackage& package)
{
    Application::ClassTransferPreviewRequest request;
    request.teachers.reserve(static_cast<std::size_t>(package.teachers.size()));
    for (const ::ClassTransferTeacher& teacher : package.teachers)
    {
        request.teachers.push_back({
            Application::ClassTransferTeacherKey(teacher.key.toStdString()),
            {
                QString(teacher.teacher.teacherEn)
                    .simplified().toCaseFolded().toStdString(),
                QString(teacher.teacher.teacherKr)
                    .simplified().toCaseFolded().toStdString()
            }
        });
    }

    request.classes.reserve(static_cast<std::size_t>(package.classes.size()));
    for (int index = 0; index < package.classes.size(); ++index)
    {
        const ::ClassTransferClass& transferClass = package.classes.at(index);
        request.classes.push_back({
            Application::ClassTransferPackageClassIndex{index},
            Application::ClassTransferTeacherKey(
                transferClass.teacherKey.toStdString()),
            transferClass.info.classGrade.simplified()
                .toCaseFolded().toStdString(),
            transferClass.info.classLevel.simplified()
                .toCaseFolded().toStdString(),
            Application::ClassTransferClassKey(transferClass.key.toStdString())
        });
    }

    return request;
}

[[nodiscard]] inline ::ClassImportPreview legacyClassTransferPreview(
    const Application::ClassTransferPreviewResult& result
    )
{
    ::ClassImportPreview preview;
    preview.teachers.reserve(static_cast<qsizetype>(result.teachers.size()));
    for (const auto& candidate : result.teachers)
    {
        ::ClassImportTeacherPreview teacher;
        const std::string& key = candidate.teacherKey.value();
        teacher.teacherKey = QString::fromUtf8(key.data(),
            static_cast<qsizetype>(key.size()));
        teacher.matchingTeacherIds.reserve(
            static_cast<qsizetype>(candidate.matchingTeacherIds.size()));
        for (const auto& id : candidate.matchingTeacherIds)
        {
            bool valid = false;
            const qint64 legacyId = QString::fromStdString(id.value())
                .toLongLong(&valid);
            if (valid)
            {
                teacher.matchingTeacherIds.append(legacyId);
            }
        }
        preview.teachers.append(std::move(teacher));
    }

    preview.classes.reserve(static_cast<qsizetype>(result.classes.size()));
    for (const auto& candidate : result.classes)
    {
        ::ClassImportClassPreview classCandidate;
        classCandidate.packageClassIndex =
            candidate.packageClassIndex.rawValue();
        classCandidate.matchingClassIds.reserve(
            static_cast<qsizetype>(candidate.matchingClassIds.size()));
        for (const auto& id : candidate.matchingClassIds)
        {
            bool valid = false;
            const int legacyId = QString::fromStdString(id.value())
                .toInt(&valid);
            if (valid)
            {
                classCandidate.matchingClassIds.append(legacyId);
            }
        }
        preview.classes.append(std::move(classCandidate));
    }

    return preview;
}

} // namespace ClassMngr::Next::Platform
