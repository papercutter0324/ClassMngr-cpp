#pragma once

#include <QString>

#include <expected>
#include <utility>

enum class RepositoryErrorCode : int
{
    InvalidInput = 1,
    NotFound = 2,
    Conflict = 3,
    Validation = 4,
    Technical = 5
};

struct RepositoryError
{
    explicit RepositoryError(QString diagnostic)
        : RepositoryError(
              RepositoryErrorCode::Technical,
              std::move(diagnostic)
              )
    {
    }

    RepositoryError(RepositoryErrorCode code, QString diagnostic)
        : code(code),
          diagnostic(std::move(diagnostic))
    {
    }

    RepositoryErrorCode code;
    QString diagnostic;
};

template<class T>
using RepositoryResult = std::expected<T, RepositoryError>;

using RepositoryStatus = RepositoryResult<void>;

// DataService still exposes QString failures. Keep that boundary conversion
// explicit so repository errors retain their stable category internally.
[[nodiscard]] inline QString toLegacyDiagnostic(
    const RepositoryError& error
    )
{
    return error.diagnostic;
}
