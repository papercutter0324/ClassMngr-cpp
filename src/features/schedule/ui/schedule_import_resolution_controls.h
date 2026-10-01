#pragma once

#include "core/result.h"
#include "domain/models/schedule_import.h"
#include "next/application/schedule_import_state_snapshot.h"

#include <QList>
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
struct TeacherControl
{
    QString teacherKey;
    QComboBox* action = nullptr;
    QComboBox* room = nullptr;
};

struct ClassControl
{
    int candidateIndex = -1;
    QString teacherKey;
    QComboBox* action = nullptr;
    QPushButton* colorButton = nullptr;
    QString color;
    QLabel* details = nullptr;
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
};

struct BuildResult
{
    QList<TeacherControl> teacherControls;
    QList<ClassControl> classControls;
};

[[nodiscard]] Result<BuildResult> build(
    const BuildRequest& request
    );

void updateClassColorButton(
    ClassControl* control
    );
}
