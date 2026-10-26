#pragma once

#include "next/domain/operation_result.h"

#include <cassert>
#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>

namespace ClassMngr::Next::Application
{

enum class CommandBusErrorCode
{
    MissingHandler,
    DuplicateHandler,
    Unavailable,
    InvalidHandler
};

struct CommandBusError
{
    CommandBusErrorCode code;
    std::string message;
};

// Represents a bus-level outcome. For dispatch, a successful outer result
// contains the handler's Domain::Result<void> unchanged, keeping handler
// failures distinct from missing, duplicate, unavailable, or invalid-handler
// errors produced by the bus.
template <typename Value>
class CommandBusResult final
{
public:
    [[nodiscard]] static CommandBusResult success(
        Value value
        )
    {
        return CommandBusResult(std::move(value));
    }

    [[nodiscard]] static CommandBusResult failure(
        CommandBusError error
        )
    {
        return CommandBusResult(std::move(error));
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

    [[nodiscard]] const CommandBusError& error() const
    {
        assert(!hasValue());
        return std::get<CommandBusError>(m_storage);
    }

private:
    explicit CommandBusResult(
        Value value
        )
        : m_storage(std::move(value))
    {
    }

    explicit CommandBusResult(
        CommandBusError error
        )
        : m_storage(std::move(error))
    {
    }

    std::variant<Value, CommandBusError> m_storage;
};

template <>
class CommandBusResult<void> final
{
public:
    [[nodiscard]] static CommandBusResult success()
    {
        return CommandBusResult(std::monostate{});
    }

    [[nodiscard]] static CommandBusResult failure(
        CommandBusError error
        )
    {
        return CommandBusResult(std::move(error));
    }

    [[nodiscard]] bool hasValue() const
    {
        return std::holds_alternative<std::monostate>(m_storage);
    }

    explicit operator bool() const
    {
        return hasValue();
    }

    [[nodiscard]] const CommandBusError& error() const
    {
        assert(!hasValue());
        return std::get<CommandBusError>(m_storage);
    }

private:
    explicit CommandBusResult(
        std::monostate value
        )
        : m_storage(value)
    {
    }

    explicit CommandBusResult(
        CommandBusError error
        )
        : m_storage(std::move(error))
    {
    }

    std::variant<std::monostate, CommandBusError> m_storage;
};

// Synchronous application command dispatch keyed by the command's C++ type.
// The owner confines a bus instance to one thread; this class has no queue or
// internal synchronization.
class CommandBus final
{
public:
    using RegistrationResult = CommandBusResult<void>;
    using DispatchResult = CommandBusResult<Domain::Result<void>>;

    CommandBus() = default;
    CommandBus(const CommandBus&) = delete;
    CommandBus& operator=(const CommandBus&) = delete;
    CommandBus(CommandBus&&) = default;
    CommandBus& operator=(CommandBus&&) = default;

    template <typename Command, typename CanExecute, typename Execute>
    [[nodiscard]] RegistrationResult registerHandler(
        CanExecute&& canExecute,
        Execute&& execute
        )
    {
        using CommandType = std::remove_cvref_t<Command>;
        using Predicate = std::function<bool(const CommandType&)>;
        using Executor = std::function<Domain::Result<void>(const CommandType&)>;

        Predicate typedCanExecute(std::forward<CanExecute>(canExecute));
        Executor typedExecute(std::forward<Execute>(execute));
        if (!typedCanExecute || !typedExecute)
        {
            return RegistrationResult::failure(CommandBusError{
                .code = CommandBusErrorCode::InvalidHandler,
                .message = "Command handler callbacks must both be set."
            });
        }

        const std::type_index commandType(typeid(CommandType));
        if (m_handlers.contains(commandType))
        {
            return RegistrationResult::failure(CommandBusError{
                .code = CommandBusErrorCode::DuplicateHandler,
                .message = "A handler is already registered for this command type."
            });
        }

        m_handlers.emplace(
            commandType,
            std::make_unique<TypedHandler<CommandType>>(
                std::move(typedCanExecute),
                std::move(typedExecute)
                )
            );
        return RegistrationResult::success();
    }

    template <typename Command>
    [[nodiscard]] bool canExecute(
        const Command& command
        ) const
    {
        using CommandType = std::remove_cvref_t<Command>;
        const auto* handler = findHandler<CommandType>();
        return handler != nullptr && handler->canExecute(&command);
    }

    template <typename Command>
    [[nodiscard]] DispatchResult dispatch(
        const Command& command
        )
    {
        using CommandType = std::remove_cvref_t<Command>;
        auto* handler = findHandler<CommandType>();
        if (handler == nullptr)
        {
            return DispatchResult::failure(CommandBusError{
                .code = CommandBusErrorCode::MissingHandler,
                .message = "No handler is registered for this command type."
            });
        }

        if (!handler->canExecute(&command))
        {
            return DispatchResult::failure(CommandBusError{
                .code = CommandBusErrorCode::Unavailable,
                .message = "The registered command handler is not currently available."
            });
        }

        return DispatchResult::success(handler->execute(&command));
    }

private:
    class Handler
    {
    public:
        virtual ~Handler() = default;

        [[nodiscard]] virtual bool canExecute(const void* command) const = 0;
        [[nodiscard]] virtual Domain::Result<void> execute(const void* command) = 0;
    };

    template <typename Command>
    class TypedHandler final : public Handler
    {
    public:
        using Predicate = std::function<bool(const Command&)>;
        using Executor = std::function<Domain::Result<void>(const Command&)>;

        TypedHandler(
            Predicate canExecute,
            Executor execute
            )
            : m_canExecute(std::move(canExecute)),
              m_execute(std::move(execute))
        {
        }

        [[nodiscard]] bool canExecute(const void* command) const override
        {
            return m_canExecute(*static_cast<const Command*>(command));
        }

        [[nodiscard]] Domain::Result<void> execute(const void* command) override
        {
            return m_execute(*static_cast<const Command*>(command));
        }

    private:
        Predicate m_canExecute;
        Executor m_execute;
    };

    template <typename Command>
    [[nodiscard]] const auto* findHandler() const
    {
        const auto it = m_handlers.find(std::type_index(typeid(Command)));
        if (it == m_handlers.end())
        {
            return static_cast<const TypedHandler<Command>*>(nullptr);
        }
        return static_cast<const TypedHandler<Command>*>(it->second.get());
    }

    template <typename Command>
    [[nodiscard]] auto* findHandler()
    {
        const auto it = m_handlers.find(std::type_index(typeid(Command)));
        if (it == m_handlers.end())
        {
            return static_cast<TypedHandler<Command>*>(nullptr);
        }
        return static_cast<TypedHandler<Command>*>(it->second.get());
    }

    std::unordered_map<std::type_index, std::unique_ptr<Handler>> m_handlers;
};

}
