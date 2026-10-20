#pragma once

#include "next/application/class_transfer_package.h"
#include "next/application/class_transfer_apply_request.h"
#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <vector>

namespace ClassMngr::Next::Application
{

class ClassTransferSkippedClassCount final
{
public:
    constexpr ClassTransferSkippedClassCount() noexcept = default;

    explicit constexpr ClassTransferSkippedClassCount(const int value) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr int rawValue() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const ClassTransferSkippedClassCount&,
        const ClassTransferSkippedClassCount&
        ) noexcept = default;

private:
    int m_value = 0;
};

struct ClassTransferApplyCommand final
{
    ClassTransferPortablePackage package;
    ClassTransferApplyRequest choices;
};

struct ClassTransferApplySummary final
{
    std::vector<Domain::ClassId> createdClassIds;
    std::vector<Domain::ClassId> replacedClassIds;
    ClassTransferSkippedClassCount skippedClassCount;
};

using ClassTransferApplyResult = Domain::Result<ClassTransferApplySummary>;

class ClassTransferApplyPort
{
public:
    virtual ~ClassTransferApplyPort() = default;

    [[nodiscard]] virtual ClassTransferApplyResult apply(
        ClassTransferApplyCommand command
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
