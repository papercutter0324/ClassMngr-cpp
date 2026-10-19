#pragma once

#include "next/domain/domain_types.h"

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

// The Sub Prep page needs campus selection metadata and a few office details.
// The identifier is typed and remaining values are owning UTF-8 strings;
// unrelated campus record fields are excluded.
struct SubPrepCampusMetadata final
{
    Domain::CampusId id;
    std::string displayName;
    std::string officeNumber;
    std::string wifiName;
    std::string wifiPassword;
    std::string photocopierCode;
};

class SubPrepCampusDirectoryQueryPort
{
public:
    virtual ~SubPrepCampusDirectoryQueryPort() = default;

    [[nodiscard]] virtual std::vector<SubPrepCampusMetadata>
    loadCampuses() const = 0;
};

} // namespace ClassMngr::Next::Application
