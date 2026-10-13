#pragma once

#include "core/result.h"
#include "domain/models/schedule_import.h"
#include "next/application/schedule_import_state_snapshot.h"

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>

#include <functional>

class QLabel;
class QObject;
class QPushButton;
class QComboBox;
class QVBoxLayout;
class QWidget;

namespace ScheduleImportResolutionControls
{
inline constexpr int TeacherRowExtent = 220;
inline constexpr int ClassRowExtent = 400;
inline constexpr int RealizedRowOverscan = 2;

struct TeacherControl
{
    int rowIndex = -1;
    QString teacherKey;
    int actionValue = -1;
    qint64 targetTeacherId = -1;
    QString selectedRoom;
    QWidget* card = nullptr;
    QComboBox* action = nullptr;
    QComboBox* room = nullptr;
};

struct ClassControl
{
    int rowIndex = -1;
    int candidateIndex = -1;
    QString teacherKey;
    int actionValue = -1;
    int targetClassId = -1;
    int skipTargetId = -1;
    QComboBox* action = nullptr;
    QPushButton* colorButton = nullptr;
    QString color;
    QLabel* details = nullptr;
    QString detailsText;
    QWidget* card = nullptr;
};

struct BuildRequest
{
    QObject* context = nullptr;
    QWidget* teacherContent = nullptr;
    QWidget* classContent = nullptr;
    QVBoxLayout* teacherLayout = nullptr;
    QVBoxLayout* classLayout = nullptr;
    const ClassMngr::Next::Application::
        ScheduleImportStateSnapshot* stateSnapshot = nullptr;
    const ScheduleImportPreview* preview = nullptr;
    ScheduleImportKind kind{};
    std::function<void()> stateChanged;
    std::function<void(int)> colorRequested;
    bool virtualized = false;
    const struct BuildResult* previousState = nullptr;
    QSet<int> realizedTeacherRows;
    QSet<int> realizedClassRows;
    int teacherRowExtent = TeacherRowExtent;
    int classRowExtent = ClassRowExtent;
};

struct BuildResult
{
    QList<TeacherControl> teacherControls;
    QList<ClassControl> classControls;
    // A single shared grade/level projection. Rows materialize combo items
    // only when their card enters the visible window.
    QHash<QString, QList<int>> classIdsByIdentity;
};

[[nodiscard]] Result<BuildResult> build(
    const BuildRequest& request
    );

void updateClassColorButton(
    ClassControl* control
    );

void captureRealizedDecisions(
    BuildResult* state
    );

void clearRealizedControls(
    BuildResult* state
    );

void forceSkippedTeachers(
    BuildResult* state,
    const QSet<QString>& skippedTeacherKeys
    );

[[nodiscard]] int realizedTeacherCount(const BuildResult& state);
[[nodiscard]] int realizedClassCount(const BuildResult& state);
[[nodiscard]] int realizedClassTargetChoiceCount(const BuildResult& state);
[[nodiscard]] QList<ScheduleImportTeacherResolution> teacherResolutions(
    const BuildResult& state
    );
[[nodiscard]] QList<ScheduleImportClassResolution> classResolutions(
    const BuildResult& state
    );
}
