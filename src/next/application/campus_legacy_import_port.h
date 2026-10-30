#pragma once

#include "next/domain/operation_result.h"

#include <cstddef>

namespace ClassMngr::Next::Application
{

class CampusLegacyImportPort
{
public:
    virtual ~CampusLegacyImportPort() = default;

    [[nodiscard]] virtual Domain::Result<std::size_t> importLegacyCampuses() const = 0;
};

} // namespace ClassMngr::Next::Application
