#pragma once

#include "next/domain/operation_result.h"

#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

// Owned UTF-8 bytes for a Sub Prep personal-Zoom login ID. The value is
// intentionally permissive; validation and presentation belong at their
// respective boundaries.
class SubPrepPersonalZoomLoginId final
{
public:
    SubPrepPersonalZoomLoginId() = default;

    explicit SubPrepPersonalZoomLoginId(std::string value)
        : m_value(std::move(value))
    {
    }

    [[nodiscard]] const std::string& value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(
        const SubPrepPersonalZoomLoginId&,
        const SubPrepPersonalZoomLoginId&
        ) = default;

private:
    std::string m_value = "N/A";
};

// Owned UTF-8 bytes for a Sub Prep personal-Zoom password. It remains a
// separate type from the login ID while preserving the exact supplied bytes.
class SubPrepPersonalZoomPassword final
{
public:
    SubPrepPersonalZoomPassword() = default;

    explicit SubPrepPersonalZoomPassword(std::string value)
        : m_value(std::move(value))
    {
    }

    [[nodiscard]] const std::string& value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(
        const SubPrepPersonalZoomPassword&,
        const SubPrepPersonalZoomPassword&
        ) = default;

private:
    std::string m_value = "N/A";
};

// Typed Sub Prep personal-Zoom display settings. The adapter owns primary /
// legacy key selection and best-effort migration; the page owns presentation.
struct SubPrepPersonalZoomPreferences final
{
    SubPrepPersonalZoomLoginId loginId;
    SubPrepPersonalZoomPassword password;
    bool unavailable = true;

    friend bool operator==(
        const SubPrepPersonalZoomPreferences&,
        const SubPrepPersonalZoomPreferences&
        ) = default;
};

using SubPrepPersonalZoomPreferencesResult =
    Domain::Result<SubPrepPersonalZoomPreferences>;

class SubPrepPersonalZoomPreferencesPort
{
public:
    virtual ~SubPrepPersonalZoomPreferencesPort() = default;

    [[nodiscard]] virtual SubPrepPersonalZoomPreferencesResult load()
        const = 0;
};

} // namespace ClassMngr::Next::Application
