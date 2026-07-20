#pragma once

#include "domain/models/teacher_import.h"

#include <QDate>
#include <QString>
#include <QStringList>

class TeacherImportTemplateRegistry;

enum class TeacherImportFileStatus
{
    Valid,
    Unreadable,
    UnsupportedTemplate,
    AmbiguousTemplate,
    RecognizedButInvalid
};

struct TeacherImportPreviewCounts
{
    int koreanTeachers = 0;
    int nativeEnglishTeachers = 0;
    int gsTeamMembers = 0;

    [[nodiscard]] int total() const
    {
        return koreanTeachers + nativeEnglishTeachers + gsTeamMembers;
    }
};

struct TeacherImportFileValidation
{
    TeacherImportFileStatus status = TeacherImportFileStatus::Unreadable;
    QString templateId;
    QString templateName;
    QDate sourceDate;
    QStringList discoveredSections;
    TeacherImportPreviewCounts previewCounts;
    QStringList diagnostics;
    TeacherImportPreview preview;

    [[nodiscard]] bool isValid() const
    {
        return status == TeacherImportFileStatus::Valid;
    }
};

[[nodiscard]] TeacherImportFileValidation validateTeacherImportData(
    const QByteArray& data,
    const TeacherImportTemplateRegistry& registry
    );

[[nodiscard]] TeacherImportFileValidation validateTeacherImportFile(
    const QString& filePath
    );
