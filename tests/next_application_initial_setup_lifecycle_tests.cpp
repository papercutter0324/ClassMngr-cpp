#include "next/application/initial_setup_lifecycle.h"

#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

namespace
{

OperationError failure(const char* message)
{
    return OperationError{
        .code = ErrorCode::Technical,
        .message = message,
        .recoverable = true
    };
}

bool require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
    }
    return condition;
}

class FakeInitialSetupPort final : public InitialSetupLifecyclePort
{
public:
    [[nodiscard]] bool hasActiveWorkspace() const override
    {
        return active;
    }

    [[nodiscard]] Result<void> closeActiveWorkspace() override
    {
        events.emplace_back("close");
        ++closeCalls;
        if (failCloseCalls.contains(closeCalls))
        {
            return Result<void>::failure(failure("close failed"));
        }
        active = false;
        return Result<void>::success();
    }

    [[nodiscard]] bool profileExists(
        const WorkspaceLocation& location
        ) const override
    {
        events.emplace_back("exists:" + location.value());
        return files.contains(location.value());
    }

    [[nodiscard]] WorkspaceLocation recoveryLocationFor(
        const WorkspaceLocation& profile
        ) override
    {
        const std::string path = profile.value() + ".recovery-"
            + std::to_string(++recoveryPathCount);
        events.emplace_back("recovery:" + path);
        return WorkspaceLocation(path);
    }

    [[nodiscard]] bool moveProfile(
        const WorkspaceLocation& source,
        const WorkspaceLocation& destination
        ) override
    {
        events.emplace_back(
            "move:" + source.value() + ">" + destination.value()
            );
        ++moveCalls;
        if (
            failMoveCalls.contains(moveCalls)
            || files.contains(destination.value())
            || !files.contains(source.value())
            )
        {
            return false;
        }
        files.emplace(destination.value(), std::move(files.at(source.value())));
        files.erase(source.value());
        return true;
    }

    [[nodiscard]] bool removeProfile(
        const WorkspaceLocation& location
        ) override
    {
        events.emplace_back("remove:" + location.value());
        ++removeCalls;
        if (failRemoveCalls.contains(removeCalls))
        {
            return false;
        }
        files.erase(location.value());
        return true;
    }

    [[nodiscard]] Result<WorkspaceSession> createWorkspace(
        const CreateWorkspaceRequest& request
        ) override
    {
        events.emplace_back("create:" + request.location.value());
        if (createFails)
        {
            if (createWritesPartialFile)
            {
                files[request.location.value()] = "partial setup profile";
            }
            return Result<WorkspaceSession>::failure(
                failure("create failed")
                );
        }

        files[request.location.value()] = "new setup profile";
        active = true;
        return Result<WorkspaceSession>::success(
            WorkspaceSession(
                *WorkspaceId::fromString("setup"),
                request.location
                )
            );
    }

    mutable std::vector<std::string> events;
    std::map<std::string, std::string> files;
    std::set<int> failCloseCalls;
    std::set<int> failMoveCalls;
    std::set<int> failRemoveCalls;
    bool active = false;
    bool createFails = false;
    bool createWritesPartialFile = true;
    int closeCalls = 0;
    int moveCalls = 0;
    int removeCalls = 0;
    int recoveryPathCount = 0;
};

bool beginExistingAndFinishKeepsBackupUntilAcceptance()
{
    FakeInitialSetupPort port;
    port.files["profile.tps"] = "original";
    InitialSetupLifecycle lifecycle(port);

    const auto started = lifecycle.begin(WorkspaceLocation("profile.tps"));
    if (
        !require(started.started(), "existing profile setup did not start")
        || !require(started.workspaceClosed, "begin did not close workspace")
        || !require(
            started.originalBackup == WorkspaceLocation("profile.tps.recovery-1"),
            "begin did not expose the preserved original path"
            )
        || !require(
            port.files.at("profile.tps.recovery-1") == "original",
            "begin did not preserve original contents"
            )
        || !require(
            port.files.at("profile.tps") == "new setup profile",
            "begin did not create the setup profile"
            )
        || !require(
            port.events == std::vector<std::string>{
                "close",
                "exists:profile.tps",
                "recovery:profile.tps.recovery-1",
                "move:profile.tps>profile.tps.recovery-1",
                "create:profile.tps"
            },
            "begin operations were out of order"
            )
        || !require(
            lifecycle.hasPendingSetup(),
            "setup ticket was not retained until wizard acceptance"
            )
        )
    {
        return false;
    }

    const auto finished = lifecycle.finish();
    return require(finished.wasActive, "finish did not complete active setup")
        && require(
            !finished.originalBackupRemovalFailed,
            "finish unexpectedly failed to remove the backup"
            )
        && require(
            !port.files.contains("profile.tps.recovery-1"),
            "finish left the original backup behind"
            )
        && require(!lifecycle.hasPendingSetup(), "finish retained setup state")
        && require(
            port.events.back() == "remove:profile.tps.recovery-1",
            "finish did not remove backup after setup acceptance"
            );
}

bool beginWithoutExistingProfileAndFinishDoesNotAttemptBackupCleanup()
{
    FakeInitialSetupPort port;
    InitialSetupLifecycle lifecycle(port);

    const auto started = lifecycle.begin(WorkspaceLocation("new.tps"));
    if (
        !require(started.started(), "new profile setup did not start")
        || !require(
            started.originalBackup.empty(),
            "setup without an original profile reported a backup"
            )
        || !require(port.active, "begin did not retain the created workspace")
        || !require(
            lifecycle.hasPendingSetup(),
            "setup ticket was not retained until wizard acceptance"
            )
        )
    {
        return false;
    }

    const auto eventsBeforeFinish = port.events;
    const auto finished = lifecycle.finish();
    return require(finished.wasActive, "finish did not complete active setup")
        && require(
            finished.originalBackup.empty(),
            "finish reported a backup for a new profile"
            )
        && require(
            !finished.originalBackupRemovalFailed,
            "finish reported cleanup failure without an original profile"
            )
        && require(port.active, "finish closed the new workspace")
        && require(
            port.files.contains("new.tps"),
            "finish removed the accepted setup profile"
            )
        && require(
            !lifecycle.hasPendingSetup(),
            "finish retained setup state"
            )
        && require(
            port.events == eventsBeforeFinish,
            "finish attempted backup cleanup without an original profile"
            );
}

bool beginWithoutExistingProfileAndCancelRemovesIncompleteProfile()
{
    FakeInitialSetupPort port;
    InitialSetupLifecycle lifecycle(port);

    const auto started = lifecycle.begin(WorkspaceLocation("new.tps"));
    if (!require(started.started(), "new profile setup did not start"))
    {
        return false;
    }

    const auto canceled = lifecycle.cancel();
    return require(
        canceled.status == InitialSetupCancelStatus::Canceled,
        "cancel without original profile did not complete"
        )
        && require(canceled.workspaceClosed, "cancel did not close workspace")
        && require(!port.files.contains("new.tps"), "cancel left incomplete profile")
        && require(
            port.events == std::vector<std::string>{
                "close", "exists:new.tps", "create:new.tps",
                "close", "exists:new.tps", "remove:new.tps"
            },
            "cancel without original profile used the wrong operation order"
            );
}

bool closeFailureStopsBeforeProfileOperations()
{
    FakeInitialSetupPort port;
    port.active = true;
    port.failCloseCalls.insert(1);
    port.files["profile.tps"] = "original";
    InitialSetupLifecycle lifecycle(port);

    const auto started = lifecycle.begin(WorkspaceLocation("profile.tps"));
    return require(
        started.failure == InitialSetupBeginFailure::CloseWorkspace,
        "begin did not report close failure"
        )
        && require(started.workspaceWasOpen, "begin lost pre-close workspace state")
        && require(!started.workspaceClosed, "failed close was reported as closed")
        && require(
            port.events == std::vector<std::string>{"close"},
            "begin touched profile files after close failure"
            )
        && require(port.files.at("profile.tps") == "original", "close failure lost profile");
}

bool preserveFailureDoesNotCreateOrLoseOriginal()
{
    FakeInitialSetupPort port;
    port.files["profile.tps"] = "original";
    port.failMoveCalls.insert(1);
    InitialSetupLifecycle lifecycle(port);

    const auto started = lifecycle.begin(WorkspaceLocation("profile.tps"));
    return require(
        started.failure == InitialSetupBeginFailure::PreserveProfile,
        "begin did not report preserve failure"
        )
        && require(
            port.events == std::vector<std::string>{
                "close", "exists:profile.tps",
                "recovery:profile.tps.recovery-1",
                "move:profile.tps>profile.tps.recovery-1"
            },
            "begin attempted creation after preserve failure"
            )
        && require(port.files.at("profile.tps") == "original", "preserve failure lost original");
}

bool createFailureRestoresExistingProfileAndRemovesIncompleteCopy()
{
    FakeInitialSetupPort port;
    port.files["profile.tps"] = "original";
    port.createFails = true;
    InitialSetupLifecycle lifecycle(port);

    const auto started = lifecycle.begin(WorkspaceLocation("profile.tps"));
    return require(
        started.failure == InitialSetupBeginFailure::CreateWorkspace,
        "begin did not report create failure"
        )
        && require(
            started.rollback.status == InitialSetupCancelStatus::Canceled,
            "create failure did not successfully roll back"
            )
        && require(
            port.events == std::vector<std::string>{
                "close", "exists:profile.tps",
                "recovery:profile.tps.recovery-1",
                "move:profile.tps>profile.tps.recovery-1",
                "create:profile.tps", "close", "exists:profile.tps",
                "recovery:profile.tps.recovery-2",
                "move:profile.tps>profile.tps.recovery-2",
                "move:profile.tps.recovery-1>profile.tps",
                "exists:profile.tps.recovery-2",
                "remove:profile.tps.recovery-2"
            },
            "create failure rollback operations were out of order"
            )
        && require(port.files.at("profile.tps") == "original", "rollback did not restore original")
        && require(!lifecycle.hasPendingSetup(), "rollback retained setup state");
}

bool failedCancelCloseRetainsTicketForRetry()
{
    FakeInitialSetupPort port;
    InitialSetupLifecycle lifecycle(port);
    const auto started = lifecycle.begin(WorkspaceLocation("new.tps"));
    if (!require(started.started(), "setup did not start before close retry test"))
    {
        return false;
    }

    port.failCloseCalls.insert(2);
    const auto firstCancel = lifecycle.cancel();
    if (
        !require(
            firstCancel.status == InitialSetupCancelStatus::CloseWorkspaceFailed,
            "cancel did not report workspace close failure"
            )
        || !require(lifecycle.hasPendingSetup(), "failed close discarded setup ticket")
        || !require(port.files.contains("new.tps"), "failed close touched incomplete profile")
        )
    {
        return false;
    }

    const auto retry = lifecycle.cancel();
    return require(
        retry.status == InitialSetupCancelStatus::Canceled,
        "cancel retry did not complete"
        )
        && require(!port.files.contains("new.tps"), "cancel retry left incomplete profile")
        && require(!lifecycle.hasPendingSetup(), "successful retry retained setup ticket");
}

bool failedIncompleteRemovalLeavesRecoverableProfile()
{
    FakeInitialSetupPort port;
    InitialSetupLifecycle lifecycle(port);
    const auto started = lifecycle.begin(WorkspaceLocation("new.tps"));
    if (!require(started.started(), "setup did not start before removal failure test"))
    {
        return false;
    }
    port.failRemoveCalls.insert(1);

    const auto canceled = lifecycle.cancel();
    return require(
        canceled.status == InitialSetupCancelStatus::IncompleteProfileRemovalFailed,
        "cancel did not report incomplete profile removal failure"
        )
        && require(port.files.contains("new.tps"), "failed remove lost incomplete profile")
        && require(!lifecycle.hasPendingSetup(), "remove failure retained setup ticket");
}

bool restoreFailureKeepsOriginalBackupAndIncompleteProfile()
{
    FakeInitialSetupPort port;
    port.files["profile.tps"] = "original";
    InitialSetupLifecycle lifecycle(port);
    const auto started = lifecycle.begin(WorkspaceLocation("profile.tps"));
    if (!require(started.started(), "setup did not start before restore failure test"))
    {
        return false;
    }

    port.failMoveCalls.insert(3);
    const auto canceled = lifecycle.cancel();
    return require(
        canceled.status == InitialSetupCancelStatus::OriginalProfileRestoreFailed,
        "cancel did not report original restore failure"
        )
        && require(
            canceled.originalBackup == WorkspaceLocation("profile.tps.recovery-1"),
            "restore failure did not report original backup location"
            )
        && require(
            port.files.at("profile.tps.recovery-1") == "original",
            "restore failure lost the original profile"
            )
        && require(
            port.files.at("profile.tps") == "new setup profile",
            "restore failure lost the incomplete setup profile"
            )
        && require(!lifecycle.hasPendingSetup(), "restore failure retained setup ticket");
}

bool failedRestoreAndFailedPutBackKeepBothRecoveryFiles()
{
    FakeInitialSetupPort port;
    port.files["profile.tps"] = "original";
    InitialSetupLifecycle lifecycle(port);
    const auto started = lifecycle.begin(WorkspaceLocation("profile.tps"));
    if (!require(started.started(), "setup did not start before double move failure test"))
    {
        return false;
    }

    port.failMoveCalls.insert(3);
    port.failMoveCalls.insert(4);
    const auto canceled = lifecycle.cancel();
    return require(
        canceled.status == InitialSetupCancelStatus::OriginalProfileRestoreFailed,
        "double move failure did not report original restore failure"
        )
        && require(
            port.files.at("profile.tps.recovery-1") == "original",
            "double move failure lost original backup"
            )
        && require(
            port.files.at("profile.tps.recovery-2") == "new setup profile",
            "double move failure lost incomplete recovery file"
            );
}

bool finishRemovalFailureLeavesBackupAndClearsTicket()
{
    FakeInitialSetupPort port;
    port.files["profile.tps"] = "original";
    InitialSetupLifecycle lifecycle(port);
    const auto started = lifecycle.begin(WorkspaceLocation("profile.tps"));
    if (!require(started.started(), "setup did not start before finish removal failure test"))
    {
        return false;
    }
    port.failRemoveCalls.insert(1);

    const auto finished = lifecycle.finish();
    return require(finished.wasActive, "finish did not report active setup")
        && require(
            finished.originalBackupRemovalFailed,
            "finish did not report backup removal failure"
            )
        && require(
            port.files.at("profile.tps.recovery-1") == "original",
            "finish removal failure lost the recovery backup"
            )
        && require(!lifecycle.hasPendingSetup(), "finish failure retained setup ticket");
}

bool beginCreateFailureWithoutOriginalRemovesPartialFile()
{
    FakeInitialSetupPort port;
    port.createFails = true;
    InitialSetupLifecycle lifecycle(port);
    const auto started = lifecycle.begin(WorkspaceLocation("new.tps"));
    return require(
        started.failure == InitialSetupBeginFailure::CreateWorkspace,
        "create failure without original was not reported"
        )
        && require(
            started.rollback.status == InitialSetupCancelStatus::Canceled,
            "create failure without original did not cancel"
            )
        && require(!port.files.contains("new.tps"), "partial profile survived successful rollback")
        && require(!lifecycle.hasPendingSetup(), "failed begin retained setup ticket");
}

} // namespace

int main()
{
    const bool passed =
        beginExistingAndFinishKeepsBackupUntilAcceptance()
        && beginWithoutExistingProfileAndFinishDoesNotAttemptBackupCleanup()
        && beginWithoutExistingProfileAndCancelRemovesIncompleteProfile()
        && closeFailureStopsBeforeProfileOperations()
        && preserveFailureDoesNotCreateOrLoseOriginal()
        && createFailureRestoresExistingProfileAndRemovesIncompleteCopy()
        && failedCancelCloseRetainsTicketForRetry()
        && failedIncompleteRemovalLeavesRecoverableProfile()
        && restoreFailureKeepsOriginalBackupAndIncompleteProfile()
        && failedRestoreAndFailedPutBackKeepBothRecoveryFiles()
        && finishRemovalFailureLeavesBackupAndClearsTicket()
        && beginCreateFailureWithoutOriginalRemovesPartialFile();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
