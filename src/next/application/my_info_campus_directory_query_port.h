#pragma once

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

// The My Information campus chooser needs only a stable identifier and its
// display label. Both values are owning UTF-8 strings; richer campus data and
// UI concerns remain outside this query contract.
struct MyInfoCampusMetadata final
{
    std::string id;
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
