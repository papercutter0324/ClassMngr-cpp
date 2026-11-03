#pragma once

#include <string>
#include <string_view>

namespace ClassMngr::Next::Application
{

// Stable, semantic identities for the long-lived Next page splitters. The
// platform adapter maps each identity to its own settings key.
enum class SplitterStateKey
{
    ClassesNavigatorContent,
    TestingClassesManager,
    SpeakingEvaluationContent,
    ClassTransferWorkflow
};

// Splitter state crosses the application boundary as an opaque binary-safe
// payload. Storage details belong to the platform adapter.
class SplitterStatePort
{
public:
    virtual ~SplitterStatePort() = default;

    [[nodiscard]] virtual std::string read(SplitterStateKey key) const = 0;
    virtual void write(
        SplitterStateKey key,
        std::string_view statePayload
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
