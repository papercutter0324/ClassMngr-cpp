#pragma once

namespace ClassMngr::Next::Application
{

// The application boundary carries only the typed class-selection reset
// policy. Persistence and legacy value conversion belong to the outer
// adapter.
enum class ClassSelectionResetPolicy
{
    OnApplicationClose,
    OnPageLeave
};

class ClassSelectionResetPolicyPort
{
public:
    virtual ~ClassSelectionResetPolicyPort() = default;

    [[nodiscard]] virtual ClassSelectionResetPolicy load() const = 0;

    virtual void save(
        ClassSelectionResetPolicy policy
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
