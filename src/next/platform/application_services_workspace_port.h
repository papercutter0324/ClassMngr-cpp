#pragma once

#include "core/application_services.h"
#include "next/platform/legacy_workspace_gateway.h"

#include <QByteArray>
#include <QFileInfo>
#include <QString>

#include <cctype>
#include <string>
#include <string_view>

namespace ClassMngr::Next::Platform
{

// Concrete outer adapter for the historical ApplicationServices boundary.
//
// This port deliberately owns no session snapshot, file-controller policy, or
// UI pointer. The caller owns ApplicationServices and must keep it alive for
// the port's lifetime. In particular, createOrOpenDatabase is only the
// explicit create hook at this seam: it delegates to the legacy open path and
// never removes or replaces an existing file. FileController remains the
// separate preparation boundary for the legacy create policy in this slice.
class ApplicationServicesWorkspacePort final : public LegacyWorkspacePort
{
public:
    explicit ApplicationServicesWorkspacePort(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesWorkspacePort(
        const ApplicationServicesWorkspacePort&
        ) = delete;
    ApplicationServicesWorkspacePort& operator=(
        const ApplicationServicesWorkspacePort&
        ) = delete;
    ApplicationServicesWorkspacePort(
        ApplicationServicesWorkspacePort&&
        ) = delete;
    ApplicationServicesWorkspacePort& operator=(
        ApplicationServicesWorkspacePort&&
        ) = delete;

    [[nodiscard]] LegacyWorkspaceResult<LegacyWorkspaceHandle> openDatabase(
        const std::string& databasePath
        ) override
    {
        const auto path = normalizeInputPath(
            databasePath,
            "Opening the workspace"
            );
        if (!path)
        {
            return LegacyWorkspaceResult<LegacyWorkspaceHandle>::failure(
                path.error()
                );
        }

        const Status opened = m_services.openDatabase(path.value());
        if (!opened)
        {
            return LegacyWorkspaceResult<LegacyWorkspaceHandle>::failure(
                legacyError(opened.error())
                );
        }

        return handleFromCurrentPath("Opening the workspace");
    }

    [[nodiscard]] LegacyWorkspaceResult<LegacyWorkspaceHandle>
    createOrOpenDatabase(
        const std::string& databasePath
        ) override
    {
        // Creation preparation (including any replacement decision) belongs
        // to FileController. This hook intentionally has the same safe,
        // non-destructive behavior as the legacy open operation.
        return openDatabase(databasePath);
    }

    [[nodiscard]] LegacyWorkspaceStatus closeDatabase(
        const LegacyWorkspaceHandle& handle
        ) override
    {
        const LegacyWorkspaceStatus valid =
            validateOpenHandle(handle, "Closing the workspace");
        if (!valid)
        {
            return valid;
        }

        m_services.closeDatabase();

        if (m_services.hasOpenDatabase())
        {
            return LegacyWorkspaceStatus::failure(
                LegacyWorkspaceError{
                    "Closing the workspace reported success while a workspace remains open."
                }
                );
        }

        if (!m_services.currentDatabasePath().trimmed().isEmpty())
        {
            return LegacyWorkspaceStatus::failure(
                LegacyWorkspaceError{
                    "Closing the workspace reported success with a current path."
                }
                );
        }

        return LegacyWorkspaceStatus::success();
    }

    [[nodiscard]] bool hasOpenDatabase() const override
    {
        return m_services.hasOpenDatabase();
    }

    [[nodiscard]] std::string currentDatabasePath() const override
    {
        const auto path = normalizeLegacyPath(
            m_services.currentDatabasePath(),
            "Reading the current workspace"
            );
        return path
            ? utf8(path.value())
            : std::string{};
    }

    [[nodiscard]] LegacyWorkspaceStatus saveDatabase(
        const LegacyWorkspaceHandle& handle
        ) override
    {
        const LegacyWorkspaceStatus valid =
            validateOpenHandle(handle, "Saving the workspace");
        if (!valid)
        {
            return valid;
        }

        // The historical operation is void. Its success is therefore the
        // open/path observation after the call, not the call itself.
        m_services.saveDatabase();

        return validateOpenHandle(handle, "Saving the workspace");
    }

    [[nodiscard]] LegacyWorkspaceResult<LegacyWorkspaceHandle>
    saveDatabaseAs(
        const LegacyWorkspaceHandle& handle,
        const std::string& destinationPath
        ) override
    {
        const LegacyWorkspaceStatus shape = validateHandleShape(
            handle,
            "Saving the workspace as"
            );
        if (!shape)
        {
            return LegacyWorkspaceResult<LegacyWorkspaceHandle>::failure(
                shape.error()
                );
        }

        // Preserve the legacy error text for the closed-workspace case. When
        // there is an open workspace, reject a stale caller token before
        // allowing the argument-less legacy service operation to act on it.
        if (m_services.hasOpenDatabase())
        {
            const LegacyWorkspaceStatus valid =
                validateOpenHandle(handle, "Saving the workspace as");
            if (!valid)
            {
                return LegacyWorkspaceResult<LegacyWorkspaceHandle>::failure(
                    valid.error()
                    );
            }
        }

        const auto destination = normalizeInputPath(
            destinationPath,
            "Save-as destination"
            );
        if (!destination)
        {
            return LegacyWorkspaceResult<LegacyWorkspaceHandle>::failure(
                destination.error()
                );
        }

        const Status saved = m_services.saveDatabaseAs(destination.value());
        if (!saved)
        {
            return LegacyWorkspaceResult<LegacyWorkspaceHandle>::failure(
                legacyError(saved.error())
                );
        }

        // FileController follows save-as by reopening the destination. Keep
        // that behavior at this outer boundary so the returned handle and
        // ApplicationServices observation agree.
        const Status reopened = m_services.openDatabase(destination.value());
        if (!reopened)
        {
            return LegacyWorkspaceResult<LegacyWorkspaceHandle>::failure(
                legacyError(reopened.error())
                );
        }

        const auto observed = normalizedCurrentPath(
            "Saving the workspace as"
            );
        if (!observed)
        {
            return LegacyWorkspaceResult<LegacyWorkspaceHandle>::failure(
                observed.error()
                );
        }

        const std::string normalizedDestination = utf8(observed.value());
        if (normalizedDestination != utf8(destination.value()))
        {
            return LegacyWorkspaceResult<LegacyWorkspaceHandle>::failure(
                LegacyWorkspaceError{
                    "Saving the workspace as reopened a different destination."
                }
                );
        }

        // Save-as changes the location, not the caller's workspace identity.
        return LegacyWorkspaceResult<LegacyWorkspaceHandle>::success(
            LegacyWorkspaceHandle(
                handle.workspaceId(),
                normalizedDestination
                )
            );
    }

    [[nodiscard]] LegacyWorkspaceResult<std::string> exportDatabaseAs(
        const LegacyWorkspaceHandle& handle,
        const std::string& destinationPath
        ) override
    {
        const LegacyWorkspaceStatus shape = validateHandleShape(
            handle,
            "Exporting the workspace"
            );
        if (!shape)
        {
            return LegacyWorkspaceResult<std::string>::failure(
                shape.error()
                );
        }

        if (m_services.hasOpenDatabase())
        {
            const LegacyWorkspaceStatus valid =
                validateOpenHandle(handle, "Exporting the workspace");
            if (!valid)
            {
                return LegacyWorkspaceResult<std::string>::failure(
                    valid.error()
                    );
            }
        }

        const auto destination = normalizeInputPath(
            destinationPath,
            "Export destination"
            );
        if (!destination)
        {
            return LegacyWorkspaceResult<std::string>::failure(
                destination.error()
                );
        }

        const Status exported = m_services.exportDatabaseAs(destination.value());
        if (!exported)
        {
            return LegacyWorkspaceResult<std::string>::failure(
                legacyError(exported.error())
                );
        }

        const auto observed = normalizedCurrentPath(
            "Exporting the workspace"
            );
        if (!observed)
        {
            return LegacyWorkspaceResult<std::string>::failure(
                observed.error()
                );
        }

        if (utf8(observed.value()) != utf8(normalizeLocation(handle)))
        {
            return LegacyWorkspaceResult<std::string>::failure(
                LegacyWorkspaceError{
                    "Exporting the workspace changed the open workspace."
                }
                );
        }

        // The legacy status has no returned path; this is the normalized path
        // that was passed to the legacy operation.
        return LegacyWorkspaceResult<std::string>::success(
            utf8(destination.value())
            );
    }

private:
    [[nodiscard]] static LegacyWorkspaceError legacyError(
        const QString& message
        )
    {
        return LegacyWorkspaceError{utf8(message)};
    }

    [[nodiscard]] static std::string utf8(
        const QString& value
        )
    {
        return value.toUtf8().toStdString();
    }

    [[nodiscard]] static bool isBlank(
        std::string_view value
        ) noexcept
    {
        if (value.empty())
        {
            return true;
        }

        for (const char character : value)
        {
            if (
                std::isspace(
                    static_cast<unsigned char>(character)
                    ) == 0
                )
            {
                return false;
            }
        }

        return true;
    }

    [[nodiscard]] static bool isBlank(
        const QString& value
        )
    {
        return value.trimmed().isEmpty();
    }

    [[nodiscard]] static bool isValidUtf8(
        const std::string& value
        )
    {
        const QByteArray encoded(
            value.data(),
            static_cast<qsizetype>(value.size())
            );
        const QString decoded = QString::fromUtf8(encoded);
        return decoded.toUtf8() == encoded;
    }

    [[nodiscard]] static LegacyWorkspaceResult<QString> normalizeInputPath(
        const std::string& path,
        const char* operation
        )
    {
        if (isBlank(path))
        {
            return LegacyWorkspaceResult<QString>::failure(
                LegacyWorkspaceError{
                    std::string(operation)
                    + " path must not be empty or whitespace-only."
                }
                );
        }

        if (!isValidUtf8(path))
        {
            return LegacyWorkspaceResult<QString>::failure(
                LegacyWorkspaceError{
                    std::string(operation) + " path is not valid UTF-8."
                }
                );
        }

        const QByteArray encoded(
            path.data(),
            static_cast<qsizetype>(path.size())
            );
        const QString decoded = QString::fromUtf8(encoded);
        return normalizeLegacyPath(decoded, operation);
    }

    [[nodiscard]] static LegacyWorkspaceResult<QString> normalizeLegacyPath(
        const QString& path,
        const char* operation
        )
    {
        if (isBlank(path))
        {
            return LegacyWorkspaceResult<QString>::failure(
                LegacyWorkspaceError{
                    std::string(operation)
                    + " returned an empty or whitespace-only path."
                }
                );
        }

        if (path.contains(QChar::Null))
        {
            return LegacyWorkspaceResult<QString>::failure(
                LegacyWorkspaceError{
                    std::string(operation) + " returned an invalid path."
                }
                );
        }

        const QString normalized = QFileInfo(path).absoluteFilePath();
        if (isBlank(normalized))
        {
            return LegacyWorkspaceResult<QString>::failure(
                LegacyWorkspaceError{
                    std::string(operation) + " returned an unresolved path."
                }
                );
        }

        return LegacyWorkspaceResult<QString>::success(normalized);
    }

    [[nodiscard]] static QString normalizeLocation(
        const LegacyWorkspaceHandle& handle
        )
    {
        return QString::fromUtf8(
            handle.location().data(),
            static_cast<qsizetype>(handle.location().size())
            );
    }

    [[nodiscard]] LegacyWorkspaceResult<QString> normalizedCurrentPath(
        const char* operation
        ) const
    {
        if (!m_services.hasOpenDatabase())
        {
            return LegacyWorkspaceResult<QString>::failure(
                LegacyWorkspaceError{
                    std::string(operation)
                    + " reported success without an open workspace."
                }
                );
        }

        return normalizeLegacyPath(
            m_services.currentDatabasePath(),
            operation
            );
    }

    [[nodiscard]] LegacyWorkspaceResult<LegacyWorkspaceHandle>
    handleFromCurrentPath(
        const char* operation
        ) const
    {
        const auto current = normalizedCurrentPath(operation);
        if (!current)
        {
            return LegacyWorkspaceResult<LegacyWorkspaceHandle>::failure(
                current.error()
                );
        }

        const std::string location = utf8(current.value());
        if (isBlank(location))
        {
            return LegacyWorkspaceResult<LegacyWorkspaceHandle>::failure(
                LegacyWorkspaceError{
                    std::string(operation)
                    + " returned an empty workspace handle."
                }
                );
        }

        return LegacyWorkspaceResult<LegacyWorkspaceHandle>::success(
            LegacyWorkspaceHandle(location, location)
            );
    }

    [[nodiscard]] LegacyWorkspaceStatus validateHandleShape(
        const LegacyWorkspaceHandle& handle,
        const char* operation
        ) const
    {
        if (isBlank(handle.workspaceId()) || isBlank(handle.location()))
        {
            return LegacyWorkspaceStatus::failure(
                LegacyWorkspaceError{
                    std::string(operation)
                    + " received an invalid workspace handle."
                }
                );
        }

        if (!isValidUtf8(handle.workspaceId()))
        {
            return LegacyWorkspaceStatus::failure(
                LegacyWorkspaceError{
                    std::string(operation)
                    + " received a workspace identity that is not valid UTF-8."
                }
                );
        }

        const auto location = normalizeInputPath(
            handle.location(),
            operation
            );
        if (!location)
        {
            return LegacyWorkspaceStatus::failure(location.error());
        }

        if (utf8(location.value()) != handle.location())
        {
            return LegacyWorkspaceStatus::failure(
                LegacyWorkspaceError{
                    std::string(operation)
                    + " received a non-normalized workspace location."
                }
                );
        }

        return LegacyWorkspaceStatus::success();
    }

    [[nodiscard]] LegacyWorkspaceStatus validateOpenHandle(
        const LegacyWorkspaceHandle& handle,
        const char* operation
        ) const
    {
        const LegacyWorkspaceStatus shape =
            validateHandleShape(handle, operation);
        if (!shape)
        {
            return shape;
        }

        const auto current = normalizedCurrentPath(operation);
        if (!current)
        {
            return LegacyWorkspaceStatus::failure(current.error());
        }

        if (utf8(current.value()) != handle.location())
        {
            return LegacyWorkspaceStatus::failure(
                LegacyWorkspaceError{
                    std::string(operation)
                    + " received a handle for a different open workspace."
                }
                );
        }

        return LegacyWorkspaceStatus::success();
    }

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
