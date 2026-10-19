#pragma once

#include "next/domain/domain_types.h"

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

// The My Information campus chooser needs only a stable identifier and its
// display label. The identifier is typed and the label is an owning UTF-8
// string; richer campus data and UI concerns remain outside this contract.
struct MyInfoCampusMetadata final
{
    Domain::CampusId id;
    std::string displayName;
};

class MyInfoCampusDirectoryQueryPort
{
public:
    virtual ~MyInfoCampusDirectoryQueryPort() = default;

    [[nodiscard]] virtual std::vector<MyInfoCampusMetadata>
    loadCampuses() const = 0;
};

} // namespace ClassMngr::Next::Application
