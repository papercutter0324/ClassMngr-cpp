#pragma once

#include "features/speaking_eval/services/speaking_eval_powerpoint_automation_port.h"
#include "shared/speaking_evaluation/qt_powerpoint_automation_host.h"
#include <functional>
#include <memory>

class SpeakingEvalPowerPointWorkspace;

namespace QtSpeakingEvalPowerPoint
{
// Optional host-operation seams allow embedding and deterministic verification
// without launching Office or writing into a user's protected application data.
struct Actions final
{
    Host host = nativeHost();
    std::function<SpeakingEvalPowerPointAvailability()> availability;
    std::function<QString()> executable;
    std::function<Status(const ProcessRequest&, QString*)> run;
    QString workspaceRoot;
    std::function<bool(SpeakingEvalPowerPointWorkspace&, QString*)> releaseWorkspace;
    std::function<void(const QString&, const QString&)> workspaceCreated;
};
}

[[nodiscard]] std::shared_ptr<SpeakingEvalPowerPointAutomationPort>
makeQtSpeakingEvalPowerPointAutomationPort(
    QtSpeakingEvalPowerPoint::Actions actions = {});
