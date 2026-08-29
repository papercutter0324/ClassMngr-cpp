#pragma once

#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace classmngr::engine
{

enum class ErrorCode
{
    InvalidArgument,
    InvalidFormat,
    NumericOverflow,
    Io,
    Database,
    Schema,
    Migration,
    Constraint,
    Unsupported,
    Cancelled,
    Internal
};

struct Error
{
    ErrorCode code;
    std::string message;
    std::optional<int> nativeCode;

    friend bool operator==(
        const Error& lhs,
        const Error& rhs
        ) = default;
};

[[nodiscard]] constexpr std::string_view errorCodeName(
    ErrorCode code
    ) noexcept
{
    switch (code)
    {
    case ErrorCode::InvalidArgument:
        return "invalid-argument";
    case ErrorCode::InvalidFormat:
        return "invalid-format";
    case ErrorCode::NumericOverflow:
        return "numeric-overflow";
    case ErrorCode::Io:
        return "io";
    case ErrorCode::Database:
        return "database";
    case ErrorCode::Schema:
        return "schema";
    case ErrorCode::Migration:
        return "migration";
    case ErrorCode::Constraint:
        return "constraint";
    case ErrorCode::Unsupported:
        return "unsupported";
    case ErrorCode::Cancelled:
        return "cancelled";
    case ErrorCode::Internal:
        return "internal";
    }

    return "unknown";
}

template<class T>
using Result = std::expected<T, Error>;

using Status = Result<void>;

} // namespace classmngr::engine
