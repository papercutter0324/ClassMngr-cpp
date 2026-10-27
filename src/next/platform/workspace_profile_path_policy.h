#pragma once

#include "next/application/normal_workspace_creation_flow.h"
#include "next/application/workspace_open_profile_path_policy.h"

#include <QDir>
#include <QByteArray>
#include <QFileInfo>
#include <QString>

#include <string>

namespace ClassMngr::Next::Platform
{

// Shared native path rules for profile creation, opening, and Initial Setup.
// The callers retain their own suffix policies while sharing path validation,
// absolute-path handling, and case-insensitive .tps recognition.
class WorkspaceProfilePathPolicy final
    : public Application::WorkspaceProfilePathPolicyPort
    , public Application::WorkspaceOpenProfilePathPolicyPort
{
public:
    [[nodiscard]] Domain::Result<Application::WorkspaceLocation>
    normalizeNewProfilePath(
        const Application::FileDialogPath& path
        ) const override
    {
        return normalize(path, SuffixPolicy::NormalCreate);
    }

    [[nodiscard]] Domain::Result<Application::WorkspaceLocation>
    normalizeOpenProfilePath(
        const Application::FileDialogPath& path
        ) const override
    {
        QString selected = toQString(path.value()).trimmed();
        if (selected.isEmpty())
        {
            return Domain::Result<Application::WorkspaceLocation>::failure(
                invalidPath("Choose an existing Teacher Profile to open.")
                );
        }

        const QFileInfo selectedInfo(selected);
        if (selectedInfo.fileName().isEmpty())
        {
            return Domain::Result<Application::WorkspaceLocation>::failure(
                invalidPath("The selected profile must name a file.")
                );
        }
        if (!hasNativeProfileSuffix(selected))
        {
            return Domain::Result<Application::WorkspaceLocation>::failure(
                invalidPath("Only .tps Teacher Profiles can be opened here.")
                );
        }

        const QString canonical = selectedInfo.canonicalFilePath();
        const QString normalized = canonical.isEmpty()
            ? absolutePath(selected)
            : QDir::cleanPath(canonical);
        if (normalized.trimmed().isEmpty())
        {
            return Domain::Result<Application::WorkspaceLocation>::failure(
                invalidPath("The selected profile path could not be resolved.")
                );
        }

        return Domain::Result<Application::WorkspaceLocation>::success(
            Application::WorkspaceLocation(toUtf8(normalized))
            );
    }

    [[nodiscard]] bool sameLocation(
        const Application::WorkspaceLocation& left,
        const Application::WorkspaceLocation& right
        ) const override
    {
        QString leftPath = absolutePath(toQString(left.value()));
        QString rightPath = absolutePath(toQString(right.value()));
        const QString leftCanonical = QFileInfo(leftPath).canonicalFilePath();
        const QString rightCanonical = QFileInfo(rightPath).canonicalFilePath();
        if (!leftCanonical.isEmpty())
        {
            leftPath = leftCanonical;
        }
        if (!rightCanonical.isEmpty())
        {
            rightPath = rightCanonical;
        }
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
        return QString::compare(leftPath, rightPath, Qt::CaseInsensitive) == 0;
#else
        return leftPath == rightPath;
#endif
    }

    [[nodiscard]] static Domain::Result<Application::WorkspaceLocation>
    profileLocationForInitialSetupPath(
        const Application::FileDialogPath& path
        )
    {
        return normalize(path, SuffixPolicy::InitialSetup);
    }

    [[nodiscard]] static bool hasNativeProfileSuffix(const QString& path)
    {
        return QFileInfo(path).suffix().compare(
            QStringLiteral("tps"),
            Qt::CaseInsensitive
            ) == 0;
    }

private:
    enum class SuffixPolicy
    {
        NormalCreate,
        InitialSetup
    };

    [[nodiscard]] static Domain::Result<Application::WorkspaceLocation>
    normalize(
        const Application::FileDialogPath& path,
        const SuffixPolicy suffixPolicy
        )
    {
        QString selected = toQString(path.value()).trimmed();
        if (selected.isEmpty())
        {
            return Domain::Result<Application::WorkspaceLocation>::failure(
                invalidPath("Choose a destination for the profile.")
                );
        }

        QFileInfo selectedInfo(selected);
        if (selectedInfo.fileName().isEmpty())
        {
            return Domain::Result<Application::WorkspaceLocation>::failure(
                invalidPath("The profile destination must name a file.")
                );
        }

        if (suffixPolicy == SuffixPolicy::InitialSetup
            && selected.endsWith(QLatin1Char('.')))
        {
            selected.chop(1);
            selectedInfo.setFile(selected);
        }

        const QString suffix = selectedInfo.suffix();
        if (suffix.isEmpty())
        {
            selected += QStringLiteral(".tps");
        }
        else if (suffix.compare(QStringLiteral("tps"), Qt::CaseInsensitive) == 0)
        {
            // Preserve the selected .tps spelling, including its case.
        }
        else if (suffixPolicy == SuffixPolicy::NormalCreate)
        {
            if (suffix.compare(QStringLiteral("db"), Qt::CaseInsensitive) == 0)
            {
                selected.chop(3);
                selected += QStringLiteral(".tps");
            }
            else
            {
                selected += QStringLiteral(".tps");
            }
        }
        else
        {
            return Domain::Result<Application::WorkspaceLocation>::failure(
                invalidPath("ClassMngr profiles must use the .tps extension.")
                );
        }

        const QFileInfo normalizedInfo(selected);
        if (normalizedInfo.fileName().isEmpty())
        {
            return Domain::Result<Application::WorkspaceLocation>::failure(
                invalidPath("The profile destination must name a file.")
                );
        }
        const QString normalized = suffixPolicy == SuffixPolicy::InitialSetup
            ? QDir::cleanPath(QFileInfo(selected).absoluteFilePath())
            : absolutePath(selected);
        if (normalized.trimmed().isEmpty())
        {
            return Domain::Result<Application::WorkspaceLocation>::failure(
                invalidPath("The profile destination could not be resolved.")
                );
        }

        return Domain::Result<Application::WorkspaceLocation>::success(
            Application::WorkspaceLocation(toUtf8(normalized))
            );
    }

    [[nodiscard]] static QString absolutePath(const QString& path)
    {
        const QFileInfo info(path);
        const QFileInfo directoryInfo(info.absolutePath());
        const QString canonicalDirectory = directoryInfo.canonicalFilePath();
        const QString directory = canonicalDirectory.isEmpty()
            ? QDir::cleanPath(directoryInfo.absoluteFilePath())
            : canonicalDirectory;
        return QDir::cleanPath(QDir(directory).filePath(info.fileName()));
    }

    [[nodiscard]] static Domain::OperationError invalidPath(
        const char* message
        )
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::InvalidInput,
            .message = message,
            .recoverable = false
        };
    }

    [[nodiscard]] static QString toQString(const std::string& value)
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
};

} // namespace ClassMngr::Next::Platform
