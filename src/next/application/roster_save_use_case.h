#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <charconv>
#include <string>
#include <system_error>
#include <vector>

namespace ClassMngr::Next::Application
{

struct RosterSnapshot final
{
    std::vector<std::u16string> columns;
    std::vector<int> columnWidths;
    std::vector<std::vector<std::u16string>> rows;

    friend bool operator==(
        const RosterSnapshot&,
        const RosterSnapshot&
        ) = default;
};

struct RosterSaveRequest final
{
    Domain::ClassId classId;
    RosterSnapshot roster;
    bool allowQuestionableKoreanNameLengths = false;

    friend bool operator==(
        const RosterSaveRequest&,
        const RosterSaveRequest&
        ) = default;
};

class RosterSavePort
{
public:
    virtual ~RosterSavePort() = default;

    [[nodiscard]] virtual Domain::Result<void> saveRoster(
        const RosterSaveRequest& request
        ) const = 0;
};

class RosterSaveUseCase final
{
public:
    [[nodiscard]] static Domain::Result<void> execute(
        const RosterSaveRequest& request,
        const RosterSavePort& port
        )
    {
        if (!isCanonicalPositiveClassId(request.classId.value()))
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Class ID must be a canonical positive integer.",
                .recoverable = true
            });
        }

        return port.saveRoster(request);
    }

private:
    [[nodiscard]] static bool isCanonicalPositiveClassId(
        const std::string& value
        )
    {
        if (value.empty() || value.front() < '1' || value.front() > '9')
        {
            return false;
        }

        int parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        return error == std::errc{}
            && end == value.data() + value.size()
            && parsed > 0
            && std::to_string(parsed) == value;
    }
};

} // namespace ClassMngr::Next::Application
