#pragma once

#include "classmngr/engine/class_info.h"
#include "classmngr/engine/roster.h"
#include "classmngr/engine/teacher.h"

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

namespace classmngr::engine
{

inline constexpr std::size_t SpeakingEvaluationRowCount = 25;
inline constexpr std::size_t SpeakingEvaluationColumnCount = 11;

using SpeakingEvaluationRows = std::vector<std::vector<std::string>>;

struct ClassTransferEvaluation
{
    std::string name;
    SpeakingEvaluationRows rows;
};

struct ClassTransferTeacher
{
    std::string key;
    Teacher teacher;
};

struct ClassTransferClass
{
    std::string key;
    std::string name;
    std::string teacherKey;
    ClassInfo info;
    Roster roster;
    std::vector<ClassTransferEvaluation> evaluations;
};

struct ClassTransferPackage
{
    static constexpr int CurrentVersion = 1;

    int version = CurrentVersion;
    std::chrono::system_clock::time_point exportedAtUtc{};
    std::vector<ClassTransferTeacher> teachers;
    std::vector<ClassTransferClass> classes;
};

struct ClassImportTeacherPreview
{
    std::string teacherKey;
    std::vector<int> matchingTeacherIds;
};

struct ClassImportClassPreview
{
    int packageClassIndex = -1;
    std::vector<int> matchingClassIds;
};

struct ClassImportPreview
{
    std::vector<ClassImportTeacherPreview> teachers;
    std::vector<ClassImportClassPreview> classes;
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
    std::string teacherKey;
    TeacherImportAction action = TeacherImportAction::Create;
    int targetTeacherId = -1;
};

struct ClassImportPlan
{
    std::vector<ClassImportResolution> classes;
    std::vector<TeacherImportResolution> teachers;
};

struct ClassImportSummary
{
    std::vector<int> createdClassIds;
    std::vector<int> replacedClassIds;
    int skippedClassCount = 0;
};

} // namespace classmngr::engine
