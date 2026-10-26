#include "next/application/command_bus.h"

#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

struct AddCommand
{
    int amount;
};

struct GatedCommand
{
    bool enabled;
};

struct FailingCommand
{
};

struct DuplicateCommand
{
    int value;
};

struct InvalidCommand
{
};

struct CommandA
{
    int value;
};

struct CommandB
{
    int value;
};

bool successDispatchesCommand()
{
    CommandBus bus;
    int executedAmount = 0;
    const auto registration = bus.registerHandler<AddCommand>(
        [](const AddCommand&) { return true; },
        [&executedAmount](const AddCommand& command) {
            executedAmount = command.amount;
            return Domain::Result<void>::success();
        }
        );

    if (!registration.hasValue() || !bus.canExecute(AddCommand{.amount = 17}))
    {
        return false;
    }

    const auto dispatch = bus.dispatch(AddCommand{.amount = 17});
    return dispatch.hasValue()
        && dispatch.value().hasValue()
        && executedAmount == 17;
}

bool canExecuteTracksPredicateAndMissingHandler()
{
    CommandBus bus;
    if (bus.canExecute(GatedCommand{.enabled = true}))
    {
        return false;
    }

    const auto registration = bus.registerHandler<GatedCommand>(
        [](const GatedCommand& command) { return command.enabled; },
        [](const GatedCommand&) { return Domain::Result<void>::success(); }
        );

    return registration.hasValue()
        && bus.canExecute(GatedCommand{.enabled = true})
        && !bus.canExecute(GatedCommand{.enabled = false});
}

bool handlerErrorIsPreservedInsideDispatchResult()
{
    CommandBus bus;
    const Domain::OperationError expected{
        .code = Domain::ErrorCode::Validation,
        .message = "The command was rejected by the handler.",
        .recoverable = true
    };
    const auto registration = bus.registerHandler<FailingCommand>(
        [](const FailingCommand&) { return true; },
        [&expected](const FailingCommand&) {
            return Domain::Result<void>::failure(expected);
        }
        );
    if (!registration.hasValue())
    {
        return false;
    }

    const auto dispatch = bus.dispatch(FailingCommand{});
    return dispatch.hasValue()
        && !dispatch.value().hasValue()
        && dispatch.value().error() == expected;
}

bool missingHandlerFailsExplicitly()
{
    CommandBus bus;
    const auto dispatch = bus.dispatch(AddCommand{.amount = 0});
    return !dispatch.hasValue()
        && dispatch.error().code == CommandBusErrorCode::MissingHandler;
}

bool duplicateRegistrationDoesNotReplaceHandler()
{
    CommandBus bus;
    int firstCalls = 0;
    int secondCalls = 0;
    const auto firstRegistration = bus.registerHandler<DuplicateCommand>(
        [](const DuplicateCommand&) { return true; },
        [&firstCalls](const DuplicateCommand&) {
            ++firstCalls;
            return Domain::Result<void>::success();
        }
        );
    const auto duplicateRegistration = bus.registerHandler<DuplicateCommand>(
        [](const DuplicateCommand&) { return true; },
        [&secondCalls](const DuplicateCommand&) {
            ++secondCalls;
            return Domain::Result<void>::success();
        }
        );
    if (!firstRegistration.hasValue()
        || duplicateRegistration.hasValue()
        || duplicateRegistration.error().code != CommandBusErrorCode::DuplicateHandler)
    {
        return false;
    }

    const auto dispatch = bus.dispatch(DuplicateCommand{.value = 1});
    return dispatch.hasValue()
        && dispatch.value().hasValue()
        && firstCalls == 1
        && secondCalls == 0;
}

bool unavailableDispatchDoesNotCallHandler()
{
    CommandBus bus;
    int executeCalls = 0;
    const auto registration = bus.registerHandler<GatedCommand>(
        [](const GatedCommand&) { return false; },
        [&executeCalls](const GatedCommand&) {
            ++executeCalls;
            return Domain::Result<void>::success();
        }
        );
    if (!registration.hasValue())
    {
        return false;
    }

    const auto dispatch = bus.dispatch(GatedCommand{.enabled = true});
    return !dispatch.hasValue()
        && dispatch.error().code == CommandBusErrorCode::Unavailable
        && executeCalls == 0;
}

bool dispatchRechecksAvailabilityBeforeExecution()
{
    CommandBus bus;
    bool available = true;
    int executeCalls = 0;
    const auto registration = bus.registerHandler<GatedCommand>(
        [&available](const GatedCommand&) { return available; },
        [&executeCalls](const GatedCommand&) {
            ++executeCalls;
            return Domain::Result<void>::success();
        }
        );
    if (!registration.hasValue() || !bus.canExecute(GatedCommand{.enabled = true}))
    {
        return false;
    }

    available = false;
    const auto dispatch = bus.dispatch(GatedCommand{.enabled = true});
    return !dispatch.hasValue()
        && dispatch.error().code == CommandBusErrorCode::Unavailable
        && executeCalls == 0;
}

bool emptyCallbacksAreRejected()
{
    CommandBus bus;
    using Predicate = std::function<bool(const InvalidCommand&)>;
    using Executor = std::function<Domain::Result<void>(const InvalidCommand&)>;

    const auto missingPredicate = bus.registerHandler<InvalidCommand>(
        Predicate{},
        [](const InvalidCommand&) { return Domain::Result<void>::success(); }
        );
    const auto missingExecutor = bus.registerHandler<InvalidCommand>(
        [](const InvalidCommand&) { return true; },
        Executor{}
        );

    return !missingPredicate.hasValue()
        && missingPredicate.error().code == CommandBusErrorCode::InvalidHandler
        && !missingExecutor.hasValue()
        && missingExecutor.error().code == CommandBusErrorCode::InvalidHandler;
}

bool commandTypesRemainIsolated()
{
    CommandBus bus;
    int commandBCalls = 0;
    const auto registration = bus.registerHandler<CommandB>(
        [](const CommandB&) { return true; },
        [&commandBCalls](const CommandB&) {
            ++commandBCalls;
            return Domain::Result<void>::success();
        }
        );
    if (!registration.hasValue() || bus.canExecute(CommandA{.value = 9}))
    {
        return false;
    }

    const auto dispatchA = bus.dispatch(CommandA{.value = 9});
    const auto dispatchB = bus.dispatch(CommandB{.value = 9});
    return !dispatchA.hasValue()
        && dispatchA.error().code == CommandBusErrorCode::MissingHandler
        && dispatchB.hasValue()
        && dispatchB.value().hasValue()
        && commandBCalls == 1;
}

bool runTest(const char* name, bool (*test)())
{
    if (!test())
    {
        std::cerr << "Failed: " << name << '\n';
        return false;
    }
    return true;
}

} // namespace

int main()
{
    return runTest("success dispatch", successDispatchesCommand)
            && runTest("canExecute", canExecuteTracksPredicateAndMissingHandler)
            && runTest("handler error propagation", handlerErrorIsPreservedInsideDispatchResult)
            && runTest("missing handler", missingHandlerFailsExplicitly)
            && runTest("duplicate registration", duplicateRegistrationDoesNotReplaceHandler)
            && runTest("unavailable dispatch", unavailableDispatchDoesNotCallHandler)
            && runTest("dispatch rechecks availability", dispatchRechecksAvailabilityBeforeExecution)
            && runTest("invalid handler", emptyCallbacksAreRejected)
            && runTest("command type isolation", commandTypesRemainIsolated)
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}
