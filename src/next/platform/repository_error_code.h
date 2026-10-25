#pragma once

#include "data/repositories/repository_error.h"
#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Platform
{

[[nodiscard]] inline Domain::ErrorCode repositoryDomainErrorCode(
    const RepositoryErrorCode code
    ) noexcept
{
    switch (code)
    {
    case RepositoryErrorCode::InvalidInput:
        return Domain::ErrorCode::InvalidInput;
    case RepositoryErrorCode::NotFound:
        return Domain::ErrorCode::NotFound;
    case RepositoryErrorCode::Conflict:
        return Domain::ErrorCode::Conflict;
    case RepositoryErrorCode::Validation:
        return Domain::ErrorCode::Validation;
    case RepositoryErrorCode::Technical:
        return Domain::ErrorCode::Technical;
    }
    return Domain::ErrorCode::Technical;
}

} // namespace ClassMngr::Next::Platform
