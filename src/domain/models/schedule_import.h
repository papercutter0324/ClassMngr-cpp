#pragma once

#include "domain/models/class_info.h"
#include "domain/models/intensive_slot_state.h"

#include <QList>
#include <QString>
#include <QStringList>

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

struct ScheduleImportDiagnostic
{
    QString sheetName;
    QString userName;
    QString cellReference;
    QString value;
    QString message;
};

struct ScheduleImportClassCandidate
{
    QString teacherKey;
    QString teacherKr;
    QStringList rooms;
    QStringList importedColors;
    QString classGrade;
    QString classLevel;
    QList<ClassTime> times;
    QStringList sourceCells;
    QString meetingPatternError;
};

struct ScheduleImportUserBlock
{
    QString name;
    QString headerCell;
    QList<ScheduleImportClassCandidate> classes;
    QList<IntensiveSlotState> intensiveSlotStates;
    QList<ScheduleImportDiagnostic> diagnostics;
};

struct ScheduleImportSheet
{
    QString name;
    bool visible = true;
    QList<ScheduleImportUserBlock> users;
    QList<ScheduleImportDiagnostic> diagnostics;
};

struct ScheduleImportWorkbook
{
    QList<ScheduleImportSheet> sheets;
};

struct ScheduleImportTeacherPreview
{
    QString teacherKey;
    QString teacherKr;
    QStringList importedRooms;
    QList<int> matchingTeacherIds;
    int affectedClassCount = 0;
};

struct ScheduleImportClassPreview
{
    int candidateIndex = -1;
    QList<int> matchingClassIds;
    int suggestedClassId = -1;
    bool exactMatch = false;
    ScheduleImportClassMatchConfidence matchConfidence =
        ScheduleImportClassMatchConfidence::None;
    QString matchExplanation;
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
    QList<ScheduleImportTeacherPreview> teachers;
    QList<ScheduleImportClassPreview> classes;
    QList<int> initiallyAbsentClassIds;
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
    QString teacherKey;
    ScheduleImportTeacherAction action =
        ScheduleImportTeacherAction::Create;
    int targetTeacherId = -1;
    QString selectedRoom;
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
    ScheduleImportClassAction action =
        ScheduleImportClassAction::CreateNew;
    int targetClassId = -1;
    QString classColor = QStringLiteral("#FFFFFF");
    QString fontColor = QStringLiteral("#000000");
};

struct ScheduleImportPlan
{
    ScheduleImportKind kind = ScheduleImportKind::Normal;
    ScheduleImportIntensiveMode intensiveMode =
        ScheduleImportIntensiveMode::UpdateExisting;
    QString selectedUserName;
    bool saveProfileNameIfBlank = false;
    bool updateProfileName = false;
    bool unknownCellsAcknowledged = false;
    QList<ScheduleImportClassCandidate> candidates;
    QList<IntensiveSlotState> intensiveSlotStates;
    QList<ScheduleImportDiagnostic> diagnostics;
    QList<ScheduleImportTeacherResolution> teachers;
    QList<ScheduleImportClassResolution> classes;
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
