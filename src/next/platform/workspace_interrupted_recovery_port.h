#pragma once

#include "next/application/interrupted_workspace_recovery_flow.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

#include <cstddef>
#include <string>
#include <vector>

namespace ClassMngr::Next::Platform
{

// Next-owned filesystem operations for manual recovery of a normal profile
// create interrupted by process termination. Initial Setup backups have a
// different suffix and are deliberately not recognized here.
class QtWorkspaceInterruptedRecoveryPort final
    : public Application::InterruptedWorkspaceRecoveryPort
{
public:
    [[nodiscard]] Application::FileDialogPath
    defaultTeacherProfileDirectory() const override
    {
        QString directory = QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation
            );
        if (directory.isEmpty() || !QDir(directory).exists())
        {
            directory = QDir::homePath();
        }
        return fileDialogPath(QDir::cleanPath(directory));
    }

    [[nodiscard]] Application::FileDialogPath profileDirectoryFor(
        const Application::WorkspaceLocation& profile
        ) const override
    {
        return fileDialogPath(QFileInfo(qString(profile.value()))
                                  .absoluteDir()
                                  .absolutePath());
    }

    [[nodiscard]] Domain::Result<std::vector<
        Application::InterruptedWorkspaceRecoveryCandidate>>
    candidatesIn(const Application::FileDialogPath& directoryPath) const override
    {
        const QString requested = qString(directoryPath.value());
        if (requested.trimmed().isEmpty())
        {
            return Domain::Result<std::vector<
                Application::InterruptedWorkspaceRecoveryCandidate>>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "The recovery scan directory is empty.",
                .recoverable = false
            });
        }

        const QFileInfo directoryInfo(requested);
        if (!directoryInfo.exists() || !directoryInfo.isDir())
        {
            return Domain::Result<std::vector<
                Application::InterruptedWorkspaceRecoveryCandidate>>::failure({
                .code = Domain::ErrorCode::NotFound,
                .message = "The recovery scan directory does not exist or is not a directory.",
                .recoverable = true
            });
        }
        if (!directoryInfo.isReadable())
        {
            return Domain::Result<std::vector<
                Application::InterruptedWorkspaceRecoveryCandidate>>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "The recovery scan directory is not readable.",
                .recoverable = true
            });
        }

        const QDir directory(directoryInfo.absoluteFilePath());
        const QFileInfoList entries = directory.entryInfoList(
            QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
            QDir::Name
            );
        std::vector<Application::InterruptedWorkspaceRecoveryCandidate>
            candidates;
        candidates.reserve(static_cast<std::size_t>(entries.size()));
        for (const QFileInfo& entry : entries)
        {
            if (!entry.isFile() || entry.isSymLink())
            {
                continue;
            }

            const QRegularExpressionMatch match =
                normalCreateRecoveryNamePattern().match(entry.fileName());
            if (!match.hasMatch())
            {
                continue;
            }

            const QString targetPath = directory.filePath(match.captured(1));
            candidates.push_back(
                Application::InterruptedWorkspaceRecoveryCandidate{
                    .sourcePath = fileDialogPath(entry.absoluteFilePath()),
                    .targetPath = fileDialogPath(targetPath),
                    .targetFileName = toUtf8(QFileInfo(targetPath).fileName())
                }
                );
        }
        return Domain::Result<std::vector<
            Application::InterruptedWorkspaceRecoveryCandidate>>::success(
                std::move(candidates)
                );
    }

    [[nodiscard]] bool pathExists(
        const Application::FileDialogPath& path
        ) const override
    {
        const QFileInfo info(qString(path.value()));
        // QFileInfo::exists() follows symlinks. Count a dangling symlink too,
        // since it occupies the destination name and must not be replaced.
        return info.exists() || info.isSymLink();
    }

    [[nodiscard]] Domain::Result<void> copyCandidateNoReplace(
        const Application::InterruptedWorkspaceRecoveryCandidate& candidate,
        const Application::WorkspaceLocation& destinationPath
        ) const override
    {
        const QString sourceName = QFileInfo(qString(
            candidate.sourcePath.value()
            )).fileName();
        const QRegularExpressionMatch match =
            normalCreateRecoveryNamePattern().match(sourceName);
        const QFileInfo sourceInfo(qString(candidate.sourcePath.value()));
        if (candidate.sourcePath.empty() || destinationPath.empty()
            || !match.hasMatch() || !sourceInfo.isFile()
            || sourceInfo.isSymLink())
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "The selected normal-create recovery copy is invalid.",
                .recoverable = false
            });
        }

        QFile source(qString(candidate.sourcePath.value()));
        if (!source.open(QIODevice::ReadOnly))
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "The recovery source could not be opened: "
                    + toUtf8(source.errorString()),
                .recoverable = true
            });
        }

        // NewOnly atomically refuses a destination that appeared after the
        // application's preflight check. Partial destinations are retained
        // after later I/O errors so no created artifact is hidden.
        QFile destination(qString(destinationPath.value()));
        if (!destination.open(QIODevice::WriteOnly | QIODevice::NewOnly))
        {
            const QFileInfo currentDestination(
                qString(destinationPath.value())
                );
            if (currentDestination.exists() || currentDestination.isSymLink())
            {
                return Domain::Result<void>::failure({
                    .code = Domain::ErrorCode::Conflict,
                    .message = "The recovery destination already exists.",
                    .recoverable = true
                });
            }
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "The recovery destination could not be created: "
                    + toUtf8(destination.errorString()),
                .recoverable = true
            });
        }

        QByteArray buffer(64 * 1024, '\0');
        while (true)
        {
            const qint64 bytesRead = source.read(buffer.data(), buffer.size());
            if (bytesRead < 0)
            {
                return Domain::Result<void>::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = "The recovery source could not be read: "
                        + toUtf8(source.errorString()),
                    .recoverable = true
                });
            }
            if (bytesRead == 0)
            {
                if (source.error() != QFileDevice::NoError)
                {
                    return Domain::Result<void>::failure({
                        .code = Domain::ErrorCode::Technical,
                        .message = "The recovery source could not be read: "
                            + toUtf8(source.errorString()),
                        .recoverable = true
                    });
                }
                break;
            }

            qint64 bytesWritten = 0;
            while (bytesWritten < bytesRead)
            {
                const qint64 written = destination.write(
                    buffer.constData() + bytesWritten,
                    bytesRead - bytesWritten
                    );
                if (written <= 0)
                {
                    return Domain::Result<void>::failure({
                        .code = Domain::ErrorCode::Technical,
                        .message = "The recovery destination could not be written: "
                            + toUtf8(destination.errorString()),
                        .recoverable = true
                    });
                }
                bytesWritten += written;
            }
        }

        if (!destination.flush()
            || !destination.setPermissions(source.permissions()))
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "The copied recovery profile could not be finalized: "
                    + toUtf8(destination.errorString()),
                .recoverable = true
            });
        }
        return Domain::Result<void>::success();
    }

private:
    [[nodiscard]] static const QRegularExpression&
    normalCreateRecoveryNamePattern()
    {
        // Match the legacy target extraction exactly: the target is greedy,
        // accepts embedded newlines, and the UUID has the strict 8-4-4-4-12
        // hexadecimal shape without version/variant restrictions.
        static const QRegularExpression pattern(QStringLiteral(
            "^\\.([\\s\\S]+)\\.workspace-create-"
            "([0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-"
            "[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12})\\.recovery$"
            ));
        return pattern;
    }

    [[nodiscard]] static QString qString(const std::string& value)
    {
        return QString::fromUtf8(
            value.data(),
            static_cast<qsizetype>(value.size())
            );
    }

    [[nodiscard]] static std::string toUtf8(const QString& value)
    {
        const QByteArray encoded = value.toUtf8();
        return std::string(
            encoded.constData(),
            static_cast<std::size_t>(encoded.size())
            );
    }

    [[nodiscard]] static Application::FileDialogPath fileDialogPath(
        const QString& path
        )
    {
        return Application::FileDialogPath(toUtf8(path));
    }
};

} // namespace ClassMngr::Next::Platform
