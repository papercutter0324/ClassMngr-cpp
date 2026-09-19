#include "next/platform/legacy_workspace_gateway.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <type_traits>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;
using namespace ClassMngr::Next::Platform;

namespace
{

LegacyWorkspaceHandle legacyHandle(
    const char* workspaceId,
    const char* location
    )
{
    return LegacyWorkspaceHandle(workspaceId, location);
}

WorkspaceSession workspaceSession(
    const char* workspaceId,
    const char* location
    )
{
    return WorkspaceSession(
        *WorkspaceId::fromString(workspaceId),
        WorkspaceLocation(location)
        );
}

class FakeLegacyWorkspacePort final : public LegacyWorkspacePort
{
public:
    [[nodiscard]] LegacyWorkspaceResult<LegacyWorkspaceHandle> openDatabase(
        const std::string& databasePath
        ) override
    {
        ++openCalls;
        lastOpenPath = databasePath;
        if (!openResponse.has_value())
        {
            return LegacyWorkspaceResult<LegacyWorkspaceHandle>::failure(
                LegacyWorkspaceError{"Open response was not configured."}
                );
        }

        const auto result = *openResponse;
        if (result && updateOpenObservation)
        {
            open = true;
            currentPath = result.value().location();
        }
        return result;
    }

    [[nodiscard]] LegacyWorkspaceResult<LegacyWorkspaceHandle>
    createOrOpenDatabase(
        const std::string& databasePath
        ) override
    {
        ++createCalls;
        lastCreatePath = databasePath;
        if (!createResponse.has_value())
        {
            return LegacyWorkspaceResult<LegacyWorkspaceHandle>::failure(
                LegacyWorkspaceError{"Create response was not configured."}
                );
        }

        const auto result = *createResponse;
        if (result && updateCreateObservation)
        {
            open = true;
            currentPath = result.value().location();
        }
        return result;
    }

    [[nodiscard]] LegacyWorkspaceStatus closeDatabase(
        const LegacyWorkspaceHandle& handle
        ) override
    {
        ++closeCalls;
        lastCloseHandle = handle;
        if (!closeResponse.has_value())
        {
            return LegacyWorkspaceStatus::failure(
                LegacyWorkspaceError{"Close response was not configured."}
                );
        }

        const auto result = *closeResponse;
        if (result && updateCloseObservation)
        {
            open = false;
            currentPath.clear();
        }
        return result;
    }

    [[nodiscard]] bool hasOpenDatabase() const override
    {
        ++hasOpenCalls;
        return open;
    }

    [[nodiscard]] std::string currentDatabasePath() const override
    {
        ++currentPathCalls;
        return currentPath;
    }

    [[nodiscard]] LegacyWorkspaceStatus saveDatabase(
        const LegacyWorkspaceHandle& handle
        ) override
    {
        ++saveCalls;
        lastSaveHandle = handle;
        if (!saveResponse.has_value())
        {
            return LegacyWorkspaceStatus::failure(
                LegacyWorkspaceError{"Save response was not configured."}
                );
        }
        return *saveResponse;
    }

    [[nodiscard]] LegacyWorkspaceResult<LegacyWorkspaceHandle>
    saveDatabaseAs(
        const LegacyWorkspaceHandle& handle,
        const std::string& destinationPath
        ) override
    {
        ++saveAsCalls;
        lastSaveAsHandle = handle;
        lastSaveAsDestination = destinationPath;
        if (!saveAsResponse.has_value())
        {
            return LegacyWorkspaceResult<LegacyWorkspaceHandle>::failure(
                LegacyWorkspaceError{
                    "Save-as response was not configured."
                }
                );
        }

        const auto result = *saveAsResponse;
        if (result && updateSaveAsObservation)
        {
            open = true;
            currentPath = result.value().location();
        }
        return result;
    }

    [[nodiscard]] LegacyWorkspaceResult<std::string> exportDatabaseAs(
        const LegacyWorkspaceHandle& handle,
        const std::string& destinationPath
        ) override
    {
        ++exportCalls;
        lastExportHandle = handle;
        lastExportDestination = destinationPath;
        if (!exportResponse.has_value())
        {
            return LegacyWorkspaceResult<std::string>::failure(
                LegacyWorkspaceError{
                    "Export response was not configured."
                }
                );
        }
        return *exportResponse;
    }

    int openCalls = 0;
    int createCalls = 0;
    int closeCalls = 0;
    mutable int hasOpenCalls = 0;
    mutable int currentPathCalls = 0;
    int saveCalls = 0;
    int saveAsCalls = 0;
    int exportCalls = 0;

    std::string lastOpenPath;
    std::string lastCreatePath;
    LegacyWorkspaceHandle lastCloseHandle;
    LegacyWorkspaceHandle lastSaveHandle;
    LegacyWorkspaceHandle lastSaveAsHandle;
    std::string lastSaveAsDestination;
    LegacyWorkspaceHandle lastExportHandle;
    std::string lastExportDestination;

    std::optional<LegacyWorkspaceResult<LegacyWorkspaceHandle>> openResponse;
    std::optional<LegacyWorkspaceResult<LegacyWorkspaceHandle>> createResponse;
    std::optional<LegacyWorkspaceStatus> closeResponse;
    std::optional<LegacyWorkspaceStatus> saveResponse;
    std::optional<LegacyWorkspaceResult<LegacyWorkspaceHandle>> saveAsResponse;
    std::optional<LegacyWorkspaceResult<std::string>> exportResponse;

    bool open = false;
    std::string currentPath;
    bool updateOpenObservation = true;
    bool updateCreateObservation = true;
    bool updateCloseObservation = true;
    bool updateSaveAsObservation = true;
};

} // namespace

class NextPlatformLegacyWorkspaceGatewayTests final : public QObject
{
    Q_OBJECT

private slots:
    void adapterSurfaceIsInjectableAndApplicationBased();
    void openMapsLocationAndPreservesTypedIdentity();
    void createUsesTheExplicitCreateOrOpenHook();
    void createOrOpenLegacyErrorTextMapsToStructuredFailure();
    void emptyCreateOrOpenLegacyErrorUsesOperationFallback();
    void createOrOpenInvalidReturnedHandleIsRejected();
    void createOrOpenInvalidReturnedLocationIsRejected();
    void createOrOpenSuccessWithBadObservationIsRejected();
    void blankCreateLocationIsRejectedBeforePortCall();
    void blankOpenLocationIsRejectedBeforePortCall();
    void blankSessionFieldsAreRejectedBeforePortCall();
    void blankSaveAsDestinationIsRejectedBeforePortCall();
    void blankExportDestinationIsRejectedBeforePortCall();
    void legacyErrorTextMapsToStructuredFailure();
    void emptyLegacyErrorUsesOperationFallback();
    void invalidReturnedHandleIsRejected();
    void invalidReturnedLocationIsRejected();
    void returnedLocationMustMatchOpenObservation();
    void closeMapsSessionAndRequiresClosedObservation();
    void closeSuccessWithOpenObservationIsRejected();
    void closeFailurePreservesLegacyErrorAndOpenState();
    void saveMapsSessionAndValidatesOpenObservation();
    void saveSuccessWithBadObservationIsRejected();
    void saveFailurePreservesLegacyError();
    void saveAsMapsDestinationAndPreservesWorkspaceIdentity();
    void saveAsLegacyErrorTextMapsToStructuredFailure();
    void saveAsSuccessWithBadObservationIsRejected();
    void saveAsRejectsReturnedIdentityChange();
    void saveAsRejectsReturnedLocation();
    void exportMapsDestinationWithoutChangingOpenWorkspace();
    void exportSuccessWithBadObservationIsRejected();
    void exportRejectsInvalidReturnedLocation();
    void exportFailurePreservesLegacyError();
    void gatewayDoesNotRetainMutableSessionState();
};

void NextPlatformLegacyWorkspaceGatewayTests::adapterSurfaceIsInjectableAndApplicationBased()
{
    static_assert(
        std::is_base_of_v<WorkspaceGateway, LegacyWorkspaceGateway>
        );
    static_assert(!std::is_copy_constructible_v<LegacyWorkspaceGateway>);
    static_assert(std::is_copy_constructible_v<LegacyWorkspaceHandle>);
    static_assert(
        std::is_copy_constructible_v<LegacyWorkspaceResult<LegacyWorkspaceHandle>>
        );

    FakeLegacyWorkspacePort port;
    LegacyWorkspaceGateway gateway(port);
    QVERIFY(&gateway != nullptr);
}

void NextPlatformLegacyWorkspaceGatewayTests::openMapsLocationAndPreservesTypedIdentity()
{
    FakeLegacyWorkspacePort port;
    port.openResponse = LegacyWorkspaceResult<LegacyWorkspaceHandle>::success(
        legacyHandle("workspace-opened", "C:/normalized/opened.tps")
        );
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.openWorkspace(
        OpenWorkspaceRequest{
            WorkspaceLocation("C:/requested/opened.tps")
        }
        );

    QVERIFY(result);
    QCOMPARE(port.openCalls, 1);
    QCOMPARE(port.createCalls, 0);
    QCOMPARE(port.lastOpenPath, std::string("C:/requested/opened.tps"));
    QCOMPARE(port.hasOpenCalls, 1);
    QCOMPARE(port.currentPathCalls, 1);
    QCOMPARE(
        result.value().workspaceId().value(),
        std::string("workspace-opened")
        );
    QCOMPARE(
        result.value().location().value(),
        std::string("C:/normalized/opened.tps")
        );
}

void NextPlatformLegacyWorkspaceGatewayTests::createUsesTheExplicitCreateOrOpenHook()
{
    FakeLegacyWorkspacePort port;
    port.createResponse =
        LegacyWorkspaceResult<LegacyWorkspaceHandle>::success(
            legacyHandle("workspace-created", "C:/created.tps")
            );
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.createWorkspace(
        CreateWorkspaceRequest{WorkspaceLocation("C:/created.tps")}
        );

    QVERIFY(result);
    QCOMPARE(port.createCalls, 1);
    QCOMPARE(port.openCalls, 0);
    QCOMPARE(port.lastCreatePath, std::string("C:/created.tps"));
    QCOMPARE(
        result.value().workspaceId().value(),
        std::string("workspace-created")
        );
}

void NextPlatformLegacyWorkspaceGatewayTests::createOrOpenLegacyErrorTextMapsToStructuredFailure()
{
    FakeLegacyWorkspacePort port;
    port.createResponse =
        LegacyWorkspaceResult<LegacyWorkspaceHandle>::failure(
            LegacyWorkspaceError{
                "The legacy workspace could not be created or opened."
            }
            );
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.createWorkspace(
        CreateWorkspaceRequest{WorkspaceLocation("C:/missing.tps")}
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Technical);
    QCOMPARE(
        result.error().message,
        std::string("The legacy workspace could not be created or opened.")
        );
    QVERIFY(!result.error().recoverable);
    QCOMPARE(port.createCalls, 1);
    QCOMPARE(port.openCalls, 0);
    QCOMPARE(port.lastCreatePath, std::string("C:/missing.tps"));
    QCOMPARE(port.hasOpenCalls, 0);
}

void NextPlatformLegacyWorkspaceGatewayTests::emptyCreateOrOpenLegacyErrorUsesOperationFallback()
{
    FakeLegacyWorkspacePort port;
    port.createResponse =
        LegacyWorkspaceResult<LegacyWorkspaceHandle>::failure(
            LegacyWorkspaceError{}
            );
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.createWorkspace(
        CreateWorkspaceRequest{WorkspaceLocation("C:/missing.tps")}
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Technical);
    QCOMPARE(
        result.error().message,
        std::string("Creating the workspace failed at the legacy boundary.")
        );
    QVERIFY(!result.error().recoverable);
    QCOMPARE(port.createCalls, 1);
    QCOMPARE(port.hasOpenCalls, 0);
}

void NextPlatformLegacyWorkspaceGatewayTests::createOrOpenInvalidReturnedHandleIsRejected()
{
    FakeLegacyWorkspacePort port;
    port.createResponse = LegacyWorkspaceResult<LegacyWorkspaceHandle>::success(
        legacyHandle("", "C:/invalid-id.tps")
        );
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.createWorkspace(
        CreateWorkspaceRequest{WorkspaceLocation("C:/invalid-id.tps")}
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QCOMPARE(
        result.error().message,
        std::string("Legacy workspace returned an invalid handle or location.")
        );
    QCOMPARE(port.createCalls, 1);
    QCOMPARE(port.hasOpenCalls, 0);
}

void NextPlatformLegacyWorkspaceGatewayTests::createOrOpenInvalidReturnedLocationIsRejected()
{
    FakeLegacyWorkspacePort port;
    port.createResponse = LegacyWorkspaceResult<LegacyWorkspaceHandle>::success(
        legacyHandle("workspace-invalid-location", " \t\r\n")
        );
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.createWorkspace(
        CreateWorkspaceRequest{WorkspaceLocation("C:/invalid-location.tps")}
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QCOMPARE(
        result.error().message,
        std::string("Legacy workspace returned an invalid handle or location.")
        );
    QCOMPARE(port.createCalls, 1);
    QCOMPARE(port.hasOpenCalls, 0);
}

void NextPlatformLegacyWorkspaceGatewayTests::createOrOpenSuccessWithBadObservationIsRejected()
{
    FakeLegacyWorkspacePort port;
    port.open = true;
    port.currentPath = "C:/observed.tps";
    port.createResponse =
        LegacyWorkspaceResult<LegacyWorkspaceHandle>::success(
            legacyHandle("workspace-created", "C:/created.tps")
            );
    port.updateCreateObservation = false;
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.createWorkspace(
        CreateWorkspaceRequest{WorkspaceLocation("C:/created.tps")}
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Technical);
    QCOMPARE(
        result.error().message,
        std::string(
            "Opening the workspace returned a location that does not match the open workspace."
            )
        );
    QCOMPARE(port.createCalls, 1);
    QCOMPARE(port.openCalls, 0);
    QCOMPARE(port.hasOpenCalls, 1);
    QCOMPARE(port.currentPathCalls, 1);
}

void NextPlatformLegacyWorkspaceGatewayTests::blankCreateLocationIsRejectedBeforePortCall()
{
    FakeLegacyWorkspacePort port;
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.createWorkspace(
        CreateWorkspaceRequest{WorkspaceLocation{}}
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QCOMPARE(
        result.error().message,
        std::string("Workspace location must not be empty or whitespace-only.")
        );
    QCOMPARE(port.createCalls, 0);
    QCOMPARE(port.openCalls, 0);
}

void NextPlatformLegacyWorkspaceGatewayTests::blankOpenLocationIsRejectedBeforePortCall()
{
    FakeLegacyWorkspacePort port;
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.openWorkspace(
        OpenWorkspaceRequest{WorkspaceLocation(" \t\r\n")}
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QCOMPARE(
        result.error().message,
        std::string("Workspace location must not be empty or whitespace-only.")
        );
    QCOMPARE(port.createCalls, 0);
    QCOMPARE(port.openCalls, 0);
}

void NextPlatformLegacyWorkspaceGatewayTests::blankSessionFieldsAreRejectedBeforePortCall()
{
    FakeLegacyWorkspacePort port;
    LegacyWorkspaceGateway gateway(port);

    const auto closeResult = gateway.closeWorkspace(
        CloseWorkspaceRequest{
            workspaceSession(" \t", "C:/current.tps")
        }
        );
    const auto saveResult = gateway.saveWorkspace(
        SaveWorkspaceRequest{
            workspaceSession("workspace-current", "")
        }
        );
    const auto saveAsResult = gateway.saveWorkspaceAs(
        SaveWorkspaceAsRequest{
            workspaceSession(" \t", "C:/current.tps"),
            WorkspaceLocation("C:/save-as.tps")
        }
        );
    const auto exportResult = gateway.exportWorkspace(
        ExportWorkspaceRequest{
            workspaceSession("workspace-current", ""),
            WorkspaceLocation("C:/export.tps")
        }
        );

    QVERIFY(!closeResult);
    QCOMPARE(closeResult.error().code, ErrorCode::InvalidInput);
    QCOMPARE(
        closeResult.error().message,
        std::string("Workspace session must contain an id and location.")
        );

    QVERIFY(!saveResult);
    QCOMPARE(saveResult.error().code, ErrorCode::InvalidInput);
    QCOMPARE(
        saveResult.error().message,
        std::string("Workspace session must contain an id and location.")
        );

    QVERIFY(!saveAsResult);
    QCOMPARE(saveAsResult.error().code, ErrorCode::InvalidInput);
    QCOMPARE(
        saveAsResult.error().message,
        std::string("Workspace session must contain an id and location.")
        );

    QVERIFY(!exportResult);
    QCOMPARE(exportResult.error().code, ErrorCode::InvalidInput);
    QCOMPARE(
        exportResult.error().message,
        std::string("Workspace session must contain an id and location.")
        );

    QCOMPARE(port.closeCalls, 0);
    QCOMPARE(port.saveCalls, 0);
    QCOMPARE(port.saveAsCalls, 0);
    QCOMPARE(port.exportCalls, 0);
    QCOMPARE(port.hasOpenCalls, 0);
    QCOMPARE(port.currentPathCalls, 0);
}

void NextPlatformLegacyWorkspaceGatewayTests::blankSaveAsDestinationIsRejectedBeforePortCall()
{
    FakeLegacyWorkspacePort port;
    LegacyWorkspaceGateway gateway(port);
    const WorkspaceSession session = workspaceSession(
        "workspace-current",
        "C:/current.tps"
        );

    const auto emptyResult = gateway.saveWorkspaceAs(
        SaveWorkspaceAsRequest{session, WorkspaceLocation{}}
        );
    const auto whitespaceResult = gateway.saveWorkspaceAs(
        SaveWorkspaceAsRequest{session, WorkspaceLocation(" \t\r\n")}
        );

    for (const auto* result : {&emptyResult, &whitespaceResult})
    {
        QVERIFY(!*result);
        QCOMPARE(result->error().code, ErrorCode::InvalidInput);
        QCOMPARE(
            result->error().message,
            std::string(
                "Save-as destination must not be empty or whitespace-only."
                )
            );
    }

    QCOMPARE(port.saveAsCalls, 0);
}

void NextPlatformLegacyWorkspaceGatewayTests::blankExportDestinationIsRejectedBeforePortCall()
{
    FakeLegacyWorkspacePort port;
    LegacyWorkspaceGateway gateway(port);
    const WorkspaceSession session = workspaceSession(
        "workspace-current",
        "C:/current.tps"
        );

    const auto emptyResult = gateway.exportWorkspace(
        ExportWorkspaceRequest{session, WorkspaceLocation{}}
        );
    const auto whitespaceResult = gateway.exportWorkspace(
        ExportWorkspaceRequest{session, WorkspaceLocation(" \t\r\n")}
        );

    for (const auto* result : {&emptyResult, &whitespaceResult})
    {
        QVERIFY(!*result);
        QCOMPARE(result->error().code, ErrorCode::InvalidInput);
        QCOMPARE(
            result->error().message,
            std::string(
                "Export destination must not be empty or whitespace-only."
                )
            );
    }

    QCOMPARE(port.exportCalls, 0);
}

void NextPlatformLegacyWorkspaceGatewayTests::legacyErrorTextMapsToStructuredFailure()
{
    FakeLegacyWorkspacePort port;
    port.openResponse = LegacyWorkspaceResult<LegacyWorkspaceHandle>::failure(
        LegacyWorkspaceError{"The legacy workspace could not be opened."}
        );
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.openWorkspace(
        OpenWorkspaceRequest{WorkspaceLocation("C:/missing.tps")}
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Technical);
    QCOMPARE(
        result.error().message,
        std::string("The legacy workspace could not be opened.")
        );
    QVERIFY(!result.error().recoverable);
    QCOMPARE(port.hasOpenCalls, 0);
}

void NextPlatformLegacyWorkspaceGatewayTests::emptyLegacyErrorUsesOperationFallback()
{
    FakeLegacyWorkspacePort port;
    port.openResponse = LegacyWorkspaceResult<LegacyWorkspaceHandle>::failure(
        LegacyWorkspaceError{}
        );
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.openWorkspace(
        OpenWorkspaceRequest{WorkspaceLocation("C:/missing.tps")}
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Technical);
    QCOMPARE(
        result.error().message,
        std::string("Opening the workspace failed at the legacy boundary.")
        );
    QVERIFY(!result.error().recoverable);
    QCOMPARE(port.openCalls, 1);
    QCOMPARE(port.hasOpenCalls, 0);
}

void NextPlatformLegacyWorkspaceGatewayTests::invalidReturnedHandleIsRejected()
{
    FakeLegacyWorkspacePort port;
    port.openResponse = LegacyWorkspaceResult<LegacyWorkspaceHandle>::success(
        legacyHandle("", "C:/invalid-id.tps")
        );
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.openWorkspace(
        OpenWorkspaceRequest{WorkspaceLocation("C:/invalid-id.tps")}
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QVERIFY(
        result.error().message
        == "Legacy workspace returned an invalid handle or location."
        );
    QCOMPARE(port.hasOpenCalls, 0);
}

void NextPlatformLegacyWorkspaceGatewayTests::invalidReturnedLocationIsRejected()
{
    FakeLegacyWorkspacePort port;
    port.openResponse = LegacyWorkspaceResult<LegacyWorkspaceHandle>::success(
        legacyHandle("workspace-invalid-location", " \t\r\n")
        );
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.openWorkspace(
        OpenWorkspaceRequest{WorkspaceLocation("C:/invalid-location.tps")}
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QVERIFY(
        result.error().message
        == "Legacy workspace returned an invalid handle or location."
        );
    QCOMPARE(port.hasOpenCalls, 0);
}

void NextPlatformLegacyWorkspaceGatewayTests::returnedLocationMustMatchOpenObservation()
{
    FakeLegacyWorkspacePort port;
    port.openResponse = LegacyWorkspaceResult<LegacyWorkspaceHandle>::success(
        legacyHandle("workspace-mismatch", "C:/returned.tps")
        );
    port.open = true;
    port.currentPath = "C:/observed.tps";
    port.updateOpenObservation = false;
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.openWorkspace(
        OpenWorkspaceRequest{WorkspaceLocation("C:/requested.tps")}
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Technical);
    QVERIFY(
        result.error().message
        == "Opening the workspace returned a location that does not match the open workspace."
        );
}

void NextPlatformLegacyWorkspaceGatewayTests::closeMapsSessionAndRequiresClosedObservation()
{
    FakeLegacyWorkspacePort port;
    port.open = true;
    port.currentPath = "C:/current.tps";
    port.closeResponse = LegacyWorkspaceStatus::success();
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.closeWorkspace(
        CloseWorkspaceRequest{
            workspaceSession("workspace-current", "C:/current.tps")
        }
        );

    QVERIFY(result);
    QCOMPARE(port.closeCalls, 1);
    QVERIFY(
        port.lastCloseHandle
        == legacyHandle("workspace-current", "C:/current.tps")
        );
    QVERIFY(!port.open);
}

void NextPlatformLegacyWorkspaceGatewayTests::closeSuccessWithOpenObservationIsRejected()
{
    FakeLegacyWorkspacePort port;
    port.open = true;
    port.currentPath = "C:/current.tps";
    port.closeResponse = LegacyWorkspaceStatus::success();
    port.updateCloseObservation = false;
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.closeWorkspace(
        CloseWorkspaceRequest{
            workspaceSession("workspace-current", "C:/current.tps")
        }
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Technical);
    QCOMPARE(
        result.error().message,
        std::string(
            "Closing the workspace reported success while a workspace remains open."
            )
        );
    QCOMPARE(port.closeCalls, 1);
    QCOMPARE(port.hasOpenCalls, 1);
    QVERIFY(port.open);
}

void NextPlatformLegacyWorkspaceGatewayTests::closeFailurePreservesLegacyErrorAndOpenState()
{
    FakeLegacyWorkspacePort port;
    port.open = true;
    port.currentPath = "C:/current.tps";
    port.closeResponse = LegacyWorkspaceStatus::failure(
        LegacyWorkspaceError{"The legacy close operation failed."}
        );
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.closeWorkspace(
        CloseWorkspaceRequest{
            workspaceSession("workspace-current", "C:/current.tps")
        }
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Technical);
    QCOMPARE(
        result.error().message,
        std::string("The legacy close operation failed.")
        );
    QVERIFY(port.open);
    QCOMPARE(port.hasOpenCalls, 0);
}

void NextPlatformLegacyWorkspaceGatewayTests::saveMapsSessionAndValidatesOpenObservation()
{
    FakeLegacyWorkspacePort port;
    port.open = true;
    port.currentPath = "C:/current.tps";
    port.saveResponse = LegacyWorkspaceStatus::success();
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.saveWorkspace(
        SaveWorkspaceRequest{
            workspaceSession("workspace-current", "C:/current.tps")
        }
        );

    QVERIFY(result);
    QCOMPARE(port.saveCalls, 1);
    QVERIFY(
        port.lastSaveHandle
        == legacyHandle("workspace-current", "C:/current.tps")
        );
    QCOMPARE(port.hasOpenCalls, 1);
    QCOMPARE(port.currentPathCalls, 1);
}

void NextPlatformLegacyWorkspaceGatewayTests::saveSuccessWithBadObservationIsRejected()
{
    FakeLegacyWorkspacePort port;
    port.saveResponse = LegacyWorkspaceStatus::success();
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.saveWorkspace(
        SaveWorkspaceRequest{
            workspaceSession("workspace-current", "C:/current.tps")
        }
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Technical);
    QCOMPARE(
        result.error().message,
        std::string(
            "Saving the workspace reported success without an open workspace."
            )
        );
    QCOMPARE(port.saveCalls, 1);
    QCOMPARE(port.hasOpenCalls, 1);
    QCOMPARE(port.currentPathCalls, 0);
}

void NextPlatformLegacyWorkspaceGatewayTests::saveFailurePreservesLegacyError()
{
    FakeLegacyWorkspacePort port;
    port.open = true;
    port.currentPath = "C:/current.tps";
    port.saveResponse = LegacyWorkspaceStatus::failure(
        LegacyWorkspaceError{"The legacy save operation failed."}
        );
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.saveWorkspace(
        SaveWorkspaceRequest{
            workspaceSession("workspace-current", "C:/current.tps")
        }
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Technical);
    QCOMPARE(
        result.error().message,
        std::string("The legacy save operation failed.")
        );
    QCOMPARE(port.hasOpenCalls, 0);
}

void NextPlatformLegacyWorkspaceGatewayTests::saveAsMapsDestinationAndPreservesWorkspaceIdentity()
{
    FakeLegacyWorkspacePort port;
    port.open = true;
    port.currentPath = "C:/current.tps";
    port.saveAsResponse =
        LegacyWorkspaceResult<LegacyWorkspaceHandle>::success(
            legacyHandle("workspace-current", "C:/saved-as.tps")
            );
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.saveWorkspaceAs(
        SaveWorkspaceAsRequest{
            workspaceSession("workspace-current", "C:/current.tps"),
            WorkspaceLocation("C:/requested-save-as.tps")
        }
        );

    QVERIFY(result);
    QCOMPARE(port.saveAsCalls, 1);
    QVERIFY(
        port.lastSaveAsHandle
        == legacyHandle("workspace-current", "C:/current.tps")
        );
    QCOMPARE(
        port.lastSaveAsDestination,
        std::string("C:/requested-save-as.tps")
        );
    QCOMPARE(result.value().value(), std::string("C:/saved-as.tps"));
    QCOMPARE(port.currentPath, std::string("C:/saved-as.tps"));
}

void NextPlatformLegacyWorkspaceGatewayTests::saveAsLegacyErrorTextMapsToStructuredFailure()
{
    FakeLegacyWorkspacePort port;
    port.open = true;
    port.currentPath = "C:/current.tps";
    port.saveAsResponse =
        LegacyWorkspaceResult<LegacyWorkspaceHandle>::failure(
            LegacyWorkspaceError{"The legacy save-as operation failed."}
            );
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.saveWorkspaceAs(
        SaveWorkspaceAsRequest{
            workspaceSession("workspace-current", "C:/current.tps"),
            WorkspaceLocation("C:/requested-save-as.tps")
        }
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Technical);
    QCOMPARE(
        result.error().message,
        std::string("The legacy save-as operation failed.")
        );
    QVERIFY(!result.error().recoverable);
    QCOMPARE(port.saveAsCalls, 1);
    QVERIFY(
        port.lastSaveAsHandle
        == legacyHandle("workspace-current", "C:/current.tps")
        );
    QCOMPARE(
        port.lastSaveAsDestination,
        std::string("C:/requested-save-as.tps")
        );
    QCOMPARE(port.hasOpenCalls, 0);
    QCOMPARE(port.currentPathCalls, 0);
}

void NextPlatformLegacyWorkspaceGatewayTests::saveAsSuccessWithBadObservationIsRejected()
{
    FakeLegacyWorkspacePort port;
    port.open = true;
    port.currentPath = "C:/unexpected.tps";
    port.saveAsResponse =
        LegacyWorkspaceResult<LegacyWorkspaceHandle>::success(
            legacyHandle("workspace-current", "C:/saved-as.tps")
            );
    port.updateSaveAsObservation = false;
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.saveWorkspaceAs(
        SaveWorkspaceAsRequest{
            workspaceSession("workspace-current", "C:/current.tps"),
            WorkspaceLocation("C:/requested-save-as.tps")
        }
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Technical);
    QCOMPARE(
        result.error().message,
        std::string(
            "Saving the workspace as returned a location that does not match the open workspace."
            )
        );
    QCOMPARE(port.saveAsCalls, 1);
    QCOMPARE(port.hasOpenCalls, 1);
    QCOMPARE(port.currentPathCalls, 1);
    QCOMPARE(port.currentPath, std::string("C:/unexpected.tps"));
}

void NextPlatformLegacyWorkspaceGatewayTests::saveAsRejectsReturnedIdentityChange()
{
    FakeLegacyWorkspacePort port;
    port.open = true;
    port.currentPath = "C:/current.tps";
    port.saveAsResponse =
        LegacyWorkspaceResult<LegacyWorkspaceHandle>::success(
            legacyHandle("different-workspace", "C:/different.tps")
            );
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.saveWorkspaceAs(
        SaveWorkspaceAsRequest{
            workspaceSession("workspace-current", "C:/current.tps"),
            WorkspaceLocation("C:/different.tps")
        }
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QVERIFY(
        result.error().message
        == "Save-as returned a different workspace identity."
        );
    QCOMPARE(port.hasOpenCalls, 0);
}

void NextPlatformLegacyWorkspaceGatewayTests::saveAsRejectsReturnedLocation()
{
    FakeLegacyWorkspacePort port;
    port.open = true;
    port.currentPath = "C:/current.tps";
    port.saveAsResponse =
        LegacyWorkspaceResult<LegacyWorkspaceHandle>::success(
            legacyHandle("workspace-current", " \t")
            );
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.saveWorkspaceAs(
        SaveWorkspaceAsRequest{
            workspaceSession("workspace-current", "C:/current.tps"),
            WorkspaceLocation("C:/save-as.tps")
        }
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QVERIFY(
        result.error().message
        == "Legacy workspace returned an invalid handle or location."
        );
    QCOMPARE(port.hasOpenCalls, 0);
}

void NextPlatformLegacyWorkspaceGatewayTests::exportMapsDestinationWithoutChangingOpenWorkspace()
{
    FakeLegacyWorkspacePort port;
    port.open = true;
    port.currentPath = "C:/current.tps";
    port.exportResponse =
        LegacyWorkspaceResult<std::string>::success("C:/exported.tps");
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.exportWorkspace(
        ExportWorkspaceRequest{
            workspaceSession("workspace-current", "C:/current.tps"),
            WorkspaceLocation("C:/requested-export.tps")
        }
        );

    QVERIFY(result);
    QCOMPARE(port.exportCalls, 1);
    QVERIFY(
        port.lastExportHandle
        == legacyHandle("workspace-current", "C:/current.tps")
        );
    QCOMPARE(
        port.lastExportDestination,
        std::string("C:/requested-export.tps")
        );
    QCOMPARE(result.value().value(), std::string("C:/exported.tps"));
    QVERIFY(port.open);
    QCOMPARE(port.currentPath, std::string("C:/current.tps"));
    QCOMPARE(port.hasOpenCalls, 1);
    QCOMPARE(port.currentPathCalls, 1);
}

void NextPlatformLegacyWorkspaceGatewayTests::exportSuccessWithBadObservationIsRejected()
{
    FakeLegacyWorkspacePort port;
    port.exportResponse =
        LegacyWorkspaceResult<std::string>::success("C:/exported.tps");
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.exportWorkspace(
        ExportWorkspaceRequest{
            workspaceSession("workspace-current", "C:/current.tps"),
            WorkspaceLocation("C:/requested-export.tps")
        }
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Technical);
    QCOMPARE(
        result.error().message,
        std::string(
            "Exporting the workspace reported success without an open workspace."
            )
        );
    QCOMPARE(port.exportCalls, 1);
    QCOMPARE(port.hasOpenCalls, 1);
    QCOMPARE(port.currentPathCalls, 0);
}

void NextPlatformLegacyWorkspaceGatewayTests::exportRejectsInvalidReturnedLocation()
{
    FakeLegacyWorkspacePort port;
    port.open = true;
    port.currentPath = "C:/current.tps";
    port.exportResponse =
        LegacyWorkspaceResult<std::string>::success(" \t\r\n");
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.exportWorkspace(
        ExportWorkspaceRequest{
            workspaceSession("workspace-current", "C:/current.tps"),
            WorkspaceLocation("C:/requested-export.tps")
        }
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QCOMPARE(
        result.error().message,
        std::string("Export returned an empty or whitespace-only location.")
        );
    QCOMPARE(port.exportCalls, 1);
    QCOMPARE(port.hasOpenCalls, 0);
    QCOMPARE(port.currentPathCalls, 0);
}

void NextPlatformLegacyWorkspaceGatewayTests::exportFailurePreservesLegacyError()
{
    FakeLegacyWorkspacePort port;
    port.open = true;
    port.currentPath = "C:/current.tps";
    port.exportResponse = LegacyWorkspaceResult<std::string>::failure(
        LegacyWorkspaceError{"The legacy export operation failed."}
        );
    LegacyWorkspaceGateway gateway(port);

    const auto result = gateway.exportWorkspace(
        ExportWorkspaceRequest{
            workspaceSession("workspace-current", "C:/current.tps"),
            WorkspaceLocation("C:/export.tps")
        }
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Technical);
    QCOMPARE(
        result.error().message,
        std::string("The legacy export operation failed.")
        );
    QCOMPARE(port.hasOpenCalls, 0);
    QVERIFY(port.open);
}

void NextPlatformLegacyWorkspaceGatewayTests::gatewayDoesNotRetainMutableSessionState()
{
    FakeLegacyWorkspacePort port;
    port.openResponse = LegacyWorkspaceResult<LegacyWorkspaceHandle>::success(
        legacyHandle("workspace-original", "C:/original.tps")
        );
    LegacyWorkspaceGateway gateway(port);

    const auto opened = gateway.openWorkspace(
        OpenWorkspaceRequest{WorkspaceLocation("C:/original.tps")}
        );

    QVERIFY(opened);
    const WorkspaceSession returnedSession = opened.value();

    port.openResponse = LegacyWorkspaceResult<LegacyWorkspaceHandle>::success(
        legacyHandle("workspace-replacement", "C:/replacement.tps")
        );
    port.open = true;
    port.currentPath = "C:/replacement.tps";
    port.closeResponse = LegacyWorkspaceStatus::success();

    QVERIFY(
        returnedSession
        == workspaceSession("workspace-original", "C:/original.tps")
        );

    const auto closed = gateway.closeWorkspace(
        CloseWorkspaceRequest{returnedSession}
        );

    QVERIFY(closed);
    QVERIFY(
        port.lastCloseHandle
        == legacyHandle("workspace-original", "C:/original.tps")
        );
}

QTEST_APPLESS_MAIN(NextPlatformLegacyWorkspaceGatewayTests)

#include "next_platform_legacy_workspace_gateway_tests.moc"
