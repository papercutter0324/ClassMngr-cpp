#pragma once

#include "classmngr/engine/class_info.h"

#include <string>
#include <vector>

namespace classmngr::engine
{

enum class ScheduleImportKind
{
    Normal,
    Intensive
};

enum class ScheduleImportIntensiveMode
{
    UpdateExisting,
    ReplaceWithNew
};

enum class ScheduleImportClassMatchConfidence
{
    None,
    Possible,
    Confident
};

struct IntensiveSlotState
{
    std::string day;
    std::string startTime;
    std::string state;
};

struct ScheduleImportDiagnostic
{
    std::string sheetName;
    std::string userName;
    std::string cellReference;
    std::string value;
    std::string message;
};

struct ScheduleImportClassCandidate
{
    std::string teacherKey;
    std::string teacherKr;
    std::vector<std::string> rooms;
    std::vector<std::string> importedColors;
    std::string classGrade;
    std::string classLevel;
    std::vector<ClassTime> times;
    std::vector<std::string> sourceCells;
    std::string meetingPatternError;
};

struct ScheduleImportUserBlock
{
    std::string name;
    std::string headerCell;
    std::vector<ScheduleImportClassCandidate> classes;
    std::vector<IntensiveSlotState> intensiveSlotStates;
    std::vector<ScheduleImportDiagnostic> diagnostics;
};

struct ScheduleImportSheet
{
    std::string name;
    bool visible = true;
    std::vector<ScheduleImportUserBlock> users;
    std::vector<ScheduleImportDiagnostic> diagnostics;
};

struct ScheduleImportWorkbook
{
    std::vector<ScheduleImportSheet> sheets;
};

struct ScheduleImportTeacherPreview
{
    std::string teacherKey;
    std::string teacherKr;
    std::vector<std::string> importedRooms;
    std::vector<int> matchingTeacherIds;
    int affectedClassCount = 0;
};

struct ScheduleImportClassPreview
{
    int candidateIndex = -1;
    std::vector<int> matchingClassIds;
    int suggestedClassId = -1;
    bool exactMatch = false;
    ScheduleImportClassMatchConfidence matchConfidence =
        ScheduleImportClassMatchConfidence::None;
    std::string matchExplanation;
};

struct ScheduleImportInventory
{
    int classCount = 0;
    bool hasRegularHours = false;
    bool hasIntensiveHours = false;
};

struct ScheduleImportPreview
{
    ScheduleImportKind kind = ScheduleImportKind::Normal;
    ScheduleImportInventory inventory;
    ScheduleImportUserBlock user;
    std::vector<ScheduleImportTeacherPreview> teachers;
    std::vector<ScheduleImportClassPreview> classes;
    std::vector<int> initiallyAbsentClassIds;
};

enum class ScheduleImportTeacherAction
{
    Reuse,
    UpdateRoom,
    Create,
    Skip
};

struct ScheduleImportTeacherResolution
{
    std::string teacherKey;
    ScheduleImportTeacherAction action = ScheduleImportTeacherAction::Create;
    int targetTeacherId = -1;
    std::string selectedRoom;
};

enum class ScheduleImportClassAction
{
    UpdateExisting,
    CreateNew,
    Skip
};

struct ScheduleImportClassResolution
{
    int candidateIndex = -1;
    ScheduleImportClassAction action = ScheduleImportClassAction::CreateNew;
    int targetClassId = -1;
    std::string classColor = "#FFFFFF";
    std::string fontColor = "#000000";
};

struct ScheduleImportPlan
{
    ScheduleImportKind kind = ScheduleImportKind::Normal;
    ScheduleImportIntensiveMode intensiveMode =
        ScheduleImportIntensiveMode::UpdateExisting;
    std::string selectedUserName;
    bool saveProfileNameIfBlank = false;
    bool updateProfileName = false;
    bool unknownCellsAcknowledged = false;
    std::vector<ScheduleImportClassCandidate> candidates;
    std::vector<IntensiveSlotState> intensiveSlotStates;
    std::vector<ScheduleImportDiagnostic> diagnostics;
    std::vector<ScheduleImportTeacherResolution> teachers;
    std::vector<ScheduleImportClassResolution> classes;
};

struct ScheduleImportSummary
{
    int teachersCreated = 0;
    int teachersUpdated = 0;
    int classesCreated = 0;
    int classesUpdated = 0;
    int classesSkipped = 0;
    int schedulesCleared = 0;
    int ignoredCells = 0;
    bool profileNameUpdated = false;
};

} // namespace classmngr::engine
