#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct CampusDirectoryEntry final
{
    Domain::CampusId id;
    std::string displayName;

    friend bool operator==(const CampusDirectoryEntry&, const CampusDirectoryEntry&) = default;
};

using CampusDirectoryReadResult = Domain::Result<std::vector<CampusDirectoryEntry>>;

class CampusDirectoryReadPort
{
public:
    virtual ~CampusDirectoryReadPort() = default;

    [[nodiscard]] virtual CampusDirectoryReadResult loadCampuses() const = 0;
};

} // namespace ClassMngr::Next::Application
