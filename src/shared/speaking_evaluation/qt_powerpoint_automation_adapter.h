#pragma once

#include "features/speaking_eval/services/speaking_eval_powerpoint_automation_port.h"
#include "features/speaking_eval/services/speaking_eval_powerpoint_automation.h"
#include <functional>
#include <memory>

class SpeakingEvalPowerPointWorkspace;

namespace QtSpeakingEvalPowerPoint
{
enum class Host { Windows, MacOS, Unsupported };
[[nodiscard]] Host nativeHost() noexcept;

// Optional host-operation seams allow embedding and deterministic verification
// without launching Office or writing into a user's protected application data.
struct Actions final
{
    Host host = nativeHost();
    std::function<SpeakingEvalPowerPointAvailability()> availability;
    std::function<QString()> executable;
    std::function<SpeakingEvalPowerPointAutomation::Status(
        const SpeakingEvalPowerPointAutomation::Request&, QString*)> run;
    QString workspaceRoot;
    std::function<bool(SpeakingEvalPowerPointWorkspace&, QString*)> releaseWorkspace;
    std::function<void(const QString&, const QString&)> workspaceCreated;
};
}

[[nodiscard]] std::shared_ptr<SpeakingEvalPowerPointAutomationPort>
makeQtSpeakingEvalPowerPointAutomationPort(
    QtSpeakingEvalPowerPoint::Actions actions = {});
