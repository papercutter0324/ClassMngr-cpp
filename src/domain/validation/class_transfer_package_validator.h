#pragma once

#include "core/result.h"
#include "domain/models/class_transfer.h"

// Normalizes and validates imported teacher and class details before either
// the compatibility service or the typed transfer adapter writes them.
class ClassTransferPackageValidator final
{
public:
    [[nodiscard]] static Result<ClassTransferPackage> normalizedAndValidated(
        const ClassTransferPackage& package
        );
};
