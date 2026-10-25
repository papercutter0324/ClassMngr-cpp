#pragma once

#include <QString>

#include <expected>

namespace ClassMngr::Next::Resources
{

enum class ResourceErrorCode
{
    InvalidArgument,
    InvalidCatalog,
    NotFound,
    TypeMismatch,
    UnsafePath,
    MissingFile,
    CorruptResource,
    IoError,
    UnsupportedOperation,
    DecodeError,
    RegistrationFailed
};

struct ResourceError final
{
    ResourceErrorCode code = ResourceErrorCode::IoError;
    QString message;
};

template <typename Value>
using ResourceResult = std::expected<Value, ResourceError>;

}
