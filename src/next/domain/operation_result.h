#pragma once

#include <cassert>
#include <string>
#include <utility>
#include <variant>

namespace ClassMngr::Next::Domain
{

enum class ErrorCode
{
    InvalidInput,
    NotFound,
    Conflict,
    Validation,
    Canceled,
    Technical
};

struct OperationError
{
    ErrorCode code = ErrorCode::Technical;
    std::string message;
    bool recoverable = false;

    friend bool operator==(
        const OperationError&,
        const OperationError&
        ) = default;
};

template <typename Value>
class Result final
{
public:
    [[nodiscard]] static Result success(
        Value value
        )
    {
        return Result(std::move(value));
    }

    [[nodiscard]] static Result failure(
        OperationError error
        )
    {
        return Result(std::move(error));
    }

    [[nodiscard]] bool hasValue() const
    {
        return std::holds_alternative<Value>(m_storage);
    }

    explicit operator bool() const
    {
        return hasValue();
    }

    [[nodiscard]] const Value& value() const
    {
        assert(hasValue());
        return std::get<Value>(m_storage);
    }

    [[nodiscard]] Value& value()
    {
        assert(hasValue());
        return std::get<Value>(m_storage);
    }

    [[nodiscard]] const OperationError& error() const
    {
        assert(!hasValue());
        return std::get<OperationError>(m_storage);
    }

private:
    explicit Result(
        Value value
        )
        : m_storage(std::move(value))
    {
    }

    explicit Result(
        OperationError error
        )
        : m_storage(std::move(error))
    {
    }

    std::variant<Value, OperationError> m_storage;
};

template <>
class Result<void> final
{
public:
    [[nodiscard]] static Result success()
    {
        return Result(std::monostate{});
    }

    [[nodiscard]] static Result failure(
        OperationError error
        )
    {
        return Result(std::move(error));
    }

    [[nodiscard]] bool hasValue() const
    {
        return std::holds_alternative<std::monostate>(m_storage);
    }

    explicit operator bool() const
    {
        return hasValue();
    }

    [[nodiscard]] const OperationError& error() const
    {
        assert(!hasValue());
        return std::get<OperationError>(m_storage);
    }

private:
    explicit Result(
        std::monostate value
        )
        : m_storage(value)
    {
    }

    explicit Result(
        OperationError error
        )
        : m_storage(std::move(error))
    {
    }

    std::variant<std::monostate, OperationError> m_storage;
};

}
