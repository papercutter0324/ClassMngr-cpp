#pragma once

#include "domain/models/class_info.h"
#include "domain/models/roster.h"
#include "domain/models/speaking_evaluation.h"
#include "domain/models/teacher.h"

#include <QDateTime>
#include <QList>
#include <QString>

struct ClassTransferEvaluation
{
    QString name;
    SpeakingEvalRows rows;
};

struct ClassTransferTeacher
{
    QString key;
    Teacher teacher;
};

struct ClassTransferClass
{
    QString key;
    QString name;
    QString teacherKey;
    ClassInfo info;
    Roster roster;
    QList<ClassTransferEvaluation> evaluations;
};

struct ClassTransferPackage
{
    static constexpr int CurrentVersion = 1;

    int version = CurrentVersion;
    QDateTime exportedAtUtc;
    QList<ClassTransferTeacher> teachers;
    QList<ClassTransferClass> classes;
};

struct ClassImportTeacherPreview
{
    QString teacherKey;
    QList<int> matchingTeacherIds;
};

struct ClassImportClassPreview
{
    int packageClassIndex = -1;
    QList<int> matchingClassIds;
};

struct ClassImportPreview
{
    QList<ClassImportTeacherPreview> teachers;
    QList<ClassImportClassPreview> classes;
};

enum class ClassImportAction
{
    Create,
    Replace,
    Skip
};

struct ClassImportResolution
{
    int packageClassIndex = -1;
    ClassImportAction action = ClassImportAction::Create;
    int targetClassId = -1;
};

enum class TeacherImportAction
{
    Create,
    KeepExisting,
    ReplaceExisting
};

struct TeacherImportResolution
{
    QString teacherKey;
    TeacherImportAction action = TeacherImportAction::Create;
    int targetTeacherId = -1;
};

struct ClassImportPlan
{
    QList<ClassImportResolution> classes;
    QList<TeacherImportResolution> teachers;
};

struct ClassImportSummary
{
    QList<int> createdClassIds;
    QList<int> replacedClassIds;
    int skippedClassCount = 0;
};
