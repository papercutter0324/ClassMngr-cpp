#pragma once

#include "domain/models/class_transfer.h"
#include "next/application/class_transfer_apply_request.h"
#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <vector>

namespace ClassMngr::Next::Application
{

struct ClassTransferApplyCommand final
{
    ::ClassTransferPackage package;
    ClassTransferApplyRequest choices;
};

struct ClassTransferApplySummary final
{
    std::vector<Domain::ClassId> createdClassIds;
    std::vector<Domain::ClassId> replacedClassIds;
    int skippedClassCount = 0;
};

using ClassTransferApplyResult = Domain::Result<ClassTransferApplySummary>;

class ClassTransferApplyPort
{
public:
    virtual ~ClassTransferApplyPort() = default;

    [[nodiscard]] virtual ClassTransferApplyResult apply(
        const ClassTransferApplyCommand& command
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
