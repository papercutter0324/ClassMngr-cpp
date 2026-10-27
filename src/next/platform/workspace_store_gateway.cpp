#include "next/platform/workspace_store_gateway.h"

#include "data/database/database_file_operations.h"
#include "data/database/workspace_store.h"
#include "data/repositories/teacher_import_repository.h"
#include "next/platform/repository_error_code.h"
#include "next/application/personal_details_save_use_case.h"
#include "next/application/initial_setup_teacher_choices_read_query.h"
#include "next/application/teacher_create_use_case.h"
#include "next/application/class_create_use_case.h"
#include "next/application/class_details_save_use_case.h"
#include "next/platform/workspace_store_personal_details_port.h"
#include "next/platform/workspace_store_initial_setup_teacher_entry_port.h"
#include "next/platform/workspace_store_custom_color_palette_preferences_port.h"
#include "next/platform/workspace_store_initial_setup_class_times_ports.h"
#include "next/platform/teacher_validator_teacher_create_validation_policy.h"

#include <QByteArray>
#include <QString>

#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{
namespace
{

[[nodiscard]] std::string toUtf8(const QString& value)
{
    const QByteArray encoded = value.toUtf8();
    return std::string(
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
        );
}

[[nodiscard]] QString fromUtf8(const std::string& value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

[[nodiscard]] Domain::OperationError operationError(
    const RepositoryError& error
    )
{
    const bool recoverable = error.recoverableOverride.value_or(
        error.code == RepositoryErrorCode::NotFound
            || error.code == RepositoryErrorCode::Conflict
            || error.code == RepositoryErrorCode::Validation
        );
    return Domain::OperationError{
        .code = repositoryDomainErrorCode(error.code),
        .message = toUtf8(error.diagnostic),
        .recoverable = recoverable
    };
}

[[nodiscard]] Domain::OperationError notOpenError()
{
    return Domain::OperationError{
        .code = Domain::ErrorCode::NotFound,
        .message = "No workspace is open in the workspace store.",
        .recoverable = false
    };
}

[[nodiscard]] Domain::OperationError unsupportedOperationError(
    const char* operation
    )
{
    return Domain::OperationError{
        .code = Domain::ErrorCode::Technical,
        .message = std::string(operation)
            + " is not available through the startup workspace adapter.",
        .recoverable = false
    };
}

} // namespace

WorkspaceStoreGateway::WorkspaceStoreGateway()
    : m_store(std::make_unique<WorkspaceStore>())
{
}

WorkspaceStoreGateway::~WorkspaceStoreGateway() = default;

Application::PersonalDetailsReadResult
WorkspaceStoreGateway::readPersonalDetails() const
{
    WorkspaceStorePersonalDetailsPort port(*m_store);
    const Application::PersonalDetailsReadQuery query(port);
    return query.execute();
}

Application::PersonalDetailsSaveResult
WorkspaceStoreGateway::savePersonalDetails(
    const Application::PersonalDetailsSaveRequest& request
    ) const
{
    WorkspaceStorePersonalDetailsPort port(*m_store);
    const Application::PersonalDetailsSaveUseCase useCase(port);
    return useCase.execute(request);
}

Application::InitialSetupTeacherChoicesResult
WorkspaceStoreGateway::loadInitialSetupTeacherChoices() const
{
    WorkspaceStoreInitialSetupTeacherEntryPort port(*m_store);
    const Application::InitialSetupTeacherChoicesReadQuery query(port);
    return query.execute();
}

Domain::Result<Application::CustomColorPalette>
WorkspaceStoreGateway::readInitialSetupClassColorPalette() const
{
    WorkspaceStoreCustomColorPalettePreferencesPort port(*m_store);
    return port.read();
}

Domain::Result<void> WorkspaceStoreGateway::saveInitialSetupClassColorPalette(
    const Application::CustomColorPalette& palette
    ) const
{
    WorkspaceStoreCustomColorPalettePreferencesPort port(*m_store);
    return port.write(palette);
}

Application::TeacherCreateResult
WorkspaceStoreGateway::createInitialSetupTeacher(
    const Application::TeacherCreateRequest& request
    ) const
{
    WorkspaceStoreInitialSetupTeacherEntryPort port(*m_store);
    TeacherValidatorTeacherCreateValidationPolicy validationPolicy;
    return Application::TeacherCreateUseCase::execute(
        request,
        validationPolicy,
        port
        );
}

Application::ClassCreateResult WorkspaceStoreGateway::createInitialSetupClass() const
{
    WorkspaceStoreInitialSetupClassCreatePort port(*m_store);
    return Application::ClassCreateUseCase::execute({}, port);
}

Application::InitialSetupClassTimesSaveResult
WorkspaceStoreGateway::saveInitialSetupClassTimes(
    const Application::InitialSetupClassTimesSaveRequest& request
    ) const
{
    WorkspaceStoreInitialSetupClassDetailsSavePort port(*m_store);
    const Application::ClassDetailsSaveRequest details{
        .classId = request.classId,
        .classGrade = request.classGrade,
        .classLevel = request.classLevel,
        .readingBook = request.readingBook,
        .essayBook = request.essayBook,
        .classColor = request.classColor,
        .fontColor = request.fontColor,
        .regularTimes = request.regularTimes,
        .intensiveTimes = request.intensiveTimes,
        .teacherId = request.teacherId
    };
    return Application::ClassDetailsSaveUseCase::execute(details, port);
}

std::expected<
    Application::TeacherImportUseCaseResult,
    Application::TeacherImportUseCaseError
    > WorkspaceStoreGateway::applyTeacherImportUseCase(
        const Application::TeacherImportUseCaseRequest& request
        ) const
{
    TeacherImportRepository* const repository =
        m_store->teacherImportRepository();
    if (!m_store->isOpen() || !repository)
    {
        return std::unexpected(Application::TeacherImportUseCaseError{
            .kind = Application::TeacherImportUseCaseErrorKind::InvalidRequest
        });
    }

    return repository->applyUseCase(request);
}

Domain::Result<Application::WorkspaceSession>
WorkspaceStoreGateway::createWorkspace(
    const Application::CreateWorkspaceRequest& request
    )
{
    const RepositoryStatus status = m_store->open(fromUtf8(request.location.value()));
    if (!status)
    {
        return Domain::Result<Application::WorkspaceSession>::failure(
            operationError(status.error())
            );
    }
    const auto session = sessionForOpenStore();
    if (session)
    {
        m_activeWorkspaceId = session.value().workspaceId();
    }
    return session;
}

Domain::Result<Application::WorkspaceSession>
WorkspaceStoreGateway::openWorkspace(
    const Application::OpenWorkspaceRequest& request
    )
{
    const RepositoryStatus status =
        m_store->openExisting(fromUtf8(request.location.value()));
    if (!status)
    {
        return Domain::Result<Application::WorkspaceSession>::failure(
            operationError(status.error())
            );
    }
    const auto session = sessionForOpenStore();
    if (session)
    {
        m_activeWorkspaceId = session.value().workspaceId();
    }
    return session;
}

Domain::Result<void> WorkspaceStoreGateway::closeWorkspace(
    const Application::CloseWorkspaceRequest& request
    )
{
    const Domain::Result<void> valid = validateSession(request.session);
    if (!valid)
    {
        return valid;
    }

    m_store->close();
    if (m_store->isOpen() || !m_store->databasePath().isEmpty())
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Workspace store did not close cleanly.",
            .recoverable = false
        });
    }
    m_activeWorkspaceId.reset();
    return Domain::Result<void>::success();
}

Domain::Result<void> WorkspaceStoreGateway::saveWorkspace(
    const Application::SaveWorkspaceRequest& request
    )
{
    const Domain::Result<void> valid = validateSession(request.session);
    if (!valid)
    {
        return valid;
    }

    const RepositoryStatus status = m_store->commit();
    if (!status)
    {
        return Domain::Result<void>::failure(operationError(status.error()));
    }
    return Domain::Result<void>::success();
}

Domain::Result<void> WorkspaceStoreGateway::discardWorkspaceChanges(
    const Application::DiscardWorkspaceChangesRequest& request
    )
{
    const Domain::Result<void> valid = validateSession(request.session);
    if (!valid)
    {
        return valid;
    }

    const RepositoryStatus status = m_store->rollback();
    if (!status)
    {
        return Domain::Result<void>::failure(operationError(status.error()));
    }
    return Domain::Result<void>::success();
}

Domain::Result<Application::WorkspaceLocation>
WorkspaceStoreGateway::saveWorkspaceAs(
    const Application::SaveWorkspaceAsRequest& request
    )
{
    const Domain::Result<void> valid = validateSession(request.session);
    if (!valid)
    {
        return Domain::Result<Application::WorkspaceLocation>::failure(
            valid.error()
            );
    }

    const QString destinationPath = fromUtf8(request.destination.value());
    const RepositoryStatus copied = DatabaseFileOperations::copyDatabaseFile(
        m_store->databasePath(),
        destinationPath
        );
    if (!copied)
    {
        return Domain::Result<Application::WorkspaceLocation>::failure(
            operationError(copied.error())
            );
    }

    const RepositoryStatus reopened = m_store->openExisting(destinationPath);
    if (!reopened)
    {
        return Domain::Result<Application::WorkspaceLocation>::failure(
            operationError(reopened.error())
            );
    }

    return Domain::Result<Application::WorkspaceLocation>::success(
        Application::WorkspaceLocation(toUtf8(m_store->databasePath()))
        );
}

Domain::Result<Application::WorkspaceLocation>
WorkspaceStoreGateway::exportWorkspace(
    const Application::ExportWorkspaceRequest& request
    )
{
    const Domain::Result<void> valid = validateSession(request.session);
    if (!valid)
    {
        return Domain::Result<Application::WorkspaceLocation>::failure(
            valid.error()
            );
    }

    const RepositoryStatus copied = DatabaseFileOperations::copyDatabaseFile(
        m_store->databasePath(),
        fromUtf8(request.destination.value())
        );
    if (!copied)
    {
        return Domain::Result<Application::WorkspaceLocation>::failure(
            operationError(copied.error())
            );
    }

    return Domain::Result<Application::WorkspaceLocation>::success(
        request.destination
        );
}

Domain::Result<Application::WorkspaceSession>
WorkspaceStoreGateway::sessionForOpenStore() const
{
    if (!m_store->isOpen() || m_store->databasePath().isEmpty())
    {
        return Domain::Result<Application::WorkspaceSession>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Workspace store reported success without an open workspace.",
            .recoverable = false
        });
    }

    const std::string location = toUtf8(m_store->databasePath());
    const auto workspaceId = Domain::WorkspaceId::fromString(location);
    if (!workspaceId)
    {
        return Domain::Result<Application::WorkspaceSession>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Workspace store returned an invalid workspace location.",
            .recoverable = false
        });
    }

    return Domain::Result<Application::WorkspaceSession>::success(
        Application::WorkspaceSession(
            *workspaceId,
            Application::WorkspaceLocation(location)
            )
        );
}

Domain::Result<void> WorkspaceStoreGateway::validateSession(
    const Application::WorkspaceSession& session
    ) const
{
    if (!m_store->isOpen())
    {
        return Domain::Result<void>::failure(notOpenError());
    }

    const std::string openLocation = toUtf8(m_store->databasePath());
    if (session.location().value() != openLocation
        || !m_activeWorkspaceId.has_value()
        || session.workspaceId() != *m_activeWorkspaceId)
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::Conflict,
            .message = "Workspace session does not match the open workspace store.",
            .recoverable = false
        });
    }
    return Domain::Result<void>::success();
}

} // namespace ClassMngr::Next::Platform
