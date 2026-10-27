#include "next/application/application_runtime.h"
#include "next/application/interrupted_workspace_recovery_flow.h"
#include "next/platform/workspace_interrupted_recovery_port.h"
#include "next/platform/workspace_profile_path_policy.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

std::string utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

QString pathText(const Application::FileDialogPath& path)
{
    return QString::fromUtf8(
        path.value().data(),
        static_cast<qsizetype>(path.value().size())
        );
}

QString pathText(const Application::WorkspaceLocation& path)
{
    return QString::fromUtf8(
        path.value().data(),
        static_cast<qsizetype>(path.value().size())
        );
}

Application::WorkspaceSession session(const QString& path)
{
    return Application::WorkspaceSession(
        *Domain::WorkspaceId::fromString("current-profile"),
        Application::WorkspaceLocation(utf8(QFileInfo(path).absoluteFilePath()))
        );
}

bool writeBytes(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(bytes) == bytes.size();
}

QByteArray readBytes(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

Application::InterruptedWorkspaceRecoveryCandidate candidate(
    const QString& directory,
    const QString& targetName
    )
{
    const QString uuid = QStringLiteral(
        "12345678-9abc-def0-1234-567890abcdef"
        );
    const QString target = QDir(directory).filePath(targetName);
    const QString source = QDir(directory).filePath(
        QStringLiteral(".%1.workspace-create-%2.recovery")
            .arg(targetName, uuid)
        );
    return {
        Application::FileDialogPath(utf8(source)),
        Application::FileDialogPath(utf8(target)),
        utf8(targetName)
    };
}

class RecordingEvents final
{
public:
    void add(std::string event)
    {
        values.push_back(std::move(event));
    }

    std::vector<std::string> values;
};

class RecordingFileDialog final : public Application::FileDialogPort
{
public:
    RecordingEvents* events = nullptr;
    std::vector<Application::FileDialogResponse<
        Application::SavedFileDialogSelection>> saveResponses;
    std::vector<Application::SaveFileDialogRequest> saveRequests;

    Domain::Result<Application::FileDialogResponse<Application::FileDialogPath>>
    openOneFile(const Application::OpenFileDialogRequest&) override
    {
        return Domain::Result<
            Application::FileDialogResponse<Application::FileDialogPath>>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unexpected open dialog.",
            .recoverable = false
        });
    }

    Domain::Result<Application::FileDialogResponse<
        std::vector<Application::FileDialogPath>>>
    openManyFiles(const Application::OpenFileDialogRequest&) override
    {
        return Domain::Result<Application::FileDialogResponse<
            std::vector<Application::FileDialogPath>>>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unexpected multi-file dialog.",
            .recoverable = false
        });
    }

    Domain::Result<Application::FileDialogResponse<
        Application::SavedFileDialogSelection>>
    saveFile(const Application::SaveFileDialogRequest& request) override
    {
        saveRequests.push_back(request);
        if (events)
        {
            events->add("save-dialog");
        }
        if (saveResponses.empty())
        {
            return Domain::Result<Application::FileDialogResponse<
                Application::SavedFileDialogSelection>>::success({});
        }
        auto response = std::move(saveResponses.front());
        saveResponses.erase(saveResponses.begin());
        return Domain::Result<Application::FileDialogResponse<
            Application::SavedFileDialogSelection>>::success(
                std::move(response)
                );
    }

    Domain::Result<Application::FileDialogResponse<Application::FileDialogPath>>
    selectDirectory(const Application::SelectDirectoryDialogRequest&) override
    {
        return Domain::Result<
            Application::FileDialogResponse<Application::FileDialogPath>>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unexpected directory dialog.",
            .recoverable = false
        });
    }
};

class RecordingPrompt final : public Application::UserPromptPort
{
public:
    RecordingEvents* events = nullptr;
    std::optional<Application::PromptActionId> selectedCandidate;
    Application::UnsavedChangesAction unsavedAction =
        Application::UnsavedChangesAction::Cancel;
    std::vector<Application::NamedActionPromptRequest> candidateRequests;
    std::vector<Application::UnsavedChangesPromptRequest> unsavedRequests;
    int messageCalls = 0;

    Domain::Result<void> showMessage(
        const Application::MessagePromptRequest& request
        ) override
    {
        ++messageCalls;
        lastMessage = request;
        return Domain::Result<void>::success();
    }

    Domain::Result<Application::ConfirmationResponse> confirm(
        const Application::ConfirmationPromptRequest& request
        ) override
    {
        static_cast<void>(request);
        return Domain::Result<Application::ConfirmationResponse>::success(
            Application::ConfirmationResponse::Declined
            );
    }

    Domain::Result<Application::UnsavedChangesAction> confirmUnsavedChanges(
        const Application::UnsavedChangesPromptRequest& request
        ) override
    {
        unsavedRequests.push_back(request);
        if (events)
        {
            events->add("unsaved-prompt");
        }
        return Domain::Result<Application::UnsavedChangesAction>::success(
            unsavedAction
            );
    }

    Domain::Result<std::optional<Application::PromptActionId>> chooseAction(
        const Application::NamedActionPromptRequest& request
        ) override
    {
        candidateRequests.push_back(request);
        if (events)
        {
            events->add("candidate-prompt");
        }
        return Domain::Result<std::optional<Application::PromptActionId>>::success(
            selectedCandidate
            );
    }

    std::optional<Application::MessagePromptRequest> lastMessage;
};

class RecordingDirectoryPreferences final
    : public Application::FileDialogDirectoryPreferencesPort
{
public:
    Application::DirectoryPreferencePath remembered;
    Application::DirectoryPreferencePath written;

    Application::DirectoryPreferencePath readDirectory(
        Application::FileDialogPurpose purpose
        ) const override
    {
        if (purpose == Application::FileDialogPurpose::TeacherProfile)
        {
            return remembered;
        }
        return {};
    }

    Domain::Result<void> writeDirectory(
        Application::FileDialogPurpose,
        const Application::DirectoryPreferencePath& directory
        ) const override
    {
        const_cast<RecordingDirectoryPreferences*>(this)->written = directory;
        return Domain::Result<void>::success();
    }
};

class RecordingRecoveryPort final
    : public Application::InterruptedWorkspaceRecoveryPort
{
public:
    RecordingEvents* events = nullptr;
    Application::FileDialogPath defaultDirectory{"default-directory"};
    Application::FileDialogPath activeDirectory{"active-profile-parent"};
    std::vector<Application::InterruptedWorkspaceRecoveryCandidate> candidates;
    std::optional<Domain::OperationError> scanError;
    std::optional<Domain::OperationError> copyError;
    bool copyFailureCreatesDestination = false;
    bool collideOnNextCopy = false;
    int scanCalls = 0;
    int copyCalls = 0;
    Application::FileDialogPath scannedDirectory;
    std::vector<std::string> existingPaths;
    std::vector<std::string> createdPaths;
    std::vector<Application::InterruptedWorkspaceRecoveryCandidate> copied;

    Application::FileDialogPath defaultTeacherProfileDirectory()
        const override
    {
        return defaultDirectory;
    }

    Application::FileDialogPath profileDirectoryFor(
        const Application::WorkspaceLocation& profile
        ) const override
    {
        const_cast<RecordingRecoveryPort*>(this)->profileLocation = profile;
        return activeDirectory;
    }

    Domain::Result<std::vector<
        Application::InterruptedWorkspaceRecoveryCandidate>>
    candidatesIn(const Application::FileDialogPath& directory) const override
    {
        auto* const self = const_cast<RecordingRecoveryPort*>(this);
        ++self->scanCalls;
        self->scannedDirectory = directory;
        if (self->events)
        {
            self->events->add("scan");
        }
        if (self->scanError.has_value())
        {
            return Domain::Result<std::vector<
                Application::InterruptedWorkspaceRecoveryCandidate>>::failure(
                    *self->scanError
                    );
        }
        return Domain::Result<std::vector<
            Application::InterruptedWorkspaceRecoveryCandidate>>::success(
                self->candidates
                );
    }

    bool pathExists(const Application::FileDialogPath& path) const override
    {
        const std::string& value = path.value();
        const bool sourceIsAvailable = std::any_of(
            candidates.begin(),
            candidates.end(),
            [&value](const auto& candidateValue)
            {
                return candidateValue.sourcePath.value() == value;
            }
            );
        return sourceIsAvailable
            || std::find(existingPaths.begin(), existingPaths.end(), value)
                != existingPaths.end()
            || std::find(createdPaths.begin(), createdPaths.end(), value)
                != createdPaths.end();
    }

    Domain::Result<void> copyCandidateNoReplace(
        const Application::InterruptedWorkspaceRecoveryCandidate& candidateValue,
        const Application::WorkspaceLocation& destination
        ) const override
    {
        auto* const self = const_cast<RecordingRecoveryPort*>(this);
        ++self->copyCalls;
        self->copied.push_back(candidateValue);
        if (self->events)
        {
            self->events->add("copy");
        }
        if (self->collideOnNextCopy)
        {
            self->collideOnNextCopy = false;
            self->createdPaths.push_back(destination.value());
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Conflict,
                .message = "Injected no-replace destination collision.",
                .recoverable = true
            });
        }
        if (self->copyError.has_value())
        {
            if (self->copyFailureCreatesDestination)
            {
                self->createdPaths.push_back(destination.value());
            }
            return Domain::Result<void>::failure(*self->copyError);
        }
        self->createdPaths.push_back(destination.value());
        return Domain::Result<void>::success();
    }

    std::optional<Application::WorkspaceLocation> profileLocation;
};

class RecordingGateway final : public Application::WorkspaceGateway
{
public:
    RecordingEvents* events = nullptr;
    int opens = 0;
    int saves = 0;
    int discards = 0;
    std::optional<Domain::OperationError> openError;

    Domain::Result<Application::WorkspaceSession> createWorkspace(
        const Application::CreateWorkspaceRequest& request
        ) override
    {
        return Domain::Result<Application::WorkspaceSession>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unexpected create call for " + request.location.value(),
            .recoverable = false
        });
    }

    Domain::Result<Application::WorkspaceSession> openWorkspace(
        const Application::OpenWorkspaceRequest& request
        ) override
    {
        ++opens;
        if (events)
        {
            events->add("open");
        }
        if (openError.has_value())
        {
            return Domain::Result<Application::WorkspaceSession>::failure(
                *openError
                );
        }
        return Domain::Result<Application::WorkspaceSession>::success(
            Application::WorkspaceSession(
                *Domain::WorkspaceId::fromString("recovered-profile"),
                request.location
                )
            );
    }

    Domain::Result<void> closeWorkspace(
        const Application::CloseWorkspaceRequest& request
        ) override
    {
        static_cast<void>(request);
        return Domain::Result<void>::success();
    }

    Domain::Result<void> saveWorkspace(
        const Application::SaveWorkspaceRequest& request
        ) override
    {
        static_cast<void>(request);
        ++saves;
        if (events)
        {
            events->add("workspace-save");
        }
        return Domain::Result<void>::success();
    }

    Domain::Result<void> discardWorkspaceChanges(
        const Application::DiscardWorkspaceChangesRequest& request
        ) override
    {
        static_cast<void>(request);
        ++discards;
        if (events)
        {
            events->add("workspace-discard");
        }
        return Domain::Result<void>::success();
    }

    Domain::Result<Application::WorkspaceLocation> saveWorkspaceAs(
        const Application::SaveWorkspaceAsRequest& request
        ) override
    {
        static_cast<void>(request);
        return Domain::Result<Application::WorkspaceLocation>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unexpected save-as call.",
            .recoverable = false
        });
    }

    Domain::Result<Application::WorkspaceLocation> exportWorkspace(
        const Application::ExportWorkspaceRequest& request
        ) override
    {
        static_cast<void>(request);
        return Domain::Result<Application::WorkspaceLocation>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unexpected export call.",
            .recoverable = false
        });
    }
};

class RecordingHistory final : public Application::RecentWorkspaceHistoryPort
{
public:
    Application::RecentWorkspaceHistory current;
    mutable int loadCalls = 0;
    mutable std::vector<Application::RecentWorkspaceHistory> saved;

    Application::RecentWorkspaceHistory load() const override
    {
        ++loadCalls;
        return current;
    }

    Domain::Result<void> save(
        const Application::RecentWorkspaceHistory& value
        ) const override
    {
        saved.push_back(value);
        const_cast<RecordingHistory*>(this)->current = value;
        return Domain::Result<void>::success();
    }

    Domain::Result<void> clear() const override
    {
        return Domain::Result<void>::success();
    }
};

class RecordingNotifications final : public Application::NotificationPort
{
public:
    std::vector<Application::NotificationRequest> requests;

    Domain::Result<void> show(
        const Application::NotificationRequest& request
        ) override
    {
        requests.push_back(request);
        return Domain::Result<void>::success();
    }

    Domain::Result<void> clear() override
    {
        return Domain::Result<void>::success();
    }
};

class RecordingPageChanges final : public Application::ActivePageChangesPort
{
public:
    RecordingEvents* events = nullptr;
    bool dirty = false;
    bool failSave = false;
    int saves = 0;
    int discards = 0;

    bool hasUnsavedChanges() const noexcept override
    {
        return dirty;
    }

    Domain::Result<void> saveChanges() override
    {
        ++saves;
        if (events)
        {
            events->add("page-save");
        }
        if (failSave)
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "Injected page save failure.",
                .recoverable = true
            });
        }
        dirty = false;
        return Domain::Result<void>::success();
    }

    Domain::Result<void> discardChanges() override
    {
        ++discards;
        if (events)
        {
            events->add("page-discard");
        }
        dirty = false;
        return Domain::Result<void>::success();
    }
};

struct Harness final
{
    Harness()
    {
        recovery.candidates = {candidate(QStringLiteral("scan-directory"),
                                         QStringLiteral("Teacher.tps"))};
        recovery.events = &events;
        dialog.events = &events;
        prompt.events = &events;
        gateway.events = &events;
        page.events = &events;
    }

    void chooseCandidate(std::size_t index = 0)
    {
        prompt.selectedCandidate = *Application::PromptActionId::create(
            "candidate-" + std::to_string(index)
            );
    }

    void selectDestination(const QString& path)
    {
        dialog.saveResponses.push_back({
            Application::SavedFileDialogSelection{
                Application::FileDialogPath(utf8(path)),
                false
            },
            std::nullopt
        });
    }

    void openActiveProfile(const QString& path)
    {
        QVERIFY(runtime.workspaceState().open(session(path)));
    }

    Application::InterruptedWorkspaceRecoveryOutcome run(
        const std::function<bool()>& setupPending = [] { return false; }
        )
    {
        Application::OpenExistingWorkspaceFlow openFlow(
            pathPolicy,
            runtime.workspaceCoordinator(),
            history,
            notifications,
            setupPending
            );
        Application::InterruptedWorkspaceRecoveryFlow flow(
            dialog,
            prompt,
            directoryPreferences,
            recovery,
            pathPolicy,
            page,
            runtime.workspaceCoordinator(),
            runtime.workspaceState(),
            openFlow,
            notifications,
            setupPending
            );
        return flow.run();
    }

    QTemporaryDir directory;
    RecordingEvents events;
    RecordingGateway gateway;
    Application::ApplicationRuntime runtime{gateway};
    RecordingFileDialog dialog;
    RecordingPrompt prompt;
    RecordingDirectoryPreferences directoryPreferences;
    RecordingRecoveryPort recovery;
    RecordingHistory history;
    RecordingNotifications notifications;
    RecordingPageChanges page;
    Platform::WorkspaceProfilePathPolicy pathPolicy;
};

} // namespace

class NextWorkspaceInterruptedRecoveryFlowTests final : public QObject
{
    Q_OBJECT

private slots:
    void scansExactlyOneDirectoryByProfilePreferenceDefaultPrecedence();
    void noCandidatesAndCandidateCancelStopBeforeDirtyResolution();
    void selectionShowsPathsUsesSafeCancelAndRetriesExistingDestination();
    void dirtySaveAndDiscardResolvePageBeforeWorkspaceAndCopy();
    void dirtyCancelAndFailedSaveLeaveRecoveryFilesUntouched();
    void pendingSetupBlocksBeforeScanPromptAndDirtyResolution();
    void copyAndOpenFailuresRetainArtifactsAndActiveStateHistory();
    void scannerUsesStrictPatternIncludesNewlineAndExcludesLookalikes();
    void copyUsesNoReplaceAndRetainsSource();
};

void NextWorkspaceInterruptedRecoveryFlowTests::
scansExactlyOneDirectoryByProfilePreferenceDefaultPrecedence()
{
    Harness active;
    QVERIFY(active.directory.isValid());
    active.openActiveProfile(active.directory.filePath("active.tps"));
    active.directoryPreferences.remembered =
        Application::DirectoryPreferencePath("remembered-directory");
    active.recovery.candidates.clear();
    QCOMPARE(active.run().status,
             Application::InterruptedWorkspaceRecoveryStatus::NoCandidates);
    QCOMPARE(active.recovery.scanCalls, 1);
    QCOMPARE(active.recovery.scannedDirectory.value(),
             std::string("active-profile-parent"));
    QCOMPARE(active.recovery.profileLocation,
             std::optional<Application::WorkspaceLocation>(
                 active.runtime.workspaceState().snapshot().session()->location()
                 ));

    Harness remembered;
    QVERIFY(remembered.directory.isValid());
    remembered.directoryPreferences.remembered =
        Application::DirectoryPreferencePath("remembered-directory");
    remembered.recovery.candidates.clear();
    QCOMPARE(remembered.run().status,
             Application::InterruptedWorkspaceRecoveryStatus::NoCandidates);
    QCOMPARE(remembered.recovery.scannedDirectory.value(),
             std::string("remembered-directory"));

    Harness fallback;
    QVERIFY(fallback.directory.isValid());
    fallback.recovery.candidates.clear();
    QCOMPARE(fallback.run().status,
             Application::InterruptedWorkspaceRecoveryStatus::NoCandidates);
    QCOMPARE(fallback.recovery.scannedDirectory,
             fallback.recovery.defaultDirectory);
}

void NextWorkspaceInterruptedRecoveryFlowTests::
noCandidatesAndCandidateCancelStopBeforeDirtyResolution()
{
    Harness noCandidates;
    QVERIFY(noCandidates.directory.isValid());
    noCandidates.recovery.candidates.clear();
    noCandidates.openActiveProfile(noCandidates.directory.filePath("active.tps"));
    noCandidates.page.dirty = true;
    QCOMPARE(noCandidates.run().status,
             Application::InterruptedWorkspaceRecoveryStatus::NoCandidates);
    QCOMPARE(noCandidates.prompt.messageCalls, 1);
    QCOMPARE(noCandidates.prompt.lastMessage->severity,
             Application::PromptSeverity::Information);
    QCOMPARE(noCandidates.prompt.unsavedRequests.size(), std::size_t(0));
    QCOMPARE(noCandidates.dialog.saveRequests.size(), std::size_t(0));

    Harness canceled;
    QVERIFY(canceled.directory.isValid());
    canceled.openActiveProfile(canceled.directory.filePath("active.tps"));
    QVERIFY(canceled.runtime.workspaceState().markDirty());
    canceled.page.dirty = true;
    canceled.recovery.candidates.push_back(
        candidate(QStringLiteral("scan-directory"),
                  QStringLiteral("Other Teacher.tps"))
        );
    const auto outcome = canceled.run();
    QCOMPARE(outcome.status,
             Application::InterruptedWorkspaceRecoveryStatus::Canceled);
    QCOMPARE(canceled.prompt.candidateRequests.size(), std::size_t(1));
    const auto& request = canceled.prompt.candidateRequests.front();
    QVERIFY(request.defaultActionId.has_value());
    QVERIFY(request.escapeActionId.has_value());
    QCOMPARE(request.defaultActionId->value(), std::string("cancel"));
    QCOMPARE(request.escapeActionId->value(), std::string("cancel"));
    QCOMPARE(request.actions.size(), std::size_t(3));
    QVERIFY(request.informativeText.find("Teacher.tps") != std::string::npos);
    QVERIFY(request.informativeText.find(
                canceled.recovery.candidates.front().sourcePath.value()
                )
        != std::string::npos);
    QCOMPARE(canceled.dialog.saveRequests.size(), std::size_t(0));
    QCOMPARE(canceled.prompt.unsavedRequests.size(), std::size_t(0));
    QCOMPARE(canceled.recovery.copyCalls, 0);
}

void NextWorkspaceInterruptedRecoveryFlowTests::
selectionShowsPathsUsesSafeCancelAndRetriesExistingDestination()
{
    Harness harness;
    QVERIFY(harness.directory.isValid());
    const QString directory = harness.directory.path();
    harness.directoryPreferences.remembered =
        Application::DirectoryPreferencePath(utf8(directory));
    const auto first = candidate(directory, QStringLiteral("First.tps"));
    const auto second = candidate(directory, QStringLiteral("Second.tps"));
    harness.recovery.candidates = {first, second};
    harness.recovery.existingPaths.push_back(second.targetPath.value());
    const QString occupied = harness.directory.filePath("occupied.tps");
    const QString raced = harness.directory.filePath("raced.tps");
    const QString destination = harness.directory.filePath("recovered.tps");
    const auto normalizedOccupied = harness.pathPolicy.normalizeNewProfilePath(
        Application::FileDialogPath(utf8(occupied))
        );
    QVERIFY(normalizedOccupied);
    harness.recovery.existingPaths.push_back(
        normalizedOccupied.value().value()
        );
    harness.chooseCandidate(1);
    harness.recovery.collideOnNextCopy = true;
    harness.selectDestination(occupied);
    harness.selectDestination(raced);
    harness.selectDestination(destination);

    const auto outcome = harness.run();

    QCOMPARE(outcome.status,
             Application::InterruptedWorkspaceRecoveryStatus::Recovered);
    QCOMPARE(harness.recovery.copied.size(), std::size_t(2));
    QCOMPARE(harness.recovery.copied.back().sourcePath, second.sourcePath);
    QCOMPARE(harness.dialog.saveRequests.size(), std::size_t(3));
    QCOMPARE(harness.dialog.saveRequests.front().initialDirectory.value(),
             utf8(directory));
    QCOMPARE(harness.dialog.saveRequests.front().suggestedFileName,
             std::string("Recovered Second.tps"));
    QCOMPARE(harness.dialog.saveRequests.front().defaultSuffix,
             std::string("tps"));
    QVERIFY(!harness.dialog.saveRequests.front().confirmOverwrite);
    QVERIFY(harness.prompt.candidateRequests.front().informativeText.find(
                second.sourcePath.value()
                ) != std::string::npos);
    QVERIFY(harness.prompt.candidateRequests.front().informativeText.find(
                second.targetPath.value()
                ) != std::string::npos);
    QVERIFY(!harness.recovery.createdPaths.empty());
    QVERIFY(harness.notifications.requests.size() >= 1);
    QCOMPARE(harness.notifications.requests.front().severity,
             Application::NotificationSeverity::Warning);
    QCOMPARE(harness.gateway.opens, 1);
    QCOMPARE(harness.recovery.copyCalls, 2);
    QCOMPARE(harness.history.saved.size(), std::size_t(1));
    QVERIFY(harness.recovery.pathExists(second.sourcePath));
    QCOMPARE(harness.runtime.workspaceState().snapshot().session(),
             outcome.session);
}

void NextWorkspaceInterruptedRecoveryFlowTests::
dirtySaveAndDiscardResolvePageBeforeWorkspaceAndCopy()
{
    Harness save;
    QVERIFY(save.directory.isValid());
    save.openActiveProfile(save.directory.filePath("active.tps"));
    QVERIFY(save.runtime.workspaceState().markDirty());
    save.page.dirty = true;
    save.prompt.unsavedAction = Application::UnsavedChangesAction::Save;
    save.chooseCandidate();
    const QString saveDestination = save.directory.filePath("saved-recovery.tps");
    save.selectDestination(saveDestination);
    QCOMPARE(save.run().status,
             Application::InterruptedWorkspaceRecoveryStatus::Recovered);
    QCOMPARE(save.dialog.saveRequests.front().suggestedFileName,
             std::string("Teacher.tps"));
    QCOMPARE(save.page.saves, 1);
    QCOMPARE(save.gateway.saves, 1);
    QVERIFY(save.events.values.size() >= 6);
    const auto candidatePrompt = std::find(
        save.events.values.begin(), save.events.values.end(), "candidate-prompt");
    const auto saveDialog = std::find(
        save.events.values.begin(), save.events.values.end(), "save-dialog");
    const auto dirtyPrompt = std::find(
        save.events.values.begin(), save.events.values.end(), "unsaved-prompt");
    const auto pageSave = std::find(
        save.events.values.begin(), save.events.values.end(), "page-save");
    const auto workspaceSave = std::find(
        save.events.values.begin(), save.events.values.end(), "workspace-save");
    const auto copy = std::find(
        save.events.values.begin(), save.events.values.end(), "copy");
    const auto open = std::find(
        save.events.values.begin(), save.events.values.end(), "open");
    QVERIFY(candidatePrompt < saveDialog);
    QVERIFY(saveDialog < dirtyPrompt);
    QVERIFY(dirtyPrompt < pageSave);
    QVERIFY(pageSave < workspaceSave);
    QVERIFY(workspaceSave < copy);
    QVERIFY(copy < open);

    Harness discard;
    QVERIFY(discard.directory.isValid());
    discard.openActiveProfile(discard.directory.filePath("active.tps"));
    QVERIFY(discard.runtime.workspaceState().markDirty());
    discard.page.dirty = true;
    discard.prompt.unsavedAction = Application::UnsavedChangesAction::Discard;
    discard.chooseCandidate();
    discard.selectDestination(
        discard.directory.filePath("discarded-recovery.tps")
        );
    QCOMPARE(discard.run().status,
             Application::InterruptedWorkspaceRecoveryStatus::Recovered);
    QCOMPARE(discard.page.discards, 1);
    QCOMPARE(discard.gateway.discards, 1);
    const auto pageDiscard = std::find(
        discard.events.values.begin(), discard.events.values.end(), "page-discard");
    const auto workspaceDiscard = std::find(
        discard.events.values.begin(), discard.events.values.end(), "workspace-discard");
    const auto copyAfterDiscard = std::find(
        discard.events.values.begin(), discard.events.values.end(), "copy");
    QVERIFY(pageDiscard < workspaceDiscard);
    QVERIFY(workspaceDiscard < copyAfterDiscard);
}

void NextWorkspaceInterruptedRecoveryFlowTests::
dirtyCancelAndFailedSaveLeaveRecoveryFilesUntouched()
{
    Harness canceled;
    QVERIFY(canceled.directory.isValid());
    canceled.openActiveProfile(canceled.directory.filePath("active.tps"));
    QVERIFY(canceled.runtime.workspaceState().markDirty());
    canceled.page.dirty = true;
    canceled.chooseCandidate();
    canceled.selectDestination(canceled.directory.filePath("cancelled.tps"));
    canceled.prompt.unsavedAction = Application::UnsavedChangesAction::Cancel;
    QCOMPARE(canceled.run().status,
             Application::InterruptedWorkspaceRecoveryStatus::Canceled);
    QCOMPARE(canceled.recovery.copyCalls, 0);
    QCOMPARE(canceled.gateway.opens, 0);
    QCOMPARE(canceled.history.saved.size(), std::size_t(0));
    QCOMPARE(canceled.runtime.workspaceState().snapshot().unsavedState(),
             Application::WorkspaceUnsavedState::Dirty);

    Harness failedSave;
    QVERIFY(failedSave.directory.isValid());
    failedSave.openActiveProfile(failedSave.directory.filePath("active.tps"));
    QVERIFY(failedSave.runtime.workspaceState().markDirty());
    failedSave.page.dirty = true;
    failedSave.page.failSave = true;
    failedSave.chooseCandidate();
    failedSave.selectDestination(failedSave.directory.filePath("failed-save.tps"));
    failedSave.prompt.unsavedAction = Application::UnsavedChangesAction::Save;
    QCOMPARE(failedSave.run().status,
             Application::InterruptedWorkspaceRecoveryStatus::Failed);
    QCOMPARE(failedSave.recovery.copyCalls, 0);
    QCOMPARE(failedSave.gateway.opens, 0);
    QCOMPARE(failedSave.history.saved.size(), std::size_t(0));
    QCOMPARE(failedSave.runtime.workspaceState().snapshot().unsavedState(),
             Application::WorkspaceUnsavedState::Dirty);
    QVERIFY(failedSave.recovery.createdPaths.empty());

    Harness saveDialogCanceled;
    QVERIFY(saveDialogCanceled.directory.isValid());
    saveDialogCanceled.openActiveProfile(
        saveDialogCanceled.directory.filePath("active.tps")
        );
    QVERIFY(saveDialogCanceled.runtime.workspaceState().markDirty());
    saveDialogCanceled.page.dirty = true;
    saveDialogCanceled.chooseCandidate();
    QCOMPARE(saveDialogCanceled.run().status,
             Application::InterruptedWorkspaceRecoveryStatus::Canceled);
    QCOMPARE(saveDialogCanceled.prompt.unsavedRequests.size(), std::size_t(0));
    QCOMPARE(saveDialogCanceled.recovery.copyCalls, 0);
    QVERIFY(saveDialogCanceled.recovery.createdPaths.empty());
}

void NextWorkspaceInterruptedRecoveryFlowTests::
pendingSetupBlocksBeforeScanPromptAndDirtyResolution()
{
    Harness harness;
    QVERIFY(harness.directory.isValid());
    harness.openActiveProfile(harness.directory.filePath("active.tps"));
    QVERIFY(harness.runtime.workspaceState().markDirty());
    harness.page.dirty = true;

    const auto outcome = harness.run([] { return true; });

    QCOMPARE(outcome.status,
             Application::InterruptedWorkspaceRecoveryStatus::Blocked);
    QCOMPARE(harness.recovery.scanCalls, 0);
    QCOMPARE(harness.prompt.candidateRequests.size(), std::size_t(0));
    QCOMPARE(harness.prompt.unsavedRequests.size(), std::size_t(0));
    QCOMPARE(harness.dialog.saveRequests.size(), std::size_t(0));
    QCOMPARE(harness.recovery.copyCalls, 0);
    QCOMPARE(harness.notifications.requests.size(), std::size_t(1));
    QCOMPARE(harness.notifications.requests.front().severity,
             Application::NotificationSeverity::Warning);
}

void NextWorkspaceInterruptedRecoveryFlowTests::
copyAndOpenFailuresRetainArtifactsAndActiveStateHistory()
{
    Harness copyFailure;
    QVERIFY(copyFailure.directory.isValid());
    const QString activePath = copyFailure.directory.filePath("active.tps");
    copyFailure.openActiveProfile(activePath);
    const auto beforeCopyFailure = copyFailure.runtime.workspaceState().snapshot();
    copyFailure.history.current.paths = {
        Application::RecentWorkspacePath("older-profile.tps")
    };
    copyFailure.chooseCandidate();
    const QString partialPath = copyFailure.directory.filePath("partial.tps");
    const auto normalizedPartial = copyFailure.pathPolicy.normalizeNewProfilePath(
        Application::FileDialogPath(utf8(partialPath))
        );
    QVERIFY(normalizedPartial);
    copyFailure.selectDestination(partialPath);
    copyFailure.recovery.copyError = Domain::OperationError{
        .code = Domain::ErrorCode::Technical,
        .message = "injected copy failure",
        .recoverable = true
    };
    copyFailure.recovery.copyFailureCreatesDestination = true;
    const auto copied = copyFailure.run();
    QCOMPARE(copied.status,
             Application::InterruptedWorkspaceRecoveryStatus::Failed);
    QCOMPARE(copyFailure.runtime.workspaceState().snapshot(), beforeCopyFailure);
    QCOMPARE(copyFailure.history.saved.size(), std::size_t(0));
    QCOMPARE(copyFailure.gateway.opens, 0);
    QVERIFY(copyFailure.recovery.pathExists(
        Application::FileDialogPath(normalizedPartial.value().value())
        ));
    QVERIFY(copied.message.find(normalizedPartial.value().value())
            != std::string::npos);
    QVERIFY(copied.message.find(
                copyFailure.recovery.candidates.front().sourcePath.value()
                ) != std::string::npos);

    Harness openFailure;
    QVERIFY(openFailure.directory.isValid());
    const QString activeOpenPath = openFailure.directory.filePath("active.tps");
    openFailure.openActiveProfile(activeOpenPath);
    const auto beforeOpenFailure = openFailure.runtime.workspaceState().snapshot();
    const QString destination = openFailure.directory.filePath("copied.tps");
    const auto normalizedDestination =
        openFailure.pathPolicy.normalizeNewProfilePath(
            Application::FileDialogPath(utf8(destination))
            );
    QVERIFY(normalizedDestination);
    openFailure.history.current.paths = {
        Application::RecentWorkspacePath(
            normalizedDestination.value().value()
            ),
        Application::RecentWorkspacePath("older-profile.tps")
    };
    const Application::RecentWorkspaceHistory beforeHistory =
        openFailure.history.current;
    openFailure.gateway.openError = Domain::OperationError{
        .code = Domain::ErrorCode::NotFound,
        .message = "injected destination disappearance",
        .recoverable = true
    };
    openFailure.chooseCandidate();
    openFailure.selectDestination(destination);
    const auto opened = openFailure.run();
    QCOMPARE(opened.status,
             Application::InterruptedWorkspaceRecoveryStatus::Failed);
    QCOMPARE(openFailure.runtime.workspaceState().snapshot(), beforeOpenFailure);
    QCOMPARE(openFailure.history.current, beforeHistory);
    QCOMPARE(openFailure.history.saved.size(), std::size_t(0));
    QVERIFY(openFailure.recovery.pathExists(
        Application::FileDialogPath(normalizedDestination.value().value())
        ));
    QVERIFY(openFailure.recovery.pathExists(
        openFailure.recovery.candidates.front().sourcePath
        ));
    QVERIFY(opened.message.find(destination.toUtf8().toStdString())
            != std::string::npos);
    QVERIFY(opened.message.find(
                openFailure.recovery.candidates.front().sourcePath.value()
                ) != std::string::npos);
}

void NextWorkspaceInterruptedRecoveryFlowTests::
scannerUsesStrictPatternIncludesNewlineAndExcludesLookalikes()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Platform::QtWorkspaceInterruptedRecoveryPort port;
    const QString targetName = QStringLiteral("teacher\nname.tps");
    const QString uuid = QStringLiteral(
        "deadBEEF-1234-0000-abcd-0123456789AB"
        );
    const QString validSource = QDir(directory.path()).filePath(
        QStringLiteral(".%1.workspace-create-%2.recovery")
            .arg(targetName, uuid)
        );
    const QString firstTargetName = QStringLiteral("alpha.tps");
    const QString firstSource = QDir(directory.path()).filePath(
        QStringLiteral(".%1.workspace-create-%2.recovery")
            .arg(firstTargetName, uuid)
        );
    const QString lastTargetName = QStringLiteral("zeta.tps");
    const QString lastSource = QDir(directory.path()).filePath(
        QStringLiteral(".%1.workspace-create-%2.recovery")
            .arg(lastTargetName, uuid)
        );
    QVERIFY(writeBytes(firstSource, QByteArrayLiteral("first")));
    QVERIFY(writeBytes(validSource, QByteArrayLiteral("valid")));
    QVERIFY(writeBytes(lastSource, QByteArrayLiteral("last")));
    QVERIFY(writeBytes(
        QDir(directory.path()).filePath(
            QStringLiteral(".bad.tps.workspace-create-12345678-1234-1234-1234-12345678901.recovery")
            ),
        QByteArrayLiteral("bad uuid")
        ));
    QVERIFY(writeBytes(
        QDir(directory.path()).filePath(
            QStringLiteral("not-hidden.tps.workspace-create-12345678-1234-1234-1234-123456789012.recovery")
            ),
        QByteArrayLiteral("not hidden")
        ));
    QVERIFY(writeBytes(
        QDir(directory.path()).filePath(
            QStringLiteral(".setup.tps.initial-setup-12345678-1234-1234-1234-123456789012.backup")
            ),
        QByteArrayLiteral("setup backup")
        ));
    const QString nested = QDir(directory.path()).filePath("nested");
    QVERIFY(QDir().mkpath(nested));
    QVERIFY(writeBytes(
        QDir(nested).filePath(
            QStringLiteral(".nested.tps.workspace-create-12345678-1234-1234-1234-123456789012.recovery")
            ),
        QByteArrayLiteral("nested")
        ));
    const QString directoryLookalike = QDir(directory.path()).filePath(
        QStringLiteral(".directory.tps.workspace-create-12345678-1234-1234-1234-123456789012.recovery")
        );
    QVERIFY(QDir().mkpath(directoryLookalike));
    const QString symlinkPath = QDir(directory.path()).filePath(
        QStringLiteral(".symlink.tps.workspace-create-12345678-1234-1234-1234-123456789012.recovery")
        );
    QVERIFY(QFile::link(validSource, symlinkPath));

    const auto scanned = port.candidatesIn(
        Application::FileDialogPath(utf8(directory.path()))
        );
    QVERIFY(scanned);
    QCOMPARE(scanned.value().size(), std::size_t(3));
    QCOMPARE(pathText(scanned.value().at(0).sourcePath),
             QFileInfo(firstSource).absoluteFilePath());
    QCOMPARE(pathText(scanned.value().at(0).targetPath),
             QDir(directory.path()).filePath(firstTargetName));
    QCOMPARE(scanned.value().at(0).targetFileName,
             utf8(firstTargetName));
    QCOMPARE(pathText(scanned.value().at(1).sourcePath),
             QFileInfo(validSource).absoluteFilePath());
    QCOMPARE(pathText(scanned.value().at(1).targetPath),
             QDir(directory.path()).filePath(targetName));
    QCOMPARE(scanned.value().at(1).targetFileName,
             utf8(QFileInfo(QDir(directory.path()).filePath(targetName)).fileName()));
    QCOMPARE(pathText(scanned.value().at(2).sourcePath),
             QFileInfo(lastSource).absoluteFilePath());
    QCOMPARE(pathText(scanned.value().at(2).targetPath),
             QDir(directory.path()).filePath(lastTargetName));
    QCOMPARE(scanned.value().at(2).targetFileName,
             utf8(lastTargetName));
}

void NextWorkspaceInterruptedRecoveryFlowTests::
copyUsesNoReplaceAndRetainsSource()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Platform::QtWorkspaceInterruptedRecoveryPort port;
    const QString targetName = QStringLiteral("source.tps");
    const QString uuid = QStringLiteral(
        "12345678-1234-1234-1234-123456789012"
        );
    const QString source = QDir(directory.path()).filePath(
        QStringLiteral(".%1.workspace-create-%2.recovery")
            .arg(targetName, uuid)
        );
    const QByteArray sourceBytes("profile contents");
    QVERIFY(writeBytes(source, sourceBytes));
    const Application::InterruptedWorkspaceRecoveryCandidate selected{
        Application::FileDialogPath(utf8(source)),
        Application::FileDialogPath(
            utf8(QDir(directory.path()).filePath(targetName))
            ),
        utf8(targetName)
    };

    const QString occupied = QDir(directory.path()).filePath("occupied.tps");
    QVERIFY(writeBytes(occupied, QByteArrayLiteral("keep existing")));
    const auto conflict = port.copyCandidateNoReplace(
        selected,
        Application::WorkspaceLocation(utf8(occupied))
        );
    QVERIFY(!conflict);
    QCOMPARE(conflict.error().code, Domain::ErrorCode::Conflict);
    QCOMPARE(readBytes(occupied), QByteArrayLiteral("keep existing"));

    const QString destination = QDir(directory.path()).filePath("copy.tps");
    const auto copied = port.copyCandidateNoReplace(
        selected,
        Application::WorkspaceLocation(utf8(destination))
        );
    QVERIFY(copied);
    QCOMPARE(readBytes(destination), sourceBytes);
    QCOMPARE(readBytes(source), sourceBytes);
}

QTEST_MAIN(NextWorkspaceInterruptedRecoveryFlowTests)
#include "next_workspace_interrupted_recovery_flow_tests.moc"
